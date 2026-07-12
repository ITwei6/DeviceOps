
```markdown
# DeviceOps Agent Development Guide


# 1. 项目定位


## 1.1 项目名称


DeviceOps


## 1.2 项目类型


基于 C++ 微服务架构和 Agent 技术的智能设备运维平台。


## 1.3 项目目标


面向机器人、工业设备、智能终端等设备场景。


实现：

- 设备接入
- 设备状态监控
- 实时数据采集
- 异常事件检测
- 日志分析
- 故障诊断
- RAG知识检索
- Agent智能辅助分析


系统目标：

模拟真实企业级机器人/工业设备云端运维平台。


业务流程：

```

智能设备

↓

数据上传

↓

设备接入服务

↓

数据处理

↓

异常检测

↓

故障分析Agent

↓

知识库检索(RAG)

↓

生成诊断报告

↓

辅助工程师解决问题

```


---

# 2. AI协作开发模式


本项目采用多 Agent 协作。


角色分为：


```

Architect Codex

负责设计

Developer Codex

负责实现

Reviewer Codex

负责审查

```


不同角色必须保持职责边界。


---

# 3. Architect Codex规范


## 3.1 角色定义


你是一名：

企业级系统架构师。


负责：

- 业务分析
- 需求设计
- 系统架构
- 服务拆分
- 数据库设计
- 通信协议设计
- 开发计划制定


---

## 3.2 工作目录


主要修改：

```

docs/

```


例如：

```

docs/

├──01_requirement.md

├──02_architecture.md

├──03_database.md

├──04_protocol.md

├──05_task_plan.md

```


---

## 3.3 禁止事项


Architect Codex禁止：


- 编写业务代码
- 修改src代码
- 随意增加技术栈
- 跳过架构设计


必须：

先设计

↓

文档确认

↓

再进入开发阶段



---

# 4. Developer Codex规范


## 4.1 角色定义


你是一名：

企业级C++后端工程师。


负责：


- 微服务开发
- C++代码实现
- Docker开发
- 单元测试
- Bug修复
- 工程优化


---

## 4.2 开发原则


必须：

根据 docs/ 中设计实现。


禁止：


- 自己重新设计系统架构
- 随意修改服务边界
- 替换技术方案


如果发现架构问题：

先提出。

不要直接修改。



---

# 5. 项目基础框架规范


## 5.1 基础框架


DeviceOps必须基于：


cpp-microservice-kit


进行开发。


项目地址：

https://github.com/ITwei6/cpp-microservice-kit



该脚手架已经提供：

- C++微服务基础环境
- 第三方依赖环境
- CMake配置
- 基础库封装
- RPC通信能力
- 服务注册发现
- 日志系统
- Elasticsearch客户端



---

# 5.2 禁止重复造轮子


禁止：


重新实现：

- RPC框架
- 服务注册中心
- 日志框架
- Elasticsearch客户端
- protobuf通信封装



必须优先使用脚手架提供能力。如果脚手架里面的封装不好，你可以修改这个脚手架！适配当前这个项目！修改完记得提交git
不过目前里面没有mqtt相关的东西，可能需要添加进去并封装



---

# 5.3 基础组件使用规范

里面由样例可以参考！

# 6. 技术栈规范


## 后端


C++17


## 微服务


- brpc
- protobuf
- etcd


## 消息通信


设备：

MQTT


服务异步：

RabbitMQ


## 数据存储


MySQL：

业务数据


Redis：

实时状态缓存


Elasticsearch：

日志检索


Milvus：

向量数据库


## AI


- LangGraph
- RAG
- Agent Tool Calling


## GUI


Qt


## 部署


Docker

Docker Compose



---

# 7. Docker开发环境规范


## 7.1 基础原则


项目开发环境不是Ubuntu宿主机。


所有开发：

必须在Docker开发容器中完成。



---

# 7.2 宿主机


Codex运行环境：

Ubuntu虚拟机。


但是：


Ubuntu宿主机禁止：


```

cmake

make

gcc/g++

运行C++服务

```


原因：

项目依赖由开发容器统一管理。



---

# 7.3 开发容器


容器名称：


```

dev-env-service

```



代码映射：


宿主机：


```

~/Desktop/projects/DeviceOps

```



容器：


```

/home/dev/workspace/projects/DeviceOps

```



关系：


```

Ubuntu

~/Desktop/projects/DeviceOps

```
    ↓
```

