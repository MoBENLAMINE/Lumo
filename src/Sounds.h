#pragma once

#include <QHash>
#include <QObject>

class QSoundEffect;

// Lumo's little voice. Every sound is synthesized at start-up into the cache
// directory (no audio files ship with the app). Drop a WAV with the same name
// in ~/.config/lumo/sounds/ to replace one.
class Sounds : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
public:
    explicit Sounds(QObject *parent = nullptr);

    bool muted() const { return m_muted; }
    void setMuted(bool m);
    double volume() const { return m_volume; }
    void setVolume(double v);

    // hello, finish, needs, attention, done, error, poke, dizzy, pop, send, reply
    Q_INVOKABLE void play(const QString &name);

signals:
    void mutedChanged();
    void volumeChanged();

private:
    QHash<QString, QSoundEffect *> m_effects;
    bool m_muted = false;
    double m_volume = 0.6;
};
