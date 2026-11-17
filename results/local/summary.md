# Results: .

```
Linux MANPRIT01 6.18.33.2-microsoft-standard-WSL2 #1 SMP PREEMPT_DYNAMIC Thu Jun 18 21:54:43 UTC 2026 x86_64 x86_64 x86_64 GNU/Linux
4
Model name:                              Intel(R) Core(TM) i5-9300H CPU @ 2.40GHz
g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
Tue Nov 17 08:59:40 UTC 2026
```

## At the JD target rate (12,000 orders/s): per-hop latency

Hop = time between two timestamps carried in the message (same CLOCK_MONOTONIC, same kernel). Medians over reps of each run's p50 / p99.

| hop | 12k_b0 p50 / p99 | 12k_b1 p50 / p99 |
|---|---:|---:|
| lg_to_gw | 61 µs / 4.5 ms | 58 µs / 1.7 ms |
| gw_in | 0 µs / 0 µs | 0 µs / 0 µs |
| gw_to_risk | 48 µs / 34.6 ms | 46 µs / 508 µs |
| risk | 0 µs / 0 µs | 0 µs / 0 µs |
| risk_to_gw | 56 µs / 1.3 ms | 56 µs / 414 µs |
| gw_out | 0 µs / 0 µs | 0 µs / 0 µs |
| gw_to_lg | 44 µs / 5.9 ms | 44 µs / 369 µs |
| e2e_from_send | 221 µs / 60.3 ms | 205 µs / 4.3 ms |
| sender_lag | 86 µs / 2.5 ms | 86 µs / 1.0 ms |
| e2e | 315 µs / 63.4 ms | 295 µs / 6.9 ms |

12k_b0: 3 reps, sent 1,080,000, lost 0, dup 0, wrong 0

12k_b1: 3 reps, sent 1,080,000, lost 0, dup 0, wrong 0

## Rate sweeps

Sustained = 0 lost, achieved ≥ 99% of offered, e2e p99 < 20 ms. Cells: median over reps (range). A rep stops after the first rate with p99 > 1 s.

### b0_r2_g1: naive writes, 2 risk, 1 gateway

| offered/s | sustained reps | achieved/s | p50 | p99 (range) | p99.9 | max | lost | loadgen lag p99 | gw CPU user+sys |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 5,000 | 3/3 | 5,000 | 242 µs | 1.5 ms (688 µs–5.5 ms) | 24.1 ms | 44.2 ms | 0 | 311 µs | 6+35% |
| 10,000 | 1/3 | 10,000 | 287 µs | 59.2 ms (991 µs–61.3 ms) | 90.2 ms | 123.2 ms | 0 | 4.7 ms | 11+60% |
| 15,000 | 2/3 | 15,000 | 315 µs | 4.9 ms (4.5 ms–37.7 ms) | 20.2 ms | 71.3 ms | 0 | 451 µs | 11+67% |
| 20,000 | 1/3 | 20,000 | 364 µs | 73.4 ms (3.6 ms–448.8 ms) | 99.6 ms | 464.5 ms | 0 | 803 µs | 12+76% |
| 25,000 | 3/3 | 25,000 | 389 µs | 7.4 ms (2.9 ms–9.7 ms) | 10.9 ms | 17.0 ms | 0 | 287 µs | 13+80% |
| 30,000 | 1/3 | 30,000 | 113.2 ms | 293.6 ms (7.4 ms–763.4 ms) | 327.2 ms | 792.8 ms | 0 | 434 µs | 14+81% |
| 40,000 | 0/3 | 40,000 | 1.3 s | 4.0 s (2.8 s–4.8 s) | 4.1 s | 4.8 s | 0 | 5.3 ms | 13+87% |

### b1_r1_g1: coalesced writes, 1 risk, 1 gateway

