#include "main_window.h"

#include "json_utils.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QAbstractItemView>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {

const QString kGatewayService = "deviceops.gateway.DeviceGatewayService";
const QString kDeviceService = "deviceops.device.DeviceService";
const QString kTelemetryService = "deviceops.telemetry.TelemetryService";
const QString kEventService = "deviceops.event.EventService";
const QString kLogService = "deviceops.log.LogService";
const QString kKnowledgeService = "deviceops.knowledge.KnowledgeService";
const QString kDiagnosisService = "deviceops.diagnosis.DiagnosisService";

QPushButton* button(const QString& text)
{
    auto* btn = new QPushButton(text);
    btn->setMinimumHeight(32);
    return btn;
}

QLineEdit* lineEdit(const QString& placeholder)
{
    auto* edit = new QLineEdit;
    edit->setPlaceholderText(placeholder);
    edit->setMinimumHeight(32);
    return edit;
}

QComboBox* enumCombo(const QVector<QPair<int, QString>>& items)
{
    auto* combo = new QComboBox;
    combo->setMinimumHeight(32);
    for (const auto& item : items) {
        combo->addItem(QString::number(item.first) + " - " + item.second, item.first);
    }
    return combo;
}

QLabel* metricLabel(const QString& title)
{
    auto* label = new QLabel(title + "\n0");
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    label->setMinimumHeight(76);
    label->setStyleSheet("QLabel { background:#ffffff; border:1px solid #d8dee9; border-radius:6px; padding:12px; font-size:14px; }");
    return label;
}

void setupTable(QTableWidget* table)
{
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setWordWrap(false);
}

QString objectToPrettyText(const QJsonObject& object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented));
}

QJsonObject parseStoredObject(const QVariant& data)
{
    const QJsonDocument doc = QJsonDocument::fromJson(data.toString().toUtf8());
    return doc.object();
}

QJsonObject dialogFields(QWidget* parent,
                         const QString& title,
                         const QVector<QPair<QString, QWidget*>>& fields)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.resize(560, 420);
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    for (const auto& field : fields) {
        form->addRow(field.first, field.second);
    }
    layout->addLayout(form);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return {};
    }

    QJsonObject object;
    for (const auto& field : fields) {
        const QString name = field.first;
        if (auto* edit = qobject_cast<QLineEdit*>(field.second)) {
            object.insert(name, edit->text());
        } else if (auto* plain = qobject_cast<QPlainTextEdit*>(field.second)) {
            object.insert(name, plain->toPlainText());
        } else if (auto* combo = qobject_cast<QComboBox*>(field.second)) {
            object.insert(name, combo->currentData().toInt());
        }
    }
    return object;
}

qint64 nowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , config_(AppConfig::load())
    , http_(config_.requestTimeoutMs(), this)
{
    setWindowTitle("DeviceOps 智能设备运维平台");

    auto* root = new QWidget;
    auto* rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    navigation_ = new QListWidget;
    navigation_->setFixedWidth(184);
    navigation_->addItems({"总览", "设备管理", "实时状态", "告警事件", "日志检索", "知识库", "诊断中心", "网关状态"});
    navigation_->setCurrentRow(0);
    rootLayout->addWidget(navigation_);

    auto* right = new QWidget;
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(16, 12, 16, 16);
    rightLayout->setSpacing(12);

    auto* topBar = new QHBoxLayout;
    statusLabel_ = new QLabel("准备连接后端");
    configLabel_ = new QLabel("配置: " + config_.sourcePath());
    autoRefreshCheck_ = new QCheckBox("自动刷新 5s");
    autoRefreshCheck_->setChecked(true);
    auto* refreshButton = button("刷新当前页");
    topBar->addWidget(statusLabel_, 1);
    topBar->addWidget(configLabel_, 2);
    topBar->addWidget(autoRefreshCheck_);
    topBar->addWidget(refreshButton);
    rightLayout->addLayout(topBar);

    stack_ = new QStackedWidget;
    stack_->addWidget(createDashboardPage());
    stack_->addWidget(createDevicePage());
    stack_->addWidget(createTelemetryPage());
    stack_->addWidget(createEventPage());
    stack_->addWidget(createLogPage());
    stack_->addWidget(createKnowledgePage());
    stack_->addWidget(createDiagnosisPage());
    stack_->addWidget(createGatewayPage());
    rightLayout->addWidget(stack_, 1);
    rootLayout->addWidget(right, 1);

    setCentralWidget(root);

    connect(navigation_, &QListWidget::currentRowChanged, this, [this](int index) {
        stack_->setCurrentIndex(index);
        refreshCurrentPage();
    });
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshCurrentPage);

    connect(&refreshTimer_, &QTimer::timeout, this, [this]() {
        if (autoRefreshCheck_->isChecked()) {
            refreshCurrentPage();
        }
    });
    refreshTimer_.setInterval(5000);
    refreshTimer_.start();

    setStyleSheet(
        "QMainWindow, QWidget { background:#f5f7fb; color:#1f2937; }"
        "QListWidget { background:#172033; color:#dbe4f0; border:0; padding-top:12px; }"
        "QListWidget::item { height:42px; padding-left:16px; }"
        "QListWidget::item:selected { background:#2563eb; color:#ffffff; }"
        "QPushButton { background:#2563eb; color:#ffffff; border:0; border-radius:5px; padding:6px 12px; }"
        "QPushButton:hover { background:#1d4ed8; }"
        "QLineEdit, QComboBox, QPlainTextEdit { background:#ffffff; border:1px solid #cfd6e4; border-radius:4px; padding:5px; }"
        "QTableWidget { background:#ffffff; alternate-background-color:#f8fafc; border:1px solid #d8dee9; gridline-color:#e5e7eb; }"
        "QHeaderView::section { background:#edf2f7; border:0; border-right:1px solid #d8dee9; padding:7px; font-weight:600; }"
        "QGroupBox { background:#ffffff; border:1px solid #d8dee9; border-radius:6px; margin-top:10px; padding:10px; }"
        "QGroupBox::title { subcontrol-origin:margin; left:10px; padding:0 4px; }");

    refreshAll();
}

