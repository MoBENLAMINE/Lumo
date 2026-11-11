#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include "CloudSessions.h"
#include "MapListModel.h"

class QLocalSocket;

// Everything the island knows about agent sessions: the live sessions, the
// permission requests and questions waiting on a click, and the plan usage.
class AgentHub : public QObject {
    Q_OBJECT
    Q_PROPERTY(MapListModel *sessions READ sessions CONSTANT)
    Q_PROPERTY(MapListModel *requests READ requests CONSTANT)
    Q_PROPERTY(int workingCount READ workingCount NOTIFY stateChanged)
    Q_PROPERTY(QString mood READ mood NOTIFY stateChanged)
    Q_PROPERTY(QString headline READ headline NOTIFY stateChanged)
    Q_PROPERTY(QString headlineAgent READ headlineAgent NOTIFY stateChanged)
    Q_PROPERTY(QString headlineColor READ headlineColor NOTIFY stateChanged)
    Q_PROPERTY(bool hasUsage READ hasUsage NOTIFY usageChanged)
    Q_PROPERTY(double fiveHour READ fiveHour NOTIFY usageChanged)
    Q_PROPERTY(double sevenDay READ sevenDay NOTIFY usageChanged)
    Q_PROPERTY(qint64 fiveHourReset READ fiveHourReset NOTIFY usageChanged)
    Q_PROPERTY(qint64 sevenDayReset READ sevenDayReset NOTIFY usageChanged)

public:
    explicit AgentHub(QObject *parent = nullptr);

    MapListModel *sessions() { return &m_sessions; }
    MapListModel *requests() { return &m_requests; }
    int workingCount() const;
    QString mood() const;
    QString headline() const;
    QString headlineAgent() const;
    QString headlineColor() const;
    bool hasUsage() const { return m_fiveReset > 0 || m_sevenReset > 0; }
    double fiveHour() const { return m_five; }
    double sevenDay() const { return m_seven; }
    qint64 fiveHourReset() const { return m_fiveReset; }
    qint64 sevenDayReset() const { return m_sevenReset; }

    Q_INVOKABLE void allow(const QString &requestId);
    Q_INVOKABLE void always(const QString &requestId);
    Q_INVOKABLE void deny(const QString &requestId);
    // Sends a plan back: Claude stays in plan mode, with your feedback if any.
    Q_INVOKABLE void keepPlanning(const QString &requestId, const QString &feedback);
    // answers: {questionText: label | [labels]}
    Q_INVOKABLE void answer(const QString &requestId, const QVariantMap &answers);
    // Leaves the decision to the agent's own terminal prompt.
    Q_INVOKABLE void replyInTerminal(const QString &requestId);
    // The global shortcuts: answer the request at the top, if a key can answer it
    // (a permission, or a plan: allow = approve, deny = keep planning).
    Q_INVOKABLE void allowTop();
    Q_INVOKABLE void denyTop();
    Q_INVOKABLE void dismissSession(const QString &sessionId);
    // The session's process chain, nearest first: its terminal or editor is in there.
    Q_INVOKABLE QVariantList sessionPids(const QString &sessionId) const { return m_pids.value(sessionId); }
    Q_INVOKABLE QString agentColor(const QString &agent) const;
    Q_INVOKABLE QString agentName(const QString &agent) const;

    // From the chat's own Claude Code runs.
    void setUsage(double five, qint64 fiveReset, double seven, qint64 sevenReset);

public slots:
    void onHookEvent(const QJsonObject &header, const QJsonObject &payload, QLocalSocket *socket);
    // The cloud sessions as the last list reported them. Replaces the cloud rows.
    void setCloudSessions(const QList<CloudSession> &cloud);

signals:
    void stateChanged();
    void usageChanged();
    void attention();                    // a request needs a click
    void sessionFinished(const QString &agent);
    // A session stopped and is waiting for your next prompt.
    void waitingForYou();
    void sessionFailed();
    void promptSubmitted();

private:
    struct Pending {
        QPointer<QLocalSocket> socket;
        QJsonObject toolInput;
        QJsonArray suggestions;
        QString sessionId;
        QString agent;
    };

    void touchSession(const QString &id, const QString &agent, const QJsonObject &payload, QVariantMap values);
    void addStep(const QString &id, const QString &icon, const QString &text, const QString &kind);
    void recordDiff(const QString &id, const QJsonObject &payload);
    void addRequest(const QString &sid, const QString &agent, const QJsonObject &payload, QLocalSocket *socket);
    void release(const QString &requestId, const QByteArray &line);
    void readStatusLine(const QJsonObject &payload);
    void prune();
    QString describeTool(const QString &tool, const QJsonObject &input, bool past) const;

    MapListModel m_sessions;
    MapListModel m_requests;
    QHash<QString, Pending> m_pending;
    QHash<QString, QVariantList> m_pids;
    QHash<QString, QString> m_cloudStatus;    // cloud session id → status shown last time
    QHash<QString, QString> m_cloudDismissed; // cloud session id → status when you closed its card
    QTimer m_pruneTimer;
    int m_nextRequest = 1;
    double m_five = 0, m_seven = 0;
    qint64 m_fiveReset = 0, m_sevenReset = 0;
};
