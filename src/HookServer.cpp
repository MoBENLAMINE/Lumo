#include "HookServer.h"

#include <QDir>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QTimer>

#include <sys/socket.h>
#include <unistd.h>

namespace {
constexpr qint64 kMaxPayload = 8 * 1024 * 1024;
constexpr int kReadTimeoutMs = 5000;
}

HookServer::HookServer(QObject *parent) : QObject(parent) {
    connect(&m_server, &QLocalServer::newConnection, this, &HookServer::accept);
}

QString HookServer::socketPath() {
    QString dir = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (dir.isEmpty() || !QDir::isAbsolutePath(dir)) dir = QStringLiteral("/run/user/%1").arg(getuid());
    return dir + QStringLiteral("/lumo.sock");
}

bool HookServer::otherInstanceRunning() {
    QLocalSocket probe;
    probe.connectToServer(socketPath());
    return probe.waitForConnected(200);
}

bool HookServer::listen() {
    QLocalServer::removeServer(socketPath());
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    return m_server.listen(socketPath());
}

void HookServer::accept() {
    while (QLocalSocket *socket = m_server.nextPendingConnection()) {
        // Only processes running as us may talk to the island.
        struct ucred cred {};
        socklen_t len = sizeof cred;
        if (getsockopt(int(socket->socketDescriptor()), SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0 ||
            cred.uid != getuid()) {
            socket->abort();
            socket->deleteLater();
            continue;
        }
        socket->setReadBufferSize(0);
        auto *timeout = new QTimer(socket);
        timeout->setSingleShot(true);
        timeout->start(kReadTimeoutMs);
        connect(timeout, &QTimer::timeout, socket, [socket] { socket->abort(); });
        // Header line with the payload's length, then the payload. The hook
        // keeps the connection open while it may still get a decision.
        connect(socket, &QLocalSocket::readyRead, this, [this, socket, timeout] {
            if (socket->property("done").toBool()) return;
            const qint64 avail = socket->bytesAvailable();
            if (avail > kMaxPayload + 1024) {
                socket->abort();
                return;
            }
            const QByteArray peek = socket->peek(avail);
            const qsizetype nl = peek.indexOf('\n');
            if (nl < 0) return;
            const qint64 len = QJsonDocument::fromJson(peek.left(nl)).object().value(QStringLiteral("len")).toInteger(-1);
            if (len < 0 || len > kMaxPayload) {
                socket->abort();
                return;
            }
            if (avail < nl + 1 + len) return;
            timeout->stop();
            socket->setProperty("done", true);
            finish(socket, socket->read(nl + 1 + len));
        });
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
    }
}

void HookServer::finish(QLocalSocket *socket, const QByteArray &data) {
    const qsizetype nl = data.indexOf('\n');
    if (nl < 0) {
        socket->disconnectFromServer();
        return;
    }
    const QJsonObject header = QJsonDocument::fromJson(data.left(nl)).object();
    QJsonParseError err;
    const QJsonObject payload = QJsonDocument::fromJson(data.mid(nl + 1), &err).object();
    if (header.isEmpty() || err.error != QJsonParseError::NoError) {
        socket->disconnectFromServer();
        return;
    }
    emit hookEvent(header, payload, socket);
}
