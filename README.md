# trading-microservices

An order path split into three C++20 services: **gateway → risk ← market data**.
It is load-tested open-loop past a "12,000 req/s at < 20 ms" target, profiled
to its bottleneck, scaled, and killed under load.

```
loadgen ──NewOrder──▶ gw-svc ──NewOrder──▶ risk-svc ×N ◀──Tick── md-svc
   ▲                    │  ▲                   │
   └────────Ack─────────┘  └───────Ack─────────┘
```

| service | job |
|---|---|
| `md-svc` | Bounded random-walk prices for 100 symbols, pushed to every subscriber (snapshot on connect). |
| `risk-svc` | Checks qty limit, ±5% price band vs last tick, stale market data (> 1 s), and notional. Stateless apart from its price cache, so instances are interchangeable. |
| `gw-svc` | Accepts client orders, routes them round-robin to live risk instances, returns acks. Keeps an in-flight map per risk link and **replays** in-flight orders to a surviving instance if one dies. |
| `loadgen` | Open-loop load generator: latency is measured from each order's *scheduled* send time; per-hop breakdown; checks every ack for lost / duplicated / wrong verdicts. |

- **Services:** each one is single-threaded (one Boost.Asio `io_context`), with
  no locks. More throughput comes from more instances.
- **Wire format:** one fixed 96-byte struct over TCP, with `TCP_NODELAY`.
- **Fail closed:** with no live risk instance the gateway rejects (`no_risk`);
  with stale prices risk rejects (`stale`).

## Results

Run on an i5-9300H laptop, inside WSL2 with **4 vCPUs**, everything on one
machine over loopback. All numbers come from
[results/local/summary.md](results/local/summary.md), which
[tools/report.py](tools/report.py) generates from the raw JSON lines.

The same fix was also checked on different hardware: GitHub Actions, an AMD
EPYC 9V45 VM with 4 vCPUs, single reps. Those numbers are in
[results/ci/summary.md](results/ci/summary.md).

| | laptop VM (WSL2) | CI VM (EPYC) |
|---|---|---|
| naive writes: highest rate with p99 < 20 ms | 5k–25k (unstable), saturated by 30k | 50k, saturated at 75k |
| coalesced writes | ≥ 400k (p99 ~2–4.5 ms) | ≥ 400k (p99 **483 µs**) |
| docker compose, 12k/s through containers | — (no Docker) | p50 120 µs, p99 152 µs, 0 lost |
| `docker kill risk1` mid-load at 12k/s | — | 0 lost (2 replayed) |
| kill -9 risk @100k/s, 3 reps | see §2 | replay: 0 lost; control: 35 lost |

Sends are much cheaper on the CI VM, so at 12k/s the two modes perform the
same there (p99 ~0.4 ms). On both machines, coalescing raised the saturation
point at least 8×.

### 1. The bottleneck and the fix: coalesced writes

The first version did one `async_write`, and so one `send()` syscall, per
message.

| 2 risk, 1 gateway, 3 reps per rate | naive: 1 `send()` per message | coalesced writes |
|---|---|---|
| p99 < 20 ms in all 3 reps | only at 5k/s; 10k–25k/s swing 1 ms ↔ 450 ms between reps | every rate from 5k to **400k/s** (p99 0.6 → 2.1 ms) |
| saturated (queues grow all run) | p50 113 ms at 30k/s, 1.3 s at 40k/s | not within the sweep |
| 12k/s (the target), 30 s × 3 reps | p99 **63 ms** | p99 **6.9 ms** ¹ |
| lost / duplicated / wrong verdicts | 0 / 0 / 0 | 0 / 0 / 0 |

¹ The 12k/s run happened during a slow-host session (see §3). In the first
session, coalesced p99 at 10k–15k was 0.9–1.2 ms.

**How it was found.**
- **Which process is saturated?** Per-process CPU from `/proc/<pid>/stat`: the
  gateway is at ~95% of one core, and **75–85% of that is kernel time**. Each
  risk instance is at ~25%.
- **Which hop holds the delay?** The per-hop timestamps at 12k/s:

  | hop (p99) | naive | coalesced |
  |---|---:|---:|
  | gateway → risk | **34.6 ms** | 0.5 ms |
  | risk → gateway | 1.3 ms | 0.4 ms |
  | gateway → client | 5.9 ms | 0.4 ms |
  | inside gateway / inside risk | < 1 µs | < 1 µs |

  The backlog is in the gateway's outbound write queue.