QWidget* MainWindow::pageShell(const QString& title, const QString& subtitle, QLayout* content)
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    auto* titleLabel = new QLabel(QString("<b style='font-size:22px'>%1</b><br><span style='color:#64748b'>%2</span>").arg(title, subtitle));
    layout->addWidget(titleLabel);
    layout->addLayout(content, 1);
    return page;
}

void MainWindow::callApi(const QString& serviceKey,
                         const QString& serviceName,
                         const QString& methodName,
                         const QJsonObject& payload,
                         const QString& operation,
                         std::function<void(const QJsonObject&)> onSuccess)
{
    setBusy(operation + "...");
    http_.postJson(config_.baseUrl(serviceKey), serviceName, methodName, payload, [this, operation, onSuccess](const ApiResult& result) {
        if (!result.ok) {
            showError(operation, result);
            return;
        }
        setReady(operation + "完成");
        onSuccess(result.body);
    });
}

void MainWindow::setBusy(const QString& text)
{
    statusLabel_->setText(text);
}

void MainWindow::setReady(const QString& text)
{
    statusLabel_->setText(text);
}

void MainWindow::showError(const QString& operation, const ApiResult& result)
{
    const QString detail = QString("%1失败：%2").arg(operation, result.message);
    statusLabel_->setText(detail);
}

void MainWindow::fillTable(QTableWidget* table, const QVector<TableColumn>& columns, const QJsonArray& rows)
{
    table->clear();
    table->setColumnCount(columns.size());
    table->setRowCount(rows.size());
    QStringList headers;
    for (const auto& column : columns) {
        headers << column.title;
    }
    table->setHorizontalHeaderLabels(headers);

    for (int row = 0; row < rows.size(); ++row) {
        const QJsonObject object = rows.at(row).toObject();
        const QString stored = QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
        for (int col = 0; col < columns.size(); ++col) {
            const auto& column = columns.at(col);
            auto* item = new QTableWidgetItem(jsonValueToDisplay(object.value(column.key), column.key));
            if (col == 0) {
                item->setData(Qt::UserRole, stored);
            }
            table->setItem(row, col, item);
        }
    }
    table->resizeColumnsToContents();
    selectFirstRow(table);
}

QJsonObject MainWindow::selectedObject(QTableWidget* table) const
{
    if (!table || !table->currentItem()) {
        return {};
    }
    const int row = table->currentRow();
    if (row < 0 || !table->item(row, 0)) {
        return {};
    }
    return parseStoredObject(table->item(row, 0)->data(Qt::UserRole));
}

void MainWindow::selectFirstRow(QTableWidget* table)
{
    if (table && table->rowCount() > 0) {
        table->selectRow(0);
    }
}

QWidget* MainWindow::createDashboardPage()
{
    auto* content = new QVBoxLayout;
    auto* metrics = new QGridLayout;
    deviceCountLabel_ = metricLabel("设备总数");
    onlineCountLabel_ = metricLabel("在线设备");
    openEventCountLabel_ = metricLabel("打开告警");
    reportCountLabel_ = metricLabel("诊断报告");
    metrics->addWidget(deviceCountLabel_, 0, 0);
    metrics->addWidget(onlineCountLabel_, 0, 1);
    metrics->addWidget(openEventCountLabel_, 0, 2);
    metrics->addWidget(reportCountLabel_, 0, 3);
    content->addLayout(metrics);

    auto* splitter = new QSplitter(Qt::Horizontal);
    dashboardTelemetryTable_ = new QTableWidget;
    dashboardEventsTable_ = new QTableWidget;
    setupTable(dashboardTelemetryTable_);
    setupTable(dashboardEventsTable_);
    auto* telemetryBox = new QGroupBox("最近设备状态");
    auto* telemetryLayout = new QVBoxLayout(telemetryBox);
    telemetryLayout->addWidget(dashboardTelemetryTable_);
    auto* eventBox = new QGroupBox("最近告警事件");
    auto* eventLayout = new QVBoxLayout(eventBox);
    eventLayout->addWidget(dashboardEventsTable_);
    splitter->addWidget(telemetryBox);
    splitter->addWidget(eventBox);
    content->addWidget(splitter, 1);

    return pageShell("总览仪表盘", "快速观察设备、告警和诊断概况", content);
}

