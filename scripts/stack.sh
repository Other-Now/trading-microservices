#!/usr/bin/env bash
# Run the services as local processes.
#   scripts/stack.sh up [N_RISK]     start md-svc, N risk-svc (default 2), gw-svc
#   scripts/stack.sh down            SIGTERM all (each prints its stats JSON to its log)
#   scripts/stack.sh kill-risk I     kill -9 risk instance I (1-based)
#   scripts/stack.sh start-risk I    (re)start risk instance I
#   scripts/stack.sh kill-md
# Env: BATCH=0|1 (write coalescing), REPLAY=0|1 (gateway failover replay),
#      GW=number of gateways (ports 7001..; each connects to every risk),
#      BIN (build dir), RUN (pid/log dir)
# Ports: gw 7001, risk 7101.., md 7201
set -euo pipefail
BIN=${BIN:-$HOME/tm-build}
RUN=${RUN:-$HOME/tm-run}
BATCH=${BATCH:-0}
REPLAY=${REPLAY:-1}
mkdir -p "$RUN"

start_risk() {
  local i=$1
  "$BIN/risk_svc" --id "$i" --port $((7100 + i)) --md 127.0.0.1:7201 --batch "$BATCH" \
    >>"$RUN/risk$i.log" 2>&1 &
  echo $! >"$RUN/risk$i.pid"
}

wait_port() { for _ in $(seq 100); do (exec 3<>/dev/tcp/127.0.0.1/$1) 2>/dev/null && return 0; sleep 0.05; done; echo "port $1 never opened" >&2; return 1; }

case "${1:-}" in
  up)
    n=${2:-2}
    rm -f "$RUN"/*.log "$RUN"/*.pid
    "$BIN/md_svc" --port 7201 --batch "$BATCH" >"$RUN/md.log" 2>&1 &
    echo $! >"$RUN/md.pid"
    wait_port 7201
    risks=()
    for i in $(seq "$n"); do start_risk "$i"; risks+=("127.0.0.1:$((7100 + i))"); done
    for i in $(seq "$n"); do wait_port $((7100 + i)); done
    "$BIN/gw_svc" --port 7001 --risk "$(IFS=,; echo "${risks[*]}")" --batch "$BATCH" --replay "$REPLAY" \
      >"$RUN/gw.log" 2>&1 &
    echo $! >"$RUN/gw.pid"
    wait_port 7001
    # wait until the gateway reports every risk link up
    for _ in $(seq 100); do [ "$(grep -c ' up$' "$RUN/gw.log" || true)" -ge "$n" ] && break; sleep 0.05; done
    sleep 0.3
    ;;
  down)
    for f in "$RUN"/*.pid; do [ -e "$f" ] && kill "$(cat "$f")" 2>/dev/null || true; done
    sleep 0.5
    rm -f "$RUN"/*.pid
    ;;
  kill-risk) kill -9 "$(cat "$RUN/risk$2.pid")"; rm -f "$RUN/risk$2.pid" ;;
  start-risk) start_risk "$2" ;;
  kill-md) kill -9 "$(cat "$RUN/md.pid")"; rm -f "$RUN/md.pid" ;;
  *) sed -n '2,12p' "$0"; exit 1 ;;
esac
