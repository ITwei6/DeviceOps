#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUN_DIR="${DEVICEOPS_RUN_DIR:-/tmp/deviceops-backend-stack}"
LOG_DIR="$RUN_DIR/logs"
mkdir -p "$LOG_DIR"
cd "$ROOT_DIR"

DEVICE_ID="${DEVICEOPS_E2E_DEVICE_ID:-robot-e2e-$(date +%s)}"
NOW_MS="$(python3 - <<'PY'
import time
print(int(time.time() * 1000))
PY
)"

post_json() {
  local url="$1"
  local body="$2"
  curl -sS --max-time 20 -X POST "$url" -H "Content-Type: application/json" -d "$body"
}

get_json_path() {
  local path="$1"
  local json_input
  json_input="$(cat)"
  JSON_INPUT="$json_input" python3 - "$path" <<'PY'
import json
import os
import sys

path = sys.argv[1].split(".")
data = json.loads(os.environ.get("JSON_INPUT", ""))
value = data
for part in path:
    if isinstance(value, list):
        value = value[int(part)]
    else:
        value = value.get(part)
    if value is None:
        print("")
        sys.exit(0)
if isinstance(value, (dict, list)):
    print(json.dumps(value, ensure_ascii=False))
else:
    print(value)
PY
}

require_ok() {
  local label="$1"
  local body="$2"
  local code
  code="$(printf '%s' "$body" | get_json_path response.code)"
  if [ -z "$code" ]; then
    code="0"
  fi
  if [ "$code" != "0" ]; then
    echo "$label failed: $body" >&2
    exit 1
  fi
  echo "$label ok"
}

require_nonempty() {
  local label="$1"
  local value="$2"
  if [ -z "$value" ] || [ "$value" = "0" ]; then
    echo "$label missing" >&2
    exit 1
  fi
  echo "$label=$value"
}

rabbitmq_probe() {
  local label="$1"
  local exchange="$2"
  local queue="$3"
  local routing_key="$4"
  local expected="$5"
  build/tools/rabbitmq_queue_probe/rabbitmq_queue_probe \
    amqp://admin:123456@rabbitmq-service:5672/ \
    "$exchange" \
    "$queue" \
    "$routing_key" \
    "$expected" \
    15000
  echo "$label ok"
}

echo "checking backend stack with device_id=$DEVICE_ID"

timeout 8 sh -c "mosquitto_sub -h 127.0.0.1 -p 1883 -t deviceops/e2e-smoke -C 1 >/tmp/deviceops-e2e-smoke.out & sleep 1; mosquitto_pub -h 127.0.0.1 -p 1883 -t deviceops/e2e-smoke -m ok; wait" >/dev/null
echo "mqtt ok"

GATEWAY_STATUS="$(post_json http://127.0.0.1:9101/deviceops.gateway.DeviceGatewayService/GetGatewayStatus '{"gateway_id":"device-gateway-001"}')"
require_ok "gateway status" "$GATEWAY_STATUS"

RAG_HEALTH="$(curl -sS --max-time 10 http://127.0.0.1:9601/health)"
if ! printf '%s' "$RAG_HEALTH" | grep -q '"status": "ok"'; then
  echo "rag health failed: $RAG_HEALTH" >&2
  exit 1
fi
echo "rag ok"

env \
  DEVICEOPS_MQTT_HOST=127.0.0.1 \
  DEVICEOPS_MQTT_PORT=1883 \
  DEVICEOPS_SIM_DEVICE_ID="$DEVICE_ID" \
  DEVICEOPS_SIM_INTERVAL_MS=400 \
  DEVICEOPS_SIM_LOOP_COUNT=12 \
  DEVICEOPS_SIM_HIGH_TEMP_PERIOD=3 \
  DEVICEOPS_SIM_ERROR_PERIOD=4 \
  DEVICEOPS_SIM_OFFLINE_PERIOD=6 \
  build/simulator/robot_device_simulator > "$LOG_DIR/e2e_simulator.log" 2>&1

sleep 3

DEVICE_BODY="$(post_json http://127.0.0.1:9201/deviceops.device.DeviceService/GetDevice "{\"device_id\":\"$DEVICE_ID\"}")"
require_ok "device get" "$DEVICE_BODY"
require_nonempty "device_id" "$(printf '%s' "$DEVICE_BODY" | get_json_path device.device_id)"

TELEMETRY_BODY="$(post_json http://127.0.0.1:9301/deviceops.telemetry.TelemetryService/GetRealtimeStatus "{\"device_id\":\"$DEVICE_ID\"}")"
require_ok "telemetry realtime" "$TELEMETRY_BODY"
require_nonempty "telemetry_reported_at" "$(printf '%s' "$TELEMETRY_BODY" | get_json_path telemetry.reported_at)"

EVENT_BODY="$(post_json http://127.0.0.1:9401/deviceops.event.EventService/ListEvents "{\"page\":{\"page\":1,\"page_size\":10},\"device_id\":\"$DEVICE_ID\"}")"
require_ok "event list" "$EVENT_BODY"
EVENT_TOTAL="$(printf '%s' "$EVENT_BODY" | get_json_path page.total)"
require_nonempty "event_total" "$EVENT_TOTAL"
EVENT_ID="$(printf '%s' "$EVENT_BODY" | get_json_path events.0.event_id)"
require_nonempty "event_id" "$EVENT_ID"