QWidget* MainWindow::createDevicePage()
{
    auto* content = new QVBoxLayout;
    auto* filters = new QHBoxLayout;
    deviceKeywordEdit_ = lineEdit("关键词");
    deviceTypeEdit_ = lineEdit("设备类型");
    deviceStatusCombo_ = enumCombo({{0, "全部"}, {1, "enabled"}, {2, "disabled"}, {3, "maintenance"}});
    auto* refreshBtn = button("查询");
    auto* createBtn = button("新建设备");
    auto* editBtn = button("编辑选中");
    auto* detailBtn = button("查看详情");
    filters->addWidget(deviceKeywordEdit_);
    filters->addWidget(deviceTypeEdit_);
    filters->addWidget(deviceStatusCombo_);
    filters->addWidget(refreshBtn);
    filters->addWidget(createBtn);
    filters->addWidget(editBtn);
    filters->addWidget(detailBtn);
    content->addLayout(filters);
    deviceTable_ = new QTableWidget;
    setupTable(deviceTable_);
    content->addWidget(deviceTable_, 1);

    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshDevices);
    connect(createBtn, &QPushButton::clicked, this, &MainWindow::createDevice);
    connect(editBtn, &QPushButton::clicked, this, &MainWindow::editDevice);
    connect(detailBtn, &QPushButton::clicked, this, [this]() { showSelectedJson(deviceTable_, "设备详情"); });

    return pageShell("设备管理", "设备基础信息、接入协议和运行状态档案", content);
}

QWidget* MainWindow::createTelemetryPage()
{
    auto* content = new QVBoxLayout;
    auto* filters = new QHBoxLayout;
    telemetryDeviceIdsEdit_ = lineEdit("设备 ID，多个用逗号分隔");
    telemetryOnlyOnlineCheck_ = new QCheckBox("仅在线");
    auto* refreshBtn = button("查询");
    filters->addWidget(telemetryDeviceIdsEdit_, 1);
    filters->addWidget(telemetryOnlyOnlineCheck_);
    filters->addWidget(refreshBtn);
    content->addLayout(filters);
    telemetryTable_ = new QTableWidget;
    setupTable(telemetryTable_);
    content->addWidget(telemetryTable_, 1);

    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshTelemetry);
    return pageShell("实时状态", "轮询展示电量、温度、速度、运行模式和错误码", content);
}

QWidget* MainWindow::createEventPage()
{
    auto* content = new QVBoxLayout;
    auto* filters = new QHBoxLayout;
    eventDeviceEdit_ = lineEdit("设备 ID");
    eventSeverityCombo_ = enumCombo({{0, "全部"}, {1, "info"}, {2, "warning"}, {3, "critical"}});
    eventStatusCombo_ = enumCombo({{0, "全部"}, {1, "open"}, {2, "processing"}, {3, "resolved"}, {4, "closed"}});
    eventNewStatusCombo_ = enumCombo({{1, "open"}, {2, "processing"}, {3, "resolved"}, {4, "closed"}});
    auto* refreshBtn = button("查询");
    auto* statusBtn = button("更新状态");
    auto* diagnoseBtn = button("从告警诊断");
    auto* detailBtn = button("查看详情");
    filters->addWidget(eventDeviceEdit_);
    filters->addWidget(eventSeverityCombo_);
    filters->addWidget(eventStatusCombo_);
    filters->addWidget(refreshBtn);
    filters->addWidget(eventNewStatusCombo_);
    filters->addWidget(statusBtn);
    filters->addWidget(diagnoseBtn);
    filters->addWidget(detailBtn);
    content->addLayout(filters);
    eventTable_ = new QTableWidget;
    setupTable(eventTable_);
    content->addWidget(eventTable_, 1);

    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshEvents);
    connect(statusBtn, &QPushButton::clicked, this, &MainWindow::updateSelectedEventStatus);
    connect(diagnoseBtn, &QPushButton::clicked, this, &MainWindow::createFaultFromSelectedEvent);
    connect(detailBtn, &QPushButton::clicked, this, [this]() { showSelectedJson(eventTable_, "告警详情"); });

    return pageShell("告警事件", "异常事件查询、状态流转和诊断入口", content);
}

