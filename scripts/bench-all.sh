#!/usr/bin/env bash
# Everything in results/<name>/:  scripts/bench-all.sh [name]   (~1 hour)
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=results/${1:-local}
mkdir -p "$OUT"
rm -f "$OUT"/*.jsonl "$OUT"/*.csv
{ uname -a; nproc; lscpu | grep 'Model name'; g++ --version | head -1; date -u; git rev-parse --short HEAD 2>/dev/null || true; } >"$OUT/env.txt"

# 1. headline: naive vs coalesced writes, 2 risk, 1 gateway, 3 reps
BATCH=0 N_RISK=2 GW=1 bash scripts/sweep.sh "$OUT/sweep.jsonl" 3
BATCH=1 N_RISK=2 GW=1 bash scripts/sweep.sh "$OUT/sweep.jsonl" 3

# 2. scaling (coalesced): risk instances x gateways
SC_RATES="50000 100000 150000 200000 250000 300000 400000"
for cfg in "1 1" "4 1" "2 2" "4 2"; do
  set -- $cfg
  BATCH=1 N_RISK=$1 GW=$2 RATES="$SC_RATES" bash scripts/sweep.sh "$OUT/sweep.jsonl" 3
done

# 3. failure tests
bash scripts/kill_test.sh "$OUT/kill_20k" 20000 5
bash scripts/kill_test.sh "$OUT/kill_100k" 100000 5

# 4. profile (user-space perf + /proc user/sys split)
bash scripts/profile.sh "$OUT/profile" 30000

python3 tools/report.py "$OUT" >"$OUT/summary.md"
echo "done: $OUT/summary.md"
