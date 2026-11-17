# Results: ci

```
Linux runnervm8axu0 7.0.0-1017-azure #17~24.04.1-Ubuntu SMP PREEMPT Thu Sep 24 18:46:57 UTC 2026 x86_64 x86_64 x86_64 GNU/Linux
4
Model name:                              AMD EPYC 9V45 96-Core Processor
g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
Tue Nov 17 10:21:41 UTC 2026
b01cdbf
```

## At the JD target rate (12,000 orders/s): per-hop latency

Hop = time between two timestamps carried in the message (same CLOCK_MONOTONIC, same kernel). Medians over reps of each run's p50 / p99.

| hop | 12k_b0 p50 / p99 | 12k_b1 p50 / p99 |
|---|---:|---:|
| lg_to_gw | 12 µs / 108 µs | 12 µs / 130 µs |
| gw_in | 0 µs / 0 µs | 0 µs / 0 µs |
| gw_to_risk | 12 µs / 61 µs | 12 µs / 62 µs |
| risk | 0 µs / 0 µs | 0 µs / 0 µs |
| risk_to_gw | 12 µs / 49 µs | 12 µs / 98 µs |
| gw_out | 0 µs / 0 µs | 0 µs / 0 µs |
| gw_to_lg | 12 µs / 74 µs | 12 µs / 61 µs |
| e2e_from_send | 50 µs / 364 µs | 49 µs / 430 µs |
| sender_lag | 58 µs / 141 µs | 57 µs / 158 µs |
| e2e | 108 µs / 512 µs | 106 µs / 590 µs |

12k_b0: 3 reps, sent 1,080,000, lost 0, dup 0, wrong 0

12k_b1: 3 reps, sent 1,080,000, lost 0, dup 0, wrong 0

## Rate sweeps

Sustained = 0 lost, achieved ≥ 99% of offered, e2e p99 < 20 ms. Cells: median over reps (range). A rep stops after the first rate with p99 > 1 s.

### b0_r2_g1: naive writes, 2 risk, 1 gateway

| offered/s | sustained reps | achieved/s | p50 | p99 (range) | p99.9 | max | lost | loadgen lag p99 | gw CPU user+sys |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 5,000 | 1/1 | 5,000 | 110 µs | 639 µs () | 1.0 ms | 1.8 ms | 0 | 225 µs | 1+13% |
| 10,000 | 1/1 | 10,000 | 108 µs | 401 µs () | 1.2 ms | 4.2 ms | 0 | 114 µs | 4+21% |
| 15,000 | 1/1 | 15,000 | 106 µs | 260 µs () | 1.1 ms | 3.2 ms | 0 | 73 µs | 9+28% |
| 20,000 | 1/1 | 20,000 | 108 µs | 762 µs () | 1.9 ms | 3.2 ms | 0 | 168 µs | 2+48% |
| 25,000 | 1/1 | 25,000 | 106 µs | 303 µs () | 631 µs | 4.2 ms | 0 | 133 µs | 7+52% |
| 30,000 | 1/1 | 30,000 | 113 µs | 410 µs () | 2.1 ms | 3.6 ms | 0 | 150 µs | 6+61% |
| 40,000 | 1/1 | 40,000 | 119 µs | 2.2 ms () | 4.6 ms | 6.8 ms | 0 | 170 µs | 7+75% |
| 50,000 | 1/1 | 50,000 | 170 µs | 17.3 ms () | 22.0 ms | 22.5 ms | 0 | 182 µs | 8+88% |
| 75,000 | 0/1 | 75,000 | 2.4 s | 3.8 s () | 3.8 s | 3.8 s | 0 | 172 µs | 8+90% |

### b1_r2_g1: coalesced writes, 2 risk, 1 gateway

