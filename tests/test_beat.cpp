#include <QRandomGenerator>
#include <QTest>
#include <cmath>

#include "Beat.h"

// A little song: a kick on every beat, a hi-hat on every off-beat eighth, a
// bass note held under it, and some noise, at the given tempo.
static QVector<qint16> song(double bpm, double seconds, int rate, int skipEvery = 0)
{
    QVector<qint16> out(int(seconds * rate));
    QRandomGenerator rng(42);
    const double period = 60.0 / bpm;
    for (int i = 0; i < out.size(); ++i) {
        const double t = double(i) / rate;
        const int beat = int(t / period);
        const double inBeat = t - beat * period;
        double x = 0;
        // kick: a falling 90→45 Hz thump, ~120 ms
        if (!(skipEvery && beat % skipEvery == skipEvery - 1) && inBeat < 0.12)
            x += 0.7 * std::exp(-inBeat * 30) * std::sin(2 * M_PI * (90 - 200 * inBeat) * inBeat);
        // hi-hat on the off-beat: short noise
        const double half = inBeat - period / 2;
        if (half >= 0 && half < 0.03)
            x += 0.25 * std::exp(-half * 120) * (rng.generateDouble() * 2 - 1);
        // bass line and a bit of noise all along
        x += 0.12 * std::sin(2 * M_PI * 55 * t) + 0.02 * (rng.generateDouble() * 2 - 1);
        out[i] = qint16(qBound(-1.0, x, 1.0) * 32000);
    }
    return out;
}

// Feeds in uneven chunks, like a pipe does.
static QList<BeatTracker::Event> run(BeatTracker &tracker, const QVector<qint16> &audio)
{
    QList<BeatTracker::Event> beats;
    int at = 0, chunk = 97;
    while (at < audio.size()) {
        const int n = std::min<int>(chunk, audio.size() - at);
        beats += tracker.feed(audio.constData() + at, n);
        at += n;
        chunk = chunk * 7 % 1013 + 50;
    }
    return beats;
}

class TestBeat : public QObject {
    Q_OBJECT
    static constexpr int Rate = 11025;

    // Of the beats after `from`, the share that land within 70 ms of a kick.
    static double onTime(const QList<BeatTracker::Event> &beats, double bpm, double from)
    {
        const double period = 60.0 / bpm;
        int n = 0, good = 0;
        for (const auto &b : beats) {
            if (b.time < from) continue;
            ++n;
            const double off = std::fmod(b.time, period);
            if (std::min(off, period - off) < 0.07) ++good;
        }
        return n ? double(good) / n : 0;
    }

private slots:
    void findsTheTempo_data()
    {
        QTest::addColumn<double>("bpm");
        QTest::newRow("95") << 95.0;
        QTest::newRow("120") << 120.0;
        QTest::newRow("128") << 128.0;
        QTest::newRow("150") << 150.0;
    }
    void findsTheTempo()
    {
        QFETCH(double, bpm);
        BeatTracker tracker(Rate);
        const auto beats = run(tracker, song(bpm, 16, Rate));
        QVERIFY2(std::abs(tracker.bpm() - bpm) < 3, qPrintable(QString::number(tracker.bpm())));
        // Once it has the tempo, one beat per beat, on time: not on the hi-hats.
        int late = 0;
        for (const auto &b : beats) if (b.time >= 6) ++late;
        const int expected = int((16 - 6) * bpm / 60);
        QVERIFY2(std::abs(late - expected) <= 1, qPrintable(QStringLiteral("%1 beats, expected %2").arg(late).arg(expected)));
        QVERIFY(onTime(beats, bpm, 6) > 0.9);
    }
    void keepsTimeThroughAMissingKick()
    {
        BeatTracker tracker(Rate);
        // Every 8th kick is missing: the beat goes on anyway.
        const auto beats = run(tracker, song(120, 16, Rate, 8));
        int late = 0;
        for (const auto &b : beats) if (b.time >= 6) ++late;
        QVERIFY2(std::abs(late - 20) <= 1, qPrintable(QString::number(late)));
        QVERIFY(onTime(beats, 120, 6) > 0.85);
    }
    void bandsFollowTheMusic()
    {
        BeatTracker tracker(Rate);
        run(tracker, song(120, 4, Rate));
        // the kicks and the bass line live in the low bands
        QVERIFY(tracker.bands()[0] > 0.05 || tracker.bands()[1] > 0.05);
        for (int b = 0; b < BeatTracker::Bands; ++b)
            QVERIFY(tracker.bands()[b] >= 0 && tracker.bands()[b] <= 1);
        run(tracker, QVector<qint16>(Rate * 3, 0));
        for (int b = 0; b < BeatTracker::Bands; ++b)
            QVERIFY2(tracker.bands()[b] < 0.02, qPrintable(QString::number(tracker.bands()[b])));
    }
    void silenceHasNoBeat()
    {
        BeatTracker tracker(Rate);
        QVector<qint16> quiet(Rate * 6, 0);
        QVERIFY(run(tracker, quiet).isEmpty());
        QCOMPARE(tracker.bpm(), 0.0);
    }
    void stopsWhenTheMusicStops()
    {
        BeatTracker tracker(Rate);
        QVector<qint16> audio = song(120, 10, Rate);
        audio += QVector<qint16>(Rate * 6, 0);
        const auto beats = run(tracker, audio);
        for (const auto &b : beats)
            QVERIFY2(b.time < 10 + 3, qPrintable(QString::number(b.time)));
    }
};

QTEST_MAIN(TestBeat)
#include "test_beat.moc"
