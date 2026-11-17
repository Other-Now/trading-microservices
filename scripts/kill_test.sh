#!/usr/bin/env bash
# Failure tests under load (2 risk instances, 1 gateway, batch writes on).
#   scripts/kill_test.sh OUTDIR [RATE] [REPS]
#
# risk-kill: 20 s at RATE; at t=6 s kill -9 risk1; at t=12 s start it again.
#   Run with REPLAY=1 (gateway replays in-flight orders) and REPLAY=0 (control).
# md-kill:   kill -9 md-svc at t=6 s, never restart. Risk keeps the last prices
#   for 1 s, then rejects everything as "stale" (fail closed), still answering.
set -euo pipefail
. "$(dirname "$0")/lib.sh"
OUT=$1
RATE=${2:-20000}
REPS=${3:-5}
mkdir -p "$OUT"
export BATCH=1

for rep in $(seq "$REPS"); do
  for replay in 1 0; do
    REPLAY=$replay bash "$ROOT/scripts/stack.sh" up 2
    ( sleep 6.2; bash "$ROOT/scripts/stack.sh" kill-risk 1; sleep 6; bash "$ROOT/scripts/stack.sh" start-risk 1 ) &
    killer=$!
    run_load "risk_kill_replay${replay}_rep${rep}" "$RATE" 20 "$OUT/kill.jsonl" --ts "$OUT/ts_risk_kill_replay${replay}_rep${rep}.csv"
    wait $killer
    bash "$ROOT/scripts/stack.sh" down
    grep -h '"svc":"gw","orders_in"' "$RUN/gw.log" | sed "s/^/{\"label\":\"risk_kill_replay${replay}_rep${rep}\",\"gw\":/; s/\$/}/" >>"$OUT/kill.jsonl"
    grep -h 'DOWN' "$RUN/gw.log" >&2 || true
  done
done

bash "$ROOT/scripts/stack.sh" up 2
( sleep 6.2; bash "$ROOT/scripts/stack.sh" kill-md ) &
run_load "md_kill" "$RATE" 12 "$OUT/kill.jsonl" --ts "$OUT/ts_md_kill.csv"
wait
bash "$ROOT/scripts/stack.sh" down
