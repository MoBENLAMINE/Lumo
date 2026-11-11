#pragma once

#include <QDBusMessage>
#include <QElapsedTimer>
#include <QObject>
#include <QColor>
#include <QStringList>

class QNetworkAccessManager;
class QTimer;

// What's playing, from any MPRIS player (Spotify, Firefox, Chromium, VLC,
// Elisa…), with play/pause, previous, next and seek, and shuffle, repeat and
// volume where the player offers them. The player that is
// playing wins; otherwise the last one that was.
class Music : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool playing READ playing NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString artist READ artist NOTIFY changed)
    Q_PROPERTY(QString artUrl READ artUrl NOTIFY changed)
    Q_PROPERTY(QString player READ player NOTIFY changed)
    Q_PROPERTY(qint64 length READ length NOTIFY changed)       // ms
    Q_PROPERTY(bool canSeek READ canSeek NOTIFY changed)
    // Each is false when the player doesn't offer it (some browsers don't).
    Q_PROPERTY(bool hasShuffle READ hasShuffle NOTIFY changed)
    Q_PROPERTY(bool shuffle READ shuffle NOTIFY changed)
    Q_PROPERTY(bool hasLoop READ hasLoop NOTIFY changed)
    // "None", "Playlist" or "Track"
    Q_PROPERTY(QString loop READ loop NOTIFY changed)
    Q_PROPERTY(bool hasVolume READ hasVolume NOTIFY changed)
    Q_PROPERTY(double volume READ volume NOTIFY changed)
    // The cover's most vivid color, to tint the music card; invalid until known.
    Q_PROPERTY(QColor color READ color NOTIFY colorChanged)
    Q_PROPERTY(bool hasColor READ hasColor NOTIFY colorChanged)
public:
    explicit Music(QObject *parent = nullptr);

    bool available() const { return !m_service.isEmpty(); }
    bool playing() const { return m_status == QLatin1String("Playing"); }
    QString title() const { return m_title; }
    QString artist() const { return m_artist; }
    QString artUrl() const { return m_art; }
    QString player() const { return m_identity; }
    qint64 length() const { return m_length; }
    bool canSeek() const { return m_canSeek; }
    bool hasShuffle() const { return m_hasShuffle; }
    bool shuffle() const { return m_shuffle; }
    // Players that offer no repeat (YouTube in a Firefox-based browser) still
    // get "repeat this track": Lumo seeks back to the start as it ends.
    bool hasLoop() const { return m_hasLoop || m_canSeek; }
    QString loop() const { return m_hasLoop ? m_loop : (m_fakeLoop ? QStringLiteral("Track") : QStringLiteral("None")); }
    QColor color() const { return m_color; }
    bool hasColor() const { return m_color.isValid(); }
    bool hasVolume() const { return m_hasVolume; }
    double volume() const { return m_volume; }

    // Position in ms, extrapolated while playing (MPRIS doesn't push it).
    Q_INVOKABLE qint64 position() const;
    Q_INVOKABLE void playPause();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seek(qint64 ms);
    Q_INVOKABLE void toggleShuffle();
    // None → Playlist → Track → None
    Q_INVOKABLE void cycleLoop();
    Q_INVOKABLE void setVolume(double volume);

signals:
    void changed();
    void colorChanged();

private slots:
    void onPropertiesChanged(const QDBusMessage &message);
    void onNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);
    void onSeeked(const QDBusMessage &message);

private:
    void scan();
    void choose();
    void load(const QString &service);
    void call(const QString &method, const QVariantList &args = {});
    // Sets one of the player's own properties (Shuffle, LoopStatus, Volume).
    void setPlayerProperty(const QString &name, const QVariant &value);
    void skip(bool forward);

    struct Player {
        QString status;
        qint64 lastActive = 0;
    };
    QHash<QString, Player> m_players;   // by bus name
    QHash<QString, QString> m_owners;   // unique name → bus name
    QString m_service, m_status, m_title, m_artist, m_art, m_identity, m_trackId;
    qint64 m_length = 0, m_positionAt = 0;
    bool m_canSeek = false;
    bool m_hasShuffle = false, m_shuffle = false, m_hasLoop = false, m_hasVolume = false;
    QString m_loop;
    bool m_fakeLoop = false;
    QTimer *m_loopWatch = nullptr;
    QString m_colorFor;
    QColor m_color;
    QNetworkAccessManager *m_net = nullptr;
    void updateColor();
    void watchLoop();
    double m_volume = 1;
    QElapsedTimer m_since, m_seekedAt;
};
