#pragma once

#include "app_config.h"
#include "http_client.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimer>

#include <functional>

class QCheckBox;
class QComboBox;
class QLayout;
class QListWidget;
class QPlainTextEdit;

struct TableColumn {
    QString key;
    QString title;
};

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    QWidget* createDashboardPage();
    QWidget* createDevicePage();
    QWidget* createTelemetryPage();
    QWidget* createEventPage();
    QWidget* createLogPage();
    QWidget* createKnowledgePage();
    QWidget* createDiagnosisPage();
    QWidget* createGatewayPage();

    QWidget* pageShell(const QString& title, const QString& subtitle, QLayout* content);
    void callApi(const QString& serviceKey,
                 const QString& serviceName,
                 const QString& methodName,
                 const QJsonObject& payload,
                 const QString& operation,
                 std::function<void(const QJsonObject&)> onSuccess);
    void setBusy(const QString& text);
    void setReady(const QString& text);
    void showError(const QString& operation, const ApiResult& result);

    void fillTable(QTableWidget* table, const QVector<TableColumn>& columns, const QJsonArray& rows);
    QJsonObject selectedObject(QTableWidget* table) const;
    void selectFirstRow(QTableWidget* table);

    void refreshAll();
    void refreshCurrentPage();
    void refreshDashboard();
    void refreshDevices();
    void refreshTelemetry();
    void refreshEvents();
    void refreshLogs();
    void refreshKnowledgeDocuments();
    void searchKnowledge();
    void refreshFaults();
    void refreshReports();
    void refreshGateway();

    void createDevice();
    void editDevice();
    void updateSelectedEventStatus();
    void createFaultFromSelectedEvent();
    void createKnowledgeDocument();
    void requestKnowledgeIndex();
    void createFaultRecord();
    void startDiagnosisForSelectedFault();
    void confirmSelectedReport(bool confirmed);
    void showSelectedJson(QTableWidget* table, const QString& title);

    AppConfig config_;
    HttpClient http_;

    QListWidget* navigation_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLabel* configLabel_ = nullptr;
    QCheckBox* autoRefreshCheck_ = nullptr;
    QTimer refreshTimer_;

    QLabel* deviceCountLabel_ = nullptr;
    QLabel* onlineCountLabel_ = nullptr;
    QLabel* openEventCountLabel_ = nullptr;
    QLabel* reportCountLabel_ = nullptr;
    QTableWidget* dashboardEventsTable_ = nullptr;
    QTableWidget* dashboardTelemetryTable_ = nullptr;

    QTableWidget* deviceTable_ = nullptr;
    QLineEdit* deviceKeywordEdit_ = nullptr;
    QComboBox* deviceStatusCombo_ = nullptr;
    QLineEdit* deviceTypeEdit_ = nullptr;

    QTableWidget* telemetryTable_ = nullptr;
    QLineEdit* telemetryDeviceIdsEdit_ = nullptr;
    QCheckBox* telemetryOnlyOnlineCheck_ = nullptr;

    QTableWidget* eventTable_ = nullptr;
    QLineEdit* eventDeviceEdit_ = nullptr;
    QComboBox* eventSeverityCombo_ = nullptr;
    QComboBox* eventStatusCombo_ = nullptr;
    QComboBox* eventNewStatusCombo_ = nullptr;

    QTableWidget* logTable_ = nullptr;
    QLineEdit* logDeviceEdit_ = nullptr;
    QLineEdit* logKeywordEdit_ = nullptr;
    QLineEdit* logEventEdit_ = nullptr;
    QComboBox* logLevelCombo_ = nullptr;

    QTableWidget* knowledgeTable_ = nullptr;
    QTableWidget* knowledgeSearchTable_ = nullptr;
    QLineEdit* knowledgeKeywordEdit_ = nullptr;
    QLineEdit* knowledgeSearchEdit_ = nullptr;

    QTableWidget* faultTable_ = nullptr;
    QTableWidget* reportTable_ = nullptr;
    QLineEdit* diagnosisDeviceEdit_ = nullptr;

    QLabel* gatewayStatusLabel_ = nullptr;
    QTableWidget* gatewayDeviceTable_ = nullptr;
    QTableWidget* gatewayStatsTable_ = nullptr;
};
