// Parakeet through Lumo's Speech class, on a WAV (16 kHz mono s16) given in
// LUMO_TEST_WAV. Skipped when the model or the file is missing.
#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QTest>

#include "Speech.h"

class TestSpeech : public QObject {
    Q_OBJECT
private slots:
    void transcribes() {
        Speech speech;
        if (!speech.modelPresent()) QSKIP("Parakeet model not installed");
        QFile wav(qEnvironmentVariable("LUMO_TEST_WAV"));
        if (!wav.open(QIODevice::ReadOnly)) QSKIP("set LUMO_TEST_WAV");
        const QByteArray data = wav.readAll().mid(44);
        std::vector<float> samples(size_t(data.size() / 2));
        const auto *pcm = reinterpret_cast<const qint16 *>(data.constData());
        for (size_t i = 0; i < samples.size(); ++i) samples[i] = pcm[i] / 32768.f;

        QSignalSpy spy(&speech, &Speech::transcribed);
        QElapsedTimer t;
        t.start();
        speech.transcribe(samples, 7); // cold: includes loading the model
        QVERIFY(spy.wait(60000));
        const qint64 cold = t.elapsed();
        QCOMPARE(spy.at(0).at(0).toULongLong(), 7ull);
        QVERIFY2(spy.at(0).at(2).toString().isEmpty(), qPrintable(spy.at(0).at(2).toString()));
        const QString text = spy.at(0).at(1).toString();

        t.restart();
        speech.transcribe(samples, 8); // warm
        QVERIFY(spy.wait(30000));
        qInfo("audio %.1f s | cold %lld ms | warm %lld ms | \"%s\"", samples.size() / 16000.0, cold, t.elapsed(), qPrintable(text));
        QVERIFY(text.contains(QStringLiteral("dictation"), Qt::CaseInsensitive));
    }
};

QTEST_GUILESS_MAIN(TestSpeech)
#include "test_speech.moc"
