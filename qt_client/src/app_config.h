#pragma once

#include <QHash>
#include <QString>
#include <QUrl>

class AppConfig {
public:
    static AppConfig load();

    QUrl baseUrl(const QString& service) const;
    int requestTimeoutMs() const;
    QString sourcePath() const;

private:
    QHash<QString, QUrl> baseUrls_;
    int requestTimeoutMs_ = 10000;
    QString sourcePath_;
};
