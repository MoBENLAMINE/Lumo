#pragma once

#include <QAudioDevice>
#include <QAudioFormat>
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantList>
#include <vector>

#include "Speech.h"
#include "Vocabulary.h"

class QAudioSource;
class QIODevice;
class ActiveWindow;
class EvdevKey;
class HoldShortcut;
class KeyboardRemote;


// Flow dictation: hold the key, speak, let go — the words are pasted where
// you type. Two quick taps: hands-free until the next press.
class Dictation : public QObject {
    Q_OBJECT
    // Also on D-Bus (io.github.mobenlamine.lumo /Dictation), so any key or script can drive it.
    Q_CLASSINFO("D-Bus Interface", "io.github.mobenlamine.lumo.Dictation")
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    // idle | listening | handsfree | transcribing | done | error
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString message READ message NOTIFY stateChanged)
    Q_PROPERTY(QString lastText READ lastText NOTIFY stateChanged)
    Q_PROPERTY(QVariantList levels READ levels NOTIFY levelsChanged)
    Q_PROPERTY(int elapsed READ elapsed NOTIFY levelsChanged)
    // "evdev" (left Ctrl alone), "portal" (a desktop shortcut) or "none"
    Q_PROPERTY(QString keyMode READ keyMode NOTIFY keyChanged)
    Q_PROPERTY(QString keyLabel READ keyLabel NOTIFY keyChanged)
    Q_PROPERTY(QString keyProblem READ keyProblem NOTIFY keyChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(int wordsToday READ wordsToday NOTIFY historyChanged)
    Q_PROPERTY(int totalWords READ totalWords NOTIFY historyChanged)
    Q_PROPERTY(int wordsPerMinute READ wordsPerMinute NOTIFY historyChanged)
    Q_PROPERTY(Speech *speech READ speech CONSTANT)
    Q_PROPERTY(Vocabulary *vocabulary READ vocabulary CONSTANT)
    // Silence the computer's sound while listening, so only your voice is heard.
    Q_PROPERTY(bool muteOutput READ muteOutput WRITE setMuteOutput NOTIFY muteOutputChanged)
    // "" follows the system default input.
    Q_PROPERTY(QString microphoneId READ microphoneId WRITE setMicrophoneId NOTIFY microphoneChanged)

public:
    Dictation(ActiveWindow *window, QObject *parent = nullptr);
    ~Dictation() override;

    bool enabled() const { return m_enabled; }
    void setEnabled(bool on);
    QString state() const { return m_state; }
    QString message() const { return m_message; }
    QString lastText() const { return m_lastText; }
    QVariantList levels() const { return m_levels; }
    int elapsed() const { return m_active ? int(m_clock.elapsed()) : 0; }
    QString keyMode() const { return m_keyMode; }
    QString keyLabel() const;
    QString keyProblem() const { return m_keyProblem; }
    QVariantList history() const { return m_history; }
    int wordsToday() const;
    int totalWords() const { return m_totalWords; }
    int wordsPerMinute() const;
    Speech *speech() const { return m_speech; }
    Vocabulary *vocabulary() const { return m_vocabulary; }
    bool muteOutput() const { return m_muteOutput; }
    void setMuteOutput(bool on);

public slots:
    Q_SCRIPTABLE void toggle(); // start hands-free, or finish
    Q_SCRIPTABLE void cancel();
    Q_SCRIPTABLE void pasteLast();

public:
    Q_INVOKABLE void copy(const QString &text);
    Q_INVOKABLE QVariantList microphones() const;
    QString microphoneId() const;
    void setMicrophoneId(const QString &id);
    Q_INVOKABLE void configureKey();
    Q_INVOKABLE void clearHistory();

signals:
    void enabledChanged();
    void stateChanged();
    void levelsChanged();
    void keyChanged();
    void historyChanged();
    void sound(const QString &name);
    void microphoneChanged();
    void muteOutputChanged();

private:
    void setupKey();
    void silenceSpeakers();
    void restoreSpeakers();
    QAudioDevice microphone() const;
    void keyDown();
    void keyUp();
    bool startRecording();
    void stopRecording(bool keep);
    void readAudio();
    void onTranscribed(quint64 ticket, const QString &text, const QString &error);
    void paste(const QString &text);
    void setState(const QString &s, const QString &message = {});
    void record(const QString &text, double seconds);
    void loadHistory();
    void saveHistory();

    ActiveWindow *m_window;
    Speech *m_speech;
    Vocabulary *m_vocabulary;
    EvdevKey *m_evdev = nullptr;
    HoldShortcut *m_shortcut = nullptr;
    KeyboardRemote *m_remote;

    bool m_enabled = true;
    QString m_state = QStringLiteral("idle"), m_message, m_lastText;
    QString m_keyMode = QStringLiteral("none"), m_keyProblem;

    QPointer<QAudioSource> m_source;
    QPointer<QIODevice> m_device;
    QAudioFormat m_format;
    std::vector<float> m_samples;
    double m_resamplePos = 0;
    float m_peak = 0;
    bool m_active = false, m_handsFree = false, m_heard = false;
    QElapsedTimer m_clock, m_keyDownAt, m_lastTap;
    QVariantList m_levels;
    QTimer m_levelTimer, m_doneTimer, m_maxTimer, m_armTimer;
    bool m_interrupted = false;
    bool m_muteOutput = true, m_outputMuted = false;
    quint64 m_ticket = 0;
    QString m_app;

    QVariantList m_history;
    int m_totalWords = 0;
    double m_totalSeconds = 0;
};
