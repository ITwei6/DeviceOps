# CHANGELOG

## [Unreleased]

### Added
- 它记录这个项目每个版本、每个阶段“做了什么变化”。
- 新增系统架构设计文档，定义分层架构、微服务拆分、通信设计、数据流和技术选型。
- 新增详细设计文档和 proto 接口契约，覆盖 MySQL、Redis、Elasticsearch、MQTT、RabbitMQ 和 RPC 设计。
- 补充设备接入网关转发定位、转发规则和网关运行状态 RPC 契约。
- 新增后端开发指导文档，说明开发顺序、服务边界、验证方式和交接要求。
- 新增 DeviceOps 后端 CMake 工程和服务目录骨架。
- 新增 `device_gateway` 最小服务，支持 MQTT Topic 订阅、JSON payload 校验和接入统计。
- 在 `cpp-microservice-kit` 中新增 `tewmqtt::MqttClient`，封装 MQTT 连接、订阅、发布、断线重连和订阅恢复。
