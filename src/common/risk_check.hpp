// The risk rules, as a pure function so they can be unit-tested without sockets.
#pragma once
#include <cstdint>
#include <cstdlib>

#include "msg.hpp"

namespace tms {

struct Limits {
    uint32_t max_qty = 1000;
    int64_t max_notional = 1'000'000'000;  // ticks (= Rs 1 crore)
    uint32_t band_bps = 500;               // +/- 5% of last price
    uint64_t stale_ns = 1'000'000'000;     // 1 s without a tick => stale
};

struct Quote {
    int64_t price = 0;
    uint64_t recv_ns = 0;
};

// Order of checks matters: cheap order-only checks first, then market data.
inline Reason check_order(const Msg& o, const Quote* q, const Limits& L, uint64_t now) {
    if (o.qty == 0 || o.qty > L.max_qty) return kBadQty;
    if (q == nullptr || q->price <= 0) return kNoPrice;
    if (now - q->recv_ns > L.stale_ns) return kStale;
    const int64_t diff = std::llabs(o.price - q->price);
    if (diff * 10000 > q->price * static_cast<int64_t>(L.band_bps)) return kPriceBand;
    if (static_cast<__int128>(o.price) * o.qty > L.max_notional) return kNotional;
    return kOk;
}

// Symbol universe shared by md-svc and loadgen: base price 1000.00 + 10.00*s.
inline int64_t base_price(uint32_t sym) { return 100000 + 1000 * static_cast<int64_t>(sym); }

}  // namespace tms
