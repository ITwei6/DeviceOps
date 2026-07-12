# DeviceOps 后端开发指导文档

| 文档项 | 内容 |
| --- | --- |
| 项目名称 | DeviceOps |
| 文档目的 | 给后续 Developer Codex 的开发交接说明 |
| 角色定位 | 企业级 C++ 后端工程师 |
| 输入文档 | AGENTS.md、docs/01_requirement.md、docs/02_architecture.md、docs/03_design.md、proto/ |
| 文档状态 | 开发前指导 |

## 1. 开发前必须阅读

Developer Codex 开始写代码前，必须按顺序阅读：

1. `AGENTS.md`
2. `docs/01_requirement.md`
3. `docs/02_architecture.md`
4. `docs/03_design.md`
5. `proto/*.proto`

阅读目标：

- 理解 DeviceOps 是智能设备运维平台，不是普通后台管理系统。
- 理解系统主链路：设备接入 -> 状态处理 -> 异常告警 -> 日志检索 -> AI 诊断。
- 理解 `device-gateway` 是设备接入转发网关，只做接入、协议解析、标准化和转发。
- 理解各服务边界，禁止在一个服务里混入其他服务职责。
- 理解所有开发、编译、测试都必须在 Docker 开发容器中完成。

## 2. 总体开发原则

### 2.1 必须遵守

- 基于 `cpp-microservice-kit` 开发，优先复用脚手架能力。
- 使用 C++17。
- 服务间同步通信使用 brpc + protobuf。
- 设备接入使用 MQTT。
- 异步任务使用 RabbitMQ。
- 业务数据使用 MySQL。
- 实时状态使用 Redis。
- 日志检索使用 Elasticsearch。
- 向量检索使用 Milvus。
- 每个服务保持 `src/`、`include/`、`proto/`、`tests/`、`CMakeLists.txt` 结构。
- 每完成一个明确阶段，更新 `CHANGELOG.md` 并提交 Git。

### 2.2 禁止事项

- 禁止重新设计系统架构。
- 禁止随意修改服务边界。
- 禁止替换技术方案。
- 禁止重新实现 brpc、服务注册、日志、Elasticsearch 客户端等脚手架已有能力。
- 禁止在宿主机直接运行 `cmake`、`make`、`gcc/g++` 或 C++ 服务。
- 禁止把设备接入凭据、密码、token 明文写入仓库。
- 禁止 `device-gateway` 直接承担告警规则、状态持久化或 AI 诊断职责。

## 3. 推荐开发顺序

### 3.1 第一步：工程环境集成

目标：

- 将 `cpp-microservice-kit` 能力接入 DeviceOps 工程。
- 建立顶层 CMake 工程。
- 建立服务目录骨架。
- 确认 Docker 开发容器内可以完成编译。

建议输出：

```text
services/
├── device_gateway/
├── device_service/
├── telemetry_service/
├── event_service/
├── log_service/
├── diagnosis_service/
└── knowledge_service/
```

验收标准：

- 容器内可以执行 CMake 配置。
- 空服务或最小服务可以编译通过。
- 不要求业务功能完整。

提交建议：

```text
chore(environment): setup service skeleton
```

### 3.2 第二步：protobuf 集成

目标：

- 将根目录 `proto/` 中的接口契约接入构建。
- 生成 C++ protobuf 和 brpc service 代码。
- 确保服务使用同一套 proto 契约。

建议处理：

- 不要复制多份 proto 到各服务后产生分叉。
- 可以在服务内部引用根目录 proto，或在构建阶段统一生成。
- 如果脚手架已有 proto 生成规则，优先复用。

验收标准：

- `proto/common.proto`
- `proto/device_gateway.proto`
- `proto/device.proto`
- `proto/telemetry.proto`
- `proto/event.proto`
- `proto/log.proto`
- `proto/diagnosis.proto`
- `proto/knowledge.proto`

全部能在容器内通过 `protoc` 生成 C++ 代码。

提交建议：

```text
chore(proto): integrate service contracts
```

### 3.3 第三步：设备模拟器

目标：

- 实现机器人设备模拟器。
- 模拟设备通过 MQTT 上报注册、状态、报警、日志、心跳。

模拟字段：

- 电量 `battery`
- 温度 `temperature`
- 速度 `speed`
- 运行模式 `run_mode`
- 错误码 `error_code`
- 在线状态 `online`

参考 MQTT Topic：

