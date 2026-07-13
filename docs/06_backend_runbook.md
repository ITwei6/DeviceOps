# DeviceOps 后端启动、停止和模拟测试

这份文档只记录日常联调最常用的命令。命令默认在 `dev-env-service` 开发容器内执行。

## 1. 进入开发容器

在 Linux 虚拟机宿主机执行：

```bash
docker exec -it dev-env-service bash
```

进入项目目录：

```bash
cd /home/dev/workspace/projects/DeviceOps
```

## 2. 编译后端

首次运行或代码更新后执行：

```bash
cmake -S . -B build
cmake --build build -j1
```

如果已经编译过，通常只需要：

```bash
cmake --build build -j1
```

## 3. 启动后端服务

```bash
./scripts/run_backend_stack.sh
```

脚本会启动：

- MQTT broker
- RAG HTTP 服务
- device-gateway
- device-service
- telemetry-service
- event-service
- log-service
- knowledge-service
- diagnosis-service

启动成功会看到：

```text
backend stack started
logs: /tmp/deviceops-backend-stack/logs
```

## 4. 停止后端服务

```bash
./scripts/stop_backend_stack.sh
```

如果单独启动了模拟器，也可以额外执行：

```bash
pkill -f build/simulator/robot_device_simulator
```

## 5. 完整端到端测试

后端启动后执行：

```bash
./scripts/check_backend_stack.sh
```

这个脚本会自动运行一次设备模拟器，并验证：

- MQTT 接入
- 设备注册
- 实时状态写入 Redis
- 告警事件写入 MySQL
- 日志写入 Elasticsearch
- 知识库创建、索引和检索
- 诊断报告生成
- RabbitMQ 业务事件队列

成功会看到：

```text
backend e2e passed
device_id=...
event_id=...
fault_id=...
report_id=...
```

## 6. 启动持续模拟器

用于前端联调时持续产生数据：

```bash
env \
  DEVICEOPS_MQTT_HOST=127.0.0.1 \
  DEVICEOPS_MQTT_PORT=1883 \
  DEVICEOPS_SIM_DEVICE_ID=robot-live-demo \
  DEVICEOPS_SIM_INTERVAL_MS=1000 \
  DEVICEOPS_SIM_LOOP_COUNT=0 \
  DEVICEOPS_SIM_HIGH_TEMP_PERIOD=5 \
  DEVICEOPS_SIM_ERROR_PERIOD=7 \
  DEVICEOPS_SIM_OFFLINE_PERIOD=11 \
  build/simulator/robot_device_simulator
```

说明：

- `DEVICEOPS_SIM_DEVICE_ID`：模拟设备 ID，前端可查询这个设备。
- `DEVICEOPS_SIM_INTERVAL_MS=1000`：每 1 秒上报一次。
- `DEVICEOPS_SIM_LOOP_COUNT=0`：无限循环。
- `DEVICEOPS_SIM_HIGH_TEMP_PERIOD=5`：每 5 轮产生一次高温。
- `DEVICEOPS_SIM_ERROR_PERIOD=7`：每 7 轮产生一次错误码。
- `DEVICEOPS_SIM_OFFLINE_PERIOD=11`：每 11 轮产生一次离线状态。

后台运行持续模拟器：

```bash
nohup env \
  DEVICEOPS_MQTT_HOST=127.0.0.1 \
  DEVICEOPS_MQTT_PORT=1883 \
  DEVICEOPS_SIM_DEVICE_ID=robot-live-demo \
  DEVICEOPS_SIM_INTERVAL_MS=1000 \
  DEVICEOPS_SIM_LOOP_COUNT=0 \
  DEVICEOPS_SIM_HIGH_TEMP_PERIOD=5 \
  DEVICEOPS_SIM_ERROR_PERIOD=7 \
  DEVICEOPS_SIM_OFFLINE_PERIOD=11 \
  build/simulator/robot_device_simulator \
  >/tmp/deviceops-backend-stack/logs/live_simulator.log 2>&1 &
```

停止持续模拟器：

```bash
pkill -f build/simulator/robot_device_simulator
```

## 7. 常用接口验证

网关状态：

```bash
curl -sS -X POST http://127.0.0.1:9101/deviceops.gateway.DeviceGatewayService/GetGatewayStatus \
  -H "Content-Type: application/json" \
  -d '{"gateway_id":"device-gateway-001"}'
```

查询实时状态：

```bash
curl -sS -X POST http://127.0.0.1:9301/deviceops.telemetry.TelemetryService/GetRealtimeStatus \
  -H "Content-Type: application/json" \
  -d '{"device_id":"robot-live-demo"}'
```

查询告警事件：

```bash
curl -sS -X POST http://127.0.0.1:9401/deviceops.event.EventService/ListEvents \
  -H "Content-Type: application/json" \
  -d '{"page":{"page":1,"page_size":10},"device_id":"robot-live-demo"}'
```

查询日志：

```bash
curl -sS -X POST http://127.0.0.1:9501/deviceops.log.LogService/QueryLogs \
  -H "Content-Type: application/json" \
  -d '{"page":{"page":1,"page_size":10},"device_id":"robot-live-demo"}'
```

查询网关转发统计：

```bash
curl -sS -X POST http://127.0.0.1:9101/deviceops.gateway.DeviceGatewayService/GetForwardingStats \
  -H "Content-Type: application/json" \
  -d '{"gateway_id":"device-gateway-001"}'
```

## 8. 日志位置

一键启动脚本的日志目录：

```text
/tmp/deviceops-backend-stack/logs
```

常用查看命令：

```bash
tail -f /tmp/deviceops-backend-stack/logs/device_gateway.log
tail -f /tmp/deviceops-backend-stack/logs/telemetry_service.log
tail -f /tmp/deviceops-backend-stack/logs/event_service.log
tail -f /tmp/deviceops-backend-stack/logs/log_service.log
tail -f /tmp/deviceops-backend-stack/logs/knowledge_service.log
tail -f /tmp/deviceops-backend-stack/logs/diagnosis_service.log
tail -f /tmp/deviceops-backend-stack/logs/rag_service.log
```

## 9. Windows Qt 联调提醒

Qt 在 Windows 上运行时，需要先保证 Windows 能访问 Linux 虚拟机。

如果使用 SSH 端口转发，在 Windows PowerShell 中执行：

```powershell
ssh -N -p 2222 dev@<Linux虚拟机IP> `
  -L 9101:127.0.0.1:9101 `
  -L 9201:127.0.0.1:9201 `
  -L 9301:127.0.0.1:9301 `
  -L 9401:127.0.0.1:9401 `
  -L 9501:127.0.0.1:9501 `
  -L 9600:127.0.0.1:9600 `
  -L 9700:127.0.0.1:9700
```

端口转发成功后，Qt 配置使用：

```text
http://127.0.0.1:9101
http://127.0.0.1:9201
http://127.0.0.1:9301
http://127.0.0.1:9401
http://127.0.0.1:9501
http://127.0.0.1:9600
http://127.0.0.1:9700
```