QWidget* MainWindow::createLogPage()
{
    auto* content = new QVBoxLayout;
    auto* filters = new QHBoxLayout;
    logDeviceEdit_ = lineEdit("设备 ID");
    logKeywordEdit_ = lineEdit("关键词");
    logEventEdit_ = lineEdit("事件 ID");
    logLevelCombo_ = new QComboBox;
    logLevelCombo_->addItems({"", "debug", "info", "warn", "error"});
    auto* refreshBtn = button("查询");
    auto* detailBtn = button("查看日志");
    filters->addWidget(logDeviceEdit_);
    filters->addWidget(logKeywordEdit_);
    filters->addWidget(logEventEdit_);
    filters->addWidget(logLevelCombo_);
    filters->addWidget(refreshBtn);
    filters->addWidget(detailBtn);
    content->addLayout(filters);
    logTable_ = new QTableWidget;
    setupTable(logTable_);
    content->addWidget(logTable_, 1);

    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshLogs);
    connect(detailBtn, &QPushButton::clicked, this, [this]() { showSelectedJson(logTable_, "日志详情"); });
    return pageShell("日志检索", "按设备、等级、关键词和事件查询日志", content);
}

QWidget* MainWindow::createKnowledgePage()
{
    auto* content = new QVBoxLayout;
    auto* splitter = new QSplitter(Qt::Vertical);

    auto* listBox = new QGroupBox("知识文档");
    auto* listLayout = new QVBoxLayout(listBox);
    auto* listFilters = new QHBoxLayout;
    knowledgeKeywordEdit_ = lineEdit("文档关键词");
    auto* refreshBtn = button("查询文档");
    auto* createBtn = button("创建文档");
    auto* indexBtn = button("触发索引");
    auto* detailBtn = button("查看文档");
    listFilters->addWidget(knowledgeKeywordEdit_);
    listFilters->addWidget(refreshBtn);
    listFilters->addWidget(createBtn);
    listFilters->addWidget(indexBtn);
    listFilters->addWidget(detailBtn);
    listLayout->addLayout(listFilters);
    knowledgeTable_ = new QTableWidget;
    setupTable(knowledgeTable_);
    listLayout->addWidget(knowledgeTable_);

    auto* searchBox = new QGroupBox("知识检索");
    auto* searchLayout = new QVBoxLayout(searchBox);
    auto* searchFilters = new QHBoxLayout;
    knowledgeSearchEdit_ = lineEdit("查询词或错误码，例如 TEMP_001");
    auto* searchBtn = button("检索");
    searchFilters->addWidget(knowledgeSearchEdit_);
    searchFilters->addWidget(searchBtn);
    searchLayout->addLayout(searchFilters);
    knowledgeSearchTable_ = new QTableWidget;
    setupTable(knowledgeSearchTable_);
    searchLayout->addWidget(knowledgeSearchTable_);

    splitter->addWidget(listBox);
    splitter->addWidget(searchBox);
    content->addWidget(splitter, 1);

    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshKnowledgeDocuments);
    connect(createBtn, &QPushButton::clicked, this, &MainWindow::createKnowledgeDocument);
    connect(indexBtn, &QPushButton::clicked, this, &MainWindow::requestKnowledgeIndex);
    connect(detailBtn, &QPushButton::clicked, this, [this]() { showSelectedJson(knowledgeTable_, "知识文档"); });
    connect(searchBtn, &QPushButton::clicked, this, &MainWindow::searchKnowledge);
    return pageShell("知识库", "文档管理、关键词检索和 RAG 索引触发", content);
}

QWidget* MainWindow::createDiagnosisPage()
{
    auto* content = new QVBoxLayout;
    auto* top = new QHBoxLayout;
    diagnosisDeviceEdit_ = lineEdit("设备 ID");
    auto* refreshBtn = button("刷新");
    auto* createFaultBtn = button("创建故障");
    auto* startBtn = button("发起诊断");
    auto* confirmBtn = button("确认报告");
    auto* rejectBtn = button("驳回报告");
    auto* detailBtn = button("查看报告");
    top->addWidget(diagnosisDeviceEdit_);
    top->addWidget(refreshBtn);
    top->addWidget(createFaultBtn);
    top->addWidget(startBtn);
    top->addWidget(confirmBtn);
    top->addWidget(rejectBtn);
    top->addWidget(detailBtn);
    content->addLayout(top);

    auto* splitter = new QSplitter(Qt::Vertical);
    faultTable_ = new QTableWidget;
    reportTable_ = new QTableWidget;
    setupTable(faultTable_);
    setupTable(reportTable_);
    auto* faultBox = new QGroupBox("故障记录");
    auto* faultLayout = new QVBoxLayout(faultBox);
    faultLayout->addWidget(faultTable_);
    auto* reportBox = new QGroupBox("诊断报告");
    auto* reportLayout = new QVBoxLayout(reportBox);
    reportLayout->addWidget(reportTable_);
    splitter->addWidget(faultBox);
    splitter->addWidget(reportBox);
    content->addWidget(splitter, 1);

    connect(refreshBtn, &QPushButton::clicked, this, [this]() { refreshFaults(); refreshReports(); });
    connect(createFaultBtn, &QPushButton::clicked, this, &MainWindow::createFaultRecord);
    connect(startBtn, &QPushButton::clicked, this, &MainWindow::startDiagnosisForSelectedFault);
    connect(confirmBtn, &QPushButton::clicked, this, [this]() { confirmSelectedReport(true); });
    connect(rejectBtn, &QPushButton::clicked, this, [this]() { confirmSelectedReport(false); });
    connect(detailBtn, &QPushButton::clicked, this, [this]() { showSelectedJson(reportTable_, "诊断报告"); });

    return pageShell("诊断中心", "故障记录、AI 诊断和工程师确认", content);
}

