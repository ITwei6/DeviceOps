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

停止模拟器后，前端不会立刻看到离线。`telemetry-service` 会按最近一次状态上报时间 `reported_at` 计算在线状态；一键启动脚本默认设置：

```text
DEVICEOPS_TELEMETRY_OFFLINE_TIMEOUT_MS=15000
```

也就是停止模拟器后大约 15 秒，实时状态查询会返回 `online=false`。

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

## 9. 进入数据库和中间件

本节命令在 Linux 虚拟机宿主机执行，也就是能直接运行 `docker ps` 的那个环境。

如果当前用户需要 sudo，命令前加：

```bash
sudo
```

### 9.1 查看基础容器

```bash
docker ps --format 'table {{.Names}}\t{{.Status}}\t{{.Ports}}'
```

常用容器名：

```text
dev-env-service
mysql-service
redis-service
elasticsearch-service
rabbitmq-service
```

### 9.2 MySQL

进入 MySQL：

```bash
docker exec -it mysql-service mysql -h127.0.0.1 -P3306 -uroot -p123456
```

直接进入 `deviceops` 数据库：

```bash
docker exec -it mysql-service mysql -h127.0.0.1 -P3306 -uroot -p123456 deviceops
```

常用 SQL：

```sql
show databases;
use deviceops;
show tables;
select count(*) from devices;
select count(*) from events;
select count(*) from knowledge_documents;
select count(*) from fault_records;
select count(*) from diagnosis_reports;
select * from devices order by id desc limit 5;
select * from events order by id desc limit 5;
```

一条命令查看主要表行数：

```bash
docker exec mysql-service sh -lc 'mysql -h127.0.0.1 -P3306 -uroot -p123456 -D deviceops -e "select count(*) as devices from devices; select count(*) as events from events; select count(*) as knowledge_documents from knowledge_documents; select count(*) as fault_records from fault_records; select count(*) as diagnosis_reports from diagnosis_reports;"'
```

### 9.3 Redis

进入 Redis：

```bash
docker exec -it redis-service redis-cli -a 123456
```

常用命令：

```redis
ping
keys deviceops:device:status:*
keys deviceops:device:online:*
get deviceops:device:status:robot-live-demo
get deviceops:device:online:robot-live-demo
ttl deviceops:device:status:robot-live-demo
```

一条命令查看实时状态 key 数量：

```bash
docker exec redis-service redis-cli -a 123456 --no-auth-warning keys 'deviceops:device:status:*' | wc -l
```

### 9.4 Elasticsearch

Elasticsearch 账号：

```text
elastic / 123456
```

查看集群状态：

```bash
curl -u elastic:123456 http://127.0.0.1:9200/_cluster/health?pretty
```

查看索引：

```bash
curl -u elastic:123456 http://127.0.0.1:9200/_cat/indices?v
```

查看日志数量：

```bash
curl -u elastic:123456 http://127.0.0.1:9200/deviceops-logs-*/_count?pretty
```

按设备查询日志：

```bash
curl -u elastic:123456 -H "Content-Type: application/json" \
  http://127.0.0.1:9200/deviceops-logs-*/_search?pretty \
  -d '{"query":{"match":{"device_id":"robot-live-demo"}},"size":5}'
```

如果在 `dev-env-service` 容器内执行，地址用服务名：

```bash
curl -u elastic:123456 http://elasticsearch-service:9200/_cat/indices?v
```

### 9.5 RabbitMQ

RabbitMQ 管理后台：

```text
http://<Linux虚拟机IP>:15672
admin / 123456
```

查看队列：

```bash
docker exec -it rabbitmq-service rabbitmqctl list_queues -p / name messages_ready messages_unacknowledged consumers
```

查看交换机：

```bash
docker exec -it rabbitmq-service rabbitmqctl list_exchanges -p / name type durable
```

查看绑定：

```bash
docker exec -it rabbitmq-service rabbitmqctl list_bindings -p /
```

DeviceOps 主要队列：

```text
telemetry.status.queue
telemetry.offline.queue
event.alarm.queue
log.ingest.queue
knowledge.index.queue
diagnosis.task.queue
```

### 9.6 MQTT / Mosquitto

MQTT broker 默认在 `dev-env-service` 容器内监听：

```text
127.0.0.1:1883
```

订阅所有设备消息：

```bash
docker exec -it dev-env-service mosquitto_sub -h 127.0.0.1 -p 1883 -t 'device/#' -v
```

发布一条测试消息：

```bash
docker exec -it dev-env-service mosquitto_pub -h 127.0.0.1 -p 1883 \
  -t device/test-device/heartbeat \
  -m '{"message_id":"manual-001","device_id":"test-device","timestamp":1720000000000,"online":true}'
```

### 9.7 进入各容器 Shell

```bash
docker exec -it dev-env-service bash
docker exec -it mysql-service bash
docker exec -it redis-service sh
docker exec -it elasticsearch-service bash
docker exec -it rabbitmq-service bash
```

### 9.8 查看容器日志

```bash
docker logs --tail 100 dev-env-service
docker logs --tail 100 mysql-service
docker logs --tail 100 redis-service
docker logs --tail 100 elasticsearch-service
docker logs --tail 100 rabbitmq-service
```

持续跟踪日志：

```bash
docker logs -f --tail 100 mysql-service
```

## 10. Windows Qt 联调提醒

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
