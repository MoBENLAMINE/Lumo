#include "Music.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDateTime>
#include <QTimer>
#include <QImage>
#include <QImageReader>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "Vivid.h"

#include <algorithm>

namespace {
const QString kPrefix = QStringLiteral("org.mpris.MediaPlayer2.");
const QString kPath = QStringLiteral("/org/mpris/MediaPlayer2");
const QString kPlayer = QStringLiteral("org.mpris.MediaPlayer2.Player");
const QString kProps = QStringLiteral("org.freedesktop.DBus.Properties");
constexpr int kTimeoutMs = 1500;

QVariant unwrap(const QVariant &v) {
    if (v.canConvert<QDBusVariant>()) return v.value<QDBusVariant>().variant();
    return v;
}

QVariantMap asMap(const QVariant &v) {
    const QVariant u = unwrap(v);
    if (u.canConvert<QDBusArgument>()) return qdbus_cast<QVariantMap>(u.value<QDBusArgument>());
    return u.toMap();
}

QVariant readProperty(const QString &service, const QString &iface, const QString &name) {
    QDBusMessage msg = QDBusMessage::createMethodCall(service, kPath, kProps, QStringLiteral("Get"));
    msg.setArguments({iface, name});
    const QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, kTimeoutMs);
    return reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty() ? unwrap(reply.arguments().first()) : QVariant();
}

bool ignored(const QString &name) {
    // playerctld mirrors the other players; it would show every track twice.
    return !name.startsWith(kPrefix) || name.contains(QLatin1String("playerctld"));
}
}

Music::Music(QObject *parent) : QObject(parent) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(QString(), kPath, kProps, QStringLiteral("PropertiesChanged"), this, SLOT(onPropertiesChanged(QDBusMessage)));
    bus.connect(QString(), kPath, kPlayer, QStringLiteral("Seeked"), this, SLOT(onSeeked(QDBusMessage)));
    // Players come and go (a browser only shows up once a tab plays). Listen
    // to the bus itself rather than a wildcard QDBusServiceWatcher, which
    // never fires for a pattern on some Qt and bus versions.
    bus.connect(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
                QStringLiteral("org.freedesktop.DBus"), QStringLiteral("NameOwnerChanged"),
                this, SLOT(onNameOwnerChanged(QString,QString,QString)));
    m_since.start();
    scan();
}

void Music::scan() {
    const QStringList names = QDBusConnection::sessionBus().interface()->registeredServiceNames().value();
    for (const auto &name : names) {
        if (ignored(name) || m_players.contains(name)) continue;
        m_owners.insert(QDBusConnection::sessionBus().interface()->serviceOwner(name).value(), name);
        m_players.insert(name, {readProperty(name, kPlayer, QStringLiteral("PlaybackStatus")).toString(), 0});
    }
    choose();
}

void Music::onNameOwnerChanged(const QString &name, const QString &, const QString &newOwner) {
    if (ignored(name)) return;
    if (newOwner.isEmpty()) {
        m_players.remove(name);
        for (auto it = m_owners.begin(); it != m_owners.end();) it = it.value() == name ? m_owners.erase(it) : std::next(it);
    } else {
        m_owners.insert(newOwner, name);
        m_players.insert(name, {readProperty(name, kPlayer, QStringLiteral("PlaybackStatus")).toString(), QDateTime::currentMSecsSinceEpoch()});
    }
    choose();
}

void Music::onPropertiesChanged(const QDBusMessage &message) {
    // A player we haven't met (we missed its arrival): look again.
    if (!m_owners.contains(message.service()) && !message.arguments().isEmpty()
        && message.arguments().at(0).toString() == kPlayer)
        scan();
    const QString name = m_owners.value(message.service());
    const QVariantList args = message.arguments();
    if (name.isEmpty() || args.size() < 2 || args.at(0).toString() != kPlayer) return;
    const QVariantMap changed = asMap(args.at(1));
    if (changed.contains(QStringLiteral("PlaybackStatus"))) {
        m_players[name].status = changed.value(QStringLiteral("PlaybackStatus")).toString();
        m_players[name].lastActive = QDateTime::currentMSecsSinceEpoch();
        choose();
    }
    if (name == m_service) load(name); // new track, new cover…
}

