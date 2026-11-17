// loadgen: open-loop load generator for gw-svc.
//
// Open loop: order k on connection c is *scheduled* at start + k*period. The
// sender never waits for replies; if it falls behind it sends every overdue
// order in one batch. Latency is measured from the scheduled time, so a stall
// anywhere (including in loadgen itself) shows up in the numbers instead of
// silently lowering the offered rate ("coordinated omission").
//
// Orders are a pure function of (connection, seq), so the receiver can work out
// the verdict each order *should* get and count wrong answers, plus lost
// (never acked) and duplicated (acked twice) orders.
//
// Plain POSIX sockets + 2 threads per connection (sender, receiver).
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "../common/hist.hpp"
#include "../common/msg.hpp"
#include "../common/risk_check.hpp"

using namespace tms;

namespace {

struct Opt {
    std::string host = "127.0.0.1";
    std::string port = "7001";  // comma list: connection i goes to port[i % n]
    double rate = 10000;  // orders/s, all connections together
    double secs = 10;
    double warmup = 1;  // seconds excluded from latency histograms (still counted for loss)
    int conns = 4;
    double bad_pct = 2;  // % of orders built to be rejected (half bad qty, half bad price)
    uint32_t nsym = 100;
    std::string json, ts;
    std::string label = "run";
};

constexpr int kHops = 7;
const char* kHopNames[kHops] = {"lg_to_gw", "gw_in",     "gw_to_risk", "risk",
                                "risk_to_gw", "gw_out", "gw_to_lg"};

uint64_t mix(uint64_t x) {  // splitmix64
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

struct Order {
    Msg m;
    Reason expect;
};

Order gen_order(int conn, uint64_t seq, const Opt& o) {
    const uint64_t h = mix((uint64_t(conn) << 40) ^ seq);
    Order r;
    r.m.type = kNewOrder;
    r.m.cl_ord_id = (uint64_t(conn) << 40) | seq;
    r.m.symbol = h % o.nsym;
    r.m.account = (h >> 8) % 64;
    r.m.qty = 1 + (h >> 16) % 1000;
    const int64_t base = base_price(r.m.symbol);
    r.m.price = base + (int64_t((h >> 26) % 201) - 100) * base / 10000;  // base +/-1%
    r.expect = kOk;
    const double u = double((h >> 40) % 1000000) / 10000.0;  // [0,100)
    if (u < o.bad_pct / 2) {
        r.m.qty = 5000;
        r.expect = kBadQty;
    } else if (u < o.bad_pct) {
        r.m.price = base * 3 / 2;
        r.expect = kPriceBand;
    }
    return r;
}

struct Conn {
    int idx = 0;
    int fd = -1;
    uint64_t n = 0;
    double period_ns = 0;
    uint64_t start = 0, offset = 0;
    std::atomic<bool> sending_done{false};
    std::atomic<uint64_t> last_send{0};
    // receiver-side results
    std::vector<uint8_t> acked;
    uint64_t unique = 0, dup = 0, wrong = 0, unknown = 0;
    uint64_t reasons[kReasonCount] = {};
    Hist e2e, e2e_send, lag, hop[kHops];
    std::vector<uint64_t> ts_cnt, ts_max;

    uint64_t sched(uint64_t seq) const { return start + offset + uint64_t(seq * period_ns); }
};

int dial(const Opt& o, int idx) {
    std::vector<std::string> ports;
    for (size_t p = 0, q; p <= o.port.size(); p = q + 1) {
        q = o.port.find(',', p);
        if (q == std::string::npos) q = o.port.size();
        ports.push_back(o.port.substr(p, q - p));
    }
    addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(o.host.c_str(), ports[idx % ports.size()].c_str(), &hints, &res) != 0) return -1;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(fd, res->ai_addr, res->ai_addrlen) != 0) {
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd >= 0) {
        int one = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
        timeval tv{0, 200000};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    }
    return fd;
}

void sleep_until(uint64_t t) {
    timespec ts{time_t(t / 1000000000ull), long(t % 1000000000ull)};
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, nullptr);
}

void sender(Conn& c, const Opt& o) {
    std::vector<Msg> batch;
    batch.reserve(512);
    uint64_t k = 0;
    while (k < c.n) {
        uint64_t now = now_ns();
        if (c.sched(k) > now) {
            sleep_until(c.sched(k));
            now = now_ns();
        }
        batch.clear();
        while (k < c.n && c.sched(k) <= now && batch.size() < 512) batch.push_back(gen_order(c.idx, k++, o).m);
        const uint64_t ts = now_ns();
        for (auto& m : batch) m.t_client_send = ts;
        const char* p = reinterpret_cast<const char*>(batch.data());
        size_t left = batch.size() * sizeof(Msg);
        while (left) {
            ssize_t w = ::send(c.fd, p, left, MSG_NOSIGNAL);
            if (w <= 0) {
                std::perror("send");
                c.sending_done = true;
                return;
            }
            p += w;
            left -= size_t(w);
        }
        c.last_send = now_ns();
    }
    c.sending_done = true;
}

