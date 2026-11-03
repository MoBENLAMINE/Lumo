#include "Dictation.h"

#include <QAudioSource>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMediaDevices>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

#include <linux/input-event-codes.h>
#include <algorithm>
#include <cmath>
#include <memory>

#include "ActiveWindow.h"
#include "EvdevKey.h"
#include "Portal.h"
#include "Speech.h"

namespace {
constexpr int kRate = 16000;
constexpr int kTapMs = 280;          // shorter than this is a tap, not a hold
constexpr int kDoubleTapMs = 450;
constexpr int kArmMs = 200;          // Ctrl held this long, alone, starts recording
constexpr int kMaxRecordMs = 10 * 60 * 1000;
constexpr int kBars = 9;
constexpr int kMaxHistory = 100;
const QString kShortcutId = QStringLiteral("dictate");

QString historyPath() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/lumo/dictations.json");
}

int countWords(const QString &s) { return int(s.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts).size()); }

// Runs a short helper command (wl-clipboard, pactl); returns stdout.
QByteArray run(const QString &program, const QStringList &args, const QByteArray &input = {}, bool *ok = nullptr) {
    QProcess p;
    p.start(program, args);
    if (!p.waitForStarted(500)) {
        if (ok) *ok = false;
        return {};
    }
    if (!input.isNull()) p.write(input);
    p.closeWriteChannel();
    p.waitForFinished(800);
    if (ok) *ok = p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
    return p.readAllStandardOutput();
}
}

Dictation::Dictation(ActiveWindow *window, QObject *parent)
    : QObject(parent), m_window(window), m_speech(new Speech(this)), m_vocabulary(new Vocabulary(this)),
      m_remote(new KeyboardRemote(this)) {
    m_enabled = QSettings().value(QStringLiteral("dictation/enabled"), true).toBool();
    m_muteOutput = QSettings().value(QStringLiteral("dictation/muteOutput"), true).toBool();
    for (int i = 0; i < kBars; ++i) m_levels << 0.0;

    connect(m_speech, &Speech::transcribed, this, &Dictation::onTranscribed);
    connect(m_remote, &KeyboardRemote::failed, this, [this](const QString &why) {
        setState(QStringLiteral("done"), tr("Copied — press Ctrl+V (%1)").arg(why));
    });

    // ~30 level updates a second while recording, nothing otherwise.
    m_levelTimer.setInterval(33);
    connect(&m_levelTimer, &QTimer::timeout, this, [this] {
        const double db = m_peak > 0 ? 20 * std::log10(m_peak) : -100;
        const double level = std::clamp((db + 52) / 40, 0.0, 1.0);
        if (level > 0.3) m_heard = true;
        m_levels.removeFirst();
        m_levels << level;
        m_peak = 0;
        emit levelsChanged();
    });
    m_doneTimer.setSingleShot(true);
    connect(&m_doneTimer, &QTimer::timeout, this, [this] { setState(QStringLiteral("idle")); });
    m_armTimer.setSingleShot(true);
    m_armTimer.setInterval(kArmMs);
    connect(&m_armTimer, &QTimer::timeout, this, [this] {
        if (!m_interrupted) startRecording();
    });
    m_maxTimer.setSingleShot(true);
    m_maxTimer.setInterval(kMaxRecordMs);
    connect(&m_maxTimer, &QTimer::timeout, this, [this] { stopRecording(true); });

    loadHistory();
    // Lumo quit (or crashed) while it had silenced the speakers: give the sound back.
    if (QSettings().value(QStringLiteral("dictation/outputMutedByLumo")).toBool()) restoreSpeakers();
    if (m_enabled) setupKey();
}

Dictation::~Dictation() {
    if (m_source) m_source->stop();
}

void Dictation::silenceSpeakers() {
    if (m_outputMuted) return;
    // Only if it isn't muted already: we give back exactly what was there.
    const QByteArray state = run(QStringLiteral("pactl"), {QStringLiteral("get-sink-mute"), QStringLiteral("@DEFAULT_SINK@")}).trimmed();
    if (state.endsWith("yes") || state.endsWith("oui") || state.isEmpty()) return;
    run(QStringLiteral("pactl"), {QStringLiteral("set-sink-mute"), QStringLiteral("@DEFAULT_SINK@"), QStringLiteral("1")});
    m_outputMuted = true;
    QSettings().setValue(QStringLiteral("dictation/outputMutedByLumo"), true);
}

