#!/usr/bin/env bash
# Rate sweep: offered load goes up until the system saturates.
#   scripts/sweep.sh OUT.jsonl [REPS]
# Env: BATCH, N_RISK (default 2), GW (default 1), SECS (default 10),
#      RATES (space-separated list)
# Stops a repetition after the first rate whose p99 exceeds 1 s (saturated:
# queues grow for the whole run, every higher rate is only worse).
set -euo pipefail
. "$(dirname "$0")/lib.sh"
OUT=$1
REPS=${2:-1}
SECS=${SECS:-10}
N_RISK=${N_RISK:-2}
GW=${GW:-1}
RATES=${RATES:-"5000 10000 15000 20000 25000 30000 40000 50000 75000 100000 150000 200000 250000 300000 400000"}
PORTS=$(seq -s, 7001 $((7000 + GW)))
tag="b${BATCH:-0}_r${N_RISK}_g${GW}"

for rep in $(seq "$REPS"); do
  for rate in $RATES; do
    GW=$GW bash "$ROOT/scripts/stack.sh" up "$N_RISK"
    run_load "${tag}_rep${rep}_${rate}" "$rate" "$SECS" "$OUT" --port "$PORTS" --conns $((4 * GW > 4 ? 4 * GW : 4))
    bash "$ROOT/scripts/stack.sh" down
    p99=$(tail -2 "$OUT" | head -1 | python3 -c 'import json,sys; print(json.load(sys.stdin)["us"]["e2e"]["p99"])')
    if python3 -c "import sys; sys.exit(0 if float('$p99') > 1e6 else 1)"; then
      echo "  saturated at $rate (p99 ${p99} us), stopping this rep" >&2
      break
    fi
  done
done