| offered/s | sustained reps | achieved/s | p50 | p99 (range) | p99.9 | max | lost | loadgen lag p99 | gw CPU user+sys |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 5,000 | 1/1 | 5,000 | 106 µs | 180 µs () | 836 µs | 1.3 ms | 0 | 69 µs | 1+12% |
| 10,000 | 1/1 | 10,000 | 108 µs | 369 µs () | 950 µs | 3.8 ms | 0 | 103 µs | 0+25% |
| 15,000 | 1/1 | 15,000 | 108 µs | 647 µs () | 1.2 ms | 3.4 ms | 0 | 162 µs | 2+36% |
| 20,000 | 1/1 | 20,000 | 110 µs | 471 µs () | 1.1 ms | 4.1 ms | 0 | 108 µs | 5+45% |
| 25,000 | 1/1 | 25,000 | 106 µs | 148 µs () | 393 µs | 3.4 ms | 0 | 69 µs | 7+52% |
| 30,000 | 1/1 | 30,000 | 111 µs | 348 µs () | 729 µs | 3.7 ms | 0 | 102 µs | 5+63% |
| 40,000 | 1/1 | 40,000 | 114 µs | 442 µs () | 1.1 ms | 4.0 ms | 0 | 111 µs | 6+79% |
| 50,000 | 1/1 | 50,000 | 137 µs | 385 µs () | 1.1 ms | 4.2 ms | 0 | 94 µs | 7+90% |
| 75,000 | 1/1 | 75,000 | 252 µs | 397 µs () | 1.8 ms | 4.3 ms | 0 | 92 µs | 7+91% |
| 100,000 | 1/1 | 100,000 | 260 µs | 459 µs () | 1.0 ms | 4.3 ms | 0 | 102 µs | 7+91% |
| 150,000 | 1/1 | 150,000 | 270 µs | 426 µs () | 2.2 ms | 5.2 ms | 0 | 96 µs | 8+89% |
| 200,000 | 1/1 | 200,000 | 291 µs | 434 µs () | 1.6 ms | 4.2 ms | 0 | 94 µs | 8+90% |
| 250,000 | 1/1 | 250,000 | 303 µs | 475 µs () | 2.4 ms | 4.4 ms | 0 | 97 µs | 10+88% |
| 300,000 | 1/1 | 300,000 | 299 µs | 463 µs () | 1.9 ms | 4.5 ms | 0 | 92 µs | 10+88% |
| 400,000 | 1/1 | 400,000 | 299 µs | 483 µs () | 2.4 ms | 5.2 ms | 0 | 102 µs | 12+86% |

### Highest sustained rate per config

Highest rate where that rate *and every lower swept rate* met the target, per rep. `≥` = never failed up to the top of the sweep (ceiling not reached). 0 = failed at the lowest swept rate.

| config | lowest swept | per rep | median |
|---|---:|---|---:|
| naive writes, 2 risk, 1 gateway | 5,000 | 50,000 | 50,000/s |
| coalesced writes, 2 risk, 1 gateway | 5,000 | ≥400,000 | 400,000/s |

## Failure tests at 100,000 orders/s (kill_100k)

risk_kill: 2 risk instances, `kill -9` risk1 at t≈6.2 s, restarted at t≈12.2 s, 20 s run.

| gateway replay | reps | sent | lost | dup | wrong verdicts | orders replayed | p99 | worst max e2e |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| on | 3 | 6,000,000 | 0 | 0 | 0 | 54 | 479 µs | 6.9 ms |
| off (control) | 3 | 6,000,000 | 35 | 0 | 0 | 0 (35 dropped by gw) | 455 µs | 5.8 ms |

Worst 100 ms bucket per replay-on rep (t, max e2e): 7.9 s (6.6 ms); 19.1 s (5.1 ms); 9.6 s (6.9 ms)

md_kill: `kill -9` md-svc at t≈6.2 s (12 s run). sent 1,200,000, lost 0, dup 0, wrong 0; accepted 684,140, rejected stale 496,601, bad_qty 12,255, price_band 7,004.

## Gateway profile

| run | e2e p50 | e2e p99 | gw CPU user | gw CPU sys | gw msgs/write | gw msgs/read |
|---|---:|---:|---:|---:|---:|---:|
| profile_b0_30000 | 117 µs | 344 µs | 7% | 64% | 1.00 | 1.03 |
| profile_b1_30000 | 116 µs | 278 µs | 5% | 67% | 1.00 | 1.01 |

Top symbols, batch=0 (perf, user+kernel):
```
 18.91% [kernel.kallsyms] _raw_spin_unlock_irqrestore
  5.42% [kernel.kallsyms] finish_task_switch.isra.0
  2.57% [kernel.kallsyms] do_syscall_64
```

Top symbols, batch=1 (perf, user+kernel):
```
 18.68% [kernel.kallsyms] _raw_spin_unlock_irqrestore
  5.80% [kernel.kallsyms] finish_task_switch.isra.0
  2.74% [kernel.kallsyms] do_syscall_64
```

