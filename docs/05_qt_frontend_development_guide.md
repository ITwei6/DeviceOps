# DeviceOps Qt 前端开发指导文档

| 文档项 | 内容 |
| --- | --- |
| 项目名称 | DeviceOps |
| 文档目的 | 给 Qt 前端开发的交接说明 |
| 角色定位 | 企业级 Qt/C++ 客户端工程师 |
| 输入文档 | docs/01_requirement.md、docs/02_architecture.md、docs/03_design.md、proto/、README.md |
| 后端状态 | MVP 后端已具备一键启动和端到端验收脚本 |

## 1. 开发前必须阅读

Qt 前端开发开始前，必须按顺序阅读：

1. `docs/01_requirement.md`
2. `docs/02_architecture.md`
3. `docs/03_design.md`
4. `proto/*.proto`
5. `README.md`

阅读目标：

- 理解 DeviceOps 是智能设备运维平台，不是普通后台 CRUD 管理系统。
- 理解主链路：设备接入 -> 实时状态 -> 异常告警 -> 日志检索 -> 知识库 -> AI 诊断。
- 理解前端只通过后端 RPC 查询和操作，不直接访问 MySQL、Redis、Elasticsearch 或 MQTT。
- 理解后端接口契约以 `proto/` 为准，页面字段必须和 protobuf 字段保持一致。

## 2. 后端联调准备

前端联调前，先在 `dev-env-service` 开发容器内启动后端：

```bash
cd /home/dev/workspace/projects/DeviceOps
./scripts/run_backend_stack.sh
./scripts/check_backend_stack.sh
```

如果验收通过，会看到：

```text
backend e2e passed
device_id=...
event_id=...
fault_id=...
report_id=...
```

联调结束后停止后端：

```bash
./scripts/stop_backend_stack.sh
```

默认后端 RPC 端口：

| 服务 | 端口 | proto |
| --- | ---: | --- |
| device-gateway | 9101 | `proto/device_gateway.proto` |
| device-service | 9201 | `proto/device.proto` |
| telemetry-service | 9301 | `proto/telemetry.proto` |
| event-service | 9401 | `proto/event.proto` |
| log-service | 9501 | `proto/log.proto` |
| knowledge-service | 9600 | `proto/knowledge.proto` |
| diagnosis-service | 9700 | `proto/diagnosis.proto` |

RAG HTTP 服务默认运行在 `9601`，Qt 客户端不直接调用 RAG，统一通过 `knowledge-service` 和 `diagnosis-service` 使用。

## 3. Qt 技术建议

推荐使用：

- Qt 5.15 或 Qt 6.x。
- C++17。
- CMake 管理 Qt 工程。
- Qt Widgets 优先；如果已有团队标准要求 QML，也可以使用 QML，但要保持业务层和 UI 层分离。
- brpc + protobuf C++ client 调后端服务。

不建议：

- 不要让 Qt 直接连 MySQL、Redis、Elasticsearch。
- 不要让 Qt 直接订阅 MQTT。
- 不要在 UI 线程里发同步 RPC。
- 不要在页面里散落 brpc 调用，应通过统一 API client 层封装。

推荐目录：

```text
qt-client/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── app/
│   ├── api/
│   ├── models/
│   ├── pages/
│   ├── widgets/
│   └── utils/
└── resources/
```

推荐分层：

| 层 | 职责 |
| --- | --- |
| `api/` | 封装 brpc channel、stub、请求、响应和错误处理 |
| `models/` | Qt Model/View 数据模型，适配表格、列表、详情 |
| `pages/` | 页面级 UI 组合 |
| `widgets/` | 可复用控件，如状态标签、分页器、时间范围筛选 |
| `app/` | 应用启动、路由、全局配置、主题 |

## 4. RPC 调用方式

Qt 客户端应基于 `proto/*.proto` 生成 C++ 代码，然后封装服务 client。

示例封装：

```text
api/
├── RpcChannelManager
├── DeviceApi
├── TelemetryApi
├── EventApi
├── LogApi
├── KnowledgeApi
├── DiagnosisApi
└── GatewayApi
```

每个 API 类只做三件事：

- 构造 protobuf request。
- 调用 brpc stub。
- 将 protobuf response 转成 Qt 页面需要的数据结构或直接返回 proto 对象。

所有 RPC 必须放到后台线程执行：

```text
UI Thread
  -> ViewModel / Controller
  -> Worker Thread / QtConcurrent
  -> Api Client
  -> brpc
  -> signal 返回 UI Thread
```