void receiver(Conn& c, const Opt& o) {
    std::vector<char> buf(1 << 16);
    size_t used = 0;
    const uint64_t warm_end = c.start + uint64_t(o.warmup * 1e9);
    const uint64_t drain_ns = 3'000'000'000ull;
    while (c.unique < c.n) {
        ssize_t r = ::recv(c.fd, buf.data() + used, buf.size() - used, 0);
        if (r <= 0) {
            if (r == 0) break;  // gateway closed
            if (c.sending_done && now_ns() - c.last_send > drain_ns) break;
            continue;  // timeout, keep waiting
        }
        used += size_t(r);
        size_t off = 0;
        const uint64_t now = now_ns();
        for (; used - off >= sizeof(Msg); off += sizeof(Msg)) {
            Msg m;
            std::memcpy(&m, buf.data() + off, sizeof m);
            const uint64_t seq = m.cl_ord_id & ((1ull << 40) - 1);
            if (m.type != kAck || seq >= c.n || int(m.cl_ord_id >> 40) != c.idx) {
                ++c.unknown;
                continue;
            }
            if (c.acked[seq]) {
                ++c.dup;
                continue;
            }
            c.acked[seq] = 1;
            ++c.unique;
            if (m.reason < kReasonCount) ++c.reasons[m.reason];
            // stale / no_price / no_risk depend on the environment (md or risk
            // down), not on the order, so they are counted but not "wrong".
            const bool env = m.reason == kStale || m.reason == kNoPrice || m.reason == kNoRisk;
            if (!env && m.reason != gen_order(c.idx, seq, o).expect) ++c.wrong;

            const uint64_t s = c.sched(seq);
            const uint64_t lat = now - s;
            const size_t b = (s - c.start) / 100'000'000ull;  // 100 ms buckets
            if (b < c.ts_cnt.size()) {
                ++c.ts_cnt[b];
                c.ts_max[b] = std::max(c.ts_max[b], lat);
            }
            if (s < warm_end) continue;
            c.e2e.record(lat);
            c.e2e_send.record(now - m.t_client_send);
            c.lag.record(m.t_client_send - s);
            if (m.reason == kNoRisk) continue;  // never reached risk: no hop stamps
            const uint64_t hops[kHops] = {m.t_gw_in - m.t_client_send,  m.t_gw_out_req - m.t_gw_in,
                                          m.t_risk_in - m.t_gw_out_req, m.t_risk_out - m.t_risk_in,
                                          m.t_gw_in_resp - m.t_risk_out, m.t_gw_out - m.t_gw_in_resp,
                                          now - m.t_gw_out};
            for (int h = 0; h < kHops; ++h) c.hop[h].record(hops[h]);
        }
        std::memmove(buf.data(), buf.data() + off, used - off);
        used -= off;
    }
}

void put_hist(FILE* f, const char* name, const Hist& h, bool comma = true) {
    std::fprintf(f, "\"%s\":{\"p50\":%.1f,\"p90\":%.1f,\"p99\":%.1f,\"p999\":%.1f,\"max\":%.1f}%s", name,
                 h.percentile(50) / 1e3, h.percentile(90) / 1e3, h.percentile(99) / 1e3,
                 h.percentile(99.9) / 1e3, h.max() / 1e3, comma ? "," : "");
}

}  // namespace

