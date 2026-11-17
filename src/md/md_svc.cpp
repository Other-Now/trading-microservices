// md-svc: market-data service.
//
// Generates a bounded random walk for N symbols and pushes Tick messages to
// every subscriber (the risk instances). A new subscriber first gets a full
// snapshot so it can price every symbol immediately.
#include <csignal>
#include <set>

#include "../common/net.hpp"
#include "../common/risk_check.hpp"

using namespace tms;

int main(int argc, char** argv) {
    Args a(argc, argv);
    const uint16_t port = a.num("port", 7201);
    const uint32_t nsym = a.num("symbols", 100);
    const long rate = a.num("rate", 10000);  // ticks per second, all symbols together
    const bool batch = a.num("batch", 0);
    log("md", "port=%u symbols=%u rate=%ld batch=%d", port, nsym, rate, batch);

    asio::io_context io;
    IoStats st;
    std::vector<int64_t> px(nsym);
    for (uint32_t s = 0; s < nsym; ++s) px[s] = base_price(s);
    std::set<std::shared_ptr<Conn>> subs;
    uint32_t next_id = 0;

    auto make_tick = [&](uint32_t s) {
        Msg m;
        m.type = kTick;
        m.symbol = s;
        m.price = px[s];
        m.t_client_send = now_ns();
        return m;
    };

    auto acc = listen_on(io, port);
    accept_loop(*acc, [&](tcp::socket s) {
        auto c = std::make_shared<Conn>(std::move(s), batch, &st);
        c->id = ++next_id;
        subs.insert(c);
        log("md", "subscriber %u connected (%zu total)", c->id, subs.size());
        c->start([](Conn&, Msg&) {},
                 [&subs, c](Conn& cc) {
                     log("md", "subscriber %u gone", cc.id);
                     subs.erase(c);
                 });
        for (uint32_t s = 0; s < nsym; ++s) c->send(make_tick(s));  // snapshot
    });

    // Every 1 ms publish rate/1000 ticks. Prices move +/-5 ticks and are
    // clamped to base +/-2%, so a client pricing off base +/-1% always stays
    // inside the risk service's +/-5% band (makes verdicts predictable).
    uint64_t rng = 0x9E3779B97F4A7C15ull;
    uint32_t cursor = 0;
    const long per_ms = std::max(1L, rate / 1000);
    asio::steady_timer timer(io);
    std::function<void()> tick = [&] {
        timer.expires_after(std::chrono::milliseconds(1));
        timer.async_wait([&](error_code ec) {
            if (ec) return;
            for (long i = 0; i < per_ms; ++i) {
                const uint32_t s = cursor++ % nsym;
                rng ^= rng << 13, rng ^= rng >> 7, rng ^= rng << 17;
                const int64_t base = base_price(s), lim = base / 50;
                px[s] = std::clamp<int64_t>(px[s] + static_cast<int64_t>(rng % 11) - 5, base - lim, base + lim);
                const Msg m = make_tick(s);
                for (auto& c : subs) c->send(m);
            }
            tick();
        });
    };
    tick();

    asio::signal_set sig(io, SIGINT, SIGTERM);
    sig.async_wait([&](error_code, int) { io.stop(); });
    io.run();
    print_io_stats("md", st);
}
