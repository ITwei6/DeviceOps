#include "http_client.h"

#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace {

QUrl buildRpcUrl(QUrl baseUrl, const QString& serviceName, const QString& methodName)
{
    QString path = baseUrl.path();
    if (!path.endsWith('/')) {
        path += '/';
    }
    path += serviceName + "/" + methodName;
    baseUrl.setPath(path);
    return baseUrl;
}

QString responseMessage(const QJsonObject& body)
{
    const QJsonObject response = body.value("response").toObject();
    return response.value("message").toString();
}

int responseCode(const QJsonObject& body)
{
    const QJsonObject response = body.value("response").toObject();
    if (response.isEmpty()) {
        return -1;
    }
    return response.value("code").toInt(0);
}

QString responseTraceId(const QJsonObject& body)
{
    const QJsonObject response = body.value("response").toObject();
    return response.value("trace_id").toString();
}

} // namespace

HttpClient::HttpClient(int timeoutMs, QObject* parent)
    : QObject(parent)
    , timeoutMs_(timeoutMs)
{
}

void HttpClient::postJson(const QUrl& baseUrl,
                          const QString& serviceName,
                          const QString& methodName,
                          const QJsonObject& payload,
                          Callback callback)
{
    QNetworkRequest request(buildRpcUrl(baseUrl, serviceName, methodName));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = manager_.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));

    QTimer* timer = new QTimer(reply);
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, reply, [reply]() {
        reply->abort();
    });
    timer->start(timeoutMs_);

    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, callback]() {
        ApiResult result;
        result.httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        result.rawBody = reply->readAll();

        const QNetworkReply::NetworkError networkError = reply->error();
        const QJsonDocument document = QJsonDocument::fromJson(result.rawBody);
        result.body = document.object();
        result.businessCode = responseCode(result.body);
        result.traceId = responseTraceId(result.body);

        if (networkError != QNetworkReply::NoError) {
            result.ok = false;
            result.message = reply->errorString();
        } else if (!document.isObject()) {
            result.ok = false;
            result.message = "响应格式不是 JSON 对象";
        } else if (result.businessCode != 0) {
            result.ok = false;
            result.message = responseMessage(result.body);
            if (result.message.isEmpty()) {
                result.message = "业务接口返回失败";
            }
        } else {
            result.ok = true;
            result.message = responseMessage(result.body);
        }

        callback(result);
        reply->deleteLater();
    });
}
