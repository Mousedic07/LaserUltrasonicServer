#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonObject>

// Adapted from llfcchat/client/llfcchat/httpmgr: asynchronous Qt HTTP POST.
class HttpMgr : public QObject {
    Q_OBJECT
public:
    explicit HttpMgr(QObject *parent = nullptr);
    void login(const QString &url, const QString &user, const QString &password);
signals:
    void loginFinished(const QJsonObject &response, const QString &error);
private:
    QNetworkAccessManager manager_;
};