错误处理统一规则：

- `response.code == 0`：成功。
- `response.code != 0`：展示 `response.message`。
- brpc controller failed：展示网络/服务不可用错误。
- 超时：提示“服务响应超时，请检查后端服务是否启动”。

## 5. 页面模块规划

### 5.1 总览仪表盘

目标：

- 让运维人员快速看到设备、告警、日志和诊断概况。

数据来源：

- `DeviceService.ListDevices`
- `TelemetryService.ListRealtimeStatus`
- `EventService.ListEvents`
- `DiagnosisService.ListDiagnosisReports`

建议内容：

- 设备总数。
- 在线设备数。
- 当前打开告警数。
- 最近诊断报告数。
- 最近异常设备列表。

### 5.2 设备管理页

数据来源：

- `DeviceService.ListDevices`
- `DeviceService.GetDevice`
- `DeviceService.CreateDevice`
- `DeviceService.UpdateDevice`

页面能力：

- 设备列表。
- 按设备类型、状态、关键词筛选。
- 查看设备详情。
- 新建设备。
- 编辑设备基础信息。

表格字段：

- `device_id`
- `device_name`
- `device_type`
- `model`
- `manufacturer`
- `location`
- `status`
- `protocol`
- `updated_at`

### 5.3 实时状态页

数据来源：

- `TelemetryService.ListRealtimeStatus`
- `TelemetryService.GetRealtimeStatus`
- `TelemetryService.QueryTelemetryHistory`

页面能力：

- 实时状态列表。
- 设备详情状态面板。
- 按在线状态筛选。
- 展示电量、温度、速度、运行模式、错误码。
- 简单历史曲线。

刷新策略：

- 列表 3 到 5 秒轮询一次。
- 详情页 1 到 3 秒轮询一次。
- 不要高频刷新所有页面，避免压垮 RPC 服务。

### 5.4 告警事件页

数据来源：

- `EventService.ListEvents`
- `EventService.GetEvent`
- `EventService.UpdateEventStatus`

页面能力：

- 告警列表。
- 按设备、事件类型、严重级别、状态、时间范围筛选。
- 查看告警详情和指标快照。
- 处理、解决、关闭告警。
- 从告警发起诊断。

重点字段：

- `event_id`
- `device_id`
- `event_type`
- `severity`
- `status`
- `error_code`
- `title`
- `description`
- `metric_snapshot_json`
- `occurred_at`

### 5.5 日志检索页

数据来源：

- `LogService.QueryLogs`
- `LogService.GetLogContext`

页面能力：

- 按设备、服务名、日志级别、关键词、事件 ID、时间范围查询日志。
- 查看日志上下文。
- 从告警详情跳转到相关日志。

重点字段：

- `timestamp`
- `device_id`
- `service_name`
- `source_type`
- `level`
- `message`
- `error_code`
- `context_json`

### 5.6 知识库页

数据来源：

- `KnowledgeService.CreateKnowledgeDocument`
- `KnowledgeService.GetKnowledgeDocument`
- `KnowledgeService.ListKnowledgeDocuments`
- `KnowledgeService.SearchKnowledge`
- `KnowledgeService.RequestKnowledgeIndex`

页面能力：

- 知识文档列表。
- 创建知识文档。
- 查看知识文档详情。
- 按分类、设备类型、关键词、状态筛选。
- 按错误码检索知识。
- 手动触发 RAG 索引。

注意：

- `SearchKnowledge` 当前是 MVP 关键词检索，建议查询词优先使用明确错误码，如 `TEMP_001`。
- `RequestKnowledgeIndex` 用于通知 RAG 服务建立内存索引。

### 5.7 诊断中心页

数据来源：

- `DiagnosisService.CreateFaultRecord`
- `DiagnosisService.GetFaultRecord`
- `DiagnosisService.ListFaultRecords`
- `DiagnosisService.StartDiagnosis`
- `DiagnosisService.GetDiagnosisReport`
- `DiagnosisService.ListDiagnosisReports`
- `DiagnosisService.ConfirmDiagnosisReport`

页面能力：

- 故障记录列表。
- 创建故障记录。
- 从告警事件生成故障记录。
- 发起 AI 诊断。
- 查看诊断报告草案。
- 工程师确认或驳回诊断报告。

建议流程：

```text
告警事件详情
  -> 创建故障记录
  -> StartDiagnosis
  -> GetDiagnosisReport
  -> ConfirmDiagnosisReport
```