| offered/s | sustained reps | achieved/s | p50 | p99 (range) | p99.9 | max | lost | loadgen lag p99 | gw CPU user+sys |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 50,000 | 3/3 | 50,000 | 397 µs | 1.3 ms (1.2 ms–1.4 ms) | 4.1 ms | 8.0 ms | 0 | 207 µs | 14+81% |
| 100,000 | 3/3 | 100,000 | 422 µs | 1.5 ms (1.3 ms–1.6 ms) | 4.4 ms | 10.2 ms | 0 | 201 µs | 14+81% |
| 150,000 | 3/3 | 150,000 | 430 µs | 1.5 ms (1.3 ms–1.5 ms) | 5.4 ms | 11.0 ms | 0 | 221 µs | 16+82% |
| 200,000 | 3/3 | 200,000 | 446 µs | 1.6 ms (1.6 ms–2.1 ms) | 4.5 ms | 12.8 ms | 0 | 236 µs | 18+79% |
| 250,000 | 2/3 | 250,000 | 459 µs | 1.9 ms (1.7 ms–43.5 ms) | 7.1 ms | 84.0 ms | 0 | 246 µs | 19+78% |
| 300,000 | 3/3 | 300,000 | 492 µs | 2.3 ms (1.8 ms–2.5 ms) | 6.7 ms | 15.9 ms | 0 | 324 µs | 20+74% |
| 400,000 | 3/3 | 400,000 | 512 µs | 2.7 ms (2.3 ms–3.0 ms) | 9.2 ms | 15.5 ms | 0 | 360 µs | 23+74% |

### b1_r2_g1: coalesced writes, 2 risk, 1 gateway

| offered/s | sustained reps | achieved/s | p50 | p99 (range) | p99.9 | max | lost | loadgen lag p99 | gw CPU user+sys |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 5,000 | 3/3 | 5,000 | 236 µs | 582 µs (541 µs–746 µs) | 2.9 ms | 6.8 ms | 0 | 135 µs | 7+36% |
| 10,000 | 3/3 | 10,000 | 274 µs | 934 µs (827 µs–1.6 ms) | 2.9 ms | 34.0 ms | 0 | 176 µs | 9+63% |
| 15,000 | 3/3 | 15,000 | 303 µs | 1.2 ms (991 µs–1.3 ms) | 4.3 ms | 13.6 ms | 0 | 203 µs | 10+76% |
| 20,000 | 3/3 | 20,000 | 336 µs | 1.2 ms (1.1 ms–1.9 ms) | 4.0 ms | 9.3 ms | 0 | 207 µs | 12+79% |
| 25,000 | 3/3 | 25,000 | 352 µs | 1.4 ms (1.1 ms–1.6 ms) | 4.5 ms | 8.6 ms | 0 | 205 µs | 12+76% |
| 30,000 | 3/3 | 30,000 | 381 µs | 1.3 ms (1.3 ms–1.4 ms) | 3.9 ms | 7.1 ms | 0 | 211 µs | 14+82% |
| 40,000 | 3/3 | 40,000 | 422 µs | 1.3 ms (1.3 ms–1.4 ms) | 3.7 ms | 11.2 ms | 0 | 221 µs | 13+85% |
| 50,000 | 3/3 | 50,000 | 442 µs | 1.2 ms (1.2 ms–1.3 ms) | 3.7 ms | 9.7 ms | 0 | 197 µs | 13+81% |
| 75,000 | 3/3 | 75,000 | 463 µs | 1.5 ms (1.3 ms–1.8 ms) | 5.0 ms | 12.6 ms | 0 | 195 µs | 14+85% |
| 100,000 | 3/3 | 100,000 | 483 µs | 1.4 ms (1.4 ms–2.1 ms) | 5.0 ms | 23.8 ms | 0 | 234 µs | 14+86% |
| 150,000 | 3/3 | 150,000 | 492 µs | 1.5 ms (1.4 ms–1.9 ms) | 5.0 ms | 9.4 ms | 0 | 219 µs | 15+84% |
| 200,000 | 3/3 | 200,000 | 508 µs | 1.6 ms (1.5 ms–2.4 ms) | 5.8 ms | 11.2 ms | 0 | 240 µs | 16+79% |
| 250,000 | 3/3 | 250,000 | 532 µs | 1.8 ms (1.7 ms–2.4 ms) | 4.5 ms | 13.0 ms | 0 | 254 µs | 19+81% |
| 300,000 | 3/3 | 300,000 | 541 µs | 2.9 ms (1.5 ms–3.3 ms) | 9.3 ms | 18.5 ms | 0 | 299 µs | 20+78% |
| 400,000 | 3/3 | 400,000 | 582 µs | 2.1 ms (1.8 ms–2.8 ms) | 4.4 ms | 15.7 ms | 0 | 274 µs | 23+72% |

