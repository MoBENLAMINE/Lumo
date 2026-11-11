#pragma once

#include <QDate>
#include <QHash>
#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QVariantList>

class QNetworkAccessManager;

// A month at a glance: public holidays (Nager.Date, by country) and your own
// events from the private calendar links you add (Google Calendar, Outlook,
// iCloud, Nextcloud… any iCal .ics link). Fetched when the Today page opens,
// at most every 30 minutes; nothing runs on a timer.
class Calendar : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList links READ links WRITE setLinks NOTIFY settingsChanged)
    // a two-letter country code for the holidays ("MA", "FR"…); "" for none
    Q_PROPERTY(QString country READ country WRITE setCountry NOTIFY settingsChanged)
    // what the grid shows, and what's on each day of it
    Q_PROPERTY(QDate month READ month WRITE setMonth NOTIFY monthChanged)
    // [{ date, title, time, allDay, holiday }] for the shown month, in order
    Q_PROPERTY(QVariantList events READ events NOTIFY eventsChanged)
    Q_PROPERTY(QString status READ status NOTIFY eventsChanged)
public:
    explicit Calendar(QObject *parent = nullptr);

    QStringList links() const;
    void setLinks(const QStringList &links);
    QString country() const;
    void setCountry(const QString &code);
    QDate month() const { return m_month; }
    void setMonth(const QDate &month);
    QVariantList events() const { return m_events; }
    QString status() const { return m_status; }

    // The Today page is about to show: fetch what's older than 30 minutes.
    Q_INVOKABLE void want();
    Q_INVOKABLE void shiftMonth(int months) { setMonth(m_month.addMonths(months)); }
    Q_INVOKABLE QVariantList eventsOn(const QDate &day) const;

    struct Event {
        QDateTime start;
        bool allDay = false;
        QString title;
        QString rrule;
        QList<QDate> exdates;
    };
    // Public for the tests.
    static QList<Event> parseIcs(const QString &ics);
    static QList<QDate> occurrences(const Event &e, const QDate &from, const QDate &to);

signals:
    void settingsChanged();
    void monthChanged();
    void eventsChanged();

private:
    void fetchLinks();
    void fetchHolidays(int year);
    void rebuild();

    QNetworkAccessManager *m_net = nullptr;
    QDate m_month;
    QList<Event> m_mine;
    QHash<int, QList<QPair<QDate, QString>>> m_holidays;   // year → holidays
    QDateTime m_fetchedAt;
    QVariantList m_events;
    QString m_status;
    int m_pending = 0;
};
