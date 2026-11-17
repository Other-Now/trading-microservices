// Wire format shared by every service: one fixed-size 96-byte message.
//
// Fixed size means no length prefix and no parser: a TCP byte stream is cut
// into messages every sizeof(Msg) bytes. All services run on the same machine
// architecture (x86-64, little-endian), so the struct is sent as raw bytes.
//
// The t_* fields are stamped by each service with CLOCK_MONOTONIC
// (std::chrono::steady_clock). All processes share one kernel, so these clocks
// are directly comparable — that is what makes per-hop latency possible.
#pragma once
#include <chrono>
#include <cstdint>
#include <type_traits>

namespace tms {

inline uint64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

enum MsgType : uint8_t { kNewOrder = 1, kAck = 2, kTick = 3 };
enum Verdict : uint8_t { kAccept = 0, kReject = 1 };
enum Reason : uint16_t {
    kOk = 0,
    kBadQty = 1,     // qty 0 or above max_qty
    kNoPrice = 2,    // risk has never seen a tick for this symbol
    kStale = 3,      // last tick older than stale limit (market-data down)
    kPriceBand = 4,  // limit price too far from last traded price
    kNotional = 5,   // price*qty above max notional
    kNoRisk = 6,     // gateway has no live risk instance
    kReasonCount = 7
};

inline const char* reason_name(uint16_t r) {
    static const char* n[] = {"ok", "bad_qty", "no_price", "stale", "price_band", "notional", "no_risk"};
    return r < kReasonCount ? n[r] : "?";
}

struct Msg {
    uint8_t type = 0;
    uint8_t verdict = 0;
    uint16_t reason = 0;
    uint32_t symbol = 0;
    uint64_t cl_ord_id = 0;  // client's id; gateway swaps in its own id while at risk
    uint32_t account = 0;
    uint32_t qty = 0;
    int64_t price = 0;  // in ticks of 0.01

    // Timestamps (ns, steady clock), one per hop boundary.
    uint64_t t_client_send = 0;  // loadgen: just before send()
    uint64_t t_gw_in = 0;        // gateway: order read from client
    uint64_t t_gw_out_req = 0;   // gateway: order handed to risk connection
    uint64_t t_risk_in = 0;      // risk: order read
    uint64_t t_risk_out = 0;     // risk: verdict handed to connection
    uint64_t t_gw_in_resp = 0;   // gateway: verdict read from risk
    uint64_t t_gw_out = 0;       // gateway: verdict handed to client connection
    uint32_t risk_id = 0;        // which risk instance answered
    uint32_t pad = 0;
};
static_assert(sizeof(Msg) == 96, "wire size is part of the protocol");
static_assert(std::is_trivially_copyable_v<Msg>);

}  // namespace tms
