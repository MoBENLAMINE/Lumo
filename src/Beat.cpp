#include "Beat.h"

#include <QtMath>
#include <algorithm>
#include <cmath>

namespace {
constexpr int Rate = 11025;
constexpr double MinPeriod = 0.33;    // 180 BPM
constexpr double MaxPeriod = 0.75;    // 80 BPM
constexpr int FluxHistory = 64;       // frames, ~1.5 s
constexpr double SteadyPeriod = 60.0 / 112;   // when nothing is heard
}

// ------------------------------------------------------------ BeatTracker

BeatTracker::BeatTracker(int sampleRate)
    : m_rate(sampleRate), m_frameSize(sampleRate >= 22050 ? 512 : 256)
{
    m_alpha = 1.0 - std::exp(-2.0 * M_PI * 150.0 / m_rate);   // bass: below ~150 Hz
    reset();
}

void BeatTracker::reset()
{
    m_low = m_sumBass = m_sumAll = 0;
    m_inFrame = 0;
    m_samples = 0;
    m_prevEnergy = 0;
    m_flux.fill(0, FluxHistory);
    m_fluxAt = 0;
    m_level = 0;
    m_peak = 1e-6;
    m_onsets.clear();
    m_lastOnset = m_lastBeat = -10;
    m_period = m_candidate = 0;
    m_candidateHits = 0;
}

QList<BeatTracker::Event> BeatTracker::feed(const qint16 *samples, int count)
{
    QList<Event> out;
    for (int i = 0; i < count; ++i) {
        const double x = samples[i] / 32768.0;
        m_low += m_alpha * (x - m_low);
        m_sumBass += m_low * m_low;
        m_sumAll += x * x;
        ++m_samples;
        if (++m_inFrame == m_frameSize)
            frame(out);
    }
    return out;
}

void BeatTracker::frame(QList<Event> &out)
{
    const double bass = m_sumBass / m_frameSize;
    const double rms = std::sqrt(m_sumAll / m_frameSize);
    m_sumBass = m_sumAll = 0;
    m_inFrame = 0;
    const double t = time();

    // Loudness, relative to the loudest moment lately.
    m_peak = std::max(rms, m_peak * 0.9995);
    m_level += 0.2 * (std::min(1.0, rms / m_peak) - m_level);

    // A rise in (log) bass energy: log so a quiet song beats like a loud one.
    const double energy = std::log1p(2000.0 * bass);
    const double flux = std::max(0.0, energy - m_prevEnergy);
    m_prevEnergy = energy;

    double mean = 0, var = 0;
    for (double f : std::as_const(m_flux)) mean += f;
    mean /= m_flux.size();
    for (double f : std::as_const(m_flux)) var += (f - mean) * (f - mean);
    const double sd = std::sqrt(var / m_flux.size());
    m_flux[m_fluxAt] = flux;
    m_fluxAt = (m_fluxAt + 1) % m_flux.size();

    const double threshold = mean + 1.5 * sd + 0.05;
    const bool warm = t > 1.0;   // the history needs a second of audio first
    const bool onset = warm && rms > 0.003 && flux > threshold && t - m_lastOnset >= 0.22;
    const double strength = std::clamp((flux - mean) / (4 * sd + 0.15), 0.15, 1.0);

    if (onset) {
        m_lastOnset = t;
        m_onsets.append(t);
        while (!m_onsets.isEmpty() && t - m_onsets.first() > 8.0)
            m_onsets.removeFirst();
        updateTempo();

        if (m_period <= 0) {
            // No tempo yet: every onset far enough apart is a beat.
            if (t - m_lastBeat >= MinPeriod) {
                out.append({t, strength});
                m_lastBeat = t;
            }
        } else if (t - m_lastBeat >= 0.6 * m_period) {
            // On (or near) the beat: snap the clock to it.
            out.append({t, strength});
            m_lastBeat = t;
        }
        // Earlier than that: an off-beat (a hi-hat, a syncopation); it still
        // counted for the tempo, but Lumo stays on the beat.
    } else if (m_period > 0 && t - m_lastBeat >= 1.08 * m_period && t - m_lastOnset < 2.5) {
        // The beat should have come and didn't: keep time through it, softly.
        m_lastBeat += m_period;
        out.append({m_lastBeat, 0.3 * std::max(0.3, m_level)});
    }
}

void BeatTracker::updateTempo()
{
    if (m_onsets.size() < 4)
        return;
    // Every gap to the next and the one after, folded into 80..180 BPM, voted
    // into 10 ms bins with a little spread to the neighbours.
    constexpr int Bins = int((MaxPeriod - MinPeriod) * 100) + 1;
    double votes[Bins] = {};
    QVector<double> gaps;
    for (int i = 0; i < m_onsets.size(); ++i) {
        for (int k = 1; k <= 2 && i + k < m_onsets.size(); ++k) {
            double d = m_onsets[i + k] - m_onsets[i];
            while (d > MaxPeriod) d /= 2;
            while (d < MinPeriod) d *= 2;
            if (d > MaxPeriod) continue;
            gaps.append(d);
            const int b = qBound(0, int(std::lround((d - MinPeriod) * 100)), Bins - 1);
            const double w = k == 1 ? 1.0 : 0.6;
            votes[b] += w;
            if (b > 0) votes[b - 1] += 0.5 * w;
            if (b < Bins - 1) votes[b + 1] += 0.5 * w;
        }
    }
    const int best = int(std::max_element(votes, votes + Bins) - votes);
    if (votes[best] < 2.5)
        return;
    // The mean of the gaps in that bin and its neighbours.
    const double centre = MinPeriod + best / 100.0;
    double sum = 0;
    int n = 0;
    for (double d : std::as_const(gaps))
        if (std::abs(d - centre) <= 0.015) { sum += d; ++n; }
    if (n == 0)
        return;
    const double estimate = sum / n;

    if (m_period <= 0) {
        m_period = estimate;
    } else if (std::abs(estimate - m_period) / m_period < 0.06) {
        m_period += 0.25 * (estimate - m_period);
        m_candidateHits = 0;
    } else if (m_candidate > 0 && std::abs(estimate - m_candidate) / m_candidate < 0.06) {
        // The song changed tempo: follow once it's clearly there.
        if (++m_candidateHits >= 3) {
            m_period = estimate;
            m_candidate = 0;
            m_candidateHits = 0;
        }
    } else {
        m_candidate = estimate;
        m_candidateHits = 1;
    }
}