### b1_r2_g2: coalesced writes, 2 risk, 2 gateway

| offered/s | sustained reps | achieved/s | p50 | p99 (range) | p99.9 | max | lost | loadgen lag p99 | gw CPU user+sys |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 50,000 | 3/3 | 50,000 | 451 µs | 5.5 ms (2.6 ms–5.7 ms) | 13.9 ms | 42.6 ms | 0 | 1.2 ms | 12+52% |
| 100,000 | 3/3 | 100,000 | 639 µs | 4.3 ms (4.1 ms–12.5 ms) | 10.1 ms | 95.6 ms | 0 | 950 µs | 11+50% |
| 150,000 | 2/3 | 150,000 | 713 µs | 4.7 ms (3.7 ms–22.5 ms) | 8.8 ms | 60.2 ms | 0 | 1.5 ms | 12+46% |
| 200,000 | 2/3 | 200,000 | 795 µs | 5.3 ms (5.0 ms–24.1 ms) | 10.4 ms | 70.2 ms | 0 | 1.7 ms | 14+46% |
| 250,000 | 1/3 | 250,000 | 967 µs | 56.6 ms (5.9 ms–84.9 ms) | 84.9 ms | 149.7 ms | 0 | 6.1 ms | 15+45% |
| 300,000 | 3/3 | 300,000 | 918 µs | 7.7 ms (5.4 ms–14.3 ms) | 15.3 ms | 78.4 ms | 0 | 1.9 ms | 15+45% |
| 400,000 | 2/3 | 400,000 | 1.1 ms | 7.3 ms (6.7 ms–36.7 ms) | 12.3 ms | 81.3 ms | 0 | 1.9 ms | 15+43% |

### b1_r4_g1: coalesced writes, 4 risk, 1 gateway

| offered/s | sustained reps | achieved/s | p50 | p99 (range) | p99.9 | max | lost | loadgen lag p99 | gw CPU user+sys |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 50,000 | 3/3 | 50,000 | 541 µs | 2.2 ms (1.8 ms–2.2 ms) | 7.1 ms | 14.0 ms | 0 | 262 µs | 13+86% |
| 100,000 | 3/3 | 100,000 | 590 µs | 1.9 ms (1.7 ms–1.9 ms) | 4.7 ms | 13.2 ms | 0 | 278 µs | 15+83% |
| 150,000 | 3/3 | 150,000 | 582 µs | 1.9 ms (1.8 ms–2.6 ms) | 10.6 ms | 21.8 ms | 0 | 254 µs | 15+84% |
| 200,000 | 3/3 | 200,000 | 623 µs | 2.9 ms (1.6 ms–4.3 ms) | 9.3 ms | 46.7 ms | 0 | 344 µs | 17+82% |
| 250,000 | 2/3 | 250,000 | 664 µs | 2.7 ms (2.1 ms–48.2 ms) | 8.5 ms | 93.8 ms | 0 | 303 µs | 19+78% |
| 300,000 | 3/3 | 300,000 | 664 µs | 2.3 ms (2.1 ms–3.4 ms) | 9.0 ms | 16.6 ms | 0 | 270 µs | 20+78% |
| 400,000 | 3/3 | 400,000 | 704 µs | 2.5 ms (2.4 ms–3.1 ms) | 9.2 ms | 16.4 ms | 0 | 315 µs | 24+73% |

### b1_r4_g2: coalesced writes, 4 risk, 2 gateway