QWidget* MainWindow::createGatewayPage()
{
    auto* content = new QVBoxLayout;
    auto* top = new QHBoxLayout;
    gatewayStatusLabel_ = new QLabel("网关状态未知");
    auto* refreshBtn = button("刷新网关");
    top->addWidget(gatewayStatusLabel_, 1);
    top->addWidget(refreshBtn);
    content->addLayout(top);

    auto* splitter = new QSplitter(Qt::Vertical);
    gatewayDeviceTable_ = new QTableWidget;
    gatewayStatsTable_ = new QTableWidget;
    setupTable(gatewayDeviceTable_);
    setupTable(gatewayStatsTable_);
    auto* deviceBox = new QGroupBox("网关在线设备视图");
    auto* deviceLayout = new QVBoxLayout(deviceBox);
    deviceLayout->addWidget(gatewayDeviceTable_);
    auto* statsBox = new QGroupBox("转发统计");
    auto* statsLayout = new QVBoxLayout(statsBox);
    statsLayout->addWidget(gatewayStatsTable_);
    splitter->addWidget(deviceBox);
    splitter->addWidget(statsBox);
    content->addWidget(splitter, 1);

    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshGateway);
    return pageShell("网关状态", "MQTT 连接、订阅 Topic、在线设备和消息转发统计", content);
}

void MainWindow::refreshAll()
{
    refreshDashboard();
    refreshDevices();
    refreshTelemetry();
    refreshEvents();
    refreshKnowledgeDocuments();
    refreshFaults();
    refreshReports();
    refreshGateway();
}

void MainWindow::refreshCurrentPage()
{
    switch (stack_->currentIndex()) {
    case 0: refreshDashboard(); break;
    case 1: refreshDevices(); break;
    case 2: refreshTelemetry(); break;
    case 3: refreshEvents(); break;
    case 4: refreshLogs(); break;
    case 5:
        refreshKnowledgeDocuments();
        searchKnowledge();
        break;
    case 6:
        refreshFaults();
        refreshReports();
        break;
    case 7: refreshGateway(); break;
    default: break;
    }
}

void MainWindow::refreshDashboard()
{
    callApi("device", kDeviceService, "ListDevices", objectWithPage(1, 200), "加载设备概况", [this](const QJsonObject& body) {
        const QJsonArray devices = body.value("devices").toArray();
        deviceCountLabel_->setText(QString("设备总数\n%1").arg(devices.size()));
    });
    QJsonObject telemetryReq = objectWithPage(1, 20);
    callApi("telemetry", kTelemetryService, "ListRealtimeStatus", telemetryReq, "加载实时概况", [this](const QJsonObject& body) {
        const QJsonArray items = body.value("items").toArray();
        int online = 0;
        for (const auto& item : items) {
            if (item.toObject().value("online").toBool()) {
                ++online;
            }
        }
        onlineCountLabel_->setText(QString("在线设备\n%1").arg(online));
        fillTable(dashboardTelemetryTable_, {{"device_id", "设备"}, {"online", "在线"}, {"battery", "电量"}, {"temperature", "温度"}, {"error_code", "错误码"}}, items);
    });
    QJsonObject eventReq = objectWithPage(1, 20);
    eventReq.insert("status", 1);
    callApi("event", kEventService, "ListEvents", eventReq, "加载告警概况", [this](const QJsonObject& body) {
        const QJsonArray events = body.value("events").toArray();
        openEventCountLabel_->setText(QString("打开告警\n%1").arg(events.size()));
        fillTable(dashboardEventsTable_, {{"event_id", "事件"}, {"device_id", "设备"}, {"severity", "级别"}, {"title", "标题"}, {"occurred_at", "发生时间"}}, events);
    });
    callApi("diagnosis", kDiagnosisService, "ListDiagnosisReports", objectWithPage(1, 20), "加载诊断概况", [this](const QJsonObject& body) {
        reportCountLabel_->setText(QString("诊断报告\n%1").arg(body.value("reports").toArray().size()));
    });
}

void MainWindow::refreshDevices()
{
    QJsonObject request = objectWithPage(1, 100);
    request.insert("keyword", deviceKeywordEdit_->text());
    request.insert("device_type", deviceTypeEdit_->text());
    request.insert("status", deviceStatusCombo_->currentData().toInt());
    callApi("device", kDeviceService, "ListDevices", request, "查询设备", [this](const QJsonObject& body) {
        fillTable(deviceTable_, {{"device_id", "设备ID"}, {"device_name", "名称"}, {"device_type", "类型"}, {"model", "型号"}, {"manufacturer", "厂商"}, {"location", "位置"}, {"status", "状态"}, {"protocol", "协议"}, {"updated_at", "更新时间"}}, body.value("devices").toArray());
    });
}

