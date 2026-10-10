#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QJsonObject>

// Based on llfcchat/client/llfcchat/tcpmgr's readyRead buffer and signal model.
// NDT1 header: magic[4], version[2], type[2], length[4], network byte order.
class TcpMgr : public QObject {
    Q_OBJECT
public:
    enum Type { Hello = 1, Start = 2, Stop = 3, Wave = 4,
                Spectrum = 5, Status = 6, File = 7, Error = 8 };
    explicit TcpMgr(QObject *parent = nullptr);
    void connectTo(const QString &host, quint16 port, const QString &token);
    void send(Type type, const QJsonObject &data);
signals:
    void connected();
    void disconnected(const QString &reason);
    void message(int type, const QJsonObject &data);
private:
    void parseBuffer();
    QTcpSocket socket_;
    QByteArray buffer_;
    QString token_;
};