```text
device/{device_id}/register
device/{device_id}/telemetry
device/{device_id}/alarm
device/{device_id}/log
device/{device_id}/heartbeat
```

验收标准：

- 模拟器能定时产生状态数据。
- 能模拟温度过高、错误码、离线等异常。
- MQTT payload 与 `docs/03_design.md` 保持一致。

提交建议：

```text
feat(simulator): implement robot device simulator
```

### 3.4 第四步：device-gateway

目标：

- 实现设备侧统一入口。
- 订阅 MQTT Topic。
- 完成设备认证、协议解析、标准化和转发。

必须保持的职责边界：

- 网关只负责接入和转发。
- 设备主数据交给 `device-service`。
- 实时状态交给 `telemetry-service`。
- 告警规则交给 `event-service`。
- 日志索引交给 `log-service`。
- AI 诊断交给 `diagnosis-service`。

转发规则：

| MQTT Topic | 网关处理 | 转发目标 |
| --- | --- | --- |
| `device/{device_id}/register` | 认证、解析注册信息 | `device-service` |
| `device/{device_id}/telemetry` | 标准化状态数据 | `telemetry-service` / RabbitMQ |
| `device/{device_id}/alarm` | 标准化报警消息 | `event-service` / RabbitMQ |
| `device/{device_id}/log` | 标准化日志 | `log-service` / RabbitMQ |
| `device/{device_id}/heartbeat` | 刷新在线状态 | `telemetry-service` / RabbitMQ |

验收标准：

- 能连接 MQTT Broker。
- 能订阅设备 Topic。
- 能解析合法 JSON payload。
- 非法 payload 不转发，并记录日志。
- 能调用下游服务或投递 RabbitMQ。
- `DeviceGatewayService` 能返回网关状态和转发统计。

提交建议：

```text
feat(gateway): implement mqtt device forwarding
```

### 3.5 第五步：device-service

目标：

- 实现设备基础信息管理。
- 支持设备注册、设备查询、设备列表、设备接入校验。

主要接口：

- `CreateDevice`
- `UpdateDevice`
- `GetDevice`
- `ListDevices`
- `VerifyDeviceAccess`

数据表：

- `devices`

验收标准：

- 设备 ID 唯一。
- 能根据设备 ID 校验设备接入。
- 能为网关提供设备身份和接入配置查询。

提交建议：

```text
feat(device): implement device management service
```

### 3.6 第六步：telemetry-service

目标：

- 接收标准化设备状态。
- 更新 Redis 实时状态。
- 保存必要状态快照。
- 发布状态变化事件。

主要接口：

- `UploadTelemetry`
- `GetRealtimeStatus`
- `ListRealtimeStatus`
- `QueryTelemetryHistory`

Redis Key：

```text
deviceops:device:status:{device_id}
deviceops:device:online:{device_id}
```

验收标准：

- 状态上传后 Redis 可查询最新状态。
- 心跳能刷新在线状态。
- 离线可被识别为候选事件。

提交建议：

```text
feat(telemetry): implement realtime status service
```

### 3.7 第七步：event-service

目标：

- 消费状态变化和设备报警消息。
- 根据规则生成告警事件。
- 管理告警状态。

MVP 告警规则：

- 温度超过阈值。
- 设备上报错误码。
- 设备离线。

主要接口：

- `CreateEvent`
- `GetEvent`
- `ListEvents`
- `UpdateEventStatus`

数据表：

- `events`

验收标准：

- 能生成 `open` 状态告警。
- 能更新为 `processing`、`resolved`、`closed`。
- 告警能写入 MySQL。
- 告警可写入 Elasticsearch 作为检索索引。

提交建议：

```text
feat(event): implement alarm event service
```

### 3.8 第八步：log-service

目标：

- 写入设备日志和服务日志。
- 支持按设备、时间、级别、关键词检索。
- 为诊断服务提供日志上下文。

主要接口：

- `WriteLog`
- `QueryLogs`
- `GetLogContext`

Elasticsearch 索引：

```text
deviceops-logs-YYYY.MM
```

验收标准：

- 日志能进入 Elasticsearch。
- 能按设备 ID 和时间范围查询。
- 能返回故障发生前后的上下文日志。

提交建议：

```text
feat(log): implement log search service
```

### 3.9 第九步：knowledge-service

目标：

- 管理知识文档。
- 支持知识检索。
- 为 RAG 提供知识片段。