// ------------------------------------------------------------ Beat

Beat::Beat(QObject *parent)
    : QObject(parent), m_tracker(Rate)
{
    m_tick.setInterval(60);
    connect(&m_tick, &QTimer::timeout, this, [this] {
        // Nothing heard for a while (no capture tool, a muted browser tab…):
        // keep a steady tempo so Lumo still dances.
        if (m_sinceReal.isValid() && m_sinceReal.elapsed() < 2500)
            return;
        if (!m_sinceBeat.isValid() || m_sinceBeat.elapsed() >= SteadyPeriod * 1000) {
            m_sinceBeat.restart();
            emit beat(m_index++, 0.5, int(SteadyPeriod * 1000));
        }
    });
    m_retry.setSingleShot(true);
    m_retry.setInterval(3000);
    connect(&m_retry, &QTimer::timeout, this, [this] { if (m_active) start(); });
}

Beat::~Beat()
{
    stop();
}

void Beat::setActive(bool on)
{
    if (on == m_active)
        return;
    m_active = on;
    if (on)
        start();
    else
        stop();
    emit activeChanged();
}

double Beat::bpm() const
{
    return m_listening && m_tracker.bpm() > 0 ? m_tracker.bpm() : 60.0 / SteadyPeriod;
}

void Beat::start()
{
    m_index = 0;
    m_sinceReal.invalidate();
    m_sinceBeat.invalidate();
    m_tick.start();
    m_attempt = 0;
    tryNext();
}

void Beat::stop()
{
    m_tick.stop();
    m_retry.stop();
    if (m_proc) {
        m_proc->disconnect(this);
        m_proc->kill();
        m_proc->waitForFinished(300);
        m_proc->deleteLater();
        m_proc = nullptr;
    }
    m_pending.clear();
    m_tracker.reset();
    setListening(false);
}

void Beat::tryNext()
{
    // What the speakers play, from the default output's monitor: parec on
    // PulseAudio and PipeWire's pulse layer, pw-record on bare PipeWire.
    struct Tool { QString program; QStringList args; };
    const QList<Tool> tools = {
        {QStringLiteral("parec"),
         {QStringLiteral("--device=@DEFAULT_MONITOR@"), QStringLiteral("--format=s16le"),
          QStringLiteral("--rate=%1").arg(Rate), QStringLiteral("--channels=1"), QStringLiteral("--raw"),
          QStringLiteral("--latency-msec=30"), QStringLiteral("--client-name=Lumo"),
          QStringLiteral("--stream-name=Beat")}},
        {QStringLiteral("pw-record"),
         {QStringLiteral("--rate"), QString::number(Rate), QStringLiteral("--channels"), QStringLiteral("1"),
          QStringLiteral("--format"), QStringLiteral("s16"), QStringLiteral("--latency"), QStringLiteral("30ms"),
          QStringLiteral("-P"), QStringLiteral("{ stream.capture.sink = true node.name = lumo-beat }"),
          QStringLiteral("-")}},
    };
    if (m_attempt >= tools.size()) {
        setListening(false);   // the steady tempo takes over
        return;
    }
    const Tool tool = tools.at(m_attempt++);

    m_heardOnce = false;
    m_tracker.reset();
    m_pending.clear();
    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::SeparateChannels);
    m_proc->setStandardErrorFile(QProcess::nullDevice());
    connect(m_proc, &QProcess::readyReadStandardOutput, this, &Beat::onAudio);
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || !m_proc)
            return;
        m_proc->deleteLater();
        m_proc = nullptr;
        if (m_active) tryNext();
    });
    connect(m_proc, &QProcess::finished, this, [this] {
        if (!m_proc)
            return;
        const bool heard = m_heardOnce;
        m_proc->deleteLater();
        m_proc = nullptr;
        setListening(false);
        if (!m_active)
            return;
        if (heard)
            m_retry.start();   // it was working: the sound server restarted?
        else
            tryNext();
    });
    m_proc->start(tool.program, tool.args, QIODevice::ReadOnly);
}

void Beat::onAudio()
{
    if (!m_proc)
        return;
    m_pending += m_proc->readAllStandardOutput();
    const int count = int(m_pending.size() / 2);
    if (count == 0)
        return;
    const auto events = m_tracker.feed(reinterpret_cast<const qint16 *>(m_pending.constData()), count);
    m_pending.remove(0, count * 2);
    if (!m_heardOnce) {
        m_heardOnce = true;
        setListening(true);
    }
    for (const auto &e : events) {
        m_sinceReal.restart();
        m_sinceBeat.restart();
        const double bpm = m_tracker.bpm();
        emit beat(m_index++, e.strength, bpm > 0 ? int(60000 / bpm) : int(SteadyPeriod * 1000));
    }
    if (!events.isEmpty())
        emit tempoChanged();
}

void Beat::setListening(bool on)
{
    if (on == m_listening)
        return;
    m_listening = on;
    emit listeningChanged();
    emit tempoChanged();
}
