#include "Lyrics.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QUrlQuery>

#include <algorithm>
#include <cmath>

namespace {
constexpr int kCacheSize = 300;
}

Lyrics::Lyrics(QObject *parent) : QObject(parent) {}

bool Lyrics::enabled() const { return QSettings().value(QStringLiteral("music/lyrics"), true).toBool(); }

void Lyrics::setEnabled(bool on) {
    QSettings().setValue(QStringLiteral("music/lyrics"), on);
    emit settingsChanged();
    if (on && m_active) lookUp();
    if (!on) { m_lines.clear(); m_plain.clear(); m_state = QStringLiteral("off"); emit linesChanged(); }
}

void Lyrics::setActive(bool on) {
    if (on == m_active) return;
    m_active = on;
    emit activeChanged();
    if (on) lookUp();
}

void Lyrics::setSong(const QString &title, const QString &artist, qint64 lengthMs) {
    const auto clean = cleanSong(title, artist);
    const QString key = clean.first.toLower() + QLatin1Char('\n') + clean.second.toLower();
    if (key == m_key) return;
    m_key = key;
    m_artist = clean.first;
    m_title = clean.second;
    m_length = lengthMs;
    m_offset = 0;
    emit offsetChanged();
    m_lines.clear();
    m_plain.clear();
    m_state = QStringLiteral("none");
    emit linesChanged();
    if (m_active) lookUp();
}