主要接口：

- `CreateKnowledgeDocument`
- `GetKnowledgeDocument`
- `ListKnowledgeDocuments`
- `SearchKnowledge`
- `RequestKnowledgeIndex`

验收标准：

- 能保存基础错误码说明和维修 SOP。
- 能按关键词检索知识。
- Milvus 向量检索可以先做接口预留，MVP 初期允许先用关键词检索替代，但接口边界不能改。

提交建议：

```text
feat(knowledge): implement knowledge retrieval service
```

### 3.10 第十步：diagnosis-service

目标：

- 聚合告警、设备状态、日志和知识库内容。
- 创建故障记录。
- 发起 AI 诊断任务。
- 保存诊断报告。

主要接口：

- `CreateFaultRecord`
- `GetFaultRecord`
- `ListFaultRecords`
- `StartDiagnosis`
- `GetDiagnosisReport`
- `ListDiagnosisReports`
- `ConfirmDiagnosisReport`

数据表：

- `fault_records`
- `diagnosis_reports`

验收标准：

- 能基于 event_id 创建故障记录。
- 能聚合事件、状态、日志、知识库内容。
- 能生成诊断报告草案。
- 工程师能确认或驳回报告。

提交建议：

```text
feat(diagnosis): implement diagnosis workflow
```

## 4. 数据库落地建议

MySQL 建表 SQL 已在 `docs/03_design.md` 中给出。开发时建议：

- 优先创建 `deploy/mysql/init.sql` 或类似初始化脚本。
- 先落地核心五张表：
  - `devices`
  - `users`
  - `events`
  - `fault_records`
  - `diagnosis_reports`
- 不要在业务代码中硬编码表结构。
- 枚举字段先使用字符串，便于调试和观察。
- JSON 字段用于快照、证据、原因列表和推荐动作。

## 5. Docker 编译和测试要求

所有编译必须进入开发容器执行。

正确方式：

```bash
docker exec dev-env-service bash -lc "
cd /home/dev/workspace/projects/DeviceOps &&
mkdir -p build &&
cd build &&
cmake .. &&
make -j\$(nproc)
"
```

测试方式：

```bash
docker exec dev-env-service bash -lc "
cd /home/dev/workspace/projects/DeviceOps/build &&
ctest
"
```

禁止：

```bash
cmake ..
make
gcc
g++
```

在宿主机直接执行。

## 6. 最小联调链路

建议后端开发优先跑通这条最小链路：

```text
设备模拟器
↓ MQTT
device-gateway
↓
telemetry-service
↓
Redis 最新状态
↓
event-service
↓
MySQL 告警事件
```

然后扩展：

```text
device-gateway
↓
log-service
↓
Elasticsearch 日志检索
↓
diagnosis-service
↓
knowledge-service
↓
诊断报告
```

## 7. 测试建议

### 7.1 单元测试

优先覆盖：

- MQTT payload 解析。
- 网关 topic 路由。
- 设备 ID 校验。
- 状态缓存序列化和反序列化。
- 告警规则判断。
- 日志查询参数转换。
- 诊断上下文组装。

### 7.2 集成测试

优先覆盖：

- 模拟器 -> MQTT -> gateway -> telemetry。
- telemetry -> Redis。
- telemetry/event -> RabbitMQ。
- event -> MySQL。
- log -> Elasticsearch。
- diagnosis -> event/log/knowledge 聚合查询。

### 7.3 异常测试

必须覆盖：

- 非法 JSON。
- topic 中 device_id 与 payload 中 device_id 不一致。
- 未注册设备。
- 下游服务不可用。
- RabbitMQ 投递失败。
- Redis 不可用。
- Elasticsearch 查询超时。

## 8. 开发完成输出格式

每次开发任务完成后，Developer Codex 应输出：

```text
修改内容
测试结果
遗留问题
下一步建议
Git commit 信息
```

如果测试无法运行，需要明确说明原因，例如容器不存在、依赖未安装、服务未启动等。

## 9. 给 Developer Codex 的重点提醒

- 先跑通最小闭环，再扩展完整能力。
- 先复用脚手架能力，再考虑补充封装。
- 先实现清晰接口，再优化性能。
- 先保证服务边界正确，再追求功能丰富。
- 发现架构问题先记录并询问，不要直接重构设计。
- `device-gateway` 只做转发网关，这是后续开发最容易混淆的点。

