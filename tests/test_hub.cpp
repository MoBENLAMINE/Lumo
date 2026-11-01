// End to end: the real lumo-hook binary talking to HookServer + AgentHub.
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "AgentHub.h"
#include "HookServer.h"

class TestHub : public QObject {
    Q_OBJECT

    QTemporaryDir m_runtime;
    HookServer *m_server = nullptr;
    AgentHub *m_hub = nullptr;

    QString hookPath() const { return QStringLiteral(LUMO_HOOK_PATH); }

    // Runs the hook with `payload`; `onRequest` acts on the island once a request shows up.
    QJsonObject run(const QString &event, const QJsonObject &payload, const std::function<void(const QString &)> &onRequest = {},
                    QByteArray *raw = nullptr, int *ms = nullptr) {
        QProcess hook;
        QElapsedTimer t;
        t.start();
        hook.start(hookPath(), {event});
        hook.write(QJsonDocument(payload).toJson(QJsonDocument::Compact));
        hook.closeWriteChannel();
        if (onRequest) {
            if (!QTest::qWaitFor([&] { return m_hub->requests()->count() > 0; }, 3000)) return {};
            onRequest(m_hub->requests()->row(0).value(QStringLiteral("requestId")).toString());
        }
        hook.waitForFinished(10000);
        if (ms) *ms = int(t.elapsed());
        const QByteArray out = hook.readAllStandardOutput();
        if (raw) *raw = out;
        return QJsonDocument::fromJson(out).object();
    }

    static QJsonObject decision(const QJsonObject &out) {
        return out.value(QStringLiteral("hookSpecificOutput")).toObject().value(QStringLiteral("decision")).toObject();
    }