| offered/s | sustained reps | achieved/s | p50 | p99 (range) | p99.9 | max | lost | loadgen lag p99 | gw CPU user+sys |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 50,000 | 3/3 | 50,000 | 598 µs | 3.4 ms (3.1 ms–3.6 ms) | 8.9 ms | 17.7 ms | 0 | 811 µs | 13+50% |
| 100,000 | 3/3 | 100,000 | 926 µs | 6.0 ms (5.0 ms–6.3 ms) | 9.6 ms | 22.1 ms | 0 | 1.2 ms | 12+46% |
| 150,000 | 3/3 | 150,000 | 1.1 ms | 6.6 ms (6.0 ms–10.0 ms) | 11.4 ms | 61.2 ms | 0 | 2.0 ms | 13+46% |
| 200,000 | 3/3 | 200,000 | 1.2 ms | 7.7 ms (7.5 ms–11.7 ms) | 13.4 ms | 55.0 ms | 0 | 2.4 ms | 12+44% |
| 250,000 | 3/3 | 250,000 | 1.3 ms | 8.4 ms (7.3 ms–13.5 ms) | 15.2 ms | 34.5 ms | 0 | 2.3 ms | 12+40% |
| 300,000 | 2/3 | 300,000 | 1.4 ms | 8.7 ms (7.9 ms–77.6 ms) | 15.3 ms | 161.8 ms | 0 | 2.7 ms | 13+41% |
| 400,000 | 3/3 | 400,000 | 1.7 ms | 10.4 ms (8.2 ms–18.6 ms) | 18.9 ms | 52.2 ms | 0 | 2.6 ms | 15+41% |

### Highest sustained rate per config

Highest rate where that rate *and every lower swept rate* met the target, per rep. `≥` = never failed up to the top of the sweep (ceiling not reached). 0 = failed at the lowest swept rate.

| config | lowest swept | per rep | median |
|---|---:|---|---:|
| naive writes, 2 risk, 1 gateway | 5,000 | 15,000, 5,000, 5,000 | 5,000/s |
| coalesced writes, 1 risk, 1 gateway | 50,000 | ≥400,000, 200,000, ≥400,000 | 400,000/s |
| coalesced writes, 2 risk, 1 gateway | 5,000 | ≥400,000, ≥400,000, ≥400,000 | 400,000/s |
| coalesced writes, 2 risk, 2 gateway | 50,000 | 100,000, 200,000, ≥400,000 | 200,000/s |
| coalesced writes, 4 risk, 1 gateway | 50,000 | ≥400,000, ≥400,000, 200,000 | 400,000/s |
| coalesced writes, 4 risk, 2 gateway | 50,000 | ≥400,000, ≥400,000, 250,000 | 400,000/s |

## Ceiling search (coalesced writes, 400k+ orders/s), session 2

Sustained = 0 lost, achieved ≥ 99% of offered, e2e p99 < 20 ms. Cells: median over reps (range). A rep stops after the first rate with p99 > 1 s.

### b1_r1_g1: coalesced writes, 1 risk, 1 gateway

| offered/s | sustained reps | achieved/s | p50 | p99 (range) | p99.9 | max | lost | loadgen lag p99 | gw CPU user+sys |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 400,000 | 0/3 | 400,000 | 1.8 ms | 335.5 ms (99.6 ms–1.7 s) | 478.2 ms | 1.7 s | 0 | 17.6 ms | 36+59% |
| 500,000 | 0/2 | 500,000 | 1.4 s | 2.4 s (394.3 ms–4.4 s) | 2.4 s | 4.4 s | 0 | 192.3 ms | 36+53% |
| 600,000 | 0/1 | 600,000 | 10.3 s | 12.9 s () | 12.9 s | 12.9 s | 0 | 5.4 s | 43+54% |

### b1_r2_g1: coalesced writes, 2 risk, 1 gateway

