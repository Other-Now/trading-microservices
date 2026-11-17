// risk-svc: pre-trade risk checks.
//
// Subscribes to md-svc for last prices, accepts connections from gateways and
// answers every NewOrder with an Ack (accept or reject + reason).
//
// It is stateless apart from the price cache, so instances are interchangeable:
// the gateway can send any order to any instance and replay to another one if
// an instance dies. (Per-account position limits would need sharding by
// account — deliberately out of scope.)
#include <csignal>

#include "../common/net.hpp"
#include "../common/risk_check.hpp"

using namespace tms;

int main(int argc, char** argv) {
    Args a(argc, argv);
    const uint16_t port = a.num("port", 7101);
    const uint32_t id = a.num("id", 1);
    const std::string md = a.str("md", "127.0.0.1:7201");
    const uint32_t nsym = a.num("symbols", 100);
    const bool batch = a.num("batch", 0);
    Limits L;
    L.stale_ns = a.num("stale-ms", 1000) * 1'000'000ull;
    log("risk", "id=%u port=%u md=%s batch=%d", id, port, md.c_str(), batch);

    asio::io_context io;
    IoStats st;
    std::vector<Quote> quotes(nsym);
    uint64_t checked = 0, accepted = 0;

    // Market-data subscription, reconnecting forever. Quotes survive a md-svc
    // outage but go stale after stale-ms, after which orders are rejected.
    std::function<void()> connect_md = [&] {
        connect_retry(io, md, [&](tcp::socket s) {
            log("risk", "id=%u connected to md", id);
            auto c = std::make_shared<Conn>(std::move(s), batch, &st);
            c->start(
                [&](Conn&, Msg& m) {
                    if (m.type == kTick && m.symbol < nsym) quotes[m.symbol] = {m.price, now_ns()};
                },
                [&](Conn&) {
                    log("risk", "id=%u lost md, reconnecting", id);
                    connect_md();
                });
        });
    };
    connect_md();

    auto acc = listen_on(io, port);
    accept_loop(*acc, [&](tcp::socket s) {
        auto c = std::make_shared<Conn>(std::move(s), batch, &st);
        log("risk", "id=%u gateway connected", id);
        c->start(
            [&](Conn& conn, Msg& m) {
                if (m.type != kNewOrder) return;
                m.t_risk_in = now_ns();
                const Quote* q = m.symbol < nsym ? &quotes[m.symbol] : nullptr;
                const Reason r = check_order(m, q, L, m.t_risk_in);
                m.type = kAck;
                m.reason = r;
                m.verdict = r == kOk ? kAccept : kReject;
                m.risk_id = id;
                ++checked;
                accepted += r == kOk;
                m.t_risk_out = now_ns();
                conn.send(m);
            },
            [id](Conn&) { log("risk", "id=%u gateway disconnected", id); });
    });

    asio::signal_set sig(io, SIGINT, SIGTERM);
    sig.async_wait([&](error_code, int) { io.stop(); });
    io.run();
    std::fprintf(stderr, "{\"svc\":\"risk\",\"id\":%u,\"checked\":%llu,\"accepted\":%llu}\n", id,
                 (unsigned long long)checked, (unsigned long long)accepted);
    print_io_stats("risk", st);
}
