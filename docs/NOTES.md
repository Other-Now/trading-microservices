# Study sheet: trading-microservices

A sheet for learning this project in 1–2 days. Read it top to bottom once, then
re-read the "Questions you will get" section before the interview.

## 1. What it is in one breath

Three C++20 services: **gateway**, **risk** and **market data**. Each is
single-threaded on Boost.Asio. They talk over TCP using one fixed 96-byte
binary message. An open-loop load generator drives the gateway and reports
latency from each order's *scheduled* send time, broken down per hop. On top of
that sit four results:

1. A rate sweep to saturation.
2. A before/after on the bottleneck that perf and `/proc` pointed to.
3. Scaling with risk and gateway instance counts.
4. A kill -9 failover test with a no-replay control.

## 2. The request path (memorise this)

```
loadgen ──NewOrder──▶ gw-svc ──NewOrder (gw id)──▶ risk-svc ◀──Tick── md-svc
   ▲                    │  ▲                          │
   └──────Ack───────────┘  └────────Ack───────────────┘
```

Timestamps written into the message as it travels (all on one CLOCK_MONOTONIC,
one kernel, so they can be subtracted directly):

| field | stamped by | hop it closes |
|---|---|---|
| `t_client_send` | loadgen, just before `send()` | — |
| `t_gw_in` | gw, order read | lg→gw (kernel + wakeup) |
| `t_gw_out_req` | gw, handed to risk conn | gw processing |
| `t_risk_in` | risk, order read | gw→risk |
| `t_risk_out` | risk, verdict ready | risk processing |
| `t_gw_in_resp` | gw, verdict read | risk→gw |
| `t_gw_out` | gw, handed to client conn | gw processing |
| (receive time) | loadgen | gw→lg |

"Handed to conn" means *queued*. Time waiting in the write queue is counted in
the next hop, not in processing.

## 3. Design choices and why

- **Fixed-size message, no framing.** A TCP stream gets cut every 96 bytes.
  There's no parser and nothing to allocate. The cost is no versioning, and it
  only works between machines with the same endianness. In real life you'd use
  SBE, protobuf or a length-prefixed header.
- **Single-threaded services.** There are no locks in the code. The way to get
  more throughput is more *instances*. That is the microservice argument, and
  the scaling result tests whether it holds on this box.
- **Risk is stateless** (apart from its price cache). So any order can go to
  any instance, and an order can be replayed on another instance after a
  crash. The honest gap: per-account position limits need state, which means
  sharding by account, and then failover needs the state replicated. That is
  not built.
- **Fail closed.**
  - If there is no live risk instance, the gateway rejects with `no_risk`
    instead of letting the order through unchecked.
  - If market data is older than 1 s, risk rejects with `stale`.
- **Failover = in-flight map + replay.** Every order sent to risk instance *i*
  sits in `links[i].inflight` until its ack arrives. If the connection closes,
  those orders are sorted by id (send order) and re-routed. An ack is forwarded
  only if its id is still in the map. That is why there are no duplicates: the
  dead instance's socket is gone, so its late acks can never arrive.
- **Detection = TCP close.** `kill -9` makes the kernel close the socket at
  once, so the gateway sees EOF/RST immediately. A *silent* failure (a
  partition, or the process hung while its socket stays open) is **not**
  detected. It would need heartbeats plus a per-order timeout. Say this
  before they ask.

## 4. Open-loop load generation (coordinated omission)

A closed-loop tester (send, wait for the reply, send the next) slows down when
the system slows down, so it never measures the stall it caused. In loadgen,
order *k* is due at `start + k·period` whatever happens. If the sender is late,
it sends every overdue order in one burst. Latency is `receive − scheduled`, so
a 50 ms stall shows up as 50 ms on every order that should have gone out during
it.

`sender_lag` (= actual send − scheduled) is reported separately. If it grows,
loadgen itself is the bottleneck and the run is invalid at that rate.

Correctness checks, done for every order:
- **Orders are a pure function of (conn, seq).** So the receiver recomputes
  the verdict the order *should* get (~1% bad qty, ~1% bad price) and counts
  `wrong`.
- **Lost** = sent − unique acks.
- **Dup** = acks for a seq already seen.

## 5. The bottleneck story (numbers in README / results/local/summary.md)

1. **Symptom.** In naive mode (one `async_write`, so one `send()`, per message),
   the system saturates at a low rate. Latency grows without bound for the
   whole run, which is the signature of a queue filling up.
