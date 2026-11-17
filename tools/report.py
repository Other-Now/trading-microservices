#!/usr/bin/env python3
"""Turn results/<name>/*.jsonl into a markdown summary (stdout).

Every number in the README comes from this output. Where a run was repeated,
the table shows the median across repetitions and the min-max range.
"""
import collections
import csv
import glob
import json
import os
import re
import statistics
import sys

TARGET_P99_US = 20_000  # the JD's "<20 ms" target


def load(path):
    loads, cpu, other = {}, {}, collections.defaultdict(dict)
    if not os.path.exists(path):
        return loads, cpu, other
    for line in open(path):
        line = line.strip()
        if not line:
            continue
        d = json.loads(line)
        if "us" in d:
            loads[d["label"]] = d
        elif "cpu" in d:
            cpu[d["label"]] = d["cpu"]
        else:
            for k, v in d.items():
                if k != "label":
                    other[d["label"]][k] = v
    return loads, cpu, other


def med(xs):
    return statistics.median(xs) if xs else float("nan")


def fmt_us(v):
    if v != v:
        return "-"
    if v >= 1_000_000:
        return f"{v / 1e6:.1f} s"
    if v >= 1000:
        return f"{v / 1000:.1f} ms"
    return f"{v:.0f} µs"


def rng(xs):
    return f"{fmt_us(min(xs))}–{fmt_us(max(xs))}" if len(xs) > 1 else ""


def ok(d):
    """Sustained = nothing lost, kept up with offered load, p99 under target."""
    return d["lost"] == 0 and d["achieved_rps"] >= 0.99 * d["rate"] and d["us"]["e2e"]["p99"] < TARGET_P99_US