void MainWindow::refreshTelemetry()
{
    QJsonObject request = objectWithPage(1, 100);
    request.insert("only_online", telemetryOnlyOnlineCheck_->isChecked());
    QJsonArray ids;
    for (const QString& id : telemetryDeviceIdsEdit_->text().split(',', Qt::SkipEmptyParts)) {
        ids.append(id.trimmed());
    }
    request.insert("device_ids", ids);
    callApi("telemetry", kTelemetryService, "ListRealtimeStatus", request, "查询实时状态", [this](const QJsonObject& body) {
        fillTable(telemetryTable_, {{"device_id", "设备ID"}, {"online", "在线"}, {"battery", "电量"}, {"temperature", "温度"}, {"speed", "速度"}, {"run_mode", "模式"}, {"error_code", "错误码"}, {"reported_at", "上报时间"}}, body.value("items").toArray());
    });
}

void MainWindow::refreshEvents()
{
    QJsonObject request = objectWithPage(1, 100);
    request.insert("device_id", eventDeviceEdit_->text());
    request.insert("severity", eventSeverityCombo_->currentData().toInt());
    request.insert("status", eventStatusCombo_->currentData().toInt());
    callApi("event", kEventService, "ListEvents", request, "查询告警", [this](const QJsonObject& body) {
        fillTable(eventTable_, {{"event_id", "事件ID"}, {"device_id", "设备ID"}, {"event_type", "类型"}, {"severity", "级别"}, {"status", "状态"}, {"error_code", "错误码"}, {"title", "标题"}, {"occurred_at", "发生时间"}}, body.value("events").toArray());
    });
}

void MainWindow::refreshLogs()
{
    QJsonObject request = objectWithPage(1, 100);
    request.insert("device_id", logDeviceEdit_->text());
    request.insert("keyword", logKeywordEdit_->text());
    request.insert("event_id", logEventEdit_->text());
    request.insert("level", logLevelCombo_->currentText());
    callApi("log", kLogService, "QueryLogs", request, "查询日志", [this](const QJsonObject& body) {
        fillTable(logTable_, {{"timestamp", "时间"}, {"device_id", "设备ID"}, {"service_name", "服务"}, {"source_type", "来源"}, {"level", "级别"}, {"message", "消息"}, {"error_code", "错误码"}, {"event_id", "事件ID"}}, body.value("logs").toArray());
    });
}

void MainWindow::refreshKnowledgeDocuments()
{
    QJsonObject request = objectWithPage(1, 100);
    request.insert("keyword", knowledgeKeywordEdit_->text());
    callApi("knowledge", kKnowledgeService, "ListKnowledgeDocuments", request, "查询知识文档", [this](const QJsonObject& body) {
        fillTable(knowledgeTable_, {{"document_id", "文档ID"}, {"title", "标题"}, {"category", "分类"}, {"device_type", "设备类型"}, {"error_code", "错误码"}, {"status", "状态"}, {"updated_at", "更新时间"}}, body.value("documents").toArray());
    });
}

void MainWindow::searchKnowledge()
{
    QJsonObject request;
    request.insert("query", knowledgeSearchEdit_->text().isEmpty() ? "TEMP_001" : knowledgeSearchEdit_->text());
    request.insert("top_k", 10);
    callApi("knowledge", kKnowledgeService, "SearchKnowledge", request, "检索知识", [this](const QJsonObject& body) {
        fillTable(knowledgeSearchTable_, {{"title", "标题"}, {"content", "片段"}, {"score", "分数"}, {"document_id", "文档ID"}, {"chunk_id", "片段ID"}}, body.value("snippets").toArray());
    });
}

void MainWindow::refreshFaults()
{
    QJsonObject request = objectWithPage(1, 100);
    request.insert("device_id", diagnosisDeviceEdit_->text());
    callApi("diagnosis", kDiagnosisService, "ListFaultRecords", request, "查询故障记录", [this](const QJsonObject& body) {
        fillTable(faultTable_, {{"fault_id", "故障ID"}, {"device_id", "设备ID"}, {"event_id", "事件ID"}, {"fault_type", "类型"}, {"severity", "级别"}, {"status", "状态"}, {"symptom", "现象"}, {"created_at", "创建时间"}}, body.value("faults").toArray());
    });
}

void MainWindow::refreshReports()
{
    QJsonObject request = objectWithPage(1, 100);
    request.insert("device_id", diagnosisDeviceEdit_->text());
    callApi("diagnosis", kDiagnosisService, "ListDiagnosisReports", request, "查询诊断报告", [this](const QJsonObject& body) {
        fillTable(reportTable_, {{"report_id", "报告ID"}, {"device_id", "设备ID"}, {"event_id", "事件ID"}, {"fault_id", "故障ID"}, {"status", "状态"}, {"summary", "摘要"}, {"confidence", "置信度"}, {"created_at", "创建时间"}}, body.value("reports").toArray());
    });
}