QPair<QString, QString> Lyrics::cleanSong(const QString &title, const QString &artist) {
    QString t = title, a = artist;
    // what video sites add to a song's name
    static const QRegularExpression extras(
        QStringLiteral(R"(\s*[\(\[][^\)\]]*(official|video|audio|lyrics?|visuali[sz]er|clip|4k|hd|hq|remaster(ed)?|live|prod\.?|feat\.?|ft\.?)[^\)\]]*[\)\]])"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression hashtags(QStringLiteral(R"(\s*#\S+)"));
    t.remove(extras).remove(hashtags);
    a.remove(QRegularExpression(QStringLiteral(R"(\s*-\s*Topic$)"), QRegularExpression::CaseInsensitiveOption));
    a.remove(QRegularExpression(QStringLiteral("VEVO$"), QRegularExpression::CaseInsensitiveOption));
    // "Artist - Title" in the title (a YouTube upload): the artist is the part before
    const int dash = t.indexOf(QStringLiteral(" - "));
    // (on YouTube the "artist" is often only the channel, so the title wins)
    if (dash > 0) {
        a = t.left(dash).trimmed();
        t = t.mid(dash + 3).trimmed();
    }
    // only the main artist: "A x B", "A feat. B", "A, B"
    static const QRegularExpression more(QStringLiteral(R"(\s+(x|&|feat\.?|ft\.?|featuring|with)\s+.*$|,.*$)"), QRegularExpression::CaseInsensitiveOption);
    a.remove(more);
    t.remove(QRegularExpression(QStringLiteral(R"(\s+(feat\.?|ft\.?)\s+.*$)"), QRegularExpression::CaseInsensitiveOption));
    return {a.simplified(), t.simplified()};
}

QString Lyrics::cachePath() const {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/lyrics");
    QDir().mkpath(dir);
    return dir + QLatin1Char('/') + QString::fromLatin1(QCryptographicHash::hash(m_key.toUtf8(), QCryptographicHash::Sha1).toHex()) + QStringLiteral(".json");
}

void Lyrics::lookUp() {
    if (!enabled()) { m_state = QStringLiteral("off"); emit linesChanged(); return; }
    if (m_title.isEmpty() || m_lookedUp == m_key) return;
    m_lookedUp = m_key;
    // the cache first
    QFile cached(cachePath());
    if (cached.open(QIODevice::ReadOnly)) {
        const QJsonObject o = QJsonDocument::fromJson(cached.readAll()).object();
        found(o.value(QStringLiteral("synced")).toString(), o.value(QStringLiteral("plain")).toString(), false);
        return;
    }
    if (!m_net) m_net = new QNetworkAccessManager(this);
    if (m_reply) { m_reply->abort(); m_reply = nullptr; }
    m_state = QStringLiteral("looking");
    emit linesChanged();
    QUrl url(QStringLiteral("https://lrclib.net/api/get"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("track_name"), m_title);
    q.addQueryItem(QStringLiteral("artist_name"), m_artist);
    if (m_length > 0) q.addQueryItem(QStringLiteral("duration"), QString::number(std::lround(m_length / 1000.0)));
    url.setQuery(q);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Lumo (https://github.com/MoBENLAMINE/Lumo)"));
    const QString key = m_key;
    m_reply = m_net->get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this, reply = m_reply, key] {
        reply->deleteLater();
        if (reply == m_reply) m_reply = nullptr;
        if (key != m_key || reply->error() == QNetworkReply::OperationCanceledError) return;
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        if (reply->error() == QNetworkReply::NoError && (!o.value(QStringLiteral("syncedLyrics")).toString().isEmpty()
                                                         || !o.value(QStringLiteral("plainLyrics")).toString().isEmpty())) {
            found(o.value(QStringLiteral("syncedLyrics")).toString(), o.value(QStringLiteral("plainLyrics")).toString(), true);
            return;
        }
        // not an exact match: search, and take the closest in length
        QUrl search(QStringLiteral("https://lrclib.net/api/search"));
        QUrlQuery sq;
        sq.addQueryItem(QStringLiteral("q"), m_artist + QLatin1Char(' ') + m_title);
        search.setQuery(sq);
        QNetworkRequest request(search);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Lumo (https://github.com/MoBENLAMINE/Lumo)"));
        m_reply = m_net->get(request);
        connect(m_reply, &QNetworkReply::finished, this, [this, reply = m_reply, key] {
            reply->deleteLater();
            if (reply == m_reply) m_reply = nullptr;
            if (key != m_key) return;
            const QJsonArray results = QJsonDocument::fromJson(reply->readAll()).array();
            QJsonObject best;
            double bestGap = 1e9;
            for (const auto &v : results) {
                const QJsonObject r = v.toObject();
                if (r.value(QStringLiteral("syncedLyrics")).toString().isEmpty()) continue;
                const double gap = m_length > 0 ? std::abs(r.value(QStringLiteral("duration")).toDouble() - m_length / 1000.0) : 0;
                if (gap < bestGap) { bestGap = gap; best = r; }
            }
            if (best.isEmpty() || bestGap > 8) { found(QString(), QString(), true); return; }
            found(best.value(QStringLiteral("syncedLyrics")).toString(), best.value(QStringLiteral("plainLyrics")).toString(), true);
        });
    });
}

void Lyrics::found(const QString &synced, const QString &plain, bool cache) {
    m_lines = parseLrc(synced);
    m_plain = plain;
    m_state = !m_lines.isEmpty() ? QStringLiteral("synced") : !plain.isEmpty() ? QStringLiteral("plain") : QStringLiteral("none");
    emit linesChanged();
    if (!cache) return;
    // keep it, and only the last 300 songs
    QFile f(cachePath());
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(QJsonObject{{QStringLiteral("synced"), synced}, {QStringLiteral("plain"), plain}}).toJson(QJsonDocument::Compact));
    QDir dir(QFileInfo(f).absolutePath());
    const QFileInfoList all = dir.entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Time);
    for (int i = kCacheSize; i < all.size(); ++i) QFile::remove(all[i].filePath());
}

QVariantList Lyrics::parseLrc(const QString &lrc) {
    static const QRegularExpression stamp(QStringLiteral(R"(\[(\d+):(\d+(?:[.:]\d+)?)\])"));
    QList<QPair<qint64, QString>> lines;
    for (const QString &raw : lrc.split(QLatin1Char('\n'))) {
        QList<qint64> times;
        int textFrom = 0;
        auto it = stamp.globalMatch(raw);
        while (it.hasNext()) {
            const auto m = it.next();
            if (m.capturedStart() != textFrom) break;   // stamps lead the line
            const double seconds = m.captured(2).replace(QLatin1Char(':'), QLatin1Char('.')).toDouble();
            times.append(qint64(m.captured(1).toInt() * 60000 + std::lround(seconds * 1000)));
            textFrom = m.capturedEnd();
        }
        const QString text = raw.mid(textFrom).trimmed();
        for (qint64 t : times) lines.append({t, text});
    }
    std::stable_sort(lines.begin(), lines.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
    QVariantList out;
    for (const auto &l : lines)
        out.append(QVariantMap{{QStringLiteral("time"), l.first}, {QStringLiteral("text"), l.second}});
    return out;
}

int Lyrics::lineAt(qint64 positionMs) const {
    const qint64 at = positionMs + m_offset;
    int lo = 0, hi = int(m_lines.size()) - 1, found = -1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        if (m_lines[mid].toMap().value(QStringLiteral("time")).toLongLong() <= at) { found = mid; lo = mid + 1; }
        else hi = mid - 1;
    }
    return found;
}