| offered/s | sustained reps | achieved/s | p50 | p99 (range) | p99.9 | max | lost | loadgen lag p99 | gw CPU user+sys |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 400,000 | 0/3 | 400,000 | 795 µs | 257.9 ms (146.8 ms–352.3 ms) | 302.0 ms | 407.6 ms | 0 | 11.1 ms | 30+68% |
| 500,000 | 1/3 | 500,000 | 819 µs | 78.6 ms (2.4 ms–381.7 ms) | 101.7 ms | 414.7 ms | 0 | 1.8 ms | 28+67% |
| 600,000 | 0/3 | 600,000 | 1.0 ms | 448.8 ms (213.9 ms–457.2 ms) | 478.2 ms | 508.7 ms | 0 | 4.7 ms | 34+60% |
| 700,000 | 0/3 | 700,000 | 3.2 s | 4.8 s (914.4 ms–5.2 s) | 4.8 s | 5.2 s | 0 | 48.2 ms | 41+47% |
| 800,000 | 0/1 | 800,000 | 117.4 ms | 1.3 s () | 1.3 s | 1.3 s | 0 | 13.2 ms | 39+37% |

### b1_r2_g2: coalesced writes, 2 risk, 2 gateway

| offered/s | sustained reps | achieved/s | p50 | p99 (range) | p99.9 | max | lost | loadgen lag p99 | gw CPU user+sys |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 400,000 | 2/3 | 400,000 | 1.2 ms | 9.4 ms (6.4 ms–494.9 ms) | 17.0 ms | 551.0 ms | 0 | 2.3 ms | 18+43% |
| 500,000 | 3/3 | 500,000 | 1.3 ms | 8.3 ms (7.9 ms–10.1 ms) | 14.9 ms | 32.1 ms | 0 | 2.1 ms | 16+39% |
| 600,000 | 3/3 | 600,000 | 1.5 ms | 9.8 ms (9.6 ms–10.7 ms) | 15.5 ms | 29.6 ms | 0 | 1.8 ms | 18+39% |
| 700,000 | 1/3 | 700,000 | 1.6 ms | 28.6 ms (11.5 ms–129.0 ms) | 71.3 ms | 177.3 ms | 0 | 2.8 ms | 20+36% |
| 800,000 | 2/3 | 800,000 | 2.0 ms | 15.3 ms (14.7 ms–82.8 ms) | 27.3 ms | 103.8 ms | 0 | 2.3 ms | 20+33% |
| 1,000,000 | 0/3 | 1,000,000 | 4.9 ms | 570.4 ms (38.8 ms–864.0 ms) | 620.8 ms | 895.4 ms | 0 | 3.0 ms | 26+26% |

### b1_r4_g1: coalesced writes, 4 risk, 1 gateway

| offered/s | sustained reps | achieved/s | p50 | p99 (range) | p99.9 | max | lost | loadgen lag p99 | gw CPU user+sys |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 400,000 | 2/3 | 400,000 | 860 µs | 8.2 ms (7.8 ms–289.4 ms) | 27.5 ms | 325.7 ms | 0 | 1.5 ms | 26+68% |
| 500,000 | 1/3 | 500,000 | 876 µs | 272.6 ms (14.2 ms–276.8 ms) | 302.0 ms | 329.4 ms | 0 | 7.7 ms | 33+65% |
| 600,000 | 1/3 | 600,000 | 1.6 ms | 73.4 ms (5.1 ms–528.5 ms) | 103.8 ms | 557.1 ms | 0 | 4.1 ms | 34+52% |
| 700,000 | 1/3 | 700,000 | 16.3 ms | 780.1 ms (18.6 ms–1.3 s) | 796.9 ms | 1.3 s | 0 | 18.6 ms | 44+38% |
| 800,000 | 0/2 | 800,000 | 48.0 ms | 327.7 ms (84.9 ms–570.4 ms) | 341.3 ms | 588.9 ms | 0 | 5.4 ms | 41+36% |
| 1,000,000 | 0/2 | 1,000,000 | 809.5 ms | 989.9 ms (889.2 ms–1.1 s) | 1.0 s | 1.1 s | 0 | 36.0 ms | 49+37% |

### b1_r4_g2: coalesced writes, 4 risk, 2 gateway

