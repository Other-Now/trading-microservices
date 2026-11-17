// Unit tests: risk rules, histogram accuracy, and message framing over a real
// loopback TCP connection in both write modes. No framework, just CHECK.
#include <cstdio>
#include <cstdlib>
#include <random>

#include "../src/common/hist.hpp"
#include "../src/common/net.hpp"
#include "../src/common/risk_check.hpp"

using namespace tms;

static int failures = 0;
#define CHECK(c)                                                         \
    do {                                                                 \
        if (!(c)) {                                                      \
            std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); \
            ++failures;                                                  \
        }                                                                \
    } while (0)

static Msg order(uint32_t qty, int64_t price) {
    Msg m;
    m.type = kNewOrder;
    m.qty = qty;
    m.price = price;
    return m;
}

static void test_risk_rules() {
    Limits L;  // max_qty 1000, band 5%, stale 1 s
    const uint64_t now = 10'000'000'000ull;
    Quote q{100000, now - 1000};
    CHECK(check_order(order(10, 100000), &q, L, now) == kOk);
    CHECK(check_order(order(0, 100000), &q, L, now) == kBadQty);
    CHECK(check_order(order(1001, 100000), &q, L, now) == kBadQty);
    CHECK(check_order(order(1000, 100000), &q, L, now) == kOk);
    // band edges: exactly 5% passes, one tick more fails
    CHECK(check_order(order(1, 105000), &q, L, now) == kOk);
    CHECK(check_order(order(1, 105001), &q, L, now) == kPriceBand);
    CHECK(check_order(order(1, 95000), &q, L, now) == kOk);
    CHECK(check_order(order(1, 94999), &q, L, now) == kPriceBand);
    // market data
    CHECK(check_order(order(1, 100000), nullptr, L, now) == kNoPrice);
    Quote never{};
    CHECK(check_order(order(1, 100000), &never, L, now) == kNoPrice);
    Quote old{100000, now - L.stale_ns - 1};
    CHECK(check_order(order(1, 100000), &old, L, now) == kStale);
    // qty is checked before market data (bad qty rejected even with no price)
    CHECK(check_order(order(5000, 100000), nullptr, L, now) == kBadQty);
    // notional
    Limits small = L;
    small.max_notional = 100000 * 10;
    CHECK(check_order(order(10, 100000), &q, small, now) == kOk);
    CHECK(check_order(order(11, 100000), &q, small, now) == kNotional);
}

static void test_hist() {
    Hist h;
    for (uint64_t v = 1; v <= 100000; ++v) h.record(v);
    CHECK(h.count() == 100000);
    CHECK(h.max() == 100000);
    // percentile is the bucket's upper edge: within 1/64 above the true value
    for (double p : {50.0, 90.0, 99.0, 99.9}) {
        const double truth = p / 100.0 * 100000;
        const double got = double(h.percentile(p));
        CHECK(got >= truth && got <= truth * (1 + 1.0 / 64) + 1);
    }
    for (uint64_t v : {0ull, 1ull, 127ull, 128ull, 1000ull, 123456789ull, ~0ull}) {
        const size_t i = Hist::index(v);
        CHECK(i < Hist::kBuckets);
        CHECK(Hist::upper(i) >= v);
    }
    Hist a, b;
    a.record(10);
    b.record(20);
    a.merge(b);
    CHECK(a.count() == 2 && a.max() == 20);
}

// Send N messages through a real TCP connection and check every one arrives,
// in order, intact — exercises partial reads and both write paths.
static void test_framing(bool batch) {
    asio::io_context io;
    IoStats st;
    auto acc = listen_on(io, 0);
    const uint16_t port = acc->local_endpoint().port();
    const int N = 200000;
    int got = 0;
    bool in_order = true;
    std::shared_ptr<Conn> server, client;
    accept_loop(*acc, [&](tcp::socket s) {
        server = std::make_shared<Conn>(std::move(s), batch, &st);
        server->start(
            [&](Conn&, Msg& m) {
                if (m.cl_ord_id != uint64_t(got) || m.qty != uint32_t(got * 7)) in_order = false;
                if (++got == N) io.stop();
            },
            [](Conn&) {});
    });
    connect_retry(io, "127.0.0.1:" + std::to_string(port), [&](tcp::socket s) {
        client = std::make_shared<Conn>(std::move(s), batch, &st);
        client->start([](Conn&, Msg&) {}, [](Conn&) {});
        for (int i = 0; i < N; ++i) {
            Msg m;
            m.cl_ord_id = i;
            m.qty = i * 7;
            client->send(m);
        }
    });
    asio::steady_timer guard(io, std::chrono::seconds(20));
    guard.async_wait([&](error_code) { io.stop(); });
    io.run();
    CHECK(got == N);
    CHECK(in_order);
    if (batch) CHECK(st.writes < uint64_t(N));  // coalescing actually happened
    // one write per message (io.stop() can beat the last completion handler)
    else CHECK(st.writes + 1 >= uint64_t(N));
    std::printf("  framing batch=%d: %d msgs, %llu writes, %llu reads\n", batch, got,
                (unsigned long long)st.writes, (unsigned long long)st.reads);
}

int main() {
    test_risk_rules();
    test_hist();
    test_framing(false);
    test_framing(true);
    if (failures) {
        std::printf("%d FAILED\n", failures);
        return 1;
    }
    std::printf("all unit tests passed\n");
}
