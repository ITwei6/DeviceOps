#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUN_DIR="${DEVICEOPS_RUN_DIR:-/tmp/deviceops-backend-stack}"
LOG_DIR="$RUN_DIR/logs"
PID_FILE="$RUN_DIR/pids"

mkdir -p "$LOG_DIR"
cd "$ROOT_DIR"

if [ ! -x build/services/device_gateway/device_gateway ]; then
  echo "missing build artifacts; run cmake build first" >&2
  exit 2
fi

cleanup_known_services() {
  local patterns=(
    "build/services/device_service/device_service"
    "build/services/telemetry_service/telemetry_service"
    "build/services/event_service/event_service"
    "build/services/log_service/log_service"
    "build/services/knowledge_service/knowledge_service"
    "build/services/diagnosis_service/diagnosis_service"
    "build/services/device_gateway/device_gateway"
    "build/simulator/robot_device_simulator"
    "services/rag_service/server.py"
  )
  local pattern pid
  for pattern in "${patterns[@]}"; do
    while read -r pid; do
      [ -n "$pid" ] || continue
      [ "$pid" = "$$" ] && continue
      kill "$pid" 2>/dev/null || true
    done < <(pgrep -f "$pattern" || true)
  done
  sleep 1
}

if [ -f "$PID_FILE" ]; then
  while read -r name pid; do
    [ -n "${pid:-}" ] || continue
    kill "$pid" 2>/dev/null || true
  done < "$PID_FILE"
  sleep 1
fi
cleanup_known_services
: > "$PID_FILE"

start_proc() {
  local name="$1"
  shift
  nohup "$@" > "$LOG_DIR/$name.log" 2>&1 &
  local pid=$!
  echo "$name $pid" >> "$PID_FILE"
  echo "$name pid=$pid"
}

post_json() {
  local url="$1"
  local body="$2"
  curl -sS --max-time 5 -X POST "$url" -H "Content-Type: application/json" -d "$body"
}

wait_post() {
  local name="$1"
  local url="$2"
  local body="$3"
  local i
  for i in $(seq 1 30); do
    if post_json "$url" "$body" | grep -q '"response"'; then
      echo "$name ready"
      return 0
    fi
    sleep 1
  done
  echo "$name did not become ready" >&2
  tail -80 "$LOG_DIR/$name.log" >&2 || true
  return 1
}

if timeout 5 sh -c "mosquitto_sub -h 127.0.0.1 -p 1883 -t deviceops/backend-smoke -C 1 >/tmp/deviceops-backend-smoke.out & sleep 1; mosquitto_pub -h 127.0.0.1 -p 1883 -t deviceops/backend-smoke -m ok; wait" >/dev/null 2>&1; then
  echo "mosquitto ready on 127.0.0.1:1883"
else
  start_proc mosquitto mosquitto -p 1883
  sleep 2
  timeout 5 sh -c "mosquitto_sub -h 127.0.0.1 -p 1883 -t deviceops/backend-smoke -C 1 >/tmp/deviceops-backend-smoke.out & sleep 1; mosquitto_pub -h 127.0.0.1 -p 1883 -t deviceops/backend-smoke -m ok; wait" >/dev/null
  echo "mosquitto started on 127.0.0.1:1883"
fi

start_proc rag_service env DEVICEOPS_RAG_PORT=9601 python3 services/rag_service/server.py
for i in $(seq 1 30); do
  if curl -sS --max-time 5 http://127.0.0.1:9601/health | grep -q '"status": "ok"'; then
    echo "rag_service ready"
    break
  fi
  if [ "$i" = "30" ]; then
    echo "rag_service did not become ready" >&2
    tail -80 "$LOG_DIR/rag_service.log" >&2 || true
    exit 1
  fi
  sleep 1
done

COMMON_DB_ENV=(
  DEVICEOPS_MYSQL_HOST=mysql-service
  DEVICEOPS_MYSQL_PORT=3306
  DEVICEOPS_MYSQL_USER=root
  DEVICEOPS_MYSQL_PASSWORD=123456
  DEVICEOPS_MYSQL_DATABASE=deviceops
)

