from pathlib import Path
from xml.etree.ElementTree import Element, ElementTree, SubElement


OUT = Path("docs/diagrams/deviceops_backend_architecture.drawio")


mxfile = Element("mxfile", {"host": "drawio", "version": "26.0.0"})
diagram = SubElement(mxfile, "diagram", {"name": "DeviceOps Backend Architecture"})
model = SubElement(
    diagram,
    "mxGraphModel",
    {
        "dx": "1800",
        "dy": "1000",
        "grid": "1",
        "gridSize": "10",
        "guides": "1",
        "tooltips": "1",
        "connect": "1",
        "arrows": "1",
        "fold": "1",
        "page": "1",
        "pageScale": "1",
        "pageWidth": "1720",
        "pageHeight": "1080",
        "math": "0",
        "shadow": "0",
    },
)
root = SubElement(model, "root")
SubElement(root, "mxCell", {"id": "0"})
SubElement(root, "mxCell", {"id": "1", "parent": "0"})

next_id = 2
ids = {}


def cell_id(key):
    global next_id
    if key not in ids:
        ids[key] = f"id{next_id}"
        next_id += 1
    return ids[key]


def font(style):
    return style + "fontFamily=Microsoft YaHei;"


def add_vertex(key, label, x, y, w, h, style, parent="1"):
    cid = cell_id(key)
    cell = SubElement(
        root,
        "mxCell",
        {
            "id": cid,
            "value": label,
            "style": font(style),
            "vertex": "1",
            "parent": parent,
        },
    )
    SubElement(
        cell,
        "mxGeometry",
        {"x": str(x), "y": str(y), "width": str(w), "height": str(h), "as": "geometry"},
    )
    return cid


def add_text(key, label, x, y, w, h, size=14, color="#111827", bold=False, align="center"):
    style = (
        "text;html=1;whiteSpace=wrap;strokeColor=none;fillColor=none;"
        f"align={align};verticalAlign=middle;fontSize={size};fontColor={color};"
    )
    if bold:
        style += "fontStyle=1;"
    return add_vertex(key, label, x, y, w, h, style)


def add_edge(key, source, target, label="", style_extra="", points=None):
    cid = cell_id(key)
    style = (
        "edgeStyle=orthogonalEdgeStyle;rounded=1;orthogonalLoop=1;jettySize=auto;"
        "html=1;endArrow=block;endFill=1;strokeWidth=2;"
        "labelBackgroundColor=#ffffff;fontSize=11;"
    )
    style += style_extra
    cell = SubElement(
        root,
        "mxCell",
        {
            "id": cid,
            "value": label,
            "style": font(style),
            "edge": "1",
            "parent": "1",
            "source": ids[source],
            "target": ids[target],
        },
    )
    geo = SubElement(cell, "mxGeometry", {"relative": "1", "as": "geometry"})
    if points:
        arr = SubElement(geo, "Array", {"as": "points"})
        for x, y in points:
            SubElement(arr, "mxPoint", {"x": str(x), "y": str(y)})
    return cid


outer = (
    "rounded=0;whiteSpace=wrap;html=1;fillColor=#ffffff;strokeColor=#111827;"
    "dashed=1;dashPattern=3 3;strokeWidth=2;verticalAlign=top;align=left;"
    "spacingLeft=12;spacingTop=8;fontStyle=1;fontSize=18;"
)
layer = (
    "swimlane;startSize=28;html=1;rounded=0;collapsible=0;container=1;"
    "recursiveResize=0;strokeWidth=1;dashed=1;dashPattern=2 3;"
    "fillColor=#f8fafc;strokeColor=#94a3b8;fontStyle=1;fontSize=14;"
)
client = "rounded=1;whiteSpace=wrap;html=1;fillColor=#e1d5e7;strokeColor=#9673a6;arcSize=8;fontSize=13;"
device = "rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;arcSize=8;fontSize=13;"
gateway = "rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;arcSize=8;fontStyle=1;fontSize=13;"
service = "rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;arcSize=8;fontSize=13;"
ai = "rounded=1;whiteSpace=wrap;html=1;fillColor=#e1d5e7;strokeColor=#9673a6;arcSize=8;fontSize=13;"
queue = "rounded=1;whiteSpace=wrap;html=1;fillColor=#fff2cc;strokeColor=#d6b656;arcSize=8;fontStyle=1;fontSize=13;"
bus = "rounded=1;whiteSpace=wrap;html=1;fillColor=#eff6ff;strokeColor=#2563eb;arcSize=8;fontStyle=1;fontSize=13;"
db = "shape=cylinder3;whiteSpace=wrap;html=1;boundedLbl=1;size=15;fillColor=#d5e8d4;strokeColor=#82b366;fontSize=13;"
db_optional = db + "dashed=1;dashPattern=4 4;"
infra = "rounded=1;whiteSpace=wrap;html=1;fillColor=#f5f5f5;strokeColor=#666666;arcSize=8;fontSize=13;"
note = "rounded=1;whiteSpace=wrap;html=1;fillColor=#f8fafc;strokeColor=#cbd5e1;arcSize=6;fontSize=11;"


