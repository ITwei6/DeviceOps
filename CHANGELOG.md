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
- 新增 `deviceops_proto` 构建目标，统一生成并编译根目录 `proto/*.proto` 的 C++ protobuf/brpc 契约代码。
- 新增 `robot_device_simulator`，支持通过 MQTT 上报注册、遥测、心跳、报警和日志消息，并模拟高温、错误码和离线异常。
- 完善 `device_gateway`，支持 MQTT 消息标准化、转发统计、设备心跳视图和 `DeviceGatewayService` brpc 状态查询接口。
- 新增 ODB/MySQL 持久化基础层，构建期从实体头文件生成 `*-odb.hxx`、`*-odb.cxx` 和 schema。
- 将 `device_service` 仓储层切换为 ODB/MySQL 持久化，支持设备创建、更新、查询、列表和接入校验 RPC。
- 新增 `telemetry_service`，支持状态上传、实时状态查询、状态列表、历史查询和 Redis 最新状态写入。
- 新增 `event_service`，支持告警事件创建、查询、列表、状态流转和 ODB/MySQL 持久化。
- 新增 `log_service`，支持日志写入 Elasticsearch、条件查询和设备故障上下文查询。
- 新增 `knowledge_service`，支持知识文档管理、关键词检索和 RAG 索引请求。
- 新增 Python `rag_service` MVP，提供 HTTP JSON 的知识索引、检索和诊断生成能力。
- 新增 `diagnosis_service`，支持故障记录、RAG 诊断草案、诊断报告查询和确认/驳回。
- 完善 `device_gateway` 下游转发链路，将遥测、告警和日志分别通过 brpc 转发到 telemetry、event 和 log 服务，心跳保留用于网关本地在线视图。
- 完善 `device_gateway` 注册链路，将设备注册消息转发到 `device_service` 并支持已注册设备的幂等接入校验。
