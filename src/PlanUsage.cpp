#include "PlanUsage.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSaveFile>

namespace {
const QUrl kUsageUrl(QStringLiteral("https://api.anthropic.com/api/oauth/usage"));
const QUrl kTokenUrl(QStringLiteral("https://platform.claude.com/v1/oauth/token"));
// Claude Code's own public client id, the one it sends when it renews a login.
const QString kClientId = QStringLiteral("9d1c250a-e61b-44d9-88ed-5944d1962f5e");
const QByteArray kBeta = "oauth-2025-04-20";
constexpr int kEveryMs = 5 * 60 * 1000;
constexpr int kMaxBackoffMs = 30 * 60 * 1000;
constexpr qint64 kRenewAheadMs = 5 * 60 * 1000;

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
    const qint64 expires = qint64(oauth.value(QStringLiteral("expiresAt")).toDouble());
    if (expires - QDateTime::currentMSecsSinceEpoch() < kRenewAheadMs) {
        renew();
        return;
    }
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

// Trades the refresh token for a new pair and saves it. Refreshing rotates the
// refresh token, so it only runs when the file can be written back.
bool PlanUsage::renew() {
    const QString path = credentialsPath();
    QJsonObject doc = readDoc(path);
    const QString refreshToken = doc.value(QStringLiteral("claudeAiOauth")).toObject().value(QStringLiteral("refreshToken")).toString();
    if (refreshToken.isEmpty() || !QFileInfo(path).isWritable()) return false;
    m_busy = true;
    QNetworkRequest req(kTokenUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setTransferTimeout(20000);
    const QJsonObject body{{QStringLiteral("grant_type"), QStringLiteral("refresh_token")},
                           {QStringLiteral("refresh_token"), refreshToken},
                           {QStringLiteral("client_id"), kClientId}};
    QNetworkReply *reply = m_net.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, path] {
        reply->deleteLater();
        m_busy = false;
        if (reply->error() != QNetworkReply::NoError) {
            qInfo("plan usage: Claude refused to renew the login (%d)", reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
            return;
        }
        const QJsonObject answer = QJsonDocument::fromJson(reply->readAll()).object();
        const QString access = answer.value(QStringLiteral("access_token")).toString();
        if (access.isEmpty()) return;
        // Re-read: Claude Code may have touched the file meanwhile.
        QJsonObject doc = readDoc(path);
        QJsonObject oauth = doc.value(QStringLiteral("claudeAiOauth")).toObject();
        oauth.insert(QStringLiteral("accessToken"), access);
        if (answer.contains(QStringLiteral("refresh_token")))
            oauth.insert(QStringLiteral("refreshToken"), answer.value(QStringLiteral("refresh_token")).toString());
        if (answer.contains(QStringLiteral("expires_in")))
            oauth.insert(QStringLiteral("expiresAt"),
                         double(QDateTime::currentMSecsSinceEpoch() + qint64(answer.value(QStringLiteral("expires_in")).toDouble()) * 1000));
        doc.insert(QStringLiteral("claudeAiOauth"), oauth);
        QSaveFile f(path);
        if (!f.open(QIODevice::WriteOnly) || f.write(QJsonDocument(doc).toJson(QJsonDocument::Compact)) < 0 || !f.commit()) {
            qWarning("plan usage: the renewed Claude login could not be saved; Claude Code may ask you to log in again");
            return;
        }
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        ask(access);
    });
    return true;
}
