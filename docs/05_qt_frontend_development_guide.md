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
- 理解前端只通过后端 HTTP JSON/RPC 接口查询和操作，不直接访问 MySQL、Redis、Elasticsearch 或 MQTT。
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

## 3. Windows Qt 开发环境

Qt 前端可以在 Windows 上开发和运行，推荐这样分工：

```text
Windows:
- 开发和运行 Qt 桌面客户端
- 使用 Qt Network 调后端 HTTP JSON 接口

Linux 虚拟机 / 后端开发容器:
- 运行 DeviceOps 后端服务
- 运行 MySQL、Redis、Elasticsearch、RabbitMQ、MQTT、RAG 服务
```

不建议把 Qt Creator 和桌面客户端放进后端容器里开发。Qt 是 GUI 程序，放容器里会额外处理 X11/Wayland、OpenGL、字体、输入法和调试器问题；Windows 本机开发更直接。

Windows 推荐安装：

| 工具 | 建议 |
| --- | --- |
| Qt | Qt 6.5+，Qt 6.6/6.7 也可以 |
| IDE | Qt Creator |
| 编译器 | MSVC 2022 优先；也可以 MinGW，但团队要统一 |
| 构建工具 | CMake + Ninja |
| Git | Git for Windows |
| SSH | Windows OpenSSH 客户端，PowerShell 可直接使用 `ssh` |
| 调试工具 | Visual Studio Build Tools 或完整 Visual Studio |

Windows Qt 客户端不需要安装：

- MySQL
- Redis
- Elasticsearch
- RabbitMQ
- MQTT broker
- Python RAG 服务

这些都由后端环境提供。

### 3.1 Windows 连接后端方式

方式一：直接访问虚拟机 IP。

如果 Windows 能直接访问后端虚拟机或宿主机映射端口，Qt 配置为：

```text
http://<后端机器IP>:9101
http://<后端机器IP>:9201
http://<后端机器IP>:9301
http://<后端机器IP>:9401
http://<后端机器IP>:9501
http://<后端机器IP>:9600
http://<后端机器IP>:9700
```

方式二：通过 SSH 端口转发。

如果后端服务只在容器/虚拟机内部监听 `127.0.0.1`，在 Windows PowerShell 执行：

```powershell
ssh -p 2222 dev@<后端机器IP> `
  -L 9101:127.0.0.1:9101 `
  -L 9201:127.0.0.1:9201 `
  -L 9301:127.0.0.1:9301 `
  -L 9401:127.0.0.1:9401 `
  -L 9501:127.0.0.1:9501 `
  -L 9600:127.0.0.1:9600 `
  -L 9700:127.0.0.1:9700
```

当前开发容器 SSH 密码：

```text
1
```

端口转发保持运行后，Qt 客户端访问：

```text
http://127.0.0.1:9101
http://127.0.0.1:9201
http://127.0.0.1:9301
http://127.0.0.1:9401
http://127.0.0.1:9501
http://127.0.0.1:9600
http://127.0.0.1:9700
```

建议 Qt 客户端提供 `config.json`：

```json
{
  "gatewayBaseUrl": "http://127.0.0.1:9101",
  "deviceBaseUrl": "http://127.0.0.1:9201",
  "telemetryBaseUrl": "http://127.0.0.1:9301",
  "eventBaseUrl": "http://127.0.0.1:9401",
  "logBaseUrl": "http://127.0.0.1:9501",
  "knowledgeBaseUrl": "http://127.0.0.1:9600",
  "diagnosisBaseUrl": "http://127.0.0.1:9700",
  "requestTimeoutMs": 10000
}
```

### 3.2 Windows 上怎么运行

开发运行流程：

1. 后端同学在 Linux 开发容器里执行：

```bash
cd /home/dev/workspace/projects/DeviceOps
./scripts/run_backend_stack.sh
```

2. Windows 前端同学打开 PowerShell，按需建立 SSH 端口转发。
3. Windows 前端同学用 Qt Creator 打开 Qt 客户端工程。
4. 选择 Kit，例如 `Desktop Qt 6.x MSVC 2022 64bit`。
5. 配置 CMake，生成构建目录。
6. 运行 Qt 客户端。
7. 客户端从 `config.json` 读取后端地址并调用接口。

联调结束后，后端同学停止服务：

```bash
./scripts/stop_backend_stack.sh
```

### 3.3 Windows 网络排查

先在 Windows PowerShell 验证后端可访问：

```powershell
curl.exe -X POST http://127.0.0.1:9101/deviceops.gateway.DeviceGatewayService/GetGatewayStatus `
  -H "Content-Type: application/json" `
  -d "{\"gateway_id\":\"device-gateway-001\"}"
```

能返回包含 `response` 的 JSON，说明网络连通。

常见问题：

