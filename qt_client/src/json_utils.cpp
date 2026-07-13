#include "json_utils.h"

#include <QDateTime>
#include <QJsonDocument>

QJsonObject pageRequest(int page, int pageSize)
{
    return {{"page", page}, {"page_size", pageSize}};
}

QJsonObject timeRange(qint64 startTime, qint64 endTime)
{
    return {{"start_time", startTime}, {"end_time", endTime}};
}

QJsonObject objectWithPage(int page, int pageSize)
{
    return {{"page", pageRequest(page, pageSize)}};
}

QString formatTimestamp(qint64 timestampMs)
{
    if (timestampMs <= 0) {
        return "-";
    }
    return QDateTime::fromMSecsSinceEpoch(timestampMs).toString("yyyy-MM-dd HH:mm:ss");
}

QString compactJson(const QJsonValue& value)
{
    if (value.isUndefined() || value.isNull()) {
        return QString();
    }
    if (value.isString()) {
        const QString text = value.toString();
        const QJsonDocument parsed = QJsonDocument::fromJson(text.toUtf8());
        if (parsed.isObject() || parsed.isArray()) {
            return QString::fromUtf8(parsed.toJson(QJsonDocument::Compact));
        }
        return text;
    }
    if (value.isObject()) {
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    }
    if (value.isArray()) {
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    }
    if (value.isBool()) {
        return value.toBool() ? "是" : "否";
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble(), 'f', 2);
    }
    return QString();
}

int enumValue(const QString& text, int fallback)
{
    const int separator = text.indexOf(" - ");
    bool ok = false;
    const int value = text.left(separator > 0 ? separator : text.size()).toInt(&ok);
    return ok ? value : fallback;
}

QString enumName(const QString& key, int value)
{
    if (key.contains("severity")) {
        switch (value) {
        case 1: return "info";
        case 2: return "warning";
        case 3: return "critical";
        default: return "unspecified";
        }
    }
    if (key == "status") {
        switch (value) {
        case 1: return "enabled/open/new/draft";
        case 2: return "disabled/processing/confirmed";
        case 3: return "maintenance/resolved/rejected";
        case 4: return "closed";
        default: return "unspecified";
        }
    }
    return QString::number(value);
}

QString jsonValueToDisplay(const QJsonValue& value, const QString& key)
{
    if (key.endsWith("_at") || key == "timestamp" || key == "started_at" || key == "resolved_at") {
        return formatTimestamp(static_cast<qint64>(value.toDouble()));
    }
    if (key.endsWith("_json") || value.isObject() || value.isArray()) {
        return compactJson(value);
    }
    if (value.isBool()) {
        return value.toBool() ? "在线" : "离线";
    }
    if (value.isDouble()) {
        const double number = value.toDouble();
        if (key == "status" || key == "severity") {
            return enumName(key, static_cast<int>(number));
        }
        return QString::number(number, 'f', (number == static_cast<qint64>(number)) ? 0 : 2);
    }
    return value.toString();
}
