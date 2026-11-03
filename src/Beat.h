#pragma once

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVector>

// Finds the beat in mono 16-bit audio, fed in any chunk size. Pure: time is
// counted in samples, so the same audio always gives the same beats (tests).
//
// Kicks and bass carry the beat in most music, so each 23 ms frame is reduced
// to its low-band energy; a sudden rise well above the last second and a half
// is an onset. The gaps between onsets give the tempo, and a beat clock that
// snaps to each onset near the expected time keeps going through a soft bar,
// so the beats stay regular even when the onsets are not.
class BeatTracker {
public:
    struct Event {
        double time;      // seconds since the first sample
        double strength;  // 0..1: how hard that beat hit
    };

    explicit BeatTracker(int sampleRate = 11025);

    // Returns the beats that fell in these samples.
    QList<Event> feed(const qint16 *samples, int count);

    double bpm() const { return m_period > 0 ? 60.0 / m_period : 0; }
    double level() const { return m_level; }   // 0..1, smoothed loudness
    double time() const { return double(m_samples) / m_rate; }
    void reset();

private:
    void frame(QList<Event> &out);
    void updateTempo();

    const int m_rate;
    const int m_frameSize;
    double m_alpha;                 // one-pole low-pass for the bass band
    double m_low = 0;
    double m_sumBass = 0, m_sumAll = 0;
    int m_inFrame = 0;
    qint64 m_samples = 0;

    double m_prevEnergy = 0;
    QVector<double> m_flux;         // recent positive rises, ~1.5 s
    int m_fluxAt = 0;
    double m_level = 0, m_peak = 1e-6;

    QVector<double> m_onsets;       // recent onset times
    double m_lastOnset = -10;
    double m_period = 0;            // seconds per beat, 0 until known
    double m_candidate = 0;         // a different tempo heard lately…
    int m_candidateHits = 0;        // …this many times in a row
    double m_lastBeat = -10;
};

// The beat of whatever the computer is playing, for Lumo to dance to.
// While `active`, it listens to the default output's monitor (parec, or
// pw-record) at a low rate, in memory only: nothing is kept or sent. If
// neither tool is there, it keeps a steady tempo instead, so Lumo still moves.
class Beat : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    // True while it hears the real music (false: a steady tempo stands in).
    Q_PROPERTY(bool listening READ listening NOTIFY listeningChanged)
    Q_PROPERTY(double bpm READ bpm NOTIFY tempoChanged)
    Q_PROPERTY(double level READ level NOTIFY tempoChanged)
public:
    explicit Beat(QObject *parent = nullptr);
    ~Beat() override;

    bool active() const { return m_active; }
    void setActive(bool on);
    bool listening() const { return m_listening; }
    double bpm() const;
    double level() const { return m_tracker.level(); }

signals:
    void activeChanged();
    void listeningChanged();
    void tempoChanged();
    // index counts the beats since dancing started; period is the time to
    // the next beat in ms, so a move can last exactly one beat.
    void beat(int index, double strength, int period);

private:
    void start();
    void stop();
    void tryNext();
    void onAudio();
    void setListening(bool on);

    bool m_active = false, m_listening = false;
    QProcess *m_proc = nullptr;
    int m_attempt = 0;
    QByteArray m_pending;
    BeatTracker m_tracker;
    QTimer m_tick;        // keeps a steady tempo when nothing is heard
    QTimer m_retry;
    QElapsedTimer m_sinceReal, m_sinceBeat;
    bool m_heardOnce = false;
    int m_index = 0;
};