- `Connection refused`：后端服务没启动，或 SSH 端口转发没开。
- `Could not resolve hostname`：`<后端机器IP>` 写错，或 Windows 不能访问该机器。
- PowerShell 命令换行失败：反引号 `` ` `` 后面不能有空格。
- Qt 请求卡 UI：网络请求没有放到异步流程。
- 中文乱码：Qt 源文件和 JSON 统一使用 UTF-8。

## 4. Qt 技术建议

推荐使用：

- Qt 5.15 或 Qt 6.x。
- C++17。
- CMake 管理 Qt 工程。
- Qt Widgets 优先；如果已有团队标准要求 QML，也可以使用 QML，但要保持业务层和 UI 层分离。
- Windows Qt 客户端优先使用 `QNetworkAccessManager` 调后端 HTTP JSON 接口。
- `proto/*.proto` 作为字段和接口契约来源；Windows 端 MVP 阶段不强制编译 brpc/protobuf client。
- 如果团队后续确认 Qt 客户端运行在 Linux，且 brpc/protobuf 依赖已经稳定，可以再切换为 brpc C++ client。

不建议：

- 不要让 Qt 直接连 MySQL、Redis、Elasticsearch。
- 不要让 Qt 直接订阅 MQTT。
- 不要让 Qt 直接消费 RabbitMQ。
- 不要让 Qt 直接调用 RAG HTTP 服务。
- 不要在 UI 线程里发同步网络请求。
- 不要在页面里散落 HTTP JSON 调用，应通过统一 API client 层封装。

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
| `api/` | 封装 HTTP client、请求、响应和错误处理 |
| `models/` | Qt Model/View 数据模型，适配表格、列表、详情 |
| `pages/` | 页面级 UI 组合 |
| `widgets/` | 可复用控件，如状态标签、分页器、时间范围筛选 |
| `app/` | 应用启动、路由、全局配置、主题 |

## 5. 后端调用方式

后端 brpc 服务同时支持 HTTP JSON 调用。Windows Qt 客户端 MVP 阶段推荐使用 HTTP JSON，避免在 Windows 上编译 brpc 依赖。

示例封装：

```text
api/
├── ApiConfig
├── HttpClient
├── DeviceApi
├── TelemetryApi
├── EventApi
├── LogApi
├── KnowledgeApi
├── DiagnosisApi
└── GatewayApi
```

每个 API 类只做三件事：

- 构造 JSON request，字段名和 `proto/*.proto` 保持一致。
- 使用 `QNetworkAccessManager` POST 到对应服务 URL。
- 将 JSON response 转成 Qt 页面需要的数据结构。

HTTP 调用示例：

```text
POST http://127.0.0.1:9201/deviceops.device.DeviceService/GetDevice
Content-Type: application/json

{
  "device_id": "robot-e2e-xxx"
}
```

所有网络请求必须异步执行：

```text
UI Thread
  -> ViewModel / Controller
  -> Api Client
  -> QNetworkAccessManager
  -> finished signal
  -> signal 返回 UI Thread
```

如果后续使用 brpc C++ client，则必须放到后台线程执行：

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
- HTTP 状态码非 2xx：展示网络/服务不可用错误。
- JSON 解析失败：展示响应格式错误，并记录原始响应。
- 超时：提示“服务响应超时，请检查后端服务是否启动”。

## 6. 页面模块规划

### 6.1 总览仪表盘

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

### 6.2 设备管理页

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

### 6.3 实时状态页

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
- 不要高频刷新所有页面，避免压垮后端服务。

### 6.4 告警事件页

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

### 6.5 日志检索页

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

### 6.6 知识库页

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

### 6.7 诊断中心页

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

### 6.8 网关状态页

数据来源：

- `DeviceGatewayService.GetGatewayStatus`
- `DeviceGatewayService.ListConnectedDevices`
- `DeviceGatewayService.GetForwardingStats`

页面能力：

- 查看 gateway 是否连接 MQTT。
- 查看订阅 Topic。
- 查看在线设备视图。
- 查看 register、telemetry、alarm、log、heartbeat 转发统计。

## 7. 前端导航建议

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

## 8. UI 设计原则

- 这是运维工作台，不是营销页面。
- 页面应该信息密度适中，便于扫描和排障。
- 表格、筛选、详情抽屉、状态标签优先。
- 告警严重级别要用稳定颜色：
  - info：蓝/灰
  - warning：黄/橙
  - critical：红
- 时间统一显示本地时间，同时保留原始毫秒时间戳字段用于调试。
- JSON 字段如 `metric_snapshot_json`、`context_json` 可用格式化只读文本框展示。

## 9. 联调验收标准

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

## 10. 开发顺序建议

### 10.1 第一阶段：Qt 工程和 API 调用基础

目标：

- 建立 Qt 工程。
- 接入 HTTP JSON API client。
- 封装统一后端调用、错误处理和线程模型。

验收：

- 能调用 `DeviceGatewayService.GetGatewayStatus` 并展示结果。

### 10.2 第二阶段：设备、状态、告警

目标：

- 完成设备管理页。
- 完成实时状态页。
- 完成告警事件页。

验收：

- 模拟器数据能在三个页面中展示。

### 10.3 第三阶段：日志、知识库、诊断

目标：

- 完成日志检索页。
- 完成知识库页。
- 完成诊断中心页。

验收：

- 能从告警跳转日志。
- 能基于事件创建故障记录。
- 能生成并查看诊断报告。

### 10.4 第四阶段：体验完善

目标：

- 加入统一主题。
- 加入加载状态、空状态、错误提示。
- 加入分页、筛选、详情抽屉。
- 加入自动刷新开关。

验收：

- 连续运行 30 分钟，不出现 UI 卡死。
- 后端服务短暂不可用时，前端能提示并恢复。

## 11. 与后端协作约定

- 如果页面需要新增字段，先修改 `proto/*.proto`，再由后端实现。
- 如果只是展示已有字段，不允许前端自造字段含义。
- 如果接口返回异常，保留请求参数、响应 body 和服务名，方便后端排查。
- 前端不要依赖数据库表结构，只依赖 proto 字段契约和 HTTP JSON 接口。
- 前端不要硬编码测试数据，调试数据由模拟器和 `check_backend_stack.sh` 产生。

## 12. 当前后端已完成能力

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
