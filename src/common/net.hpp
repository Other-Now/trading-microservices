// Small Boost.Asio layer shared by the three services.
//
// Every service is single-threaded: one io_context, one thread, callbacks.
// No locks anywhere. To get more throughput you run more instances — that is
// the point of splitting into services.
//
// Conn has two write modes (--batch flag on every service):
//   batch=false: one async_write (one send() syscall) per message.
//   batch=true : messages queued while a write is in flight are coalesced and
//                sent with one async_write when it completes.
#pragma once
#include <array>
#include <boost/asio.hpp>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "msg.hpp"

namespace tms {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using error_code = boost::system::error_code;

struct IoStats {
    uint64_t msgs_in = 0, msgs_out = 0, reads = 0, writes = 0;
};

class Conn : public std::enable_shared_from_this<Conn> {
public:
    using OnMsg = std::function<void(Conn&, Msg&)>;
    using OnClose = std::function<void(Conn&)>;

    Conn(tcp::socket s, bool batch, IoStats* st) : sock_(std::move(s)), batch_(batch), st_(st) {
        error_code ec;
        sock_.set_option(tcp::no_delay(true), ec);  // never let Nagle hold an order
    }

    void start(OnMsg on_msg, OnClose on_close) {
        on_msg_ = std::move(on_msg);
        on_close_ = std::move(on_close);
        do_read();
    }

    void send(const Msg& m) {
        if (closed_) return;
        ++st_->msgs_out;
        if (batch_) {
            const char* p = reinterpret_cast<const char*>(&m);
            out_.insert(out_.end(), p, p + sizeof(Msg));
        } else {
            q_.push_back(m);
        }
        if (!writing_) do_write();
    }

    void close() {
        if (closed_) return;
        closed_ = true;
        error_code ec;
        sock_.shutdown(tcp::socket::shutdown_both, ec);
        sock_.close(ec);
        if (on_close_) {
            auto cb = std::move(on_close_);
            on_close_ = nullptr;
            cb(*this);
        }
    }

    bool closed() const { return closed_; }
    uint32_t id = 0;

private:
    void do_read() {
        auto self = shared_from_this();
        sock_.async_read_some(asio::buffer(rbuf_.data() + rused_, rbuf_.size() - rused_),
                              [this, self](error_code ec, size_t n) {
                                  if (ec) return close();
                                  ++st_->reads;
                                  rused_ += n;
                                  size_t off = 0;
                                  while (rused_ - off >= sizeof(Msg)) {
                                      Msg m;
                                      std::memcpy(&m, rbuf_.data() + off, sizeof(Msg));
                                      off += sizeof(Msg);
                                      ++st_->msgs_in;
                                      on_msg_(*this, m);
                                      if (closed_) return;
                                  }
                                  if (off) {
                                      std::memmove(rbuf_.data(), rbuf_.data() + off, rused_ - off);
                                      rused_ -= off;
                                  }
                                  do_read();
                              });
    }

    void do_write() {
        auto self = shared_from_this();
        if (batch_) {
            if (out_.empty()) {
                writing_ = false;
                return;
            }
            writing_ = true;
            wbuf_.swap(out_);  // out_ is now empty and keeps collecting
            asio::async_write(sock_, asio::buffer(wbuf_), [this, self](error_code ec, size_t) {
                if (ec) return close();
                ++st_->writes;
                wbuf_.clear();
                do_write();
            });
        } else {
            if (q_.empty()) {
                writing_ = false;
                return;
            }
            writing_ = true;
            asio::async_write(sock_, asio::buffer(&q_.front(), sizeof(Msg)), [this, self](error_code ec, size_t) {
                if (ec) return close();
                ++st_->writes;
                q_.pop_front();
                do_write();
            });
        }
    }

    tcp::socket sock_;
    bool batch_;
    IoStats* st_;
    OnMsg on_msg_;
    OnClose on_close_;
    bool closed_ = false;
    bool writing_ = false;
    std::array<char, 1 << 16> rbuf_{};
    size_t rused_ = 0;
    std::deque<Msg> q_;              // batch=false
    std::vector<char> out_, wbuf_;   // batch=true
};

// ---- helpers --------------------------------------------------------------

// "--key value" argument parsing. Every service prints its args on start.
struct Args {
    std::map<std::string, std::string> kv;
    Args(int argc, char** argv) {
        for (int i = 1; i + 1 < argc; i += 2) kv[std::string(argv[i]).substr(2)] = argv[i + 1];
    }
    std::string str(const std::string& k, const std::string& d) const {
        auto it = kv.find(k);
        return it == kv.end() ? d : it->second;
    }
    long long num(const std::string& k, long long d) const {
        auto it = kv.find(k);
        return it == kv.end() ? d : std::stoll(it->second);
    }
};

inline std::pair<std::string, std::string> split_hostport(const std::string& s) {
    auto c = s.rfind(':');
    return {s.substr(0, c), s.substr(c + 1)};
}

inline void accept_loop(tcp::acceptor& acc, std::function<void(tcp::socket)> fn) {
    acc.async_accept([&acc, fn](error_code ec, tcp::socket s) {
        if (!ec) fn(std::move(s));
        if (acc.is_open()) accept_loop(acc, fn);
    });
}

inline std::unique_ptr<tcp::acceptor> listen_on(asio::io_context& io, uint16_t port) {
    auto acc = std::make_unique<tcp::acceptor>(io);
    tcp::endpoint ep(tcp::v4(), port);
    acc->open(ep.protocol());
    acc->set_option(asio::socket_base::reuse_address(true));
    acc->bind(ep);
    acc->listen(1024);
    return acc;
}

// Connect to host:port, retrying every 200 ms until it works (services start
// in any order; under Docker the DNS name may not exist yet).
inline void connect_retry(asio::io_context& io, std::string addr, std::function<void(tcp::socket)> on_ok) {
    auto [host, port] = split_hostport(addr);
    error_code ec;
    tcp::resolver r(io);
    auto eps = r.resolve(host, port, ec);
    auto sock = std::make_shared<tcp::socket>(io);
    auto retry = [&io, addr, on_ok]() {
        auto t = std::make_shared<asio::steady_timer>(io, std::chrono::milliseconds(200));
        t->async_wait([&io, addr, on_ok, t](error_code) { connect_retry(io, addr, on_ok); });
    };
    if (ec) return retry();
    asio::async_connect(*sock, eps, [sock, on_ok, retry](error_code ec2, const tcp::endpoint&) {
        if (ec2) return retry();
        on_ok(std::move(*sock));
    });
}

inline void log(const char* svc, const char* fmt, auto... a) {
    std::fprintf(stderr, "[%s] ", svc);
    std::fprintf(stderr, fmt, a...);
    std::fputc('\n', stderr);
}

inline void print_io_stats(const char* svc, const IoStats& s) {
    std::fprintf(stderr,
                 "{\"svc\":\"%s\",\"msgs_in\":%llu,\"msgs_out\":%llu,\"reads\":%llu,\"writes\":%llu,"
                 "\"msgs_per_write\":%.2f}\n",
                 svc, (unsigned long long)s.msgs_in, (unsigned long long)s.msgs_out,
                 (unsigned long long)s.reads, (unsigned long long)s.writes,
                 s.writes ? double(s.msgs_out) / s.writes : 0.0);
}

}  // namespace tms