add_text("title", "DeviceOps 后端微服务架构图", 600, 20, 520, 36, 24, "#0f172a", True)

add_vertex("platform", "DeviceOps Backend Platform", 180, 70, 1320, 810, outer)
add_vertex("access_layer", "接入层", 220, 120, 260, 260, layer)
add_vertex("service_layer", "业务服务层", 520, 120, 500, 520, layer)
add_vertex("ai_layer", "AI 服务层", 520, 670, 500, 160, layer)
add_vertex("data_layer", "数据层 / 基础设施", 1060, 120, 390, 710, layer)

add_vertex("devices", "机器人 / 工业设备\n设备模拟器", 40, 150, 120, 78, device)
add_vertex("qt_client", "Qt 客户端\n运维工作台", 40, 430, 120, 72, client)

add_vertex("mqtt", "MQTT Broker\nMosquitto :1883", 250, 160, 200, 60, queue)
add_vertex(
    "device_gateway",
    "device-gateway\n设备接入 / 协议解析 / 转发\nRPC :9101",
    250,
    260,
    200,
    76,
    gateway,
)
add_vertex("api_bus", "brpc + Protobuf\nHTTP JSON 调试入口", 550, 168, 440, 54, bus)

services = [
    ("device_svc", "device-service\n设备主数据 / 接入校验\n:9201", 550, 270),
    ("telemetry_svc", "telemetry-service\n实时状态 / 历史状态\n:9301", 790, 270),
    ("event_svc", "event-service\n规则匹配 / 告警流转\n:9401", 550, 385),
    ("log_svc", "log-service\n日志写入 / 检索\n:9501", 790, 385),
    ("knowledge_svc", "knowledge-service\n知识文档 / 检索 / 索引\n:9600", 550, 500),
    ("diagnosis_svc", "diagnosis-service\n故障记录 / 诊断报告\n:9700", 790, 500),
]
for key, label, x, y in services:
    add_vertex(key, label, x, y, 200, 70, service)

add_vertex("rabbitmq", "RabbitMQ\n业务事件总线", 660, 610, 220, 58, queue)

add_vertex("rag", "rag-service\n/retrieve /diagnose\nHTTP :9601", 550, 715, 200, 70, ai)
add_vertex("agent", "Agent / RAG 编排\n上下文聚合 + 诊断生成", 790, 715, 200, 70, ai)

add_vertex("mysql", "MySQL\n设备 / 事件 / 故障 / 报告 / 知识", 1100, 160, 145, 82, db)
add_vertex("redis", "Redis\n实时状态 / 在线 TTL", 1270, 160, 145, 82, db)
add_vertex("es", "Elasticsearch\n日志 / 事件检索", 1100, 310, 145, 82, db)
add_vertex("milvus", "Milvus\n向量库（规划）", 1270, 310, 145, 82, db_optional)
add_vertex("etcd", "etcd\n服务注册发现", 1100, 470, 145, 70, infra)
add_vertex("docker", "Docker / Compose\n本地联调环境", 1270, 470, 145, 70, infra)
add_vertex("mq_ops", "交换机 / 队列\ntelemetry.status\ntelemetry.offline\nevent.alarm\nlog.ingest\ndiagnosis.task", 1100, 610, 315, 118, note)

add_edge(
    "device_to_mqtt",
    "devices",
    "mqtt",
    "",
    "exitX=1;exitY=0.5;entryX=0;entryY=0.5;strokeColor=#16a34a;",
)
add_edge(
    "mqtt_to_gateway",
    "mqtt",
    "device_gateway",
    "订阅 device/+/+",
    "exitX=0.5;exitY=1;entryX=0.5;entryY=0;strokeColor=#d97706;",
)
add_edge(
    "gateway_to_api",
    "device_gateway",
    "api_bus",
    "",
    "exitX=1;exitY=0.45;entryX=0;entryY=0.55;strokeColor=#d97706;",
)
add_edge(
    "client_to_api",
    "qt_client",
    "api_bus",
    "",
    "exitX=1;exitY=0.45;entryX=0;entryY=0.45;strokeColor=#2563eb;",
    [(500, 465), (500, 195)],
)

