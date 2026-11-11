#include <QTest>

#include "Calendar.h"

class TestCalendar : public QObject {
    Q_OBJECT
private slots:
    void readsIcs() {
        const auto events = Calendar::parseIcs(QStringLiteral(
            "BEGIN:VCALENDAR\r\nBEGIN:VEVENT\r\nDTSTART;VALUE=DATE:20261111\r\nSUMMARY:Sara's birthday\\, at home\r\nEND:VEVENT\r\n"
            "BEGIN:VEVENT\r\nDTSTART:20261112T090000\r\nSUMMARY:Stand-up with a very long\r\n  title folded\r\n"
            "RRULE:FREQ=WEEKLY;BYDAY=MO,TH;COUNT=4\r\nEND:VEVENT\r\n"
            "BEGIN:VEVENT\r\nDTSTART:20261113T100000\r\nSUMMARY:Cancelled\r\nSTATUS:CANCELLED\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n"));
        QCOMPARE(events.size(), 2);
        QCOMPARE(events[0].title, QStringLiteral("Sara's birthday, at home"));
        QVERIFY(events[0].allDay);
        QCOMPARE(events[0].start.date(), QDate(2026, 11, 11));
        QCOMPARE(events[1].title, QStringLiteral("Stand-up with a very long title folded"));
        QCOMPARE(events[1].start.time(), QTime(9, 0));
        // Thursday the 12th, then Monday 16, Thursday 19, Monday 23: four in all
        const auto days = Calendar::occurrences(events[1], QDate(2026, 11, 1), QDate(2026, 11, 30));
        QCOMPARE(days, (QList<QDate>{QDate(2026, 11, 12), QDate(2026, 11, 16), QDate(2026, 11, 19), QDate(2026, 11, 23)}));
    }
    void repeats() {
        Calendar::Event yearly;
        yearly.start = QDateTime(QDate(1990, 3, 4), QTime(0, 0));
        yearly.allDay = true;
        yearly.rrule = QStringLiteral("FREQ=YEARLY");
        QCOMPARE(Calendar::occurrences(yearly, QDate(2026, 3, 1), QDate(2026, 3, 31)), QList<QDate>{QDate(2026, 3, 4)});
        Calendar::Event monthly;
        monthly.start = QDateTime(QDate(2026, 1, 31), QTime(8, 0));
        monthly.rrule = QStringLiteral("FREQ=MONTHLY;UNTIL=20260601");
        // no 31st in February or April; nothing after the end
        QCOMPARE(Calendar::occurrences(monthly, QDate(2026, 1, 1), QDate(2026, 12, 31)),
                 (QList<QDate>{QDate(2026, 1, 31), QDate(2026, 3, 31), QDate(2026, 5, 31)}));
        Calendar::Event daily;
        daily.start = QDateTime(QDate(2026, 11, 1), QTime(7, 0));
        daily.rrule = QStringLiteral("FREQ=DAILY;INTERVAL=10");
        daily.exdates = {QDate(2026, 11, 11)};
        QCOMPARE(Calendar::occurrences(daily, QDate(2026, 11, 1), QDate(2026, 11, 30)),
                 (QList<QDate>{QDate(2026, 11, 1), QDate(2026, 11, 21)}));
    }
};

QTEST_GUILESS_MAIN(TestCalendar)
#include "test_calendar.moc"