void Music::onSeeked(const QDBusMessage &message) {
    if (m_owners.value(message.service()) != m_service || message.arguments().isEmpty()) return;
    m_positionAt = message.arguments().first().toLongLong() / 1000;
    m_since.restart();
    emit changed();
}

void Music::choose() {
    QString best;
    qint64 bestAt = -1;
    bool bestPlaying = false;
    for (auto it = m_players.cbegin(); it != m_players.cend(); ++it) {
        const bool playing = it->status == QLatin1String("Playing");
        if (playing > bestPlaying || (playing == bestPlaying && it->lastActive > bestAt)) {
            best = it.key();
            bestAt = it->lastActive;
            bestPlaying = playing;
        }
    }
    // A player that is stopped with nothing loaded isn't worth a pill.
    if (!best.isEmpty() && m_players.value(best).status == QLatin1String("Stopped")) best.clear();
    if (best != m_service) {
        m_service = best;
        if (best.isEmpty()) {
            m_title.clear(), m_artist.clear(), m_art.clear(), m_status.clear();
            emit changed();
            return;
        }
        emit changed();   // even if the player is slow to answer below
    }
    if (!best.isEmpty()) load(best);
}

void Music::load(const QString &service) {
    QDBusMessage msg = QDBusMessage::createMethodCall(service, kPath, kProps, QStringLiteral("GetAll"));
    msg.setArguments({kPlayer});
    const QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, kTimeoutMs);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) return;
    const QVariantMap props = asMap(reply.arguments().first());
    const QVariantMap meta = asMap(props.value(QStringLiteral("Metadata")));
    m_status = props.value(QStringLiteral("PlaybackStatus")).toString();
    const QString title = meta.value(QStringLiteral("xesam:title")).toString();
    const bool sameTrack = title == m_title;
    m_title = title;
    m_artist = unwrap(meta.value(QStringLiteral("xesam:artist"))).toStringList().join(QStringLiteral(", "));
    m_art = meta.value(QStringLiteral("mpris:artUrl")).toString();
    // Firefox-based browsers drop the length from the metadata after a seek;
    // keep the one we know for the same track.
    const qint64 length = meta.value(QStringLiteral("mpris:length")).toLongLong() / 1000;
    if (length > 0 || !sameTrack) m_length = length;
    m_trackId = unwrap(meta.value(QStringLiteral("mpris:trackid"))).value<QDBusObjectPath>().path();
    m_canSeek = props.value(QStringLiteral("CanSeek")).toBool() && !m_trackId.isEmpty();
    m_hasShuffle = props.contains(QStringLiteral("Shuffle"));
    m_shuffle = props.value(QStringLiteral("Shuffle")).toBool();
    m_hasLoop = props.contains(QStringLiteral("LoopStatus"));
    m_loop = props.value(QStringLiteral("LoopStatus")).toString();
    m_hasVolume = props.contains(QStringLiteral("Volume"));
    m_volume = std::clamp(props.value(QStringLiteral("Volume"), 1.0).toDouble(), 0.0, 1.0);
    // Right after our own seek the player may still report the old position.
    if (!(sameTrack && m_seekedAt.isValid() && m_seekedAt.elapsed() < 2000)) {
        m_positionAt = props.value(QStringLiteral("Position")).toLongLong() / 1000;
        m_since.restart();
    }
    m_identity = readProperty(service, QStringLiteral("org.mpris.MediaPlayer2"), QStringLiteral("Identity")).toString();
    if (m_title.isEmpty()) m_title = m_identity;
    if (!m_canSeek || m_hasLoop) m_fakeLoop = false;
    watchLoop();
    updateColor();
    emit changed();
}

// "Repeat this track" for players without one: shortly before the end, back
// to the start. Checked twice a second, and only while that is on and playing.
void Music::watchLoop() {
    const bool needed = m_fakeLoop && playing() && m_length > 3000;
    if (!needed) { if (m_loopWatch) m_loopWatch->stop(); return; }
    if (!m_loopWatch) {
        m_loopWatch = new QTimer(this);
        m_loopWatch->setInterval(500);
        connect(m_loopWatch, &QTimer::timeout, this, [this] {
            if (m_fakeLoop && playing() && m_length > 3000 && position() >= m_length - 900) seek(0);
        });
    }
    m_loopWatch->start();
}