add_edge(
    "service_layer_to_mq",
    "service_layer",
    "rabbitmq",
    "",
    "exitX=0.5;exitY=1;entryX=0.3;entryY=0;strokeColor=#b45309;",
)
add_edge(
    "mq_to_services",
    "rabbitmq",
    "service_layer",
    "",
    "dashed=1;exitX=0.7;exitY=0;entryX=0.72;entryY=1;strokeColor=#b45309;",
)
add_edge(
    "mq_to_ops",
    "rabbitmq",
    "mq_ops",
    "",
    "exitX=1;exitY=0.5;entryX=0;entryY=0.55;strokeColor=#b45309;",
    [(1040, 640)],
)

add_edge(
    "services_to_data_layer",
    "service_layer",
    "data_layer",
    "",
    "exitX=1;exitY=0.5;entryX=0;entryY=0.5;strokeColor=#16a34a;",
)
add_edge(
    "services_to_registry",
    "service_layer",
    "etcd",
    "",
    "dashed=1;endArrow=open;exitX=1;exitY=0.72;entryX=0;entryY=0.5;strokeColor=#6b7280;fontColor=#6b7280;",
)
add_edge(
    "docker_to_infra",
    "docker",
    "etcd",
    "",
    "dashed=1;endArrow=open;exitX=0;exitY=0.5;entryX=1;entryY=0.5;strokeColor=#9ca3af;",
)

add_edge(
    "diagnosis_to_agent",
    "diagnosis_svc",
    "agent",
    "",
    "exitX=0.5;exitY=1;entryX=0.5;entryY=0;strokeColor=#7c3aed;",
)
add_edge(
    "agent_to_rag",
    "agent",
    "rag",
    "",
    "exitX=0;exitY=0.5;entryX=1;entryY=0.5;strokeColor=#7c3aed;",
)
add_edge(
    "knowledge_to_rag",
    "knowledge_svc",
    "rag",
    "",
    "exitX=0.35;exitY=1;entryX=0.4;entryY=0;strokeColor=#7c3aed;",
)

add_text("flow_note_1", "设备侧 MQTT 接入", 185, 120, 140, 24, 12, "#166534", False, "left")
add_text("flow_note_2", "客户端通过 HTTP JSON 调用 brpc 服务", 185, 405, 250, 24, 12, "#1d4ed8", False, "left")
add_text("flow_note_3", "业务服务间使用 brpc + protobuf，同步查询和操作", 642, 236, 340, 26, 12, "#1d4ed8")
add_text("flow_note_4", "绿色：数据读写 / 缓存 / 检索", 1095, 255, 280, 24, 12, "#166534", False, "left")
add_text("flow_note_5", "紫色：RAG / 向量检索链路", 1095, 415, 260, 24, 12, "#6d28d9", False, "left")
add_text("flow_note_6", "棕色：RabbitMQ 异步事件链路", 1095, 750, 280, 24, 12, "#92400e", False, "left")

add_text("legend_title", "图例", 40, 900, 120, 24, 14, "#111827", True)
legend_items = [
    ("服务", "#dae8fc", "#6c8ebf"),
    ("网关 / 接入", "#ffe6cc", "#d79b00"),
    ("消息队列", "#fff2cc", "#d6b656"),
    ("数据存储", "#d5e8d4", "#82b366"),
]
for i, (label, fill, stroke) in enumerate(legend_items):
    y = 932 + i * 28
    add_vertex(f"legend_box_{i}", "", 45, y, 28, 16, f"rounded=0;html=1;fillColor={fill};strokeColor={stroke};")
    add_vertex(
        f"legend_text_{i}",
        label,
        84,
        y - 2,
        90,
        20,
        "text;html=1;align=left;verticalAlign=middle;strokeColor=none;fillColor=none;fontSize=12;",
    )

tech_tags = [
    ("C++17", "#334155"),
    ("BRPC", "#2563eb"),
    ("PROTOBUF", "#2563eb"),
    ("HTTP JSON", "#2563eb"),
    ("MQTT", "#16a34a"),
    ("RABBITMQ", "#b45309"),
    ("MYSQL", "#0f766e"),
    ("REDIS", "#dc2626"),
    ("ELASTICSEARCH", "#7c3aed"),
    ("RAG/AGENT", "#9333ea"),
    ("DOCKER", "#0ea5e9"),
    ("CMAKE", "#ea580c"),
]
add_text("tech_title", "核心技术栈", 225, 900, 180, 24, 14, "#111827", True, "left")
for idx, (name, color) in enumerate(tech_tags):
    x = 225 + (idx % 6) * 200
    y = 932 + (idx // 6) * 38
    add_vertex(
        f"tech_{idx}",
        name,
        x,
        y,
        155,
        26,
        f"rounded=0;whiteSpace=wrap;html=1;fillColor={color};strokeColor={color};"
        "fontColor=#ffffff;fontStyle=1;fontSize=15;",
    )

OUT.parent.mkdir(parents=True, exist_ok=True)
ElementTree(mxfile).write(OUT, encoding="utf-8", xml_declaration=True)
print(OUT)