void Dictation::restoreSpeakers() {
    if (!m_outputMuted && !QSettings().value(QStringLiteral("dictation/outputMutedByLumo")).toBool()) return;
    QProcess::startDetached(QStringLiteral("pactl"), {QStringLiteral("set-sink-mute"), QStringLiteral("@DEFAULT_SINK@"), QStringLiteral("0")});
    m_outputMuted = false;
    QSettings().remove(QStringLiteral("dictation/outputMutedByLumo"));
}

void Dictation::setMuteOutput(bool on) {
    if (on == m_muteOutput) return;
    m_muteOutput = on;
    QSettings().setValue(QStringLiteral("dictation/muteOutput"), on);
    emit muteOutputChanged();
}

void Dictation::setEnabled(bool on) {
    if (on == m_enabled) return;
    m_enabled = on;
    QSettings().setValue(QStringLiteral("dictation/enabled"), on);
    if (on && m_keyMode == QLatin1String("none")) setupKey();
    if (!on) cancel();
    emit enabledChanged();
}

// ---------------------------------------------------------------- the key

void Dictation::setupKey() {
    // Left Ctrl alone when the keyboards are readable (user in `input` group)…
    m_evdev = new EvdevKey({KEY_LEFTCTRL}, this);
    if (m_evdev->available()) {
        m_keyMode = QStringLiteral("evdev");
        connect(m_evdev, &EvdevKey::pressed, this, &Dictation::keyDown);
        connect(m_evdev, &EvdevKey::released, this, &Dictation::keyUp);
        connect(m_evdev, &EvdevKey::otherKey, this, [this](int code) {
            if (code == KEY_ESC) {
                cancel();
                return;
            }
            // Ctrl+C, Ctrl+click, Ctrl+scroll…: a shortcut, not dictation.
            m_interrupted = true;
            m_armTimer.stop();
            m_lastTap.invalidate();
            if (m_active && !m_handsFree) cancel();
        });
        emit keyChanged();
        return;
    }
    delete m_evdev;
    m_evdev = nullptr;
    // …otherwise a shortcut held through the desktop's portal.
    m_shortcut = new HoldShortcut(kShortcutId, tr("Hold to dictate"), QStringLiteral("CTRL+ALT+space"), this);
    connect(m_shortcut, &HoldShortcut::pressed, this, &Dictation::keyDown);
    connect(m_shortcut, &HoldShortcut::released, this, &Dictation::keyUp);
    connect(m_shortcut, &HoldShortcut::readyChanged, this, [this](bool ready, const QString &why) {
        m_keyMode = ready ? QStringLiteral("portal") : QStringLiteral("none");
        m_keyProblem = why;
        emit keyChanged();
    });
    m_shortcut->start();
}

QString Dictation::keyLabel() const {
    if (m_keyMode == QLatin1String("evdev")) return tr("Left Ctrl");
    if (m_shortcut) {
        QString t = m_shortcut->trigger();
        return t.replace(QStringLiteral("CTRL"), QStringLiteral("Ctrl")).replace(QStringLiteral("ALT"), QStringLiteral("Alt"))
                .replace(QStringLiteral("space"), QStringLiteral("Space"));
    }
    return {};
}

void Dictation::configureKey() {
    if (m_shortcut) m_shortcut->configure();
}

void Dictation::keyDown() {
    if (!m_enabled) return;
    if (m_active && m_handsFree) { // the press that ends hands-free
        m_handsFree = false;
        stopRecording(true);
        return;
    }
    if (m_active) return;
    m_keyDownAt.start();
    m_interrupted = false;
    // Ctrl alone: wait a beat, so Ctrl+C and friends never open the mic.
    if (m_keyMode == QLatin1String("evdev")) m_armTimer.start();
    else startRecording();
}

void Dictation::keyUp() {
    if (m_armTimer.isActive()) { // a quick tap of Ctrl alone
        m_armTimer.stop();
        if (m_interrupted) return;
        if (m_lastTap.isValid() && m_lastTap.elapsed() < kDoubleTapMs) {
            m_lastTap.invalidate();
            if (startRecording()) {
                m_handsFree = true;
                setState(QStringLiteral("handsfree"));
            }
            return;
        }
        m_lastTap.start();
        return;
    }
    if (!m_active || m_handsFree) return;
    // The desktop shortcut records from the press: a short one is a tap.
    if (m_keyMode != QLatin1String("evdev") && m_keyDownAt.elapsed() < kTapMs) {
        if (m_lastTap.isValid() && m_lastTap.elapsed() < kDoubleTapMs) {
            m_lastTap.invalidate();
            m_handsFree = true;
            setState(QStringLiteral("handsfree"));
            return;
        }
        m_lastTap.start();
        stopRecording(false);
        setState(QStringLiteral("idle"));
        return;
    }
    stopRecording(true);
}