// The cover's color, once per cover: from the file, or downloaded (small).
void Music::updateColor() {
    if (m_art == m_colorFor) return;
    m_colorFor = m_art;
    const QUrl url(m_art);
    auto apply = [this](const QImage &img) {
        const QColor c = Vivid::color(img);
        if (c == m_color) return;
        m_color = c;
        emit colorChanged();
    };
    if (m_art.isEmpty()) { apply({}); return; }
    if (url.isLocalFile()) {
        QImageReader reader(url.toLocalFile());
        reader.setScaledSize(QSize(48, 48));
        apply(reader.read());
        return;
    }
    if (url.scheme() != QLatin1String("https") && url.scheme() != QLatin1String("http")) return;
    if (!m_net) m_net = new QNetworkAccessManager(this);
    QNetworkReply *reply = m_net->get(QNetworkRequest(url));
    const QString wanted = m_art;
    connect(reply, &QNetworkReply::finished, this, [this, reply, wanted, apply] {
        reply->deleteLater();
        if (wanted != m_art || reply->error() != QNetworkReply::NoError) return;
        apply(QImage::fromData(reply->readAll()));
    });
}

qint64 Music::position() const {
    const qint64 p = m_positionAt + (playing() ? m_since.elapsed() : 0);
    return m_length > 0 ? std::min(p, m_length) : p;
}

void Music::call(const QString &method, const QVariantList &args) {
    if (m_service.isEmpty()) return;
    QDBusMessage msg = QDBusMessage::createMethodCall(m_service, kPath, kPlayer, method);
    msg.setArguments(args);
    QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
}

void Music::setPlayerProperty(const QString &name, const QVariant &value) {
    if (m_service.isEmpty()) return;
    QDBusMessage msg = QDBusMessage::createMethodCall(m_service, kPath, kProps, QStringLiteral("Set"));
    msg.setArguments({kPlayer, name, QVariant::fromValue(QDBusVariant(value))});
    QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
}

// The player confirms with PropertiesChanged; we show the change at once.
void Music::toggleShuffle() {
    if (!m_hasShuffle) return;
    m_shuffle = !m_shuffle;
    setPlayerProperty(QStringLiteral("Shuffle"), m_shuffle);
    emit changed();
}

void Music::cycleLoop() {
    if (!m_hasLoop) {
        if (!m_canSeek) return;
        m_fakeLoop = !m_fakeLoop;
        watchLoop();
        emit changed();
        return;
    }
    m_loop = m_loop == QLatin1String("None") ? QStringLiteral("Playlist")
           : m_loop == QLatin1String("Playlist") ? QStringLiteral("Track") : QStringLiteral("None");
    setPlayerProperty(QStringLiteral("LoopStatus"), m_loop);
    emit changed();
}

void Music::setVolume(double volume) {
    if (!m_hasVolume) return;
    m_volume = std::clamp(volume, 0.0, 1.0);
    setPlayerProperty(QStringLiteral("Volume"), m_volume);
    emit changed();
}

void Music::playPause() { call(QStringLiteral("PlayPause")); }
// Some players advertise Next/Previous and then ignore them (a browser playing
// a single YouTube video). If the track hasn't changed shortly after, fall
// back: Previous restarts the track, Next jumps to its end so the site's
// autoplay moves on.
void Music::skip(bool forward) {
    call(forward ? QStringLiteral("Next") : QStringLiteral("Previous"));
    const QString before = m_title;
    const QString service = m_service;
    QTimer::singleShot(1200, this, [this, forward, before, service] {
        if (m_service != service || m_title != before || !m_canSeek) return;
        if (forward && m_length > 3000) seek(m_length - 1500);
        else if (!forward) seek(0);
    });
}

void Music::next() { skip(true); }
void Music::previous() { skip(false); }

void Music::seek(qint64 ms) {
    if (!m_canSeek) return;
    call(QStringLiteral("SetPosition"), {QVariant::fromValue(QDBusObjectPath(m_trackId)), qlonglong(ms * 1000)});
    m_positionAt = ms;
    m_since.restart();
    m_seekedAt.restart();
    emit changed();
}
