#pragma once

#include <QDBusMessage>
#include <QElapsedTimer>
#include <QObject>
#include <QStringList>

// What's playing, from any MPRIS player (Spotify, Firefox, Chromium, VLC,
// Elisa…), with play/pause, previous, next and seek. The player that is
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

    // Position in ms, extrapolated while playing (MPRIS doesn't push it).
    Q_INVOKABLE qint64 position() const;
    Q_INVOKABLE void playPause();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seek(qint64 ms);

signals:
    void changed();

private slots:
    void onPropertiesChanged(const QDBusMessage &message);
    void onNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);
    void onSeeked(const QDBusMessage &message);

private:
    void scan();
    void choose();
    void load(const QString &service);
    void call(const QString &method, const QVariantList &args = {});
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
    QElapsedTimer m_since, m_seekedAt;
};
