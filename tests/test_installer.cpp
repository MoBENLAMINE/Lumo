// HookInstaller against a throwaway $HOME: it must merge into settings.json,
// never replace it, and never write over a file it couldn't parse.
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include "HookInstaller.h"
#include "protocol.h"

class TestInstaller : public QObject {
    Q_OBJECT

    QTemporaryDir m_home;

    QString settings() const { return m_home.path() + QStringLiteral("/.claude/settings.json"); }

    void write(const QByteArray &data) {
        QDir().mkpath(QFileInfo(settings()).absolutePath());
        QFile f(settings());
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(data);
    }

    QByteArray contents() const {
        QFile f(settings());
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }

    QJsonObject json() const { return QJsonDocument::fromJson(contents()).object(); }

private slots:
    void initTestCase() {
        QVERIFY(m_home.isValid());
        qputenv("HOME", m_home.path().toUtf8());
        qunsetenv("XDG_DATA_HOME");
        qunsetenv("XDG_CONFIG_HOME");
    }

    void init() {
        QDir(m_home.path()).removeRecursively();
        QDir().mkpath(m_home.path());
    }

    void malformedSettingsAreLeftAlone() {
        const QByteArray broken = "{ \"model\": \"opus\", oops }";
        write(broken);
        HookInstaller installer;
        installer.prepare(true, false, true);
        QVERIFY(installer.preview().isEmpty());
        QVERIFY(installer.message().contains(QStringLiteral("valid JSON")));
        QVERIFY(!installer.apply());
        QCOMPARE(contents(), broken);
    }

    void fileBrokenAfterPreviewIsLeftAlone() {
        write(R"({"model":"opus"})");
        HookInstaller installer;
        installer.prepare(true, false, false);
        QVERIFY(!installer.preview().isEmpty());
        const QByteArray broken = "not json at all";
        write(broken);
        QVERIFY(!installer.apply());
        QCOMPARE(contents(), broken);
    }

    void installKeepsEverythingElse() {
        write(R"json({"model":"opus","permissions":{"allow":["Bash(ls)"]},
                      "hooks":{"Stop":[{"hooks":[{"type":"command","command":"notify-send done"}]}]}})json");
        HookInstaller installer;
        installer.prepare(true, false, false);
        QVERIFY(installer.apply());

        const QJsonObject s = json();
        QCOMPARE(s.value(QStringLiteral("model")).toString(), QStringLiteral("opus"));
        QCOMPARE(s.value(QStringLiteral("permissions")).toObject().value(QStringLiteral("allow")).toArray().size(), 1);
        const QJsonObject hooks = s.value(QStringLiteral("hooks")).toObject();
        // The user's own Stop hook stays, ours joins it.
        QCOMPARE(hooks.value(QStringLiteral("Stop")).toArray().size(), 2);
        const QJsonObject permission =
            hooks.value(QStringLiteral("PermissionRequest")).toArray().first().toObject().value(QStringLiteral("hooks")).toArray().first().toObject();
        QVERIFY(permission.value(QStringLiteral("command")).toString().contains(QStringLiteral("lumo-hook")));
        QCOMPARE(permission.value(QStringLiteral("timeout")).toInt(), LUMO_PERMISSION_HOOK_TIMEOUT_S);
        QVERIFY(installer.installed());

        // A dated backup of what was there, and the hook deployed and executable.
        QCOMPARE(QDir(QFileInfo(settings()).absolutePath()).entryList({QStringLiteral("settings.json.lumo-backup-*")}).size(), 1);
        const QFileInfo hook(installer.hookPath());
        QVERIFY(hook.isFile());
        QVERIFY(hook.isExecutable());
        QVERIFY(!QFile::exists(installer.hookPath() + QStringLiteral(".new")));

        // Disconnecting takes only ours away.
        installer.prepare(false, false, false);
        QVERIFY(installer.apply());
        const QJsonObject after = json();
        QCOMPARE(after.value(QStringLiteral("model")).toString(), QStringLiteral("opus"));
        const QJsonObject left = after.value(QStringLiteral("hooks")).toObject();
        QCOMPARE(left.keys(), QStringList{QStringLiteral("Stop")});
        QVERIFY(!installer.installed());
    }

    void noSettingsYetIsFine() {
        HookInstaller installer;
        installer.prepare(true, false, false);
        QVERIFY(!installer.preview().isEmpty());
        QVERIFY(installer.apply());
        QVERIFY(json().contains(QStringLiteral("hooks")));
    }
};

QTEST_GUILESS_MAIN(TestInstaller)
#include "test_installer.moc"