| offered/s | sustained reps | achieved/s | p50 | p99 (range) | p99.9 | max | lost | loadgen lag p99 | gw CPU user+sys |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 400,000 | 3/3 | 400,000 | 1.8 ms | 13.6 ms (9.6 ms–14.8 ms) | 23.9 ms | 60.5 ms | 0 | 2.8 ms | 14+40% |
| 500,000 | 2/3 | 500,000 | 2.0 ms | 14.7 ms (13.5 ms–253.8 ms) | 22.3 ms | 339.1 ms | 0 | 2.8 ms | 17+38% |
| 600,000 | 2/3 | 600,000 | 1.9 ms | 17.0 ms (13.0 ms–364.9 ms) | 39.8 ms | 405.7 ms | 0 | 2.8 ms | 20+37% |
| 700,000 | 1/3 | 700,000 | 2.3 ms | 69.2 ms (13.6 ms–230.7 ms) | 84.9 ms | 344.3 ms | 0 | 2.8 ms | 19+32% |
| 800,000 | 1/3 | 800,000 | 2.6 ms | 48.8 ms (17.8 ms–86.0 ms) | 66.1 ms | 114.5 ms | 0 | 2.8 ms | 22+31% |
| 1,000,000 | 0/3 | 1,000,000 | 3.2 ms | 45.6 ms (36.2 ms–66.6 ms) | 82.8 ms | 104.3 ms | 0 | 2.6 ms | 23+31% |

### Highest sustained rate per config

Highest rate where that rate *and every lower swept rate* met the target, per rep. `≥` = never failed up to the top of the sweep (ceiling not reached). 0 = failed at the lowest swept rate.

| config | lowest swept | per rep | median |
|---|---:|---|---:|
| coalesced writes, 1 risk, 1 gateway | 400,000 | 0, 0, 0 | 0/s |
| coalesced writes, 2 risk, 1 gateway | 400,000 | 0, 0, 0 | 0/s |
| coalesced writes, 2 risk, 2 gateway | 400,000 | 0, 800,000, 600,000 | 600,000/s |
| coalesced writes, 4 risk, 1 gateway | 400,000 | 0, 400,000, 400,000 | 400,000/s |
| coalesced writes, 4 risk, 2 gateway | 400,000 | 500,000, 800,000, 400,000 | 500,000/s |

## Re-run of the 100k-600k range, session 3 (reproducibility check)

Sustained = 0 lost, achieved ≥ 99% of offered, e2e p99 < 20 ms. Cells: median over reps (range). A rep stops after the first rate with p99 > 1 s.

### b1_r2_g1: coalesced writes, 2 risk, 1 gateway

| offered/s | sustained reps | achieved/s | p50 | p99 (range) | p99.9 | max | lost | loadgen lag p99 | gw CPU user+sys |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 100,000 | 3/3 | 100,000 | 520 µs | 2.6 ms (1.6 ms–10.5 ms) | 8.5 ms | 59.0 ms | 0 | 524 µs | 15+81% |
| 200,000 | 3/3 | 200,000 | 582 µs | 3.9 ms (2.8 ms–5.2 ms) | 12.8 ms | 37.8 ms | 0 | 524 µs | 18+80% |
| 300,000 | 3/3 | 300,000 | 614 µs | 3.0 ms (2.0 ms–4.6 ms) | 10.2 ms | 29.6 ms | 0 | 426 µs | 21+77% |
| 400,000 | 3/3 | 400,000 | 655 µs | 2.9 ms (2.7 ms–4.5 ms) | 10.6 ms | 38.3 ms | 0 | 479 µs | 25+74% |
| 600,000 | 2/3 | 600,000 | 836 µs | 5.2 ms (4.3 ms–1.1 s) | 13.4 ms | 1.1 s | 0 | 762 µs | 29+66% |

### b1_r2_g2: coalesced writes, 2 risk, 2 gateway