int main(int argc, char** argv) {
    Opt o;
    for (int i = 1; i + 1 < argc; i += 2) {
        std::string k = argv[i], v = argv[i + 1];
        if (k == "--host") o.host = v;
        else if (k == "--port") o.port = v;
        else if (k == "--rate") o.rate = std::stod(v);
        else if (k == "--secs") o.secs = std::stod(v);
        else if (k == "--warmup") o.warmup = std::stod(v);
        else if (k == "--conns") o.conns = std::stoi(v);
        else if (k == "--bad-pct") o.bad_pct = std::stod(v);
        else if (k == "--symbols") o.nsym = std::stoul(v);
        else if (k == "--json") o.json = v;
        else if (k == "--ts") o.ts = v;
        else if (k == "--label") o.label = v;
        else {
            std::fprintf(stderr, "unknown flag %s\n", k.c_str());
            return 2;
        }
    }

    std::vector<Conn> cs(o.conns);
    const double per_conn = o.rate / o.conns;
    const uint64_t start = now_ns() + 200'000'000ull;  // give every thread time to start
    for (int i = 0; i < o.conns; ++i) {
        Conn& c = cs[i];
        c.idx = i;
        c.fd = dial(o, i);
        if (c.fd < 0) {
            std::fprintf(stderr, "loadgen: cannot connect to %s:%s\n", o.host.c_str(), o.port.c_str());
            return 1;
        }
        c.n = uint64_t(per_conn * o.secs);
        c.period_ns = 1e9 / per_conn;
        c.start = start;
        c.offset = uint64_t(c.period_ns * i / o.conns);  // spread connections evenly
        c.acked.assign(c.n, 0);
        const size_t buckets = size_t(o.secs * 10) + 2;
        c.ts_cnt.assign(buckets, 0);
        c.ts_max.assign(buckets, 0);
    }
    std::vector<std::thread> th;
    for (auto& c : cs) {
        th.emplace_back(sender, std::ref(c), std::cref(o));
        th.emplace_back(receiver, std::ref(c), std::cref(o));
    }
    for (auto& t : th) t.join();
    const double elapsed = (now_ns() - start) / 1e9;

    // merge
    uint64_t sent = 0, unique = 0, dup = 0, wrong = 0, unknown = 0, reasons[kReasonCount] = {};
    Hist e2e, e2e_send, lag, hop[kHops];
    for (auto& c : cs) {
        sent += c.n;
        unique += c.unique;
        dup += c.dup;
        wrong += c.wrong;
        unknown += c.unknown;
        for (int r = 0; r < kReasonCount; ++r) reasons[r] += c.reasons[r];
        e2e.merge(c.e2e);
        e2e_send.merge(c.e2e_send);
        lag.merge(c.lag);
        for (int h = 0; h < kHops; ++h) hop[h].merge(c.hop[h]);
        close(c.fd);
    }

    FILE* out = o.json.empty() ? stdout : std::fopen(o.json.c_str(), "a");
    std::fprintf(out,
                 "{\"label\":\"%s\",\"rate\":%.0f,\"secs\":%.1f,\"conns\":%d,\"sent\":%llu,\"acked\":%llu,"
                 "\"lost\":%llu,\"dup\":%llu,\"wrong\":%llu,\"unknown\":%llu,\"achieved_rps\":%.0f,\"reasons\":{",
                 o.label.c_str(), o.rate, o.secs, o.conns, (unsigned long long)sent, (unsigned long long)unique,
                 (unsigned long long)(sent - unique), (unsigned long long)dup, (unsigned long long)wrong,
                 (unsigned long long)unknown, unique / o.secs);
    for (int r = 0; r < kReasonCount; ++r)
        std::fprintf(out, "\"%s\":%llu%s", reason_name(r), (unsigned long long)reasons[r],
                     r + 1 < kReasonCount ? "," : "");
    std::fprintf(out, "},\"elapsed_s\":%.2f,\"us\":{", elapsed);
    put_hist(out, "e2e", e2e);
    put_hist(out, "e2e_from_send", e2e_send);
    put_hist(out, "sender_lag", lag);
    for (int h = 0; h < kHops; ++h) put_hist(out, kHopNames[h], hop[h], h + 1 < kHops);
    std::fprintf(out, "}}\n");
    if (out != stdout) std::fclose(out);

    if (!o.ts.empty()) {  // 100 ms time series: when were orders scheduled, how slow at worst
        FILE* f = std::fopen(o.ts.c_str(), "w");
        std::fprintf(f, "t_ms,acked,max_e2e_us\n");
        for (size_t b = 0; b < cs[0].ts_cnt.size(); ++b) {
            uint64_t n = 0, mx = 0;
            for (auto& c : cs) n += c.ts_cnt[b], mx = std::max(mx, c.ts_max[b]);
            std::fprintf(f, "%zu,%llu,%.1f\n", b * 100, (unsigned long long)n, mx / 1e3);
        }
        std::fclose(f);
    }
    std::fprintf(stderr, "loadgen %s: rate=%.0f sent=%llu acked=%llu lost=%llu dup=%llu wrong=%llu p50=%.1fus p99=%.1fus p99.9=%.1fus max=%.1fus\n",
                 o.label.c_str(), o.rate, (unsigned long long)sent, (unsigned long long)unique,
                 (unsigned long long)(sent - unique), (unsigned long long)dup, (unsigned long long)wrong,
                 e2e.percentile(50) / 1e3, e2e.percentile(99) / 1e3, e2e.percentile(99.9) / 1e3, e2e.max() / 1e3);
    return (sent == unique && dup == 0 && wrong == 0) ? 0 : 3;
}
