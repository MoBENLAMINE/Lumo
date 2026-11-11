#include <QTest>

#include "Notifications.h"

class TestNotifications : public QObject {
    Q_OBJECT
private slots:
    void parsesNotifyCalls() {
        QByteArray buf =
            "method call time=1.2 sender=:1.40 -> destination=:1.12 serial=7 path=/org/freedesktop/Notifications; "
            "interface=org.freedesktop.Notifications; member=Notify\n"
            "   string \"Thunderbird\"\n   uint32 0\n   string \"mail\"\n   string \"New mail\"\n"
            "   string \"From Sara:\nSee you at 5\"\n   array [\n   ]\n   array [\n   ]\n   int32 -1\n"
            "method call time=1.3 sender=:1.41 -> destination=:1.12 serial=8 path=/org/freedesktop/Notifications; "
            "interface=org.freedesktop.Notifications; member=Notify\n"
            "   string \"Discord\"\n   uint32 0\n   string \"\"\n";
        const auto first = Notifications::parse(buf);
        QCOMPARE(first.size(), 1);
        QCOMPARE(first[0].app, QStringLiteral("Thunderbird"));
        QCOMPARE(first[0].summary, QStringLiteral("New mail"));
        QCOMPARE(first[0].body, QStringLiteral("From Sara:\nSee you at 5"));
        // the second one is cut in the middle: it waits for the rest
        buf += "   string \"Ping\"\n   string \"hey\"\n   array [\n";
        const auto second = Notifications::parse(buf);
        QCOMPARE(second.size(), 1);
        QCOMPARE(second[0].app, QStringLiteral("Discord"));
        QCOMPARE(second[0].summary, QStringLiteral("Ping"));
        QCOMPARE(second[0].body, QStringLiteral("hey"));
        QVERIFY(Notifications::parse(buf).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestNotifications)
#include "test_notifications.moc"