def sweep_section(outdir, fname="sweep.jsonl", heading="Rate sweeps"):
    loads, cpu, _ = load(f"{outdir}/{fname}")
    if not loads:
        return
    by_cfg = collections.defaultdict(lambda: collections.defaultdict(list))
    for label, d in loads.items():
        m = re.match(r"(b\d_r\d+_g\d+)_rep(\d+)_(\d+)", label)
        if m:
            by_cfg[m.group(1)][int(m.group(3))].append((int(m.group(2)), d, cpu.get(label, {})))

    print(f"## {heading}\n")
    print(f"Sustained = 0 lost, achieved ≥ 99% of offered, e2e p99 < {TARGET_P99_US // 1000} ms. "
          "Cells: median over reps (range). A rep stops after the first rate with p99 > 1 s.\n")
    summary = []
    for cfg in sorted(by_cfg):
        b, r, g = re.match(r"b(\d)_r(\d+)_g(\d+)", cfg).groups()
        title = f"{'coalesced' if b == '1' else 'naive'} writes, {r} risk, {g} gateway"
        print(f"### {cfg}: {title}\n")
        print("| offered/s | sustained reps | achieved/s | p50 | p99 (range) | p99.9 | max | lost | loadgen lag p99 | gw CPU user+sys |")
        print("|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
        # Highest sustained rate per rep = highest rate R such that R and every
        # lower swept rate passed in that rep (a lucky pass above a failure
        # does not count).
        best_per_rep, broken = collections.defaultdict(int), set()
        top = max(by_cfg[cfg])
        for rate in sorted(by_cfg[cfg]):
            runs = by_cfg[cfg][rate]
            p99s = [d["us"]["e2e"]["p99"] for _, d, _ in runs]
            gw = [c.get("gw") for _, _, c in runs if c.get("gw")]
            gwtxt = f"{med([x['user'] for x in gw]):.0f}+{med([x['sys'] for x in gw]):.0f}%" if gw else "-"
            npass = sum(ok(d) for _, d, _ in runs)
            print(f"| {rate:,} | {npass}/{len(runs)} | {med([d['achieved_rps'] for _, d, _ in runs]):,.0f} "
                  f"| {fmt_us(med([d['us']['e2e']['p50'] for _, d, _ in runs]))} "
                  f"| {fmt_us(med(p99s))} ({rng(p99s)}) "
                  f"| {fmt_us(med([d['us']['e2e']['p999'] for _, d, _ in runs]))} "
                  f"| {fmt_us(max(d['us']['e2e']['max'] for _, d, _ in runs))} "
                  f"| {sum(d['lost'] for _, d, _ in runs)} "
                  f"| {fmt_us(med([d['us']['sender_lag']['p99'] for _, d, _ in runs]))} | {gwtxt} |")
            for rep, d, _ in runs:
                if rep in broken:
                    continue
                if ok(d):
                    best_per_rep[rep] = rate
                else:
                    broken.add(rep)
        reps = sorted({rep for runs in by_cfg[cfg].values() for rep, _, _ in runs})
        bests = [best_per_rep.get(rep, 0) for rep in reps]
        never_broke = [rep not in broken and best_per_rep.get(rep) == top for rep in reps]
        summary.append((cfg, title, bests, never_broke, min(by_cfg[cfg])))
        print()

    print("### Highest sustained rate per config\n")
    print("Highest rate where that rate *and every lower swept rate* met the target, per rep. "
          "`≥` = never failed up to the top of the sweep (ceiling not reached). 0 = failed at the lowest swept rate.\n")
    print("| config | lowest swept | per rep | median |")
    print("|---|---:|---|---:|")
    for cfg, title, bests, nb, low in summary:
        cells = ", ".join(f"{'≥' if n else ''}{x:,}" for x, n in zip(bests, nb))
        print(f"| {title} | {low:,} | {cells} | {med(bests):,.0f}/s |")
    print()


def hops_section(outdir):
    loads, _, _ = load(f"{outdir}/target.jsonl")
    if not loads:
        return
    print("## At the JD target rate (12,000 orders/s): per-hop latency\n")
    print("Hop = time between two timestamps carried in the message (same CLOCK_MONOTONIC, same kernel). "
          "Medians over reps of each run's p50 / p99.\n")
    groups = collections.defaultdict(list)
    for label, d in loads.items():
        groups[re.sub(r"_rep\d+", "", label)].append(d)
    hops = ["lg_to_gw", "gw_in", "gw_to_risk", "risk", "risk_to_gw", "gw_out", "gw_to_lg", "e2e_from_send", "sender_lag", "e2e"]
    names = sorted(groups)
    print("| hop | " + " | ".join(f"{n} p50 / p99" for n in names) + " |")
    print("|---|" + "---:|" * len(names))
    for h in hops:
        cells = []
        for n in names:
            runs = groups[n]
            cells.append(f"{fmt_us(med([r['us'][h]['p50'] for r in runs]))} / {fmt_us(med([r['us'][h]['p99'] for r in runs]))}")
        print(f"| {h} | " + " | ".join(cells) + " |")
    for n in names:
        runs = groups[n]
        print(f"\n{n}: {len(runs)} reps, sent {sum(r['sent'] for r in runs):,}, lost {sum(r['lost'] for r in runs)}, "
              f"dup {sum(r['dup'] for r in runs)}, wrong {sum(r['wrong'] for r in runs)}")
    print()


def kill_section(outdir, sub):
    path = f"{outdir}/{sub}/kill.jsonl"
    loads, _, other = load(path)
    if not loads:
        return
    rate = next(iter(loads.values()))["rate"]
    print(f"## Failure tests at {rate:,.0f} orders/s ({sub})\n")
    print("risk_kill: 2 risk instances, `kill -9` risk1 at t≈6.2 s, restarted at t≈12.2 s, 20 s run.\n")
    print("| gateway replay | reps | sent | lost | dup | wrong verdicts | orders replayed | p99 | worst max e2e |")
    print("|---|---:|---:|---:|---:|---:|---:|---:|---:|")
    for rp in ("1", "0"):
        runs = [(l, d) for l, d in loads.items() if l.startswith(f"risk_kill_replay{rp}_")]
        if not runs:
            continue
        repl = sum(other[l].get("gw", {}).get("replayed", 0) for l, _ in runs)
        dropped = sum(other[l].get("gw", {}).get("dropped", 0) for l, _ in runs)
        print(f"| {'on' if rp == '1' else 'off (control)'} | {len(runs)} | {sum(d['sent'] for _, d in runs):,} "
              f"| {sum(d['lost'] for _, d in runs)} | {sum(d['dup'] for _, d in runs)} | {sum(d['wrong'] for _, d in runs)} "
              f"| {repl if rp == '1' else f'0 ({dropped} dropped by gw)'} "
              f"| {fmt_us(med([d['us']['e2e']['p99'] for _, d in runs]))} | {fmt_us(max(d['us']['e2e']['max'] for _, d in runs))} |")
    # where does the worst latency happen? (100 ms buckets)
    worst = []
    for f in sorted(glob.glob(f"{outdir}/{sub}/ts_risk_kill_replay1_*.csv")):
        rows = list(csv.DictReader(open(f)))
        top = max(rows, key=lambda r: float(r["max_e2e_us"]))
        worst.append(f"{int(top['t_ms']) / 1000:.1f} s ({fmt_us(float(top['max_e2e_us']))})")
    if worst:
        print(f"\nWorst 100 ms bucket per replay-on rep (t, max e2e): {'; '.join(worst)}")
    md = loads.get("md_kill")
    if md:
        r = md["reasons"]
        print(f"\nmd_kill: `kill -9` md-svc at t≈6.2 s (12 s run). sent {md['sent']:,}, lost {md['lost']}, dup {md['dup']}, "
              f"wrong {md['wrong']}; accepted {r['ok']:,}, rejected stale {r['stale']:,}, bad_qty {r['bad_qty']:,}, "
              f"price_band {r['price_band']:,}.")
    print()


def profile_section(outdir):
    loads, cpu, other = load(f"{outdir}/profile/profile.jsonl")
    if not loads:
        return
    print("## Gateway profile\n")
    print("| run | e2e p50 | e2e p99 | gw CPU user | gw CPU sys | gw msgs/write | gw msgs/read |")
    print("|---|---:|---:|---:|---:|---:|---:|")
    for label in sorted(loads):
        d, c, io = loads[label], cpu.get(label, {}).get("gw", {}), other[label].get("io", {})
        mpr = io["msgs_in"] / io["reads"] if io.get("reads") else float("nan")
        print(f"| {label} | {fmt_us(d['us']['e2e']['p50'])} | {fmt_us(d['us']['e2e']['p99'])} | {c.get('user', '-')}% "
              f"| {c.get('sys', '-')}% | {io.get('msgs_per_write', float('nan')):.2f} | {mpr:.2f} |")
    par = f"{outdir}/profile/paranoid.txt"
    scope = "user+kernel" if os.path.exists(par) and int(open(par).read()) <= 1 else "user-only"
    for b in (0, 1):
        f = f"{outdir}/profile/gw_b{b}.top.txt"
        if os.path.exists(f):
            print(f"\nTop symbols, batch={b} (perf, {scope}):\n```")
            for line in open(f):
                if line.startswith("#") or not line.strip():
                    continue
                m = re.match(r"\s*([\d.]+%)\s+\S+\s+(\S+)\s+\[[.k]\]\s+(.*)", line)
                if m:
                    print(f"{m.group(1):>7} {m.group(2):<12} {m.group(3)[:110]}")
            print("```")
    print()


def main():
    outdir = sys.argv[1]
    env = open(f"{outdir}/env.txt").read().strip() if os.path.exists(f"{outdir}/env.txt") else ""
    print(f"# Results: {os.path.basename(os.path.normpath(outdir))}\n\n```\n{env}\n```\n")
    hops_section(outdir)
    sweep_section(outdir)
    sweep_section(outdir, "ceiling.jsonl", "Ceiling search (coalesced writes, 400k+ orders/s), session 2")
    sweep_section(outdir, "repro.jsonl", "Re-run of the 100k-600k range, session 3 (reproducibility check)")
    for sub in sorted(os.listdir(outdir)):
        if sub.startswith("kill"):
            kill_section(outdir, sub)
    profile_section(outdir)


if __name__ == "__main__":
    main()
