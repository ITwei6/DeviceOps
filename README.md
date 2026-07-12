# DeviceOps AI 智能设备运维平台

基于 AI 的智能设备运维管理平台。

## 后端构建

后端工程基于 `cpp-microservice-kit` 和 CMake，当前已接入 `device_gateway` 最小服务。

```bash
cmake -S . -B build
cmake --build build
```

构建时会统一生成并编译根目录 `proto/*.proto` 的 C++ 契约代码，生成目录位于 `build/generated/proto/`。

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
./build/services/device_service/device_service
```

当前 `device_service` 实现了设备创建、更新、查询、列表和接入校验 RPC。仓储层先使用进程内内存实现，接口边界已独立，后续可替换为 MySQL/ODB 持久化。

机器人设备模拟器：

```bash
DEVICEOPS_SIM_DEVICE_ID=robot-001 \
DEVICEOPS_SIM_LOOP_COUNT=10 \
DEVICEOPS_SIM_INTERVAL_MS=1000 \
./build/simulator/robot_device_simulator
```

模拟器会发布注册、遥测、心跳、报警和日志消息，并按周期模拟高温、错误码和离线状态。
