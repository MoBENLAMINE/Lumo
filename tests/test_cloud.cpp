// Cloud sessions: the list parser, and how the hub shows and sounds them.
#include <QDateTime>
#include <QTimeZone>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

#include "AgentHub.h"
#include "CloudSessions.h"

namespace {
qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }
QString isoAt(qint64 ms) { return QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::utc()).toString(Qt::ISODateWithMs); }

CloudSession cs(const QString &id, const QString &status, qint64 updated = nowMs(), const QString &repo = {}) {
    return CloudSession{id, QStringLiteral("Task ") + id, status, repo, QStringLiteral("https://claude.ai/code/") + id, updated};
}

QJsonObject item(const QString &id, const QString &status, const QString &updated) {
    QJsonObject o{{QStringLiteral("id"), id}, {QStringLiteral("session_status"), status}, {QStringLiteral("title"), QStringLiteral("T")}};
    if (!updated.isEmpty()) o.insert(QStringLiteral("updated_at"), updated);
    return o;
}
}

class TestCloud : public QObject {
    Q_OBJECT

    static QString listOf(const QJsonArray &data) {
        return QString::fromUtf8(QJsonDocument(QJsonObject{{QStringLiteral("data"), data}}).toJson(QJsonDocument::Compact));
    }

    static int rowOf(AgentHub &hub, const QString &id) { return hub.sessions()->indexOf(QStringLiteral("cloud:") + id); }

private slots:
    void parsesTheList() {
        const QString now = isoAt(nowMs());
        QJsonObject withRepo = item(QStringLiteral("session_a"), QStringLiteral("running"), now);
        withRepo.insert(QStringLiteral("sources"),
                        QJsonArray{QJsonObject{{QStringLiteral("type"), QStringLiteral("git_repository")},
                                               {QStringLiteral("url"), QStringLiteral("https://github.com/acme/web.git")}}});
        QJsonObject createdOnly = item(QStringLiteral("session_b"), QStringLiteral("idle"), QString());
        createdOnly.insert(QStringLiteral("created_at"), now);

        const QJsonArray data{withRepo,
                              item(QStringLiteral("session_c"), QStringLiteral("archived"), now),         // dropped: archived
                              item(QStringLiteral("../etc"), QStringLiteral("idle"), now),               // dropped: bad id
                              item(QStringLiteral("session_d"), QStringLiteral("idle"), QString()),       // dropped: no date
                              item(QStringLiteral("session_e"), QStringLiteral("sleeping"), now),        // dropped: unknown status
                              createdOnly};
        const auto out = parseCloudSessions(listOf(data).toUtf8());
        QCOMPARE(out.size(), 2);
        QCOMPARE(out[0].id, QStringLiteral("session_a"));
        QCOMPARE(out[0].repo, QStringLiteral("acme/web"));
        QCOMPARE(out[0].url, QStringLiteral("https://claude.ai/code/session_a"));
        QCOMPARE(out[0].status, QStringLiteral("running"));
        QVERIFY(qAbs(out[0].updatedAt - nowMs()) < 60'000);
        QCOMPARE(out[1].id, QStringLiteral("session_b"));
        QVERIFY(out[1].repo.isEmpty());
        QVERIFY(out[1].updatedAt > 0);
    }

    void readsTheServerFormat() {
        // What the server sends: prefixed status, microsecond timestamps.
        const QJsonArray data{QJsonObject{{QStringLiteral("id"), QStringLiteral("session_018WoC1YAV5RmYjptj1beY84")},
                                          {QStringLiteral("session_status"), QStringLiteral("SESSION_STATUS_REQUIRES_ACTION")},
                                          {QStringLiteral("title"), QStringLiteral("Gestion des risques du Projet")},
                                          {QStringLiteral("updated_at"), QStringLiteral("2026-11-03T08:30:03.914844Z")}}};
        const auto out = parseCloudSessions(listOf(data).toUtf8());
        QCOMPARE(out.size(), 1);
        QCOMPARE(out[0].status, QStringLiteral("requires_action"));
        QCOMPARE(out[0].updatedAt, QDateTime::fromString(QStringLiteral("2026-11-03T08:30:03.914Z"), Qt::ISODateWithMs).toMSecsSinceEpoch());
    }

    void emptyOrBrokenBodiesGiveNothing() {
        QVERIFY(parseCloudSessions("{\"data\":[]}").isEmpty());
        QVERIFY(parseCloudSessions("not json").isEmpty());
        QVERIFY(parseCloudSessions("{\"error\":\"nope\"}").isEmpty());
    }

    void showsActiveAndRecentOnly() {
        AgentHub hub;
        hub.setCloudSessions({cs(QStringLiteral("a"), QStringLiteral("running"), nowMs(), QStringLiteral("acme/web")),
                              cs(QStringLiteral("b"), QStringLiteral("requires_action")),
                              cs(QStringLiteral("c"), QStringLiteral("idle"), nowMs() - 2 * 3600 * 1000), // settled 2 h ago
                              cs(QStringLiteral("d"), QStringLiteral("idle"))});
        QCOMPARE(hub.sessions()->count(), 3);
        QCOMPARE(rowOf(hub, QStringLiteral("c")), -1);

        const QVariantMap a = hub.sessions()->row(rowOf(hub, QStringLiteral("a")));
        QCOMPARE(a.value(QStringLiteral("status")).toString(), QStringLiteral("working"));
        QCOMPARE(a.value(QStringLiteral("project")).toString(), QStringLiteral("web"));
        QCOMPARE(a.value(QStringLiteral("agent")).toString(), QStringLiteral("claude-cloud"));
        QCOMPARE(a.value(QStringLiteral("agentName")).toString(), QStringLiteral("Claude (cloud)"));
        QCOMPARE(a.value(QStringLiteral("url")).toString(), QStringLiteral("https://claude.ai/code/a"));
        QCOMPARE(hub.sessions()->row(rowOf(hub, QStringLiteral("b")))
                     .value(QStringLiteral("status")).toString(), QStringLiteral("waiting"));
        QCOMPARE(hub.sessions()->row(rowOf(hub, QStringLiteral("d")))
                     .value(QStringLiteral("status")).toString(), QStringLiteral("idle"));
        // Cloud sessions with no repository show their title.
        QCOMPARE(hub.sessions()->row(rowOf(hub, QStringLiteral("d"))).value(QStringLiteral("project")).toString(), QStringLiteral("Task d"));
    }

    void soundsOnlyForChangesWeSaw() {
        AgentHub hub;
        QSignalSpy finished(&hub, &AgentHub::sessionFinished);
        QSignalSpy waiting(&hub, &AgentHub::waitingForYou);
        QSignalSpy failed(&hub, &AgentHub::sessionFailed);

        // Already waiting when Lumo starts: no sound.
        hub.setCloudSessions({cs(QStringLiteral("x"), QStringLiteral("requires_action"))});
        QCOMPARE(waiting.count(), 0);

        hub.setCloudSessions({cs(QStringLiteral("y"), QStringLiteral("running"))});
        hub.setCloudSessions({cs(QStringLiteral("y"), QStringLiteral("idle"))});
        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.first().first().toString(), QStringLiteral("claude-cloud"));

        hub.setCloudSessions({cs(QStringLiteral("y"), QStringLiteral("running")), cs(QStringLiteral("x"), QStringLiteral("requires_action"))});
        hub.setCloudSessions({cs(QStringLiteral("y"), QStringLiteral("requires_action")), cs(QStringLiteral("x"), QStringLiteral("requires_action"))});
        QCOMPARE(waiting.count(), 1); // y now needs you; x was already waiting

        hub.setCloudSessions({cs(QStringLiteral("y"), QStringLiteral("failed"))});
        QCOMPARE(failed.count(), 1);
    }

