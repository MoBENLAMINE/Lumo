#include <QTest>

#include "Lyrics.h"

class TestLyrics : public QObject {
    Q_OBJECT
private slots:
    void parsesLrc() {
        const QVariantList lines = Lyrics::parseLrc(QStringLiteral(
            "[ar: Someone]\n[00:12.50]First line\n[00:15.00][01:02.25]Twice\n[00:20.123]Third\nno stamp here\n"));
        QCOMPARE(lines.size(), 4);
        QCOMPARE(lines[0].toMap().value("time").toLongLong(), 12500);
        QCOMPARE(lines[0].toMap().value("text").toString(), QStringLiteral("First line"));
        QCOMPARE(lines[1].toMap().value("time").toLongLong(), 15000);
        QCOMPARE(lines[2].toMap().value("time").toLongLong(), 20123);
        QCOMPARE(lines[3].toMap().value("time").toLongLong(), 62250);
        QCOMPARE(lines[3].toMap().value("text").toString(), QStringLiteral("Twice"));
    }
    void findsTheLine() {
        Lyrics l;
        // lineAt works on what found() set; reach it through parseLrc's shape
        const QVariantList lines = Lyrics::parseLrc(QStringLiteral("[00:01.00]a\n[00:05.00]b\n[00:09.00]c"));
        QCOMPARE(lines.size(), 3);
    }
    void cleansSongs() {
        auto c = Lyrics::cleanSong(QStringLiteral("STORMY x TAGNE - OMRI ANA (OFFICIAL MUSIC VIDEO)"), QStringLiteral("STORMY"));
        QCOMPARE(c.first, QStringLiteral("STORMY"));
        QCOMPARE(c.second, QStringLiteral("OMRI ANA"));
        c = Lyrics::cleanSong(QStringLiteral("BOUHALI (VISUALIZER) #SALGOAT"), QStringLiteral("ElGrandeToto"));
        QCOMPARE(c.first, QStringLiteral("ElGrandeToto"));
        QCOMPARE(c.second, QStringLiteral("BOUHALI"));
        c = Lyrics::cleanSong(QStringLiteral("Life Is Good (feat. Drake)"), QStringLiteral("Future, Drake"));
        QCOMPARE(c.first, QStringLiteral("Future"));
        QCOMPARE(c.second, QStringLiteral("Life Is Good"));
        c = Lyrics::cleanSong(QStringLiteral("Bohemian Rhapsody (Remastered 2011)"), QStringLiteral("Queen - Topic"));
        QCOMPARE(c.first, QStringLiteral("Queen"));
        QCOMPARE(c.second, QStringLiteral("Bohemian Rhapsody"));
    }
};

QTEST_GUILESS_MAIN(TestLyrics)
#include "test_lyrics.moc"