RABBITMQ_ENV=(
  DEVICEOPS_RABBITMQ_ENABLED=1
  DEVICEOPS_RABBITMQ_URL=amqp://admin:123456@rabbitmq-service:5672/
)

start_proc device_service env DEVICEOPS_DEVICE_RPC_PORT=9201 "${COMMON_DB_ENV[@]}" build/services/device_service/device_service
start_proc telemetry_service env DEVICEOPS_TELEMETRY_RPC_PORT=9301 DEVICEOPS_REDIS_ENABLED=1 DEVICEOPS_REDIS_HOST=redis-service DEVICEOPS_REDIS_PORT=6379 DEVICEOPS_REDIS_PASSWORD=123456 "${RABBITMQ_ENV[@]}" build/services/telemetry_service/telemetry_service
start_proc event_service env DEVICEOPS_EVENT_RPC_PORT=9401 "${COMMON_DB_ENV[@]}" "${RABBITMQ_ENV[@]}" build/services/event_service/event_service
start_proc log_service env DEVICEOPS_LOG_RPC_PORT=9501 DEVICEOPS_ES_URL=http://elastic:123456@elasticsearch-service:9200/ "${RABBITMQ_ENV[@]}" build/services/log_service/log_service
start_proc knowledge_service env DEVICEOPS_KNOWLEDGE_RPC_PORT=9600 "${COMMON_DB_ENV[@]}" DEVICEOPS_RAG_URL=http://127.0.0.1:9601 "${RABBITMQ_ENV[@]}" build/services/knowledge_service/knowledge_service
start_proc diagnosis_service env DEVICEOPS_DIAGNOSIS_RPC_PORT=9700 "${COMMON_DB_ENV[@]}" DEVICEOPS_RAG_URL=http://127.0.0.1:9601 "${RABBITMQ_ENV[@]}" build/services/diagnosis_service/diagnosis_service

wait_post device_service http://127.0.0.1:9201/deviceops.device.DeviceService/GetDevice '{"device_id":"__ready_check__"}'
wait_post telemetry_service http://127.0.0.1:9301/deviceops.telemetry.TelemetryService/GetRealtimeStatus '{"device_id":"__ready_check__"}'
wait_post event_service http://127.0.0.1:9401/deviceops.event.EventService/ListEvents '{"page":{"page":1,"page_size":1}}'
wait_post log_service http://127.0.0.1:9501/deviceops.log.LogService/QueryLogs '{"page":{"page":1,"page_size":1}}'
wait_post knowledge_service http://127.0.0.1:9600/deviceops.knowledge.KnowledgeService/ListKnowledgeDocuments '{"page":{"page":1,"page_size":1}}'
wait_post diagnosis_service http://127.0.0.1:9700/deviceops.diagnosis.DiagnosisService/ListFaultRecords '{"page":{"page":1,"page_size":1}}'

start_proc device_gateway env \
  DEVICEOPS_GATEWAY_RPC_PORT=9101 \
  DEVICEOPS_MQTT_HOST=127.0.0.1 \
  DEVICEOPS_MQTT_PORT=1883 \
  DEVICEOPS_DOWNSTREAM_RPC_ENABLED=1 \
  DEVICEOPS_DOWNSTREAM_RPC_TIMEOUT_MS=10000 \
  DEVICEOPS_DEVICE_RPC_ADDR=127.0.0.1:9201 \
  DEVICEOPS_TELEMETRY_RPC_ADDR=127.0.0.1:9301 \
  DEVICEOPS_EVENT_RPC_ADDR=127.0.0.1:9401 \
  DEVICEOPS_LOG_RPC_ADDR=127.0.0.1:9501 \
  build/services/device_gateway/device_gateway

wait_post device_gateway http://127.0.0.1:9101/deviceops.gateway.DeviceGatewayService/GetGatewayStatus '{"gateway_id":"device-gateway-001"}'

echo "backend stack started"
echo "logs: $LOG_DIR"
