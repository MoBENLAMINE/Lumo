#pragma once

#include <QHash>
#include <QObject>

class QMediaDevices;
class QSoundEffect;

// Lumo's little voice. Every sound is synthesized at start-up into the cache
// directory (no audio files ship with the app). Drop a WAV with the same name
// in ~/.config/lumo/sounds/ to replace one.
//
// The sounds follow the system's default output: at login Lumo may start
// before the sound server has picked one, and a sound tied to that first
// device stays silent until the output changes.
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

    // hello, greet, finish, needs, attention, done, error, poke, dizzy, pop, send, reply
    Q_INVOKABLE void play(const QString &name);
    // Cuts a sound short (the greeting, when you skip it).
    Q_INVOKABLE void stop(const QString &name);

signals:
    void mutedChanged();
    void volumeChanged();

private:
    // Points every sound at the current default output.
    void followDefaultOutput();

    QHash<QString, QSoundEffect *> m_effects;
    QMediaDevices *m_devices = nullptr;
    bool m_muted = false;
    double m_volume = 0.6;
};
