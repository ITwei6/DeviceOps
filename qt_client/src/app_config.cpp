#include "app_config.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

QHash<QString, QUrl> defaultBaseUrls()
{
    return {
        {"gateway", QUrl("http://127.0.0.1:9101")},
        {"device", QUrl("http://127.0.0.1:9201")},
        {"telemetry", QUrl("http://127.0.0.1:9301")},
        {"event", QUrl("http://127.0.0.1:9401")},
        {"log", QUrl("http://127.0.0.1:9501")},
        {"knowledge", QUrl("http://127.0.0.1:9600")},
        {"diagnosis", QUrl("http://127.0.0.1:9700")},
    };
}

QString configPath()
{
    const QString appDirPath = QCoreApplication::applicationDirPath() + "/config.json";
    if (QFile::exists(appDirPath)) {
        return appDirPath;
    }
    const QString cwdPath = QDir::currentPath() + "/config.json";
    if (QFile::exists(cwdPath)) {
        return cwdPath;
    }
    return appDirPath;
}

void applyUrl(QHash<QString, QUrl>& urls, const QJsonObject& obj, const QString& key, const QString& service)
{
    const QString value = obj.value(key).toString();
    if (!value.isEmpty()) {
        urls[service] = QUrl(value);
    }
}

} // namespace

AppConfig AppConfig::load()
{
    AppConfig config;
    config.baseUrls_ = defaultBaseUrls();
    config.sourcePath_ = configPath();

    QFile file(config.sourcePath_);
    if (!file.open(QIODevice::ReadOnly)) {
        return config;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    const QJsonObject obj = document.object();

    applyUrl(config.baseUrls_, obj, "gatewayBaseUrl", "gateway");
    applyUrl(config.baseUrls_, obj, "deviceBaseUrl", "device");
    applyUrl(config.baseUrls_, obj, "telemetryBaseUrl", "telemetry");
    applyUrl(config.baseUrls_, obj, "eventBaseUrl", "event");
    applyUrl(config.baseUrls_, obj, "logBaseUrl", "log");
    applyUrl(config.baseUrls_, obj, "knowledgeBaseUrl", "knowledge");
    applyUrl(config.baseUrls_, obj, "diagnosisBaseUrl", "diagnosis");

    const int timeoutMs = obj.value("requestTimeoutMs").toInt(config.requestTimeoutMs_);
    if (timeoutMs > 0) {
        config.requestTimeoutMs_ = timeoutMs;
    }

    return config;
}

QUrl AppConfig::baseUrl(const QString& service) const
{
    return baseUrls_.value(service);
}

int AppConfig::requestTimeoutMs() const
{
    return requestTimeoutMs_;
}

QString AppConfig::sourcePath() const
{
    return sourcePath_;
}