    static QJsonObject bash(const QString &sid) {
        return {{QStringLiteral("session_id"), sid},
                {QStringLiteral("cwd"), QStringLiteral("/tmp/demo")},
                {QStringLiteral("hook_event_name"), QStringLiteral("PermissionRequest")},
                {QStringLiteral("tool_name"), QStringLiteral("Bash")},
                {QStringLiteral("tool_input"), QJsonObject{{QStringLiteral("command"), QStringLiteral("ls")}}},
                {QStringLiteral("permission_suggestions"),
                 QJsonArray{QJsonObject{{QStringLiteral("type"), QStringLiteral("addRules")},
                                        {QStringLiteral("behavior"), QStringLiteral("allow")},
                                        {QStringLiteral("destination"), QStringLiteral("session")}}}}};
    }

private slots:
    void initTestCase() {
        QVERIFY(m_runtime.isValid());
        QFile::setPermissions(m_runtime.path(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        qputenv("XDG_RUNTIME_DIR", m_runtime.path().toUtf8());
    }

    void init() {
        m_server = new HookServer(this);
        m_hub = new AgentHub(this);
        connect(m_server, &HookServer::hookEvent, m_hub, &AgentHub::onHookEvent);
        QVERIFY(m_server->listen());
    }

    void cleanup() {
        delete m_server;
        delete m_hub;
    }

    void allow() {
        const auto out = run(QStringLiteral("PermissionRequest"), bash(QStringLiteral("s1")), [&](const QString &id) { m_hub->allow(id); });
        QCOMPARE(out.value(QStringLiteral("hookSpecificOutput")).toObject().value(QStringLiteral("hookEventName")).toString(), QStringLiteral("PermissionRequest"));
        QCOMPARE(decision(out).value(QStringLiteral("behavior")).toString(), QStringLiteral("allow"));
        QVERIFY(!decision(out).contains(QStringLiteral("updatedPermissions")));
        QCOMPARE(m_hub->requests()->count(), 0);
    }

    void alwaysAppliesSuggestions() {
        const auto out = run(QStringLiteral("PermissionRequest"), bash(QStringLiteral("s1")), [&](const QString &id) { m_hub->always(id); });
        QCOMPARE(decision(out).value(QStringLiteral("behavior")).toString(), QStringLiteral("allow"));
        QCOMPARE(decision(out).value(QStringLiteral("updatedPermissions")).toArray().size(), 1);
    }

    void deny() {
        const auto out = run(QStringLiteral("PermissionRequest"), bash(QStringLiteral("s1")), [&](const QString &id) { m_hub->deny(id); });
        QCOMPARE(decision(out).value(QStringLiteral("behavior")).toString(), QStringLiteral("deny"));
    }

    void terminalPrintsNothing() {
        QByteArray raw;
        run(QStringLiteral("PermissionRequest"), bash(QStringLiteral("s1")), [&](const QString &id) { m_hub->replyInTerminal(id); }, &raw);
        QVERIFY(raw.trimmed().isEmpty());
    }

    void questionAnswers() {
        const QJsonObject input{{QStringLiteral("questions"),
                                 QJsonArray{QJsonObject{{QStringLiteral("question"), QStringLiteral("Which?")},
                                                        {QStringLiteral("options"), QJsonArray{QJsonObject{{QStringLiteral("label"), QStringLiteral("A")}},
                                                                                               QJsonObject{{QStringLiteral("label"), QStringLiteral("B")}}}}},
                                            QJsonObject{{QStringLiteral("question"), QStringLiteral("Extras?")},
                                                        {QStringLiteral("multiSelect"), true},
                                                        {QStringLiteral("options"), QJsonArray{QJsonObject{{QStringLiteral("label"), QStringLiteral("Tests")}},
                                                                                               QJsonObject{{QStringLiteral("label"), QStringLiteral("Docs")}}}}}}}};
        QJsonObject p = bash(QStringLiteral("s2"));
        p.insert(QStringLiteral("tool_name"), QStringLiteral("AskUserQuestion"));
        p.insert(QStringLiteral("tool_input"), input);
        const auto out = run(QStringLiteral("PermissionRequest"), p, [&](const QString &id) {
            QCOMPARE(m_hub->requests()->row(0).value(QStringLiteral("kind")).toString(), QStringLiteral("question"));
            // A label that wasn't offered is refused; the request stays.
            m_hub->answer(id, {{QStringLiteral("Which?"), QStringLiteral("C")}, {QStringLiteral("Extras?"), QStringList{QStringLiteral("Tests")}}});
            QCOMPARE(m_hub->requests()->count(), 1);
            m_hub->answer(id, {{QStringLiteral("Which?"), QStringLiteral("B")}, {QStringLiteral("Extras?"), QStringList{QStringLiteral("Tests"), QStringLiteral("Docs")}}});
        });
        const QJsonObject updated = decision(out).value(QStringLiteral("updatedInput")).toObject();
        QCOMPARE(decision(out).value(QStringLiteral("behavior")).toString(), QStringLiteral("allow"));
        QCOMPARE(updated.value(QStringLiteral("questions")).toArray(), input.value(QStringLiteral("questions")).toArray());
        QCOMPARE(updated.value(QStringLiteral("answers")).toObject().value(QStringLiteral("Which?")).toString(), QStringLiteral("B"));
        QCOMPARE(updated.value(QStringLiteral("answers")).toObject().value(QStringLiteral("Extras?")).toArray().size(), 2);
    }

    void planApproval() {
        QJsonObject p = bash(QStringLiteral("plan"));
        p.insert(QStringLiteral("tool_name"), QStringLiteral("ExitPlanMode"));
        p.insert(QStringLiteral("tool_input"), QJsonObject{{QStringLiteral("plan"), QStringLiteral("1. Do it\n2. Test it")}});
        const auto out = run(QStringLiteral("PermissionRequest"), p, [&](const QString &id) {
            QCOMPARE(m_hub->requests()->row(0).value(QStringLiteral("kind")).toString(), QStringLiteral("plan"));
            QCOMPARE(m_hub->requests()->row(0).value(QStringLiteral("detail")).toString(), QStringLiteral("1. Do it\n2. Test it"));
            m_hub->keepPlanning(id, QStringLiteral("add tests first"));
        });
        QCOMPARE(decision(out).value(QStringLiteral("behavior")).toString(), QStringLiteral("deny"));
        QVERIFY(decision(out).value(QStringLiteral("message")).toString().contains(QStringLiteral("add tests first")));
    }

    void hookDyingClearsRequest() {
        QProcess hook;
        hook.start(hookPath(), {QStringLiteral("PermissionRequest")});
        hook.write(QJsonDocument(bash(QStringLiteral("s3"))).toJson(QJsonDocument::Compact));
        hook.closeWriteChannel();
        QVERIFY(QTest::qWaitFor([&] { return m_hub->requests()->count() == 1; }, 3000));
        hook.kill(); // the user answered in the terminal
        hook.waitForFinished();
        QVERIFY(QTest::qWaitFor([&] { return m_hub->requests()->count() == 0; }, 3000));
    }

    // Agents that can't take a decision from Lumo (Gemini…) only notify: the
    // hook is gone at once, and the card must still stay up for a while.
    void noticeOutlivesTheHook() {
        QProcess hook;
        hook.start(hookPath(), {QStringLiteral("--agent"), QStringLiteral("gemini"), QStringLiteral("PermissionRequest")});
        hook.write(QJsonDocument(bash(QStringLiteral("g1"))).toJson(QJsonDocument::Compact));
        hook.closeWriteChannel();
        QVERIFY(hook.waitForFinished(2000));
        QVERIFY(QTest::qWaitFor([&] { return m_hub->requests()->count() == 1; }, 3000));
        QTest::qWait(500);
        QCOMPARE(m_hub->requests()->count(), 1);
        QCOMPARE(m_hub->requests()->row(0).value(QStringLiteral("kind")).toString(), QStringLiteral("notice"));
        // Dismissing it is the same call as "answer in the terminal".
        m_hub->replyInTerminal(m_hub->requests()->row(0).value(QStringLiteral("requestId")).toString());
        QCOMPARE(m_hub->requests()->count(), 0);
    }

    void pidsAreForgotten() {
        QProcess hook;
        hook.start(hookPath(), {QStringLiteral("SessionStart")});
        hook.write(R"({"session_id":"p1","hook_event_name":"SessionStart","cwd":"/tmp/p"})");
        hook.closeWriteChannel();
        QVERIFY(QTest::qWaitFor([&] { return !m_hub->sessionPids(QStringLiteral("p1")).isEmpty(); }, 3000));
        m_hub->dismissSession(QStringLiteral("p1"));
        QVERIFY(m_hub->sessionPids(QStringLiteral("p1")).isEmpty());
        hook.waitForFinished();
    }

    void sessionFlow() {
        const QString cwd = QStringLiteral("/home/me/Projets/coucou");
        auto ev = [&](const QString &name, QJsonObject extra) {
            extra.insert(QStringLiteral("session_id"), QStringLiteral("flow"));
            extra.insert(QStringLiteral("cwd"), cwd);
            extra.insert(QStringLiteral("hook_event_name"), name);
            int ms = 0;
            run(name, extra, {}, nullptr, &ms);
            QVERIFY2(ms < 500, "a fire-and-forget hook must never hold the agent");
        };
        ev(QStringLiteral("UserPromptSubmit"), {{QStringLiteral("prompt"), QStringLiteral("hello")}});
        QVERIFY(QTest::qWaitFor([&] { return m_hub->sessions()->count() == 1; }, 2000));
        QCOMPARE(m_hub->sessions()->row(0).value(QStringLiteral("project")).toString(), QStringLiteral("coucou"));
        QCOMPARE(m_hub->mood(), QStringLiteral("working"));
        ev(QStringLiteral("PostToolUse"), {{QStringLiteral("tool_name"), QStringLiteral("Edit")},
                                          {QStringLiteral("tool_input"), QJsonObject{{QStringLiteral("file_path"), QStringLiteral("/a/b.cpp")},
                                                                                     {QStringLiteral("old_string"), QStringLiteral("a\nb\nc")},
                                                                                     {QStringLiteral("new_string"), QStringLiteral("a\nB\nc\nd")}}}});
        QVERIFY(QTest::qWaitFor([&] { return m_hub->sessions()->row(0).value(QStringLiteral("added")).toInt() == 2; }, 2000));
        QCOMPARE(m_hub->sessions()->row(0).value(QStringLiteral("removed")).toInt(), 1);
        QSignalSpy finished(m_hub, &AgentHub::sessionFinished);
        ev(QStringLiteral("Stop"), {});
        QVERIFY(finished.wait(2000) || finished.count() == 1);
        QCOMPARE(m_hub->mood(), QStringLiteral("happy"));
        ev(QStringLiteral("SessionEnd"), {});
        QVERIFY(QTest::qWaitFor([&] { return m_hub->sessions()->count() == 0; }, 2000));
    }

    void promptsAreCleaned() {
        auto prompt = [&](const QString &text) {
            run(QStringLiteral("UserPromptSubmit"), {{QStringLiteral("session_id"), QStringLiteral("clean")},
                                                      {QStringLiteral("hook_event_name"), QStringLiteral("UserPromptSubmit")},
                                                      {QStringLiteral("prompt"), text}});
            QTest::qWait(150);
            return m_hub->sessions()->row(m_hub->sessions()->indexOf(QStringLiteral("clean"))).value(QStringLiteral("prompt")).toString();
        };
        QCOMPARE(prompt(QStringLiteral("Fix the <div> layout")), QStringLiteral("Fix the <div> layout"));
        // Machinery only: the prompt you typed before stays.
        QCOMPARE(prompt(QStringLiteral("<task-notification> <task-id>x</task-id> done </task-notification>")), QStringLiteral("Fix the <div> layout"));
        QCOMPARE(prompt(QStringLiteral("<system-reminder>be nice</system-reminder>Add tests")), QStringLiteral("Add tests"));
    }

    void statusLineFeedsUsage() {
        QProcess hook;
        hook.start(hookPath(), {QStringLiteral("--statusline")});
        hook.write(R"({"session_id":"x","rate_limits":{"five_hour":{"used_percentage":42,"resets_at":1793231400},"seven_day":{"used_percentage":7.5,"resets_at":1793268000}}})");
        hook.closeWriteChannel();
        QVERIFY(QTest::qWaitFor([&] { return m_hub->hasUsage(); }, 3000));
        QCOMPARE(m_hub->fiveHour(), 42.0);
        QCOMPARE(m_hub->sevenDay(), 7.5);
        QCOMPARE(m_hub->fiveHourReset(), 1793231400);
        hook.waitForFinished();
    }

    void noAppMeansNoOpinion() {
        delete m_server;
        m_server = nullptr;
        QByteArray raw;
        int ms = 0;
        run(QStringLiteral("PermissionRequest"), bash(QStringLiteral("s4")), {}, &raw, &ms);
        QVERIFY(raw.trimmed().isEmpty());
        QVERIFY2(ms < 500, "without Lumo the hook must exit at once");
    }
};

QTEST_GUILESS_MAIN(TestHub)
#include "test_hub.moc"
