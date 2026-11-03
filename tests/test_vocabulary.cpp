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
    void snippets() {
        Vocabulary v;
        const QString signOff = QStringLiteral("Best regards,\nMohammed");
        v.addSnippet(QStringLiteral("sign off"), signOff);
        // Whatever the case and punctuation Parakeet gives the trigger.
        QCOMPARE(v.snippet(QStringLiteral("Sign off.")), signOff);
        QCOMPARE(v.snippet(QStringLiteral("  SIGN, OFF! ")), signOff);
        // Only when the trigger is all that was said.
        QVERIFY(v.snippet(QStringLiteral("please sign off now")).isEmpty());
        QVERIFY(v.snippet(QString()).isEmpty());
        // The same trigger again replaces it; kept across restarts.
        v.addSnippet(QStringLiteral("Sign off"), QStringLiteral("Cheers"));
        Vocabulary again;
        QCOMPARE(again.snippets().size(), 1);
        QCOMPARE(again.snippet(QStringLiteral("sign off")), QStringLiteral("Cheers"));
        again.removeSnippet(QStringLiteral("SIGN OFF"));
        QVERIFY(again.snippets().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestVocabulary)
#include "test_vocabulary.moc"