Docker

/home/dev/workspace/projects/DeviceOps

````


---

# 8. 编译规范


所有编译必须通过：


docker exec


执行。


禁止：


```bash
cmake ..
make
````

虚拟机和开发环境容器的登录凭据通过安全渠道获取，不写入仓库文档。

直接运行。

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

---

# 9. 测试规范

测试必须进入容器。

例如：

```bash
docker exec dev-env-service bash -lc "
cd /home/dev/workspace/projects/DeviceOps/build &&
ctest
"
```

---

# 10. 项目目录规范

最终结构：

```
DeviceOps/


├── AGENTS.md

├── README.md

├── CHANGELOG.md


├── docs/


│
├── services/


│
│
├── device_gateway/


│
├── device_service/


│
├── telemetry_service/


│
├── event_service/


│
├── diagnosis_service/


│
├── knowledge_service/


│
├── simulator/


│
├── qt_client/


│
├── deploy/


│
├── tests/


│
└── scripts/

```

---

# 11. 微服务设计规范

服务必须满足：

高内聚

低耦合

每个服务：

包含：

```
service_name/

├──src/

├──include/

├──proto/

├──tests/

└──CMakeLists.txt

```

---

# 12. Git管理规范

项目统一使用Git。仓库地址是：https://github.com/ITwei6/DeviceOps
已经通过ssh key认证

当前阶段：

所有Agent使用：

main分支。

Git作用：

* 保存开发过程
* 记录阶段成果
* 支持协作
* 方便回滚

---

# 13. Commit规范

格式：

```
type(scope): description
```

类型：

```
feat

fix

docs

test

refactor

chore

```

例如：

需求：

```
docs(requirement): add business analysis
```

架构：

```
docs(architecture): design microservice architecture
```

设备模拟器：

```
feat(simulator): implement device simulator
```

MQTT：

```
feat(mqtt): add mqtt communication
```

诊断Agent：

```
feat(agent): implement diagnosis workflow
```

---

# 14. 提交要求

完成以下阶段必须提交：

```
需求分析完成

架构设计完成

数据库设计完成

协议设计完成

环境搭建完成

服务开发完成

测试完成
```

禁止：

大量修改长期不提交。

---

# 15. CHANGELOG规范

项目必须维护：

```
CHANGELOG.md
```

用于记录：

* 功能增加
* 架构变化
* Bug修复
* 版本变化

格式：

```markdown

## v0.1.0


### Added

新增设备模拟器


### Changed

修改通信协议


### Fixed

修复MQTT连接问题

```

---

# 16. C++开发规范

使用：

C++17

要求：

* RAII
* 智能指针
* const正确性
* namespace隔离
* 清晰模块划分
* 完善日志
* 单元测试

禁止：

* 大量全局变量
* 魔法数字
* 单文件过大
* 重复代码

---

# 17. 每次任务执行流程

开始任务：

1.

查看Git状态

2.

阅读相关docs

3.

确认当前阶段

开发：

1.

修改代码

2.

Docker编译

3.

运行测试

完成：

输出：

```
修改内容

测试结果

下一步建议

Git commit信息
```

禁止：

自动进入下一阶段。

---

# 18. 项目推进阶段

## Phase 1

需求分析

输出：

docs/01_requirement.md

提交：

docs(requirement): add requirement document

---

## Phase 2

系统设计

输出：

* 架构图
* 服务划分
* 数据流

提交：

docs(architecture): add system architecture

---

## Phase 3

脚手架环境集成

完成：

* Docker环境
* cpp-microservice-kit
* CMake工程

提交：

chore(environment): setup development environment

---

## Phase 4

设备模拟器

实现：

模拟机器人设备。

产生：

* 电量
* 温度
* 速度
* 错误码

---

## Phase 5

设备接入服务

实现：

MQTT接入。

---

## Phase 6

数据处理服务

实现：

状态存储

异常检测

---

## Phase 7

故障诊断Agent

实现：

RAG

工具调用

故障分析

---

## Phase 8

Qt客户端

实现：

* 设备列表
* 实时状态
* 报警
* AI报告

---

# 19. 最终开发原则

本项目遵循：

```
架构设计

↓

文档驱动

↓

脚手架基础

↓

微服务开发

↓

Docker验证

↓

Git记录 

↓

持续迭代
```

目标：

开发一个接近真实企业架构的：

智能设备运维 Agent 平台。

```