    void leavesWhenTheListDropsIt() {
        AgentHub hub;
        hub.setCloudSessions({cs(QStringLiteral("a"), QStringLiteral("running")), cs(QStringLiteral("b"), QStringLiteral("idle"))});
        QCOMPARE(hub.sessions()->count(), 2);
        hub.setCloudSessions({cs(QStringLiteral("b"), QStringLiteral("idle"))});
        QCOMPARE(rowOf(hub, QStringLiteral("a")), -1);
        hub.setCloudSessions({});
        QCOMPARE(hub.sessions()->count(), 0);
    }

    void dismissedStaysClosedUntilItChanges() {
        AgentHub hub;
        hub.setCloudSessions({cs(QStringLiteral("a"), QStringLiteral("idle"))});
        hub.dismissSession(QStringLiteral("cloud:a"));
        QCOMPARE(hub.sessions()->count(), 0);

        hub.setCloudSessions({cs(QStringLiteral("a"), QStringLiteral("idle"))}); // still the same
        QCOMPARE(hub.sessions()->count(), 0);

        hub.setCloudSessions({cs(QStringLiteral("a"), QStringLiteral("running"))}); // moved on: back
        QCOMPARE(rowOf(hub, QStringLiteral("a")) >= 0, true);
    }
};

QTEST_GUILESS_MAIN(TestCloud)
#include "test_cloud.moc"