- **User-space profile:** `perf` (user-only, since `perf_event_paranoid=2` and
  there's no root) shows Asio reactor plumbing, with ~9% of *user* time in
  `pthread_mutex_lock/unlock` (Asio's scheduler locks even with one thread).
  User time is ~15% of the total, so removing that lock is worth ≤ 1.5%. Not
  pursued. The CI job runs perf as root and records kernel stacks too.
- **Why each send costs ~10 µs of CPU:**
  - A loopback `send()` runs the receiver's side of the TCP stack in the
    sender's call, and wakes the reader on another vCPU.
  - Under Hyper-V that wake-up has to go through the hypervisor.
  - md-svc shows the same cost: ~20% of a core to send 20k ticks/s.

**The fix** ([src/common/net.hpp](src/common/net.hpp), `--batch 1`).
- While one write is in flight, new messages are appended to a buffer. When
  the write completes, the whole buffer goes out in one `send()`.
- It never *waits* to fill a batch: at light load each batch is one message.
- At 30k/s the gateway's messages per write rise only from 1.00 to 1.23. That
  is enough to stay out of the queueing collapse: p50 drops from 262 ms to
  0.4 ms, at the same gateway CPU.

### 2. Failover: `kill -9` a risk instance under load

2 risk instances, 20 s runs. risk1 is killed at t ≈ 6.2 s and restarted at
t ≈ 12.2 s. 5 reps per row.

| rate | gateway replay | orders sent | lost | dup | wrong | orders replayed |
|---|---|---:|---:|---:|---:|---:|
| 20k/s | **on** | 2,000,000 | **0** | 0 | 0 | 27 |
| 20k/s | off (control) | 2,000,000 | 35 | 0 | 0 | — |
| 100k/s | **on** | 10,000,000 | **0** | 0 | 0 | 187 |
| 100k/s | off (control) | 10,000,000 | 1,859 | 0 | 0 | — |

- **The two counts agree.** In the control runs, loadgen's lost count equals
  the gateway's own `dropped` counter exactly.
- **Failover doesn't drive the tail.** The worst 100 ms window of each
  replay-on rep is spread over the run (e.g. 3.1 s, 9.8 s, 14.2 s, 16.5 s), not
  at the kill.
- **The restarted instance rejoins** and gets its share of traffic again.
- **`kill -9` md-svc at 100k/s:** 0 lost. Risk keeps answering, and after the
  1 s stale limit rejects with `stale` (497,388 orders). It fails closed.
- **Why there are no duplicates:** an ack is forwarded only if its id is still
  in the in-flight map, and the dead instance's socket is gone, so its late
  acks can't arrive.
- **Not handled:** a *silent* failure (partition, or a hung process with an
  open socket) needs heartbeats and timeouts. They aren't built; detection
  here relies on the TCP connection closing.

### 3. Scaling: it didn't help on this box, and that's the result

Coalesced writes. Median p50 at **400k/s**, first session:

| | 1 gateway | 2 gateways |
|---|---:|---:|
| 1 risk | 512 µs | — |
| 2 risk | 582 µs | 1.1 ms |
| 4 risk | 704 µs | 1.7 ms |

- **More risk instances** don't raise throughput: risk is ~25% busy, and the
  gateway is the saturated process. They only add processes competing for 4
  vCPUs, so latency gets slightly worse.
- **A second gateway halves each gateway's load** (~60% CPU each), but p50
  roughly doubles. Two caveats:
  - The two-gateway runs use 8 loadgen connections (16 threads), and
    loadgen's own send lag p99 rises from ~0.3 ms to 1–2.7 ms. Part of the
    penalty is the load generator sharing the same 4 vCPUs.
  - Horizontal scaling needs cores the gateway isn't sharing. On a single
    4-vCPU VM there aren't any.
- **The ceiling for one gateway is 400–600k/s.** A third session (re-run of
  100k–600k) passed 400k in 3/3 reps (p99 2.7–4.5 ms) and 600k in 2/3.
- **Session variance is real.** A middle session failed 400k in every rep
  (p99 ~260–340 ms), but in that session loadgen's own send lag was 20–50×
  higher across the board, i.e. the host was slow. All three sessions are in
  `summary.md`. Don't quote a single run.

## Run it

```bash
sudo apt install g++ cmake libboost-dev     # Boost.Asio is header-only
BUILD_DIR=$HOME/tm-build bash scripts/build.sh && $HOME/tm-build/unit_tests

BATCH=1 bash scripts/stack.sh up 2          # md + 2 risk + gateway on :7001
$HOME/tm-build/loadgen --rate 12000 --secs 10
bash scripts/stack.sh kill-risk 1           # while loadgen runs
bash scripts/stack.sh down

bash scripts/bench-all.sh local             # everything in results/local (~1.5 h)

docker compose up -d && docker compose run --rm loadgen     # containers (runs in CI)
```

| script | does |
|---|---|
| `scripts/sweep.sh` | rate sweep up to saturation, N reps |
| `scripts/target.sh` | 12k/s × 30 s, both write modes, per-hop breakdown |
| `scripts/kill_test.sh` | risk kill with replay on/off, plus md kill |
| `scripts/profile.sh` | perf record/stat of the gateway + flame graphs |
| `tools/report.py` | JSON lines → `summary.md` (median over reps + range) |

CI ([.github/workflows/ci.yml](.github/workflows/ci.yml)):
- unit tests
- an ASan+UBSan build plus an end-to-end kill test under the sanitizers
- a benchmark job with perf kernel stacks (runs as root)
- the docker compose stack with `docker kill` mid-load

## Not built (on purpose)

- **gRPC / protobuf.** A fixed binary struct is the hot-path choice. gRPC
  fits control-plane APIs.
- **Kubernetes manifests.**
- **Per-account position limits.** These need state, so sharding by account
  and replicating that state for failover.
- **Gateway redundancy.** If the gateway dies, clients lose in-flight orders
  and must resend; the dedup then belongs downstream.
- **Heartbeat-based failure detection.**
- **Multi-host runs.** Everything is loopback on one VM, so the absolute
  latencies are laptop numbers.

Study notes for all of this: [docs/NOTES.md](docs/NOTES.md).