void MainWindow::refreshGateway()
{
    const QJsonObject statusReq{{"gateway_id", "device-gateway-001"}};
    callApi("gateway", kGatewayService, "GetGatewayStatus", statusReq, "查询网关状态", [this](const QJsonObject& body) {
        const QJsonObject status = body.value("status").toObject();
        gatewayStatusLabel_->setText(QString("MQTT: %1    Broker: %2    启动时间: %3")
            .arg(status.value("mqtt_connected").toBool() ? "已连接" : "未连接")
            .arg(status.value("mqtt_broker").toString())
            .arg(formatTimestamp(static_cast<qint64>(status.value("started_at").toDouble()))));
    });
    callApi("gateway", kGatewayService, "ListConnectedDevices", objectWithPage(1, 100), "查询网关设备", [this](const QJsonObject& body) {
        fillTable(gatewayDeviceTable_, {{"device_id", "设备ID"}, {"client_id", "客户端"}, {"remote_addr", "地址"}, {"connected_at", "连接时间"}, {"last_heartbeat_at", "最后心跳"}}, body.value("devices").toArray());
    });
    callApi("gateway", kGatewayService, "GetForwardingStats", statusReq, "查询转发统计", [this](const QJsonObject& body) {
        fillTable(gatewayStatsTable_, {{"message_type", "类型"}, {"received_count", "接收"}, {"forwarded_count", "转发"}, {"dropped_count", "丢弃"}, {"failed_count", "失败"}, {"updated_at", "更新时间"}}, body.value("stats").toArray());
    });
}

void MainWindow::createDevice()
{
    auto* name = lineEdit("robot-001");
    auto* display = lineEdit("仓储机器人001");
    auto* type = lineEdit("robot");
    auto* model = lineEdit("RBT-A1");
    auto* manufacturer = lineEdit("DeviceOpsLab");
    auto* location = lineEdit("warehouse-a");
    auto* token = lineEdit("可选接入 token");
    auto* protocol = lineEdit("mqtt");
    protocol->setText("mqtt");
    auto* description = lineEdit("备注");
    const QJsonObject request = dialogFields(this, "新建设备", {
        {"device_id", name}, {"device_name", display}, {"device_type", type}, {"model", model},
        {"manufacturer", manufacturer}, {"location", location}, {"access_token", token},
        {"protocol", protocol}, {"description", description},
    });
    if (request.isEmpty()) {
        return;
    }
    callApi("device", kDeviceService, "CreateDevice", request, "新建设备", [this](const QJsonObject&) { refreshDevices(); });
}

void MainWindow::editDevice()
{
    const QJsonObject current = selectedObject(deviceTable_);
    if (current.isEmpty()) {
        return;
    }
    auto* name = lineEdit("");
    name->setText(current.value("device_name").toString());
    auto* type = lineEdit("");
    type->setText(current.value("device_type").toString());
    auto* model = lineEdit("");
    model->setText(current.value("model").toString());
    auto* manufacturer = lineEdit("");
    manufacturer->setText(current.value("manufacturer").toString());
    auto* location = lineEdit("");
    location->setText(current.value("location").toString());
    auto* status = enumCombo({{1, "enabled"}, {2, "disabled"}, {3, "maintenance"}});
    status->setCurrentIndex(qMax(0, static_cast<int>(current.value("status").toDouble()) - 1));
    auto* description = lineEdit("");
    description->setText(current.value("description").toString());
    QJsonObject request = dialogFields(this, "编辑设备", {
        {"device_name", name}, {"device_type", type}, {"model", model}, {"manufacturer", manufacturer},
        {"location", location}, {"status", status}, {"description", description},
    });
    if (request.isEmpty()) {
        return;
    }
    request.insert("device_id", current.value("device_id").toString());
    callApi("device", kDeviceService, "UpdateDevice", request, "编辑设备", [this](const QJsonObject&) { refreshDevices(); });
}

void MainWindow::updateSelectedEventStatus()
{
    const QJsonObject event = selectedObject(eventTable_);
    if (event.isEmpty()) {
        return;
    }
    QJsonObject request;
    request.insert("event_id", event.value("event_id").toString());
    request.insert("status", eventNewStatusCombo_->currentData().toInt());
    request.insert("comment", "Qt 客户端更新");
    request.insert("operator_user_id", 1);
    callApi("event", kEventService, "UpdateEventStatus", request, "更新告警状态", [this](const QJsonObject&) { refreshEvents(); });
}

