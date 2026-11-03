#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QMetaType>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>

// One Claude Code session that runs in the cloud (claude.ai/code), as the
// session list reports it. Archived and malformed entries never get here.
struct CloudSession {
    QString id;                 // "session_…"
    QString title;
    QString status;             // running · pending · idle · requires_action · failed
    QString repo;               // "owner/name", empty when the session has no git repository
    QString url;               // https://claude.ai/code/<id>
    qint64 updatedAt = 0;       // ms since the epoch, from the server
};
Q_DECLARE_METATYPE(CloudSession)

// The body of the session list (`{"data":[…]}`) as sessions. Pure: no network.
QList<CloudSession> parseCloudSessions(const QByteArray &body);

// Lists your cloud sessions every minute, so Lumo can show them next to the
// local ones. It uses the claude.ai login Claude Code keeps in
// ~/.claude/.credentials.json (the same one PlanUsage reads) and the org id
// from ~/.claude.json, and asks api.anthropic.com only. Approvals for a cloud
// session still happen on claude.ai: the hooks never run there.
class CloudSessions : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
public:
    explicit CloudSessions(QObject *parent = nullptr);

    bool enabled() const;
    void setEnabled(bool on);
    // Ask now, unless a request is already running or Anthropic asked us to wait.
    Q_INVOKABLE void refresh();

signals:
    void enabledChanged();
    void sessionsChanged(const QList<CloudSession> &sessions);

private:
    QString configDir() const;
    void ask(const QString &token, const QString &org);

    QNetworkAccessManager m_net;
    QTimer m_timer;
    QDateTime m_quietUntil;
    int m_backoffMs = 0;
    bool m_busy = false;
    bool m_warned = false;
};