void Dictation::toggle() {
    if (m_active) {
        m_handsFree = false;
        stopRecording(true);
    } else if (startRecording()) {
        m_handsFree = true;
        setState(QStringLiteral("handsfree"));
    }
}

void Dictation::cancel() {
    if (m_active) stopRecording(false);
    ++m_ticket; // drop any transcription still running
    m_handsFree = false;
    setState(QStringLiteral("idle"));
}

// ---------------------------------------------------------------- audio

QAudioDevice Dictation::microphone() const {
    const QList<QAudioDevice> inputs = QMediaDevices::audioInputs();
    // The one picked in Settings, else the system default as PipeWire/Pulse
    // has it (Qt's own idea of the default can be a different mic).
    QByteArray wanted = QSettings().value(QStringLiteral("dictation/microphone")).toByteArray();
    if (wanted.isEmpty()) wanted = run(QStringLiteral("pactl"), {QStringLiteral("get-default-source")}).trimmed();
    for (const auto &d : inputs)
        if (d.id() == wanted) return d;
    return QMediaDevices::defaultAudioInput();
}

QVariantList Dictation::microphones() const {
    QVariantList out;
    for (const auto &d : QMediaDevices::audioInputs())
        out << QVariantMap{{QStringLiteral("id"), QString::fromUtf8(d.id())}, {QStringLiteral("name"), d.description()}};
    return out;
}

QString Dictation::microphoneId() const { return QSettings().value(QStringLiteral("dictation/microphone")).toString(); }

void Dictation::setMicrophoneId(const QString &id) {
    QSettings().setValue(QStringLiteral("dictation/microphone"), id);
    emit microphoneChanged();
}

