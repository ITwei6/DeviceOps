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
```
