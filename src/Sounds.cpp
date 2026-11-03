#include "Sounds.h"

#include <QAudioDevice>
#include <QDir>
#include <QFile>
#include <QMediaDevices>
#include <QSettings>
#include <QSoundEffect>
#include <QStandardPaths>
#include <QUrl>
#include <QtEndian>

#include <cmath>
#include <functional>
#include <vector>

namespace {
constexpr int kRate = 44100;

struct Note {
    double start, length, freq, freqEnd, gain;
    double vibrato = 0; // Hz of wobble depth
};

// Soft bell-ish voice: sine plus a quiet octave, fast attack, exponential decay.
std::vector<float> render(const std::vector<Note> &notes) {
    double end = 0;
    for (const auto &n : notes) end = std::max(end, n.start + n.length);
    std::vector<float> out(size_t((end + 0.05) * kRate), 0.f);
    for (const auto &n : notes) {
        double phase = 0;
        const size_t from = size_t(n.start * kRate), count = size_t(n.length * kRate);
        for (size_t i = 0; i < count && from + i < out.size(); ++i) {
            const double t = double(i) / kRate, k = t / n.length;
            double f = n.freq * std::pow(n.freqEnd / n.freq, k);
            if (n.vibrato > 0) f += n.vibrato * std::sin(2 * M_PI * 18 * t);
            phase += 2 * M_PI * f / kRate;
            const double env = std::min(1.0, t / 0.006) * std::exp(-4.2 * k);
            out[from + i] += float(n.gain * env * (std::sin(phase) + 0.18 * std::sin(2 * phase) + 0.05 * std::sin(3 * phase)));
        }
    }
    return out;
}

QByteArray wav(const std::vector<float> &samples) {
    QByteArray data;
    data.resize(qsizetype(samples.size() * 2));
    auto *p = reinterpret_cast<qint16 *>(data.data());
    for (size_t i = 0; i < samples.size(); ++i) p[i] = qToLittleEndian(qint16(std::clamp(samples[i], -1.f, 1.f) * 32000));
    QByteArray h;
    auto u32 = [&h](quint32 v) { v = qToLittleEndian(v); h.append(reinterpret_cast<const char *>(&v), 4); };
    auto u16 = [&h](quint16 v) { v = qToLittleEndian(v); h.append(reinterpret_cast<const char *>(&v), 2); };
    h.append("RIFF"); u32(quint32(36 + data.size())); h.append("WAVEfmt ");
    u32(16); u16(1); u16(1); u32(kRate); u32(kRate * 2); u16(2); u16(16);
    h.append("data"); u32(quint32(data.size()));
    return h + data;
}

double note(double semitonesFromA4) { return 440.0 * std::pow(2.0, semitonesFromA4 / 12.0); }

const QHash<QString, std::vector<Note>> &recipes() {
    static const QHash<QString, std::vector<Note>> r = {
        {QStringLiteral("hello"), {{0, .16, note(3), note(3), .35}, {.09, .16, note(7), note(7), .35}, {.18, .3, note(10), note(10), .35}}},
        {QStringLiteral("attention"), {{0, .22, note(12), note(12), .4}, {.16, .4, note(19), note(19), .4}}},
        {QStringLiteral("done"), {{0, .14, note(3), note(3), .3}, {.07, .14, note(7), note(7), .3}, {.14, .14, note(10), note(10), .3}, {.21, .45, note(15), note(15), .35}}},
        {QStringLiteral("error"), {{0, .22, note(5), note(5), .35}, {.14, .4, note(0), note(-1), .35}}},
        {QStringLiteral("poke"), {{0, .18, note(14), note(7), .45, 25}}},
        {QStringLiteral("dizzy"), {{0, .7, note(10), note(2), .35, 60}}},
        // Claude finished: a bright rising chime with a sparkle on top.
        {QStringLiteral("finish"), {{0, .13, note(15), note(15), .30}, {.075, .13, note(19), note(19), .30}, {.15, .13, note(22), note(22), .30},
                                    {.225, .6, note(27), note(27), .36}, {.30, .35, note(39), note(39), .10}, {.39, .3, note(34), note(34), .08}}},
        // Claude needs you: "dum-DING?", twice, the second note bending up like a question.
        {QStringLiteral("needs"), {{0, .22, note(14), note(14), .40}, {.13, .55, note(21), note(22.5), .45},
                                   {.78, .2, note(14), note(14), .22}, {.91, .45, note(21), note(22.5), .25}}},
        {QStringLiteral("pop"), {{0, .07, note(17), note(24), .25}}},
        {QStringLiteral("send"), {{0, .12, note(7), note(19), .25}}},
        {QStringLiteral("reply"), {{0, .1, note(14), note(14), .2}, {.07, .16, note(19), note(19), .2}}},
    };
    return r;
}
}

Sounds::Sounds(QObject *parent) : QObject(parent) {
    QSettings s;
    m_muted = s.value(QStringLiteral("sound/muted"), false).toBool();
    m_volume = s.value(QStringLiteral("sound/volume"), 0.6).toDouble();
    const QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/sounds");
    const QString custom = QDir::homePath() + QStringLiteral("/.config/lumo/sounds");
    QDir().mkpath(cache);
    const auto &all = recipes();
    for (auto it = all.begin(); it != all.end(); ++it) {
        QString path = custom + QLatin1Char('/') + it.key() + QStringLiteral(".wav");
        if (!QFile::exists(path)) {
            path = cache + QLatin1Char('/') + it.key() + QStringLiteral(".wav");
            QFile f(path);
            if (f.open(QIODevice::WriteOnly)) f.write(wav(render(it.value())));
        }
        auto *fx = new QSoundEffect(this);
        fx->setSource(QUrl::fromLocalFile(path));
        fx->setVolume(float(m_volume));
        m_effects.insert(it.key(), fx);
    }
    // A new default output (the sound server came up, headphones were
    // plugged in, you picked another one): the sounds go there.
    m_devices = new QMediaDevices(this);
    connect(m_devices, &QMediaDevices::audioOutputsChanged, this, &Sounds::followDefaultOutput);
    followDefaultOutput();
}

void Sounds::followDefaultOutput() {
    const QAudioDevice out = QMediaDevices::defaultAudioOutput();
    if (out.isNull()) return; // no output yet: keep what we have until one shows up
    for (auto *fx : std::as_const(m_effects))
        if (fx->audioDevice() != out) fx->setAudioDevice(out);
}

void Sounds::setMuted(bool m) {
    if (m == m_muted) return;
    m_muted = m;
    QSettings().setValue(QStringLiteral("sound/muted"), m);
    emit mutedChanged();
}

void Sounds::setVolume(double v) {
    m_volume = std::clamp(v, 0.0, 1.0);
    for (auto *fx : std::as_const(m_effects)) fx->setVolume(float(m_volume));
    QSettings().setValue(QStringLiteral("sound/volume"), m_volume);
    emit volumeChanged();
}

void Sounds::play(const QString &name) {
    if (m_muted) return;
    auto *fx = m_effects.value(name);
    if (!fx) return;
    // The default output may have changed without a notice reaching us.
    followDefaultOutput();
    fx->play();
}
