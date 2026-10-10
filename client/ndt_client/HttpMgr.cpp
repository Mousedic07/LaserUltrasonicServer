#include "HttpMgr.h"
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

HttpMgr::HttpMgr(QObject *parent) : QObject(parent) {}

void HttpMgr::login(const QString &url, const QString &user, const QString &password) {
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject body{{"username", user}, {"password", password}};
    QNetworkReply *reply = manager_.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QTimer *timer = new QTimer(reply);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    timer->start(5000);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray response = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            const QJsonDocument doc = QJsonDocument::fromJson(response);
            if (doc.isObject() && doc.object().contains("error"))
                emit loginFinished(doc.object(), doc.object().value("error").toString());
            else
                emit loginFinished(QJsonObject(), reply->errorString());
        } else {
            QJsonParseError parse;
            QJsonDocument doc = QJsonDocument::fromJson(response, &parse);
            if (parse.error != QJsonParseError::NoError || !doc.isObject())
                emit loginFinished(QJsonObject(), tr("Invalid login response"));
            else
                emit loginFinished(doc.object(), QString());
        }
        reply->deleteLater();
    });
}
