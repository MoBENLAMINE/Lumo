#pragma once

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>

// Your Claude plan's 5-hour and weekly usage, straight from Anthropic, for
// every way you use Claude (claude.ai, the apps, Claude Code). It uses the
// login Claude Code keeps in ~/.claude/.credentials.json — no API key — and
// only reads it: renewing an expired login is left to Claude Code itself.
// Only Anthropic's own usage endpoint is called.
class PlanUsage : public QObject {
    Q_OBJECT
public:
    explicit PlanUsage(QObject *parent = nullptr);
    // Ask now, unless the last answer is younger than `minAgeMs`.
    Q_INVOKABLE void refresh(int minAgeMs = 0);

signals:
    void usage(double five, qint64 fiveReset, double seven, qint64 sevenReset);

private:
    void ask(const QString &token);
    QString credentialsPath() const;

    QNetworkAccessManager m_net;
    QTimer m_timer;
    QDateTime m_last, m_quietUntil;
    int m_backoffMs = 0;
    bool m_busy = false;
};
