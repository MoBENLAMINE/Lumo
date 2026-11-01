#pragma once

#include <QJsonObject>
#include <QLocalServer>
#include <QPointer>

class QLocalSocket;

// Listens on $XDG_RUNTIME_DIR/lumo.sock for lumo-hook. Each connection carries
// one header line, then the agent's raw payload until the hook shuts its write
// side. The socket stays open only while a decision may still be written.
class HookServer : public QObject {
    Q_OBJECT
public:
    explicit HookServer(QObject *parent = nullptr);
    bool listen();
    static QString socketPath();
    // True when another Lumo already answers on the socket.
    static bool otherInstanceRunning();

signals:
    void hookEvent(const QJsonObject &header, const QJsonObject &payload, QLocalSocket *socket);

private:
    void accept();
    void finish(QLocalSocket *socket, const QByteArray &data);

    QLocalServer m_server;
};
