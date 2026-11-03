#include "CloudSessions.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSettings>
#include <QStringList>
#include <QUrl>

#include <algorithm>

namespace {
const QUrl kSessionsUrl(QStringLiteral("https://api.anthropic.com/v1/code/sessions?limit=50"));
const QStringList kKnownStatuses = {QStringLiteral("running"), QStringLiteral("pending"), QStringLiteral("idle"),
                                    QStringLiteral("requires_action"), QStringLiteral("failed")};
constexpr int kEveryMs = 60 * 1000;
constexpr int kMaxBackoffMs = 30 * 60 * 1000;
constexpr int kTimeoutMs = 20 * 1000;
constexpr qint64 kRenewAheadMs = 60 * 1000;

QJsonObject readDoc(const QString &path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()).object() : QJsonObject();
}

// The server writes microseconds ("…08:30:03.914844Z"); Qt reads milliseconds.
qint64 isoMs(QString s) {
    static const QRegularExpression extraDigits(QStringLiteral("(\\.\\d{3})\\d+"));
    s.replace(extraDigits, QStringLiteral("\\1"));
    QDateTime t = QDateTime::fromString(s, Qt::ISODateWithMs);
    if (!t.isValid()) t = QDateTime::fromString(s, Qt::ISODate);
    return t.isValid() ? t.toMSecsSinceEpoch() : 0;
}

// "SESSION_STATUS_IDLE" and "idle" are the same value.
QString plain(const QJsonValue &v, const QString &prefix) {
    QString s = v.toString().toLower();
    if (s.startsWith(prefix)) s.remove(0, prefix.size());
    return s;
}

// The status of a list entry, by the rules the Claude Code CLI uses: the entry has
// status, worker_status and connection_status, not one session_status field.
QString statusOf(const QJsonObject &o) {
    const QString prefixS = QStringLiteral("session_status_"), prefixW = QStringLiteral("worker_status_");
    if (o.contains(QStringLiteral("session_status"))) return plain(o.value(QStringLiteral("session_status")), prefixS);
    const QString status = plain(o.value(QStringLiteral("status")), prefixS);
    const QString worker = plain(o.value(QStringLiteral("worker_status")), prefixW);
    if (status == QLatin1String("archived") || status == QLatin1String("failed")) return status;
    if (status == QLatin1String("paused")) return worker == QLatin1String("requires_action") ? QStringLiteral("requires_action") : QStringLiteral("idle");
    if (worker.isEmpty() || worker == QLatin1String("unspecified")) return QStringLiteral("pending");
    if (worker == QLatin1String("running"))
        return o.value(QStringLiteral("connection_status")).toString().toLower() == QLatin1String("disconnected") ? QStringLiteral("idle") : QStringLiteral("running");
    if (worker == QLatin1String("requires_action")) return QStringLiteral("requires_action");
    return QStringLiteral("idle");
}

// "https://github.com/owner/name.git" → "owner/name"; empty for anything else.
QString repoOf(const QJsonArray &sources) {
    for (const auto &s : sources) {
        const QJsonObject o = s.toObject();
        if (o.value(QStringLiteral("type")).toString() != QLatin1String("git_repository")) continue;
        QString path = QUrl(o.value(QStringLiteral("url")).toString()).path();
        if (path.startsWith(u'/')) path.remove(0, 1);
        if (path.endsWith(QLatin1String(".git"))) path.chop(4);
        const QStringList parts = path.split(u'/', Qt::SkipEmptyParts);
        if (parts.size() == 2) return parts.join(u'/');
    }
    return {};
}
}

QList<CloudSession> parseCloudSessions(const QByteArray &body) {
    static const QRegularExpression idPattern(QStringLiteral("^[A-Za-z0-9_-]{1,128}$"));
    QList<CloudSession> out;
    const QJsonArray items = QJsonDocument::fromJson(body).object().value(QStringLiteral("data")).toArray();
    int dropped = 0;
    for (const auto &item : items) {
        const QJsonObject o = item.toObject();
        const QString id = o.value(QStringLiteral("id")).toString();
        const QString status = statusOf(o);
        if (!idPattern.match(id).hasMatch() || !kKnownStatuses.contains(status)) {
            // Archived sessions are expected to be left out; anything else is worth a line in the log.
            if (status != QLatin1String("archived")) ++dropped;
            continue;
        }
        // The first timestamp that reads as a date: when the session last moved.
        qint64 updated = 0;
        for (const char *key : {"updated_at", "last_event_at", "created_at"}) {
            updated = isoMs(o.value(QLatin1String(key)).toString());
            if (updated) break;
        }
        if (!updated) {
            ++dropped;
            continue;
        }
        out << CloudSession{id, o.value(QStringLiteral("title")).toString(), status, repoOf(o.value(QStringLiteral("sources")).toArray()),
                            QStringLiteral("https://claude.ai/code/") + id, updated};
    }
    if (dropped) qInfo("cloud sessions: %d entries left out (unknown status or date)", dropped);
    return out;
}

