#!/usr/bin/env bash
# CPU profile of the gateway under load, in both write modes.
#   scripts/profile.sh OUTDIR [RATE]
# Needs `perf` (PERF env, default: perf on PATH) and FlameGraph scripts
# (FLAMEGRAPH env = dir with stackcollapse-perf.pl / flamegraph.pl).
#
# With kernel.perf_event_paranoid=2 (the default; no root here) perf only
# samples user space, so kernel time is measured separately as utime/stime
# from /proc (see lib.sh) and the flame graphs show the user-space part only.
set -euo pipefail
. "$(dirname "$0")/lib.sh"
OUT=$1
RATE=${2:-15000}
PERF=${PERF:-perf}
mkdir -p "$OUT"
cat /proc/sys/kernel/perf_event_paranoid >"$OUT/paranoid.txt"  # <=1: kernel stacks included

# perf.data goes to local /tmp: perf record cannot write it on a 9p/drvfs
# mount (WSL /mnt/d) and silently leaves a header-only file.
PD=$(mktemp -d)
trap 'rm -rf "$PD"' EXIT
for b in 0 1; do
  BATCH=$b bash "$ROOT/scripts/stack.sh" up 2
  gwpid=$(cat "$RUN/gw.pid")
  "$PERF" record -F 1999 -g -p "$gwpid" -o "$PD/gw_b$b.perf.data" -- sleep 8 2>"$PD/rec.err" &
  prec=$!
  "$PERF" stat -e task-clock,context-switches -p "$gwpid" -o "$OUT/gw_b$b.stat.txt" -- sleep 8 &
  pstat=$!
  run_load "profile_b${b}_$RATE" "$RATE" 9 "$OUT/profile.jsonl"
  wait $prec $pstat || true
  bash "$ROOT/scripts/stack.sh" down
  grep -h '"svc":"gw","msgs_in"' "$RUN/gw.log" | sed "s/^/{\"label\":\"profile_b${b}_$RATE\",\"io\":/; s/\$/}/" >>"$OUT/profile.jsonl"
  "$PERF" report -i "$PD/gw_b$b.perf.data" --stdio --no-children --percent-limit 2 -g none \
    2>/dev/null | grep -v '^$' | head -40 >"$OUT/gw_b$b.top.txt"
  if [ -n "${FLAMEGRAPH:-}" ]; then
    "$PERF" script -i "$PD/gw_b$b.perf.data" 2>/dev/null | "$FLAMEGRAPH/stackcollapse-perf.pl" \
      | "$FLAMEGRAPH/flamegraph.pl" --title "gw-svc CPU, batch=$b, $RATE orders/s" >"$OUT/gw_b$b.svg" \
      || echo "flamegraph failed for batch=$b: $(cat "$PD/rec.err")" >&2
  fi
  rm -f "$PD/gw_b$b.perf.data"
done