LOG_BODY="$(post_json http://127.0.0.1:9501/deviceops.log.LogService/QueryLogs "{\"page\":{\"page\":1,\"page_size\":10},\"device_id\":\"$DEVICE_ID\"}")"
require_ok "log query" "$LOG_BODY"
LOG_TOTAL="$(printf '%s' "$LOG_BODY" | get_json_path page.total)"
require_nonempty "log_total" "$LOG_TOTAL"

ES_COUNT="$(curl -sS --max-time 10 -u elastic:123456 -H "Content-Type: application/json" \
  http://elasticsearch-service:9200/deviceops-logs-*/_count \
  -d "{\"query\":{\"match\":{\"device_id\":\"$DEVICE_ID\"}}}" | get_json_path count)"
require_nonempty "es_log_count" "$ES_COUNT"

DOC_BODY="$(post_json http://127.0.0.1:9600/deviceops.knowledge.KnowledgeService/CreateKnowledgeDocument "{\"title\":\"TEMP_001 robot troubleshooting $DEVICE_ID\",\"category\":\"fault\",\"device_type\":\"robot\",\"error_code\":\"TEMP_001\",\"content\":\"TEMP_001 means robot controller temperature is too high. Check cooling fan, airflow and motor current before restarting the robot.\",\"created_by\":1}")"
require_ok "knowledge create" "$DOC_BODY"
DOCUMENT_ID="$(printf '%s' "$DOC_BODY" | get_json_path document.document_id)"
require_nonempty "document_id" "$DOCUMENT_ID"

INDEX_BODY="$(post_json http://127.0.0.1:9600/deviceops.knowledge.KnowledgeService/RequestKnowledgeIndex "{\"document_id\":\"$DOCUMENT_ID\",\"force_rebuild\":true}")"
require_ok "knowledge index" "$INDEX_BODY"
require_nonempty "index_task_id" "$(printf '%s' "$INDEX_BODY" | get_json_path task_id)"

SEARCH_BODY="$(post_json http://127.0.0.1:9600/deviceops.knowledge.KnowledgeService/SearchKnowledge '{"query":"TEMP_001","device_type":"robot","error_code":"TEMP_001","top_k":3}')"
require_ok "knowledge search" "$SEARCH_BODY"
require_nonempty "knowledge_snippet" "$(printf '%s' "$SEARCH_BODY" | get_json_path snippets.0.document_id)"

FAULT_BODY="$(post_json http://127.0.0.1:9700/deviceops.diagnosis.DiagnosisService/CreateFaultRecord "{\"device_id\":\"$DEVICE_ID\",\"event_id\":\"$EVENT_ID\",\"owner_user_id\":1,\"fault_type\":\"temperature_high\",\"severity\":\"EVENT_SEVERITY_CRITICAL\",\"symptom\":\"Robot reported TEMP_001 and high temperature during e2e check.\",\"started_at\":$NOW_MS}")"
require_ok "fault create" "$FAULT_BODY"
FAULT_ID="$(printf '%s' "$FAULT_BODY" | get_json_path fault.fault_id)"
require_nonempty "fault_id" "$FAULT_ID"

DIAG_BODY="$(post_json http://127.0.0.1:9700/deviceops.diagnosis.DiagnosisService/StartDiagnosis "{\"event_id\":\"$EVENT_ID\",\"fault_id\":\"$FAULT_ID\",\"device_id\":\"$DEVICE_ID\",\"requested_by\":1,\"engineer_note\":\"E2E diagnosis check for TEMP_001\"}")"
require_ok "diagnosis start" "$DIAG_BODY"
REPORT_ID="$(printf '%s' "$DIAG_BODY" | get_json_path task_id)"
require_nonempty "report_id" "$REPORT_ID"

REPORT_BODY="$(post_json http://127.0.0.1:9700/deviceops.diagnosis.DiagnosisService/GetDiagnosisReport "{\"report_id\":\"$REPORT_ID\"}")"
require_ok "diagnosis report" "$REPORT_BODY"
require_nonempty "diagnosis_summary" "$(printf '%s' "$REPORT_BODY" | get_json_path report.summary)"

rabbitmq_probe "rabbitmq telemetry status" deviceops.telemetry.exchange telemetry.status.queue telemetry.status.updated "$DEVICE_ID"
rabbitmq_probe "rabbitmq telemetry offline" deviceops.telemetry.exchange telemetry.offline.queue telemetry.device.offline "$DEVICE_ID"
rabbitmq_probe "rabbitmq alarm created" deviceops.event.exchange event.alarm.queue event.alarm.created "$DEVICE_ID"
rabbitmq_probe "rabbitmq log received" deviceops.log.exchange log.ingest.queue log.device.received "$DEVICE_ID"
rabbitmq_probe "rabbitmq knowledge index" deviceops.knowledge.exchange knowledge.index.queue knowledge.document.index_requested "$DEVICE_ID"
rabbitmq_probe "rabbitmq diagnosis task" deviceops.diagnosis.exchange diagnosis.task.queue diagnosis.task.created "$DEVICE_ID"

STATS_BODY="$(post_json http://127.0.0.1:9101/deviceops.gateway.DeviceGatewayService/GetForwardingStats '{"gateway_id":"device-gateway-001"}')"
require_ok "gateway stats" "$STATS_BODY"

echo "backend e2e passed"
echo "device_id=$DEVICE_ID"
echo "event_id=$EVENT_ID"
echo "fault_id=$FAULT_ID"
echo "report_id=$REPORT_ID"