| offered/s | sustained reps | achieved/s | p50 | p99 (range) | p99.9 | max | lost | loadgen lag p99 | gw CPU user+sys |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 100,000 | 2/3 | 100,000 | 786 µs | 6.6 ms (4.2 ms–23.6 ms) | 19.7 ms | 76.2 ms | 0 | 1.8 ms | 12+49% |
| 200,000 | 3/3 | 200,000 | 942 µs | 5.2 ms (4.7 ms–5.9 ms) | 9.3 ms | 17.9 ms | 0 | 1.8 ms | 14+46% |
| 300,000 | 3/3 | 300,000 | 1.0 ms | 6.7 ms (6.0 ms–8.8 ms) | 13.8 ms | 58.9 ms | 0 | 2.2 ms | 15+43% |
| 400,000 | 2/3 | 400,000 | 1.2 ms | 7.3 ms (7.1 ms–755.0 ms) | 18.9 ms | 823.9 ms | 0 | 2.2 ms | 17+43% |
| 600,000 | 2/3 | 600,000 | 1.8 ms | 10.2 ms (9.7 ms–788.5 ms) | 17.8 ms | 878.3 ms | 0 | 2.3 ms | 18+35% |

### Highest sustained rate per config

Highest rate where that rate *and every lower swept rate* met the target, per rep. `≥` = never failed up to the top of the sweep (ceiling not reached). 0 = failed at the lowest swept rate.

| config | lowest swept | per rep | median |
|---|---:|---|---:|
| coalesced writes, 2 risk, 1 gateway | 100,000 | 400,000, ≥600,000, ≥600,000 | 600,000/s |
| coalesced writes, 2 risk, 2 gateway | 100,000 | 0, 300,000, ≥600,000 | 300,000/s |

## Failure tests at 100,000 orders/s (kill_100k)

risk_kill: 2 risk instances, `kill -9` risk1 at t≈6.2 s, restarted at t≈12.2 s, 20 s run.

| gateway replay | reps | sent | lost | dup | wrong verdicts | orders replayed | p99 | worst max e2e |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| on | 5 | 10,000,000 | 0 | 0 | 0 | 187 | 2.7 ms | 68.5 ms |
| off (control) | 5 | 10,000,000 | 1859 | 0 | 0 | 0 (1859 dropped by gw) | 2.3 ms | 73.6 ms |

Worst 100 ms bucket per replay-on rep (t, max e2e): 14.2 s (68.5 ms); 16.5 s (59.3 ms); 3.1 s (15.6 ms); 16.5 s (55.7 ms); 9.8 s (10.1 ms)

md_kill: `kill -9` md-svc at t≈6.2 s (12 s run). sent 1,200,000, lost 0, dup 0, wrong 0; accepted 683,358, rejected stale 497,388, bad_qty 12,255, price_band 6,999.

## Failure tests at 20,000 orders/s (kill_20k)

risk_kill: 2 risk instances, `kill -9` risk1 at t≈6.2 s, restarted at t≈12.2 s, 20 s run.

| gateway replay | reps | sent | lost | dup | wrong verdicts | orders replayed | p99 | worst max e2e |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| on | 5 | 2,000,000 | 0 | 0 | 0 | 27 | 2.3 ms | 115.6 ms |
| off (control) | 5 | 2,000,000 | 35 | 0 | 0 | 0 (35 dropped by gw) | 14.9 ms | 122.4 ms |

Worst 100 ms bucket per replay-on rep (t, max e2e): 0.0 s (23.2 ms); 8.2 s (12.7 ms); 7.8 s (46.7 ms); 15.0 s (8.2 ms); 16.6 s (115.6 ms)

md_kill: `kill -9` md-svc at t≈6.2 s (12 s run). sent 240,000, lost 0, dup 0, wrong 0; accepted 136,471, rejected stale 99,797, bad_qty 2,353, price_band 1,379.

## Gateway profile

| run | e2e p50 | e2e p99 | gw CPU user | gw CPU sys | gw msgs/write | gw msgs/read |
|---|---:|---:|---:|---:|---:|---:|
| profile_b0_30000 | 262.1 ms | 461.4 ms | 15% | 82% | 1.00 | 1.88 |
| profile_b1_30000 | 401 µs | 1.8 ms | 15% | 79% | 1.23 | 1.81 |