2. **Who is saturated?** Per-service CPU from `/proc/<pid>/stat`
   (utime/stime): the gateway is at ~90% of one core. Each risk instance is at
   ~25%.
3. **Where does the gateway's time go?** Most of it is **sys** (kernel), not
   user.
   - perf on this box sees only user space (`perf_event_paranoid=2`, no root).
     The user-space flame graph is Asio reactor plumbing plus about 9%
     `pthread_mutex_lock/unlock`. That locking is Asio's scheduler, which
     locks even with one thread.
   - That 9% is of the *user* share (~15%), so removing it is worth ≤1.5%. Not
     worth doing. That's Amdahl's law in one line.
   - The CI job runs perf as root, which gives kernel stacks as well.
4. **Why is a send so expensive?**
   - Loopback TCP `send()` runs the *receiver's* side of the network stack in
     the sender's call: TCP receive, queue to the socket, wake the reader.
   - On a VM (WSL2 / Hyper-V), waking a thread on another vCPU means an
     inter-processor interrupt (IPI), which the hypervisor has to handle.
   - Measured: about 10 µs of CPU per send. md-svc spends ~20% of a core
     sending 20k ticks/s.
5. **Fix: coalesce writes.** While one `async_write` is in flight, new
   messages go into a buffer. When the write completes, everything in the
   buffer goes out in one `send()`.
   - This is **self-tuning**. At low load each read holds one order, so the
     batch is one message and latency is unchanged.
   - Under load one `recv()` returns dozens of orders, and their forwards
     leave in one `send()`.
   - The number to quote is msgs/write going from 1.0 to N at the profiled
     rate.
6. **What this means for real systems.** The same idea is behind `writev` /
   `sendmmsg`, Nagle (done badly: it waits on a timer), and kernel-bypass
   batching (DPDK bursts). The difference here: it never *waits* to fill a
   batch. It only batches what is already queued.

## 6. Scaling result: read it carefully

On a 4-vCPU box with every process on the same machine:
- **Adding risk instances doesn't raise the ceiling** if the gateway is the
  saturated process. Check the gw CPU column.
- **Adding gateways** means splitting clients across them, and that
  competes for the same 4 vCPUs with risk, md and loadgen.

Whatever the table shows, the lesson is "measure who is saturated before
scaling". Don't claim linear scaling unless the table shows it.

## 7. Questions you will get

- **"Why not gRPC?"**
  - gRPC is HTTP/2 framing plus protobuf, so every message pays for HPACK
    headers and a protobuf encode/decode.
  - For a fixed-schema, same-datacenter hot path, a fixed binary struct is
    what exchanges and SBE use.
  - gRPC is the right answer for control-plane APIs and for services owned
    by different teams.
- **"How would you run this in Kubernetes?"**
  - One Deployment per service; risk and md behind headless Services so the
    gateway sees each pod's IP.
  - Readiness probe = "md connected and quotes fresh".
  - Pin pods to cores with the CPU manager set to static.
  - Expect the per-hop latency to grow by the veth/bridge cost. The
    docker-compose CI job measures that.
- **"What if the gateway dies?"**
  - Clients lose their in-flight orders. They must reconnect and resend with
    the same client order id, so dedup has to happen at the *exchange/OMS*
    layer.
  - Not built here. The gateway is a single point of failure for its clients.
- **"Exactly-once?"** Exactly-once *ack delivery to the client* while the
  gateway lives. The work itself is at-least-once: a killed risk instance may
  already have checked the order. That's fine only because risk is stateless.
  With position limits, a replay could count an order twice.
- **"Ordering?"** Per connection, TCP keeps order. Across risk instances,
  round-robin can reorder an account's orders. A real system routes by
  account (consistent hashing) to keep per-account order.
- **"p99 vs p99.9 vs max?"**
  - The histogram is log-linear with ~1.6% bucket error, and it reports the
    bucket's *upper* edge, so it never under-reports.
  - Max is exact.
  - Report the median over reps *and* the range. Single runs on a laptop VM
    are noisy.
- **"Why steady_clock across processes?"** It is `CLOCK_MONOTONIC`. One kernel
  means one clock, so differences between processes are valid.
  - Containers share it too: there is no time namespace by default.
  - Across machines you would need PTP and hardware timestamps.
