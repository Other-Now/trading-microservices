// gw-svc: order-entry gateway.
//
// Clients connect and send NewOrders. The gateway gives each order an internal
// id, sends it to a live risk instance (round-robin), and when the Ack comes
// back restores the client's id and forwards it.
//
// Failover: every order sent to a risk instance stays in that instance's
// in-flight map until its Ack arrives. If the connection drops (instance
// killed), the in-flight orders are replayed — in original order — to the
// surviving instances. An Ack is only forwarded if the order is still in the
// map, so each order gets exactly one Ack: none lost, none duplicated.
// --replay 0 turns replay off (the "naive" control for the kill test).
//
// Limitation: a dead instance is detected by the TCP connection closing, which
// happens immediately on kill -9 / crash. A silent network partition would need
// heartbeats + timeouts, which are not implemented.
#include <algorithm>
#include <csignal>
#include <unordered_map>

#include "../common/net.hpp"

using namespace tms;

struct Pending {
    uint32_t client;
    uint64_t client_ord_id;
    Msg m;
};

struct RiskLink {
    std::string addr;
    std::shared_ptr<Conn> c;  // null while down
    std::unordered_map<uint64_t, Pending> inflight;
    uint64_t sent = 0;
};

int main(int argc, char** argv) {
    Args a(argc, argv);
    const uint16_t port = a.num("port", 7001);
    const std::string risk_csv = a.str("risk", "127.0.0.1:7101");
    const bool batch = a.num("batch", 0);
    const bool replay = a.num("replay", 1);
    log("gw", "port=%u risk=%s batch=%d replay=%d", port, risk_csv.c_str(), batch, replay);

    asio::io_context io;
    IoStats st;
    std::vector<RiskLink> links;
    for (size_t p = 0, q; p <= risk_csv.size(); p = q + 1) {
        q = risk_csv.find(',', p);
        if (q == std::string::npos) q = risk_csv.size();
        links.push_back({risk_csv.substr(p, q - p), nullptr, {}, 0});
    }
    std::unordered_map<uint32_t, std::shared_ptr<Conn>> clients;
    uint32_t next_client = 0;
    uint64_t next_id = 0;
    size_t rr = 0;
    uint64_t orders_in = 0, replayed = 0, dropped = 0, no_risk = 0, orphan_acks = 0, stray_acks = 0;

    auto deliver = [&](uint32_t client, Msg& m) {
        auto it = clients.find(client);
        if (it == clients.end()) {
            ++orphan_acks;
            return;
        }
        m.t_gw_out = now_ns();
        it->second->send(m);
    };

    auto route = [&](Pending p) {
        RiskLink* L = nullptr;
        for (size_t k = 0; k < links.size() && !L; ++k) {
            RiskLink& cand = links[rr++ % links.size()];
            if (cand.c) L = &cand;
        }
        if (!L) {  // fail closed: no risk instance => reject, never pass unchecked
            ++no_risk;
            Msg r = p.m;
            r.type = kAck;
            r.verdict = kReject;
            r.reason = kNoRisk;
            r.cl_ord_id = p.client_ord_id;
            deliver(p.client, r);
            return;
        }
        const uint64_t id = ++next_id;
        p.m.cl_ord_id = id;
        p.m.t_gw_out_req = now_ns();
        L->c->send(p.m);
        L->inflight.emplace(id, p);
        ++L->sent;
    };

    std::function<void(size_t)> connect_risk = [&](size_t i) {
        connect_retry(io, links[i].addr, [&, i](tcp::socket s) {
            auto c = std::make_shared<Conn>(std::move(s), batch, &st);
            links[i].c = c;
            log("gw", "risk %s up", links[i].addr.c_str());
            c->start(
                [&, i](Conn&, Msg& m) {
                    if (m.type != kAck) return;
                    auto& inf = links[i].inflight;
                    auto it = inf.find(m.cl_ord_id);
                    if (it == inf.end()) {
                        ++stray_acks;
                        return;
                    }
                    const Pending p = it->second;
                    inf.erase(it);
                    m.t_gw_in_resp = now_ns();
                    m.cl_ord_id = p.client_ord_id;
                    deliver(p.client, m);
                },
                [&, i](Conn&) {
                    RiskLink& L = links[i];
                    L.c.reset();
                    std::vector<std::pair<uint64_t, Pending>> lost(L.inflight.begin(), L.inflight.end());
                    L.inflight.clear();
                    std::sort(lost.begin(), lost.end(),
                              [](auto& x, auto& y) { return x.first < y.first; });  // keep send order
                    log("gw", "risk %s DOWN with %zu in flight -> %s", L.addr.c_str(), lost.size(),
                        replay ? "replaying" : "dropping");
                    for (auto& [id, p] : lost) {
                        if (replay) {
                            ++replayed;
                            route(p);
                        } else {
                            ++dropped;
                        }
                    }
                    connect_risk(i);
                });
        });
    };
    for (size_t i = 0; i < links.size(); ++i) connect_risk(i);

    auto acc = listen_on(io, port);
    accept_loop(*acc, [&](tcp::socket s) {
        auto c = std::make_shared<Conn>(std::move(s), batch, &st);
        c->id = ++next_client;
        clients[c->id] = c;
        log("gw", "client %u connected", c->id);
        c->start(
            [&](Conn& conn, Msg& m) {
                if (m.type != kNewOrder) return;
                m.t_gw_in = now_ns();
                ++orders_in;
                route(Pending{conn.id, m.cl_ord_id, m});
            },
            [&](Conn& conn) { clients.erase(conn.id); });
    });

    asio::signal_set sig(io, SIGINT, SIGTERM);
    sig.async_wait([&](error_code, int) { io.stop(); });
    io.run();
    std::fprintf(stderr,
                 "{\"svc\":\"gw\",\"orders_in\":%llu,\"replayed\":%llu,\"dropped\":%llu,\"no_risk\":%llu,"
                 "\"orphan_acks\":%llu,\"stray_acks\":%llu,\"per_risk\":[",
                 (unsigned long long)orders_in, (unsigned long long)replayed, (unsigned long long)dropped,
                 (unsigned long long)no_risk, (unsigned long long)orphan_acks, (unsigned long long)stray_acks);
    for (size_t i = 0; i < links.size(); ++i)
        std::fprintf(stderr, "%s%llu", i ? "," : "", (unsigned long long)links[i].sent);
    std::fprintf(stderr, "]}\n");
    print_io_stats("gw", st);
}
