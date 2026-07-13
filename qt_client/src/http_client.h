#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QUrl>

#include <functional>

struct ApiResult {
    bool ok = false;
    int httpStatus = 0;
    int businessCode = -1;
    QString message;
    QString traceId;
    QJsonObject body;
    QByteArray rawBody;
};

class HttpClient : public QObject {
    Q_OBJECT

public:
    using Callback = std::function<void(const ApiResult&)>;

    explicit HttpClient(int timeoutMs, QObject* parent = nullptr);

    void postJson(const QUrl& baseUrl,
                  const QString& serviceName,
                  const QString& methodName,
                  const QJsonObject& payload,
                  Callback callback);

private:
    QNetworkAccessManager manager_;
    int timeoutMs_ = 10000;
};