### 5.8 网关状态页

数据来源：

- `DeviceGatewayService.GetGatewayStatus`
- `DeviceGatewayService.ListConnectedDevices`
- `DeviceGatewayService.GetForwardingStats`

页面能力：

- 查看 gateway 是否连接 MQTT。
- 查看订阅 Topic。
- 查看在线设备视图。
- 查看 register、telemetry、alarm、log、heartbeat 转发统计。

## 6. 前端导航建议

推荐左侧导航：

```text
总览
设备管理
实时状态
告警事件
日志检索
知识库
诊断中心
网关状态
```

顶部区域建议：

- 当前后端连接状态。
- 当前用户角色占位。
- 刷新按钮。
- 全局时间范围选择。

MVP 阶段可以不做复杂登录权限，先保留用户信息占位；后续如实现用户服务，再接入认证。

## 7. UI 设计原则

- 这是运维工作台，不是营销页面。
- 页面应该信息密度适中，便于扫描和排障。
- 表格、筛选、详情抽屉、状态标签优先。
- 告警严重级别要用稳定颜色：
  - info：蓝/灰
  - warning：黄/橙
  - critical：红
- 时间统一显示本地时间，同时保留原始毫秒时间戳字段用于调试。
- JSON 字段如 `metric_snapshot_json`、`context_json` 可用格式化只读文本框展示。

## 8. 联调验收标准

前端开始开发前，后端必须通过：

```bash
./scripts/run_backend_stack.sh
./scripts/check_backend_stack.sh
```

前端页面开发完成后，至少验收：

1. 能打开客户端并连接后端。
2. 设备列表能看到模拟器注册的设备。
3. 实时状态页能看到电量、温度、速度、运行模式。
4. 告警事件页能看到高温、错误码、离线事件。
5. 日志页能查到模拟器日志。
6. 知识库页能创建文档并检索 `TEMP_001`。
7. 诊断中心能创建故障记录并生成诊断报告。
8. 网关状态页能看到 MQTT 连接状态和转发统计。

## 9. 开发顺序建议

### 9.1 第一阶段：Qt 工程和 RPC 基础

目标：

- 建立 Qt 工程。
- 接入 protobuf/brpc client。
- 封装统一 RPC 调用、错误处理和线程模型。

验收：

- 能调用 `DeviceGatewayService.GetGatewayStatus` 并展示结果。

### 9.2 第二阶段：设备、状态、告警

目标：

- 完成设备管理页。
- 完成实时状态页。
- 完成告警事件页。

验收：

- 模拟器数据能在三个页面中展示。

### 9.3 第三阶段：日志、知识库、诊断

目标：

- 完成日志检索页。
- 完成知识库页。
- 完成诊断中心页。

验收：

- 能从告警跳转日志。
- 能基于事件创建故障记录。
- 能生成并查看诊断报告。

### 9.4 第四阶段：体验完善

目标：

- 加入统一主题。
- 加入加载状态、空状态、错误提示。
- 加入分页、筛选、详情抽屉。
- 加入自动刷新开关。

验收：

- 连续运行 30 分钟，不出现 UI 卡死。
- 后端服务短暂不可用时，前端能提示并恢复。

## 10. 与后端协作约定

- 如果页面需要新增字段，先修改 `proto/*.proto`，再由后端实现。
- 如果只是展示已有字段，不允许前端自造字段含义。
- 如果接口返回异常，保留请求参数、响应 body 和服务名，方便后端排查。
- 前端不要依赖数据库表结构，只依赖 proto 和 RPC。
- 前端不要硬编码测试数据，调试数据由模拟器和 `check_backend_stack.sh` 产生。

## 11. 当前后端已完成能力

当前后端已经完成：

- 设备注册和设备管理。
- MQTT 接入和 gateway 转发统计。
- 实时状态写入 Redis 并提供 RPC 查询。
- 告警事件写入 MySQL 并提供 RPC 查询和状态流转。
- 日志写入 Elasticsearch 并提供 RPC 检索。
- 知识库文档管理和 RAG 索引请求。
- RAG MVP HTTP 服务。
- 故障记录和 AI 诊断报告。
- RabbitMQ 后端异步事件发布链路。
- 一键启动、停止和端到端验收脚本。

当前不阻塞 Qt 前端开发的后续增强：

- 复杂用户权限。
- 多租户组织体系。
- 工单和 SLA。
- 生产级部署和监控。