bool Dictation::startRecording() {
    if (!m_speech->modelPresent()) {
        setState(QStringLiteral("error"), tr("Install the Parakeet model in Settings → Dictation."));
        return false;
    }
    const QAudioDevice device = microphone();
    if (device.isNull()) {
        setState(QStringLiteral("error"), tr("No microphone found."));
        return false;
    }
    m_speech->warmUp(); // loads while you talk
    QAudioFormat wanted;
    wanted.setSampleRate(kRate);
    wanted.setChannelCount(1);
    wanted.setSampleFormat(QAudioFormat::Float);
    m_format = device.isFormatSupported(wanted) ? wanted : device.preferredFormat();

    m_samples.clear();
    m_samples.reserve(kRate * 30);
    m_resamplePos = 0;
    m_peak = 0;
    m_heard = false;
    m_app = m_window ? m_window->appClass() : QString();

    qInfo("dictation: recording from %s", qPrintable(device.description()));
    m_source = new QAudioSource(device, m_format, this);
    m_source->setBufferSize(m_format.bytesForDuration(100'000));
    m_device = m_source->start();
    if (!m_device) {
        delete m_source;
        setState(QStringLiteral("error"), tr("The microphone couldn't be opened."));
        return false;
    }
    connect(m_device, &QIODevice::readyRead, this, &Dictation::readAudio);
    m_active = true;
    m_clock.start();
    m_levelTimer.start();
    m_maxTimer.start();
    m_doneTimer.stop();
    emit sound(QStringLiteral("pop"));
    // Silence the speakers (after the little start sound) so only your voice is heard.
    if (m_muteOutput) QTimer::singleShot(140, this, [this] { if (m_active) silenceSpeakers(); });
    setState(QStringLiteral("listening"));
    return true;
}

void Dictation::readAudio() {
    if (!m_device) return;
    const QByteArray data = m_device->readAll();
    const int bpf = m_format.bytesPerFrame();
    if (bpf <= 0) return;
    const int channels = m_format.channelCount();
    const int bps = m_format.bytesPerSample();
    const qsizetype frames = data.size() / bpf;
    const char *p = data.constData();
    const bool direct = m_format.sampleRate() == kRate && channels == 1 && m_format.sampleFormat() == QAudioFormat::Float;
    if (direct) {
        const auto *f = reinterpret_cast<const float *>(p);
        for (qsizetype i = 0; i < frames; ++i) {
            m_samples.push_back(f[i]);
            m_peak = std::max(m_peak, std::abs(f[i]));
        }
        return;
    }
    // Downmix and resample (linear) to 16 kHz mono.
    const double step = double(m_format.sampleRate()) / kRate;
    auto frameValue = [&](qsizetype i) {
        float v = 0;
        for (int c = 0; c < channels; ++c) v += m_format.normalizedSampleValue(p + i * bpf + c * bps);
        return v / channels;
    };
    while (m_resamplePos < frames - 1) {
        const qsizetype i = qsizetype(m_resamplePos);
        const float t = float(m_resamplePos - i);
        const float v = frameValue(i) * (1 - t) + frameValue(i + 1) * t;
        m_samples.push_back(v);
        m_peak = std::max(m_peak, std::abs(v));
        m_resamplePos += step;
    }
    m_resamplePos -= frames;
}

void Dictation::stopRecording(bool keep) {
    if (!m_active) return;
    if (m_device) readAudio();
    if (m_source) {
        m_source->stop();
        m_source->deleteLater();
    }
    m_active = false;
    restoreSpeakers();
    m_levelTimer.stop();
    m_maxTimer.stop();
    for (auto &l : m_levels) l = 0.0;
    emit levelsChanged();
    if (!keep) return;

    const double seconds = m_samples.size() / double(kRate);
    if (seconds < 0.3 || !m_heard) {
        setState(QStringLiteral("error"), tr("Didn't catch that."));
        return;
    }
    emit sound(QStringLiteral("send"));
    setState(QStringLiteral("transcribing"));
    // A little silence at the end helps the last word through.
    m_samples.insert(m_samples.end(), kRate / 4, 0.f);
    m_speech->transcribe(std::move(m_samples), ++m_ticket);
    m_samples = {};
    m_totalSeconds += seconds;
}

// ---------------------------------------------------------------- result

void Dictation::onTranscribed(quint64 ticket, const QString &heard, const QString &error) {
    if (ticket != m_ticket) return;
    if (!error.isEmpty()) {
        setState(QStringLiteral("error"), error);
        return;
    }
    // A snippet's trigger and nothing else: its whole text, untouched.
    const QString snippet = m_vocabulary->snippet(heard);
    if (!snippet.isEmpty()) {
        finish(snippet);
        return;
    }
    const QString text = m_vocabulary->apply(heard);
    if (text.isEmpty()) {
        setState(QStringLiteral("error"), tr("Didn't catch that."));
        return;
    }
    const QString style = styleFor(m_app);
    if (style == QLatin1String("natural") || style == QLatin1String("polished")) polish(text, style);
    else finish(text);
}

void Dictation::finish(const QString &text) {
    m_lastText = text;
    record(text, 0);
    setState(QStringLiteral("done"));
    paste(text);
}

void Dictation::polish(const QString &text, const QString &style) {
    static const QString natural = QStringLiteral(
        "You clean up dictated text. Remove filler words (um, uh, euh, like), false starts and words said twice; "
        "fix punctuation and capital letters. Keep the speaker's own words, language, tone and meaning. Never "
        "translate, never add anything, never answer or comment: reply with the cleaned text only.");
    static const QString polished = QStringLiteral(
        "You turn dictated text into well-written text. Remove filler words and false starts, fix grammar, "
        "punctuation and capital letters, and make the sentences read smoothly. Keep the same language, tone and "
        "meaning, and keep names, numbers and technical terms exactly. Never translate, never add facts, never "
        "answer or comment: reply with the text only.");
    setState(QStringLiteral("transcribing"), tr("Polishing…"));
    const quint64 ticket = m_ticket;
    auto *proc = new QProcess(this);
    // Your Pro/Max plan, never an API key that would be billed instead.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const char *var : {"ANTHROPIC_API_KEY", "ANTHROPIC_AUTH_TOKEN", "CLAUDE_CODE_USE_BEDROCK", "CLAUDE_CODE_USE_VERTEX"})
        env.remove(QString::fromLatin1(var));
    proc->setProcessEnvironment(env);
    proc->setWorkingDirectory(QDir::homePath());
    // No tools, no hooks (it would show up as a session), no MCP: just the words.
    const QStringList args{QStringLiteral("-p"), QStringLiteral("--output-format"), QStringLiteral("text"),
                           QStringLiteral("--model"), QStringLiteral("haiku"),
                           QStringLiteral("--system-prompt"), style == QLatin1String("polished") ? polished : natural,
                           QStringLiteral("--tools"), QString(), QStringLiteral("--disable-slash-commands"),
                           QStringLiteral("--setting-sources"), QString(), QStringLiteral("--strict-mcp-config")};
    auto settled = std::make_shared<bool>(false);
    // Whatever goes wrong, your words still get pasted, as said.
    auto settle = [this, ticket, text, settled](const QString &polishedText) {
        if (*settled) return;
        *settled = true;
        if (ticket != m_ticket) return; // you have dictated again since
        finish(polishedText.isEmpty() ? text : polishedText);
    };
    connect(proc, &QProcess::finished, this, [proc, settle](int code, QProcess::ExitStatus status) {
        const QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        if (status != QProcess::NormalExit || code != 0) qInfo("dictation: Claude couldn't polish the words (%d)", code);
        settle(status == QProcess::NormalExit && code == 0 ? out : QString());
    });
    connect(proc, &QProcess::errorOccurred, this, [proc, settle](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart) return;
        qInfo("dictation: `claude` isn't installed; pasting as said");
        proc->deleteLater();
        settle(QString());
    });
    QTimer::singleShot(25000, proc, [proc, settle] {
        qInfo("dictation: Claude took too long; pasting as said");
        settle(QString());
        proc->kill();
    });
    proc->start(QStringLiteral("claude"), args);
    proc->write(text.toUtf8());
    proc->closeWriteChannel();
}

void Dictation::paste(const QString &text) {
    // Keep what the clipboard held, if it was text, to give it back after.
    const QString types = QString::fromUtf8(run(QStringLiteral("wl-paste"), {QStringLiteral("--list-types")}));
    QByteArray saved;
    const bool hadText = types.contains(QLatin1String("text/plain")) && !types.contains(QLatin1String("image/"));
    if (hadText) saved = run(QStringLiteral("wl-paste"), {QStringLiteral("--no-newline")});

    bool ok = false;
    run(QStringLiteral("wl-copy"), {QStringLiteral("--type"), QStringLiteral("text/plain;charset=utf-8")}, text.toUtf8(), &ok);
    if (!ok) {
        setState(QStringLiteral("error"), tr("wl-copy is missing (install wl-clipboard)."));
        return;
    }
    const bool terminal = m_window && m_window->isTerminal();
    QTimer::singleShot(60, this, [this, terminal, saved, hadText] {
        if (terminal) m_remote->chord({KEY_LEFTCTRL, KEY_LEFTSHIFT, KEY_V});
        else m_remote->chord({KEY_LEFTCTRL, KEY_V});
        // Give the clipboard back — unless the paste failed and the words are
        // waiting there for a manual Ctrl+V.
        if (hadText)
            QTimer::singleShot(900, this, [this, saved] {
                if (m_remote->ready()) run(QStringLiteral("wl-copy"), {}, saved);
            });
    });
}

void Dictation::pasteLast() {
    if (!m_lastText.isEmpty()) paste(m_lastText);
}

void Dictation::copy(const QString &text) {
    run(QStringLiteral("wl-copy"), {}, text.toUtf8());
}

void Dictation::setState(const QString &s, const QString &message) {
    m_state = s;
    m_message = message;
    if (s == QLatin1String("done")) m_doneTimer.start(2600);
    else if (s == QLatin1String("error")) m_doneTimer.start(3200);
    if (s == QLatin1String("error")) emit sound(QStringLiteral("error"));
    emit stateChanged();
}

// ---------------------------------------------------------------- history

void Dictation::record(const QString &text, double) {
    const int words = countWords(text);
    m_totalWords += words;
    m_appWords[m_app.isEmpty() ? QStringLiteral("other") : m_app] += words;
    m_history.prepend(QVariantMap{{QStringLiteral("text"), text},
                                  {QStringLiteral("app"), m_app},
                                  {QStringLiteral("words"), words},
                                  {QStringLiteral("at"), QDateTime::currentMSecsSinceEpoch()}});
    while (m_history.size() > kMaxHistory) m_history.removeLast();
    QSettings s;
    const QString day = QStringLiteral("dictation/daily/") + QDate::currentDate().toString(Qt::ISODate);
    s.setValue(day, s.value(day, 0).toInt() + words);
    saveHistory();
    emit historyChanged();
}

int Dictation::wordsToday() const {
    return QSettings().value(QStringLiteral("dictation/daily/") + QDate::currentDate().toString(Qt::ISODate), 0).toInt();
}

int Dictation::wordsPerMinute() const { return m_totalSeconds > 5 ? int(m_totalWords / (m_totalSeconds / 60)) : 0; }

namespace {
int dailyCount(const QSettings &s, const QDate &day) {
    return s.value(QStringLiteral("dictation/daily/") + day.toString(Qt::ISODate), 0).toInt();
}
}

int Dictation::streak() const {
    const QSettings s;
    QDate day = QDate::currentDate();
    if (dailyCount(s, day) == 0) day = day.addDays(-1); // today isn't over yet
    int n = 0;
    while (n < 3660 && dailyCount(s, day) > 0) {
        ++n;
        day = day.addDays(-1);
    }
    return n;
}

int Dictation::minutesSaved() const { return std::max(0, int(m_totalWords / 40.0 - m_totalSeconds / 60.0)); }

QVariantList Dictation::apps() const {
    QList<QPair<QString, int>> all;
    for (auto it = m_appWords.cbegin(); it != m_appWords.cend(); ++it) all << qMakePair(it.key(), it.value());
    std::sort(all.begin(), all.end(), [](const auto &a, const auto &b) { return a.second > b.second; });
    QVariantList out;
    for (const auto &[app, words] : all) {
        if (out.size() == 8) break;
        out << QVariantMap{{QStringLiteral("app"), app}, {QStringLiteral("words"), words}};
    }
    return out;
}

QVariantList Dictation::dailyWords(int days) const {
    const QSettings s;
    QVariantList out;
    const QDate today = QDate::currentDate();
    for (int i = days - 1; i >= 0; --i) {
        const QDate day = today.addDays(-i);
        out << QVariantMap{{QStringLiteral("date"), day.toString(Qt::ISODate)},
                           {QStringLiteral("day"), QLocale().dayName(day.dayOfWeek(), QLocale::NarrowFormat)},
                           {QStringLiteral("words"), dailyCount(s, day)}};
    }
    return out;
}

QString Dictation::style() const { return QSettings().value(QStringLiteral("dictation/style"), QStringLiteral("said")).toString(); }

void Dictation::setStyle(const QString &style) {
    if (style == this->style()) return;
    QSettings().setValue(QStringLiteral("dictation/style"), style);
    emit styleChanged();
}

QVariantMap Dictation::appStyles() const {
    return QJsonDocument::fromJson(QSettings().value(QStringLiteral("dictation/appStyles")).toByteArray()).object().toVariantMap();
}

void Dictation::setAppStyle(const QString &app, const QString &style) {
    QVariantMap styles = appStyles();
    if (style.isEmpty()) styles.remove(app);
    else styles.insert(app, style);
    QSettings().setValue(QStringLiteral("dictation/appStyles"), QJsonDocument(QJsonObject::fromVariantMap(styles)).toJson(QJsonDocument::Compact));
    emit styleChanged();
}

QString Dictation::styleFor(const QString &app) const {
    const QString own = appStyles().value(app).toString();
    return own.isEmpty() ? style() : own;
}

void Dictation::loadHistory() {
    QFile f(historyPath());
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    m_history = o.value(QStringLiteral("history")).toArray().toVariantList();
    m_totalWords = o.value(QStringLiteral("totalWords")).toInt();
    m_totalSeconds = o.value(QStringLiteral("totalSeconds")).toDouble();
    const QJsonObject apps = o.value(QStringLiteral("apps")).toObject();
    for (auto it = apps.begin(); it != apps.end(); ++it) m_appWords.insert(it.key(), it.value().toInt());
    if (!m_history.isEmpty()) m_lastText = m_history.first().toMap().value(QStringLiteral("text")).toString();
}

void Dictation::saveHistory() {
    QDir().mkpath(QFileInfo(historyPath()).absolutePath());
    QSaveFile f(historyPath());
    if (!f.open(QIODevice::WriteOnly)) return;
    QJsonObject apps;
    for (auto it = m_appWords.cbegin(); it != m_appWords.cend(); ++it) apps.insert(it.key(), it.value());
    f.write(QJsonDocument(QJsonObject{{QStringLiteral("history"), QJsonArray::fromVariantList(m_history)},
                                      {QStringLiteral("totalWords"), m_totalWords},
                                      {QStringLiteral("totalSeconds"), m_totalSeconds},
                                      {QStringLiteral("apps"), apps}})
                .toJson(QJsonDocument::Compact));
    f.commit();
}

void Dictation::clearHistory() {
    m_history.clear();
    saveHistory();
    emit historyChanged();
}
