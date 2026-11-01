#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

#include "Vocabulary.h"

class TestVocabulary : public QObject {
    Q_OBJECT
    QTemporaryDir m_dir;
private slots:
    void initTestCase() {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_dir.path());
    }
    void words() {
        Vocabulary v;
        v.addWord(QStringLiteral("Lumo"));
        v.addWord(QStringLiteral("Parakeet"));
        v.addWord(QStringLiteral("Claude Code"));
        QCOMPARE(v.apply(QStringLiteral("hey lumo, open LUMO.")), QStringLiteral("hey Lumo, open Lumo."));
        QCOMPARE(v.apply(QStringLiteral("parakit runs offline")), QStringLiteral("Parakeet runs offline"));
        QCOMPARE(v.apply(QStringLiteral("ask claude code to fix it")), QStringLiteral("ask Claude Code to fix it"));
        // Unrelated words stay as they are.
        QCOMPARE(v.apply(QStringLiteral("a lump of sugar")), QStringLiteral("a lump of sugar"));
    }
    void replacements() {
        Vocabulary v;
        v.addReplacement(QStringLiteral("my email"), QStringLiteral("me@example.com"));
        v.addReplacement(QStringLiteral("my work email"), QStringLiteral("me@work.com"));
        QCOMPARE(v.apply(QStringLiteral("Send it to my email.")), QStringLiteral("Send it to me@example.com."));
        QCOMPARE(v.apply(QStringLiteral("Use My Work Email please")), QStringLiteral("Use me@work.com please"));
        // Kept across restarts.
        Vocabulary again;
        QCOMPARE(again.replacements().size(), 2);
    }
};

QTEST_GUILESS_MAIN(TestVocabulary)
#include "test_vocabulary.moc"