CloudSessions::CloudSessions(QObject *parent) : QObject(parent) {
    m_timer.setInterval(kEveryMs);
    connect(&m_timer, &QTimer::timeout, this, [this] { refresh(); });
    m_timer.start();
    QTimer::singleShot(2000, this, [this] { refresh(); });
}

bool CloudSessions::enabled() const { return QSettings().value(QStringLiteral("cloud/enabled"), true).toBool(); }

void CloudSessions::setEnabled(bool on) {
    if (on == enabled()) return;
    QSettings().setValue(QStringLiteral("cloud/enabled"), on);
    emit enabledChanged();
    if (on) {
        QTimer::singleShot(0, this, [this] { refresh(); });
    } else {
        m_quietUntil = {};
        emit sessionsChanged({});
    }
}

QString CloudSessions::configDir() const {
    QString dir = qEnvironmentVariable("CLAUDE_CONFIG_DIR");
    if (dir.isEmpty()) dir = QDir::homePath() + QStringLiteral("/.claude");
    return dir;
}

void CloudSessions::refresh() {
    if (m_busy || !enabled()) return;
    if (m_quietUntil.isValid() && QDateTime::currentDateTimeUtc() < m_quietUntil) return; // Anthropic asked us to wait
    const QJsonObject oauth = readDoc(configDir() + QStringLiteral("/.credentials.json")).value(QStringLiteral("claudeAiOauth")).toObject();
    const QString token = oauth.value(QStringLiteral("accessToken")).toString();
    if (token.isEmpty()) return; // not signed in to Claude Code with a claude.ai account
    const qint64 expires = qint64(oauth.value(QStringLiteral("expiresAt")).toDouble());
    if (expires - QDateTime::currentMSecsSinceEpoch() < kRenewAheadMs) return; // PlanUsage renews the login; we try again next minute

    // The organization is in ~/.claude.json (or $CLAUDE_CONFIG_DIR/.claude.json).
    QString org;
    for (const QString &path : {configDir() + QStringLiteral("/.claude.json"), QDir::homePath() + QStringLiteral("/.claude.json")}) {
        org = readDoc(path).value(QStringLiteral("oauthAccount")).toObject().value(QStringLiteral("organizationUuid")).toString();
        if (!org.isEmpty()) break;
    }
    if (org.isEmpty()) return;
    ask(token, org);
}

void CloudSessions::ask(const QString &token, const QString &org) {
    m_busy = true;
    QNetworkRequest req(kSessionsUrl);
    req.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    req.setRawHeader("anthropic-version", "2023-06-01");
    req.setRawHeader("x-organization-uuid", org.toUtf8());
    req.setTransferTimeout(kTimeoutMs);
    QNetworkReply *reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_busy = false;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 429 || status == 403 || status == 404) {
            // Told to slow down, or the list isn't open to this account: wait longer each time.
            m_backoffMs = std::min(kMaxBackoffMs, std::max(kEveryMs, m_backoffMs * 2));
            m_quietUntil = QDateTime::currentDateTimeUtc().addMSecs(m_backoffMs);
            if (!m_warned) qInfo("cloud sessions: Anthropic answered %d; next try in %d min", status, m_backoffMs / 60000);
            m_warned = true;
            return;
        }
        // 401 and network trouble: the token may be renewed or the network back by the next minute.
        if (reply->error() != QNetworkReply::NoError || status != 200) {
            if (!m_warned) qInfo("cloud sessions: no list from Anthropic (%d)", status);
            m_warned = true;
            return;
        }
        const QByteArray body = reply->readAll();
        if (!QJsonDocument::fromJson(body).object().contains(QStringLiteral("data"))) return; // not the list: keep what we show
        m_backoffMs = 0;
        m_warned = false;
        emit sessionsChanged(parseCloudSessions(body));
    });
}
