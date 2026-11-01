// Music against a running test player (org.mpris.MediaPlayer2.lumotest).
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QSignalSpy>
#include <QTest>

#include "Music.h"

class TestMusic : public QObject {
    Q_OBJECT
private slots:
    void followsThePlayer() {
        if (!QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.mpris.MediaPlayer2.lumotest")))
            QSKIP("no test player running");
        Music music;
        QVERIFY(music.available());
        QCOMPARE(music.title(), QStringLiteral("Midnight City"));
        QCOMPARE(music.artist(), QStringLiteral("M83"));
        QCOMPARE(music.length(), 243000);
        QVERIFY(music.position() >= 61000);
        const bool wasPlaying = music.playing();
        QSignalSpy changed(&music, &Music::changed);
        music.playPause();
        QVERIFY(changed.wait(2000));
        QCOMPARE(music.playing(), !wasPlaying);
        music.playPause(); // leave it as it was
        QVERIFY(changed.wait(2000));
        QCOMPARE(music.playing(), wasPlaying);
    }
};

QTEST_GUILESS_MAIN(TestMusic)
#include "test_music.moc"
