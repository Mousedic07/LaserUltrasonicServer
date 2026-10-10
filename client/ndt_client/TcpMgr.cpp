#include "TcpMgr.h"
#include <QJsonDocument>
#include <QJsonParseError>

namespace {
quint16 get16(const char *p) {
    return (quint16(quint8(p[0])) << 8) | quint8(p[1]);
}
quint32 get32(const char *p) {
    return (quint32(quint8(p[0])) << 24) | (quint32(quint8(p[1])) << 16)
            | (quint32(quint8(p[2])) << 8) | quint8(p[3]);
}
void put16(QByteArray &a, quint16 n) { a.append(char(n >> 8)); a.append(char(n)); }
void put32(QByteArray &a, quint32 n) {
    a.append(char(n >> 24)); a.append(char(n >> 16)); a.append(char(n >> 8)); a.append(char(n));
}
}

TcpMgr::TcpMgr(QObject *parent) : QObject(parent) {
    connect(&socket_, &QTcpSocket::connected, this, [this]() {
        emit connected();
        send(Hello, QJsonObject{{"token", token_}});
    });
    connect(&socket_, &QTcpSocket::readyRead, this, [this]() {
        buffer_.append(socket_.readAll());
        parseBuffer();
    });
    connect(&socket_, &QTcpSocket::disconnected, this, [this]() {
        emit disconnected(tr("TCP connection closed"));
    });
    connect(&socket_, static_cast<void(QTcpSocket::*)(QAbstractSocket::SocketError)>(&QTcpSocket::error),
            this, [this](QAbstractSocket::SocketError) { emit disconnected(socket_.errorString()); });
}

void TcpMgr::connectTo(const QString &host, quint16 port, const QString &token) {
    token_ = token;
    buffer_.clear();
    socket_.abort();
    socket_.connectToHost(host, port);
}

void TcpMgr::send(Type type, const QJsonObject &data) {
    if (socket_.state() != QAbstractSocket::ConnectedState) return;
    QByteArray body = QJsonDocument(data).toJson(QJsonDocument::Compact);
    if (body.size() > 1048576) return;
    QByteArray packet("NDT1", 4);
    put16(packet, 1);
    put16(packet, quint16(type));
    put32(packet, quint32(body.size()));
    packet.append(body);
    socket_.write(packet);
}

void TcpMgr::parseBuffer() {
    while (buffer_.size() >= 12) {
        const char *h = buffer_.constData();
        const quint32 length = get32(h + 8);
        if (buffer_.left(4) != "NDT1" || get16(h + 4) != 1 || length > 1048576) {
            socket_.abort();
            buffer_.clear();
            emit disconnected(tr("Invalid NDT frame"));
            return;
        }
        if (quint32(buffer_.size()) < 12u + length) return;
        const int type = get16(h + 6);
        QJsonParseError parse;
        QJsonDocument doc = QJsonDocument::fromJson(buffer_.mid(12, int(length)), &parse);
        buffer_.remove(0, 12 + int(length));
        if (parse.error != QJsonParseError::NoError || !doc.isObject()) {
            socket_.abort();
            emit disconnected(tr("Invalid JSON payload"));
            return;
        }
        emit message(type, doc.object());
    }
}
