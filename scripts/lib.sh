#!/usr/bin/env bash
# Shared helpers for the bench scripts (source this).
BIN=${BIN:-$HOME/tm-build}
RUN=${RUN:-$HOME/tm-run}
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
CLK=$(getconf CLK_TCK)

# utime+stime (ticks) of a pid: "user sys"
cpu_ticks() { awk '{print $14, $15}' "/proc/$1/stat" 2>/dev/null || echo "0 0"; }

# run_load LABEL RATE SECS OUT.jsonl [extra loadgen flags]
# Runs loadgen and appends one JSON line; then appends one "cpu" JSON line with
# each service's user/sys CPU (% of one core) during the run.
run_load() {
  local label=$1 rate=$2 secs=$3 out=$4; shift 4
  declare -A u0 s0
  local names=()
  for f in "$RUN"/*.pid; do
    local n; n=$(basename "$f" .pid); names+=("$n")
    read -r u0[$n] s0[$n] < <(cpu_ticks "$(cat "$f")")
  done
  local t0; t0=$(date +%s.%N)
  "$BIN/loadgen" --rate "$rate" --secs "$secs" --label "$label" --json "$out" "$@" || true
  local t1; t1=$(date +%s.%N)
  local line="{\"label\":\"$label\",\"cpu\":{"
  local first=1
  for n in "${names[@]}"; do
    [ -e "$RUN/$n.pid" ] || continue
    local u s; read -r u s < <(cpu_ticks "$(cat "$RUN/$n.pid")")
    local pu ps
    pu=$(awk -v a="${u0[$n]}" -v b="$u" -v t0="$t0" -v t1="$t1" -v c="$CLK" 'BEGIN{printf "%.0f", (b-a)/c/(t1-t0)*100}')
    ps=$(awk -v a="${s0[$n]}" -v b="$s" -v t0="$t0" -v t1="$t1" -v c="$CLK" 'BEGIN{printf "%.0f", (b-a)/c/(t1-t0)*100}')
    [ $first = 1 ] || line+=","
    line+="\"$n\":{\"user\":$pu,\"sys\":$ps}"
    first=0
  done
  echo "$line}}" >>"$out"
}
