#pragma once

#include <QObject>
#include <QVariantList>

class QNetworkAccessManager;
class QNetworkReply;

// Time-synced lyrics for what's playing, from LRCLIB (free, open, no
// account). Looked up once per song, only while the fullscreen player shows
// them, and kept on disk for the last 300 songs so a replay needs no network.
class Lyrics : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    // [{ time (ms), text }] in order; empty while looking or when none
    Q_PROPERTY(QVariantList lines READ lines NOTIFY linesChanged)
    // looking | synced | plain | none | off
    Q_PROPERTY(QString state READ state NOTIFY linesChanged)
    Q_PROPERTY(QString plain READ plain NOTIFY linesChanged)
    // the song's name without what video sites add ("(Official Video)"…)
    Q_PROPERTY(QString title READ title NOTIFY linesChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY linesChanged)
    // a nudge for lyrics that run early or late, in ms ([ and ] in the player)
    Q_PROPERTY(int offset READ offset WRITE setOffset NOTIFY offsetChanged)
public:
    explicit Lyrics(QObject *parent = nullptr);

    bool enabled() const;
    void setEnabled(bool on);
    bool active() const { return m_active; }
    void setActive(bool on);
    QVariantList lines() const { return m_lines; }
    QString state() const { return m_state; }
    QString plain() const { return m_plain; }
    QString title() const { return m_title; }
    QString artist() const { return m_artist; }
    int offset() const { return m_offset; }
    void setOffset(int ms) { if (ms == m_offset) return; m_offset = ms; emit offsetChanged(); }

    // The song to find lyrics for; the player calls it on every change.
    Q_INVOKABLE void setSong(const QString &title, const QString &artist, qint64 lengthMs);
    // The line playing at this position, or -1 before the first.
    Q_INVOKABLE int lineAt(qint64 positionMs) const;

    // Public for the tests.
    static QVariantList parseLrc(const QString &lrc);
    // "Artist - Title (Official Video) [4K]" → { artist, title }
    static QPair<QString, QString> cleanSong(const QString &title, const QString &artist);

signals:
    void settingsChanged();
    void activeChanged();
    void linesChanged();
    void offsetChanged();

private:
    void lookUp();
    void found(const QString &synced, const QString &plain, bool cache);
    QString cachePath() const;

    bool m_active = false;
    QString m_title, m_artist, m_key, m_lookedUp;
    qint64 m_length = 0;
    QVariantList m_lines;
    QString m_state = QStringLiteral("none"), m_plain;
    int m_offset = 0;
    QNetworkAccessManager *m_net = nullptr;
    QNetworkReply *m_reply = nullptr;
};