Top symbols, batch=0 (perf, user-only):
```
  7.73% gw_svc       std::_Function_handler<void (tms::Conn&, tms::Msg&), main::{lambda(unsigned long)#1}::operator()(unsigned long
  6.48% gw_svc       boost::asio::detail::scheduler::do_run_one(boost::asio::detail::conditionally_enabled_mutex::scoped_lock&, boo
  4.57% gw_svc       boost::asio::detail::reactive_socket_send_op_base<boost::asio::const_buffers_1>::do_perform(boost::asio::detai
  4.43% gw_svc       boost::asio::detail::reactive_socket_recv_op<boost::asio::mutable_buffers_1, tms::Conn::do_read()::{lambda(boo
  4.40% libc.so.6    pthread_mutex_unlock
  4.37% gw_svc       tms::Conn::do_write()
  4.17% gw_svc       boost::asio::detail::epoll_reactor::start_op(int, int, boost::asio::detail::epoll_reactor::descriptor_state*&,
  4.02% libc.so.6    pthread_mutex_lock
  3.82% gw_svc       boost::asio::detail::reactive_socket_send_op<boost::asio::const_buffers_1, boost::asio::detail::write_op<boost
  2.72% gw_svc       main::{lambda(Pending)#1}::operator()(Pending) const
  2.65% libc.so.6    malloc
  2.52% gw_svc       main::{lambda(unsigned int, tms::Msg&)#1}::operator()(unsigned int, tms::Msg&) const
  2.41% libc.so.6    cfree
  2.40% gw_svc       boost::asio::detail::binder2<boost::asio::detail::write_op<boost::asio::basic_stream_socket<boost::asio::ip::t
  2.20% gw_svc       boost::asio::detail::reactive_socket_recv_op_base<boost::asio::mutable_buffers_1>::do_perform(boost::asio::det
  2.16% gw_svc       std::_Sp_counted_base<(__gnu_cxx::_Lock_policy)2>::_M_release()
  2.15% gw_svc       boost::asio::detail::scheduler::post_immediate_completion(boost::asio::detail::scheduler_operation*, bool)
```

Top symbols, batch=1 (perf, user-only):
```
  5.72% gw_svc       boost::asio::detail::reactive_socket_recv_op<boost::asio::mutable_buffers_1, tms::Conn::do_read()::{lambda(boo
  5.49% gw_svc       boost::asio::detail::scheduler::do_run_one(boost::asio::detail::conditionally_enabled_mutex::scoped_lock&, boo
  5.04% gw_svc       tms::Conn::do_write()
  4.88% gw_svc       boost::asio::detail::epoll_reactor::start_op(int, int, boost::asio::detail::epoll_reactor::descriptor_state*&,
  4.67% libc.so.6    pthread_mutex_lock
  4.38% gw_svc       boost::asio::detail::reactive_socket_send_op_base<boost::asio::const_buffers_1>::do_perform(boost::asio::detai
  4.12% libc.so.6    pthread_mutex_unlock
  3.68% gw_svc       std::_Function_handler<void (tms::Conn&, tms::Msg&), main::{lambda(unsigned long)#1}::operator()(unsigned long
  3.48% gw_svc       boost::asio::detail::reactive_socket_send_op<boost::asio::const_buffers_1, boost::asio::detail::write_op<boost
  2.82% libc.so.6    malloc
  2.70% gw_svc       main::{lambda(unsigned int, tms::Msg&)#1}::operator()(unsigned int, tms::Msg&) const
  2.50% gw_svc       main::{lambda(Pending)#1}::operator()(Pending) const
  2.38% gw_svc       tms::Conn::do_read()
  2.37% libc.so.6    clock_gettime
  2.30% gw_svc       boost::asio::detail::reactive_socket_recv_op_base<boost::asio::mutable_buffers_1>::do_perform(boost::asio::det
  2.12% gw_svc       boost::asio::detail::epoll_reactor::descriptor_state::perform_io(unsigned int)
  2.09% libc.so.6    cfree
  2.02% [vdso]       __vdso_clock_gettime
```

