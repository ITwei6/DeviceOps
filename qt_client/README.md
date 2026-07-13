# DeviceOps Qt Client

DeviceOps 桌面客户端，基于 Qt Widgets + Qt Network，通过后端 brpc HTTP JSON 接口访问设备、状态、告警、日志、知识库、诊断和网关状态。

## 构建

推荐使用 Qt Creator 打开本目录，也可以命令行构建：

```powershell
cmake -S . -B build -G Ninja
cmake --build build
```

运行前确认后端已启动，并按需修改运行目录下的 `config.json`。
