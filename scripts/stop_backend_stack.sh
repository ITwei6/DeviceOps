#!/usr/bin/env bash
set -euo pipefail

RUN_DIR="${DEVICEOPS_RUN_DIR:-/tmp/deviceops-backend-stack}"
PID_FILE="$RUN_DIR/pids"

if [ ! -f "$PID_FILE" ]; then
  echo "no backend stack pid file: $PID_FILE"
  exit 0
fi

while read -r name pid; do
  [ -n "${pid:-}" ] || continue
  if kill "$pid" 2>/dev/null; then
    echo "stopping $name pid=$pid"
  fi
done < "$PID_FILE"

sleep 2

while read -r name pid; do
  [ -n "${pid:-}" ] || continue
  if kill -0 "$pid" 2>/dev/null; then
    kill -9 "$pid" 2>/dev/null || true
    echo "killed $name pid=$pid"
  fi
done < "$PID_FILE"

rm -f "$PID_FILE"
echo "backend stack stopped"
