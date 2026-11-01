#include "PlanUsage.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

namespace {
const QUrl kUsageUrl(QStringLiteral("https://api.anthropic.com/api/oauth/usage"));
const QByteArray kBeta = "oauth-2025-04-20";
constexpr int kEveryMs = 5 * 60 * 1000;
constexpr int kMaxBackoffMs = 30 * 60 * 1000;
constexpr qint64 kExpiryMarginMs = 60 * 1000;

QJsonObject readDoc(const QString &path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()).object() : QJsonObject();
}
}

PlanUsage::PlanUsage(QObject *parent) : QObject(parent) {
    m_timer.setInterval(kEveryMs);
    connect(&m_timer, &QTimer::timeout, this, [this] { refresh(); });
    m_timer.start();
    QTimer::singleShot(1500, this, [this] { refresh(); });
}

QString PlanUsage::credentialsPath() const {
    QString dir = qEnvironmentVariable("CLAUDE_CONFIG_DIR");
    if (dir.isEmpty()) dir = QDir::homePath() + QStringLiteral("/.claude");
    return dir + QStringLiteral("/.credentials.json");
}

void PlanUsage::refresh(int minAgeMs) {
    if (m_busy || (m_last.isValid() && m_last.msecsTo(QDateTime::currentDateTimeUtc()) < minAgeMs)) return;
    if (m_quietUntil.isValid() && QDateTime::currentDateTimeUtc() < m_quietUntil) return; // told to slow down
    const QJsonObject oauth = readDoc(credentialsPath()).value(QStringLiteral("claudeAiOauth")).toObject();
    const QString token = oauth.value(QStringLiteral("accessToken")).toString();
    if (token.isEmpty()) return; // not signed in to Claude Code with a claude.ai account
    // An expired login is Claude Code's to renew, not ours: refresh tokens
    // rotate, and renewing it here could sign Claude Code out. Until it does,
    // the gauge lives on the status line's numbers.
    const qint64 expires = qint64(oauth.value(QStringLiteral("expiresAt")).toDouble());
    if (expires > 0 && expires - QDateTime::currentMSecsSinceEpoch() < kExpiryMarginMs) return;
    ask(token);
}

void PlanUsage::ask(const QString &token) {
    m_busy = true;
    QNetworkRequest req(kUsageUrl);
    req.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    req.setRawHeader("anthropic-beta", kBeta);
    req.setTransferTimeout(20000);
    QNetworkReply *reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_busy = false;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 429) {
            // Too many requests: wait (as told, or longer each time) before asking again.
            const int told = reply->rawHeader("retry-after").toInt() * 1000;
            m_backoffMs = std::min(kMaxBackoffMs, std::max(told, m_backoffMs ? m_backoffMs * 2 : kEveryMs));
            m_quietUntil = QDateTime::currentDateTimeUtc().addMSecs(m_backoffMs);
            qInfo("plan usage: Anthropic asks to slow down; next try in %d min", m_backoffMs / 60000);
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            qInfo("plan usage: no answer from Anthropic (%d)", status);
            return;
        }
        m_backoffMs = 0;
        const QJsonObject body = QJsonDocument::fromJson(reply->readAll()).object();
        auto window = [&](const char *name, double &used, qint64 &resets) {
            const QJsonObject w = body.value(QLatin1String(name)).toObject();
            if (w.isEmpty() || !w.value(QStringLiteral("utilization")).isDouble()) return false;
            used = w.value(QStringLiteral("utilization")).toDouble();
            resets = QDateTime::fromString(w.value(QStringLiteral("resets_at")).toString(), Qt::ISODateWithMs).toSecsSinceEpoch();
            return true;
        };
        double five = 0, seven = 0;
        qint64 fiveReset = 0, sevenReset = 0;
        const bool a = window("five_hour", five, fiveReset), b = window("seven_day", seven, sevenReset);
        if (!a && !b) return;
        m_last = QDateTime::currentDateTimeUtc();
        emit usage(five, fiveReset, seven, sevenReset);
    });
}
