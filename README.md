# DeviceOps AI 智能设备运维平台

基于 AI 的智能设备运维管理平台。

## 后端构建

后端工程基于 `cpp-microservice-kit` 和 CMake，当前已接入 `device_gateway` 最小服务。

```bash
cmake -S . -B build
cmake --build build
```

构建时会统一生成并编译根目录 `proto/*.proto` 的 C++ 契约代码，生成目录位于 `build/generated/proto/`。
MySQL 持久化使用 ODB，源码只维护 `common/include/deviceops/db/*_entity.h` 实体定义，`*-odb.hxx`、`*-odb.cxx` 和 schema 会在构建时生成到 `build/generated/odb/`。

运行前可通过环境变量配置 MQTT Broker：

```bash
export DEVICEOPS_MQTT_HOST=127.0.0.1
export DEVICEOPS_MQTT_PORT=1883
export DEVICEOPS_MQTT_USERNAME=
export DEVICEOPS_MQTT_PASSWORD=
export DEVICEOPS_GATEWAY_RPC_PORT=9101
```

`device_gateway` 会订阅 `device/+/register`、`device/+/telemetry`、`device/+/alarm`、`device/+/log` 和 `device/+/heartbeat`，并暴露 brpc 状态接口：

```bash
curl -X POST http://127.0.0.1:9101/deviceops.gateway.DeviceGatewayService/GetGatewayStatus \
  -H 'Content-Type: application/json' \
  -d '{"gateway_id":"device-gateway-001"}'

curl -X POST http://127.0.0.1:9101/deviceops.gateway.DeviceGatewayService/GetForwardingStats \
  -H 'Content-Type: application/json' \
  -d '{"gateway_id":"device-gateway-001"}'
```

设备管理服务：

```bash
export DEVICEOPS_DEVICE_RPC_PORT=9201
export DEVICEOPS_MYSQL_HOST=mysql-service
export DEVICEOPS_MYSQL_PASSWORD=123456
./build/services/device_service/device_service
```

当前 `device_service` 实现了设备创建、更新、查询、列表和接入校验 RPC，仓储层通过 ODB 写入 MySQL。

遥测状态服务：

```bash
export DEVICEOPS_TELEMETRY_RPC_PORT=9301
export DEVICEOPS_REDIS_ENABLED=1
export DEVICEOPS_REDIS_HOST=redis-service
export DEVICEOPS_REDIS_PASSWORD=123456
./build/services/telemetry_service/telemetry_service
```

`telemetry_service` 实现状态上传、实时状态查询、实时状态列表和历史查询。开启 Redis 后会写入：

```text
deviceops:device:status:{device_id}
deviceops:device:online:{device_id}
```

异常事件服务：

```bash
export DEVICEOPS_EVENT_RPC_PORT=9401
export DEVICEOPS_MYSQL_HOST=mysql-service
export DEVICEOPS_MYSQL_PASSWORD=123456
./build/services/event_service/event_service
```

`event_service` 实现告警事件创建、查询、列表和状态流转，事件数据通过 ODB 写入 MySQL。

日志检索服务：

```bash
export DEVICEOPS_LOG_RPC_PORT=9501
export DEVICEOPS_ES_URL=http://elastic:123456@elasticsearch-service:9200/
./build/services/log_service/log_service
```

`log_service` 实现日志写入、条件查询和故障上下文查询，日志索引按月写入 Elasticsearch，索引名形如 `deviceops-logs-YYYY.MM`。

机器人设备模拟器：

```bash
DEVICEOPS_SIM_DEVICE_ID=robot-001 \
DEVICEOPS_SIM_LOOP_COUNT=10 \
DEVICEOPS_SIM_INTERVAL_MS=1000 \
./build/simulator/robot_device_simulator
```

模拟器会发布注册、遥测、心跳、报警和日志消息，并按周期模拟高温、错误码和离线状态。
