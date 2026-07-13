#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QPair>
#include <QString>

QJsonObject pageRequest(int page = 1, int pageSize = 50);
QJsonObject timeRange(qint64 startTime, qint64 endTime);
QJsonObject objectWithPage(int page = 1, int pageSize = 50);
QString jsonValueToDisplay(const QJsonValue& value, const QString& key = QString());
QString formatTimestamp(qint64 timestampMs);
QString compactJson(const QJsonValue& value);
int enumValue(const QString& text, int fallback = 0);
QString enumName(const QString& key, int value);