void MainWindow::createFaultFromSelectedEvent()
{
    const QJsonObject event = selectedObject(eventTable_);
    if (event.isEmpty()) {
        return;
    }
    const QString note = QInputDialog::getText(this, "诊断备注", "工程师备注：", QLineEdit::Normal, event.value("title").toString());
    QJsonObject faultReq;
    faultReq.insert("device_id", event.value("device_id").toString());
    faultReq.insert("event_id", event.value("event_id").toString());
    faultReq.insert("owner_user_id", 1);
    faultReq.insert("fault_type", event.value("event_type").toString());
    faultReq.insert("severity", static_cast<int>(event.value("severity").toDouble()));
    faultReq.insert("symptom", note.isEmpty() ? event.value("description").toString() : note);
    faultReq.insert("started_at", static_cast<double>(nowMs()));
    callApi("diagnosis", kDiagnosisService, "CreateFaultRecord", faultReq, "创建故障记录", [this, event, note](const QJsonObject& body) {
        QJsonObject diagnosisReq;
        diagnosisReq.insert("event_id", event.value("event_id").toString());
        diagnosisReq.insert("fault_id", body.value("fault").toObject().value("fault_id").toString());
        diagnosisReq.insert("device_id", event.value("device_id").toString());
        diagnosisReq.insert("requested_by", 1);
        diagnosisReq.insert("engineer_note", note);
        callApi("diagnosis", kDiagnosisService, "StartDiagnosis", diagnosisReq, "发起 AI 诊断", [this](const QJsonObject&) {
            refreshFaults();
            refreshReports();
        });
    });
}

void MainWindow::createKnowledgeDocument()
{
    auto* title = lineEdit("TEMP_001 高温处理 SOP");
    auto* category = lineEdit("sop");
    auto* deviceType = lineEdit("robot");
    auto* errorCode = lineEdit("TEMP_001");
    auto* content = new QPlainTextEdit;
    content->setPlaceholderText("填写故障现象、可能原因和处理步骤");
    QJsonObject request = dialogFields(this, "创建知识文档", {
        {"title", title}, {"category", category}, {"device_type", deviceType}, {"error_code", errorCode}, {"content", content},
    });
    if (request.isEmpty()) {
        return;
    }
    request.insert("created_by", 1);
    callApi("knowledge", kKnowledgeService, "CreateKnowledgeDocument", request, "创建知识文档", [this](const QJsonObject&) {
        refreshKnowledgeDocuments();
    });
}

void MainWindow::requestKnowledgeIndex()
{
    const QJsonObject doc = selectedObject(knowledgeTable_);
    if (doc.isEmpty()) {
        return;
    }
    QJsonObject request;
    request.insert("document_id", doc.value("document_id").toString());
    request.insert("force_rebuild", true);
    callApi("knowledge", kKnowledgeService, "RequestKnowledgeIndex", request, "触发知识索引", [this](const QJsonObject&) {
        refreshKnowledgeDocuments();
    });
}

void MainWindow::createFaultRecord()
{
    auto* deviceId = lineEdit("robot-001");
    auto* eventId = lineEdit("可选事件 ID");
    auto* type = lineEdit("temperature_high");
    auto* severity = enumCombo({{2, "warning"}, {3, "critical"}});
    auto* symptom = new QPlainTextEdit;
    symptom->setPlaceholderText("描述故障现象");
    QJsonObject request = dialogFields(this, "创建故障记录", {
        {"device_id", deviceId}, {"event_id", eventId}, {"fault_type", type}, {"severity", severity}, {"symptom", symptom},
    });
    if (request.isEmpty()) {
        return;
    }
    request.insert("owner_user_id", 1);
    request.insert("started_at", static_cast<double>(nowMs()));
    callApi("diagnosis", kDiagnosisService, "CreateFaultRecord", request, "创建故障记录", [this](const QJsonObject&) { refreshFaults(); });
}

void MainWindow::startDiagnosisForSelectedFault()
{
    const QJsonObject fault = selectedObject(faultTable_);
    if (fault.isEmpty()) {
        return;
    }
    const QString note = QInputDialog::getText(this, "诊断备注", "工程师备注：", QLineEdit::Normal, fault.value("symptom").toString());
    QJsonObject request;
    request.insert("event_id", fault.value("event_id").toString());
    request.insert("fault_id", fault.value("fault_id").toString());
    request.insert("device_id", fault.value("device_id").toString());
    request.insert("requested_by", 1);
    request.insert("engineer_note", note);
    callApi("diagnosis", kDiagnosisService, "StartDiagnosis", request, "发起 AI 诊断", [this](const QJsonObject&) { refreshReports(); });
}

void MainWindow::confirmSelectedReport(bool confirmed)
{
    const QJsonObject report = selectedObject(reportTable_);
    if (report.isEmpty()) {
        return;
    }
    QJsonObject request;
    request.insert("report_id", report.value("report_id").toString());
    request.insert("operator_user_id", 1);
    request.insert("confirmed", confirmed);
    request.insert("comment", confirmed ? "Qt 客户端确认" : "Qt 客户端驳回");
    callApi("diagnosis", kDiagnosisService, "ConfirmDiagnosisReport", request, confirmed ? "确认诊断报告" : "驳回诊断报告", [this](const QJsonObject&) {
        refreshReports();
    });
}

void MainWindow::showSelectedJson(QTableWidget* table, const QString& title)
{
    const QJsonObject object = selectedObject(table);
    if (object.isEmpty()) {
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.resize(720, 560);
    auto* layout = new QVBoxLayout(&dialog);
    auto* text = new QTextEdit;
    text->setReadOnly(true);
    text->setPlainText(objectToPrettyText(object));
    layout->addWidget(text);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    layout->addWidget(buttons);
    dialog.exec();
}
