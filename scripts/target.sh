#!/usr/bin/env bash
# The JD's target point: 12,000 orders/s for 30 s, both write modes, 3 reps.
# Gives the per-hop latency breakdown.   scripts/target.sh OUT.jsonl [REPS]
set -euo pipefail
. "$(dirname "$0")/lib.sh"
OUT=$1
REPS=${2:-3}
for rep in $(seq "$REPS"); do
  for b in 0 1; do
    BATCH=$b bash "$ROOT/scripts/stack.sh" up 2
    run_load "12k_b${b}_rep${rep}" 12000 30 "$OUT"
    bash "$ROOT/scripts/stack.sh" down
  done
done
