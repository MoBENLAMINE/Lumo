#include "Calendar.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QTimeZone>

#include <algorithm>

namespace {
constexpr int kRefreshSecs = 30 * 60;

QString unescape(QString s) {
    s.replace(QStringLiteral("\\n"), QStringLiteral(" ")).replace(QStringLiteral("\\N"), QStringLiteral(" "));
    s.replace(QStringLiteral("\\,"), QStringLiteral(",")).replace(QStringLiteral("\\;"), QStringLiteral(";"));
    s.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
    return s.trimmed();
}

// DTSTART's value with its parameters: "VALUE=DATE", "TZID=Europe/Paris"…
QDateTime parseTime(const QString &params, const QString &value, bool *allDay) {
    *allDay = params.contains(QLatin1String("VALUE=DATE"), Qt::CaseInsensitive) && !params.contains(QLatin1String("DATE-TIME"));
    if (*allDay || value.size() == 8) {
        *allDay = true;
        return QDateTime(QDate::fromString(value.left(8), QStringLiteral("yyyyMMdd")), QTime(0, 0));
    }
    QDateTime t = QDateTime::fromString(value.left(15), QStringLiteral("yyyyMMdd'T'HHmmss"));
    if (value.endsWith(QLatin1Char('Z'))) {
        t.setTimeZone(QTimeZone::utc());
        return t.toLocalTime();
    }
    const int tz = params.indexOf(QLatin1String("TZID="), 0, Qt::CaseInsensitive);
    if (tz >= 0) {
        const QString id = params.mid(tz + 5).section(QLatin1Char(';'), 0, 0).remove(QLatin1Char('"'));
        const QTimeZone zone(id.toUtf8());
        if (zone.isValid()) { t.setTimeZone(zone); return t.toLocalTime(); }
    }
    return t;   // floating: the local time
}
}

Calendar::Calendar(QObject *parent) : QObject(parent), m_month(QDate::currentDate().addDays(1 - QDate::currentDate().day())) {}

QStringList Calendar::links() const { return QSettings().value(QStringLiteral("calendar/links")).toStringList(); }

void Calendar::setLinks(const QStringList &links) {
    QStringList clean;
    for (QString l : links) {
        l = l.trimmed();
        if (l.startsWith(QLatin1String("webcal://"))) l.replace(0, 6, QStringLiteral("https"));
        if (!l.isEmpty()) clean.append(l);
    }
    QSettings().setValue(QStringLiteral("calendar/links"), clean);
    emit settingsChanged();
    m_fetchedAt = {};
    want();
}

QString Calendar::country() const {
    const QString saved = QSettings().value(QStringLiteral("calendar/country"), QStringLiteral("auto")).toString();
    if (saved != QLatin1String("auto")) return saved;
    return QLocale::territoryToCode(QLocale().territory());
}

void Calendar::setCountry(const QString &code) {
    QSettings().setValue(QStringLiteral("calendar/country"), code.toUpper());
    m_holidays.clear();
    emit settingsChanged();
    fetchHolidays(m_month.year());
}

void Calendar::setMonth(const QDate &month) {
    const QDate first(month.year(), month.month(), 1);
    if (first == m_month) return;
    m_month = first;
    emit monthChanged();
    if (!m_holidays.contains(first.year())) fetchHolidays(first.year());
    rebuild();
}

void Calendar::want() {
    if (!m_holidays.contains(m_month.year())) fetchHolidays(m_month.year());
    if (m_fetchedAt.isValid() && m_fetchedAt.secsTo(QDateTime::currentDateTime()) < kRefreshSecs) { rebuild(); return; }
    fetchLinks();
}

void Calendar::fetchHolidays(int year) {
    const QString code = country();
    if (code.size() != 2) return;
    if (!m_net) m_net = new QNetworkAccessManager(this);
    m_holidays.insert(year, {});   // asked once per year and country
    QNetworkReply *reply = m_net->get(QNetworkRequest(QUrl(QStringLiteral("https://date.nager.at/api/v3/PublicHolidays/%1/%2").arg(year).arg(code))));
    connect(reply, &QNetworkReply::finished, this, [this, reply, year] {
        reply->deleteLater();
        QList<QPair<QDate, QString>> list;
        for (const auto &v : QJsonDocument::fromJson(reply->readAll()).array()) {
            const QJsonObject h = v.toObject();
            list.append({QDate::fromString(h.value(QStringLiteral("date")).toString(), Qt::ISODate),
                         h.value(QStringLiteral("localName")).toString()});
        }
        m_holidays.insert(year, list);
        rebuild();
    });
}

void Calendar::fetchLinks() {
    const QStringList urls = links();
    m_fetchedAt = QDateTime::currentDateTime();
    if (urls.isEmpty()) { m_mine.clear(); rebuild(); return; }
    if (!m_net) m_net = new QNetworkAccessManager(this);
    auto collected = std::make_shared<QList<Event>>();
    auto left = std::make_shared<int>(urls.size());
    auto failed = std::make_shared<int>(0);
    for (const QString &url : urls) {
        QNetworkRequest request{QUrl(url)};
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply *reply = m_net->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, collected, left, failed] {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError) *collected += parseIcs(QString::fromUtf8(reply->readAll()));
            else ++*failed;
            if (--*left) return;
            m_mine = *collected;
            m_status = *failed ? tr("A calendar link couldn't be read.") : QString();
            rebuild();
        });
    }
}

QList<Calendar::Event> Calendar::parseIcs(const QString &ics) {
    // unfold: a line starting with a space continues the one before
    QStringList lines;
    for (const QString &raw : ics.split(QLatin1Char('\n'))) {
        QString line = raw;
        if (line.endsWith(QLatin1Char('\r'))) line.chop(1);
        if ((line.startsWith(QLatin1Char(' ')) || line.startsWith(QLatin1Char('\t'))) && !lines.isEmpty()) lines.last() += line.mid(1);
        else lines.append(line);
    }
    QList<Event> out;
    Event e;
    bool in = false, cancelled = false;
    for (const QString &line : std::as_const(lines)) {
        if (line == QLatin1String("BEGIN:VEVENT")) { in = true; e = Event(); cancelled = false; continue; }
        if (line == QLatin1String("END:VEVENT")) {
            if (in && e.start.isValid() && !cancelled) out.append(e);
            in = false;
            continue;
        }
        if (!in) continue;
        const int colon = line.indexOf(QLatin1Char(':'));
        if (colon < 0) continue;
        const QString head = line.left(colon), value = line.mid(colon + 1);
        const QString name = head.section(QLatin1Char(';'), 0, 0).toUpper();
        const QString params = head.section(QLatin1Char(';'), 1);
        if (name == QLatin1String("DTSTART")) e.start = parseTime(params, value, &e.allDay);
        else if (name == QLatin1String("SUMMARY")) e.title = unescape(value);
        else if (name == QLatin1String("RRULE")) e.rrule = value;
        else if (name == QLatin1String("STATUS") && value == QLatin1String("CANCELLED")) cancelled = true;
        else if (name == QLatin1String("EXDATE"))
            for (const QString &d : value.split(QLatin1Char(','))) {
                bool allDay = false;
                e.exdates.append(parseTime(params, d, &allDay).date());
            }
    }
    return out;
}

// The days an event falls on between two dates, its repeats included
// (daily, weekly on some weekdays, monthly, yearly; with an interval, a count
// or an end).
QList<QDate> Calendar::occurrences(const Event &e, const QDate &from, const QDate &to) {
    QList<QDate> out;
    const QDate start = e.start.date();
    if (e.rrule.isEmpty()) {
        if (start >= from && start <= to) out.append(start);
        return out;
    }
    QHash<QString, QString> rule;
    for (const QString &part : e.rrule.split(QLatin1Char(';')))
        rule.insert(part.section(QLatin1Char('='), 0, 0).toUpper(), part.section(QLatin1Char('='), 1));
    const QString freq = rule.value(QStringLiteral("FREQ"));
    const int interval = std::max(1, rule.value(QStringLiteral("INTERVAL"), QStringLiteral("1")).toInt());
    const int count = rule.value(QStringLiteral("COUNT")).toInt();
    QDate until;
    if (rule.contains(QStringLiteral("UNTIL"))) until = QDate::fromString(rule.value(QStringLiteral("UNTIL")).left(8), QStringLiteral("yyyyMMdd"));
    static const QStringList dayNames = {QStringLiteral("MO"), QStringLiteral("TU"), QStringLiteral("WE"), QStringLiteral("TH"),
                                         QStringLiteral("FR"), QStringLiteral("SA"), QStringLiteral("SU")};
    QList<int> byDay;   // 1 = Monday
    for (const QString &d : rule.value(QStringLiteral("BYDAY")).split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const int i = dayNames.indexOf(d.right(2).toUpper());
        if (i >= 0) byDay.append(i + 1);
    }
    std::sort(byDay.begin(), byDay.end());

    int n = 0;
    auto emitDay = [&](const QDate &d) -> bool {   // false: stop
        if (d < start) return true;
        if (until.isValid() && d > until) return false;
        if (count && n >= count) return false;
        ++n;
        if (d > to) return false;
        if (d >= from && !e.exdates.contains(d)) out.append(d);
        return true;
    };
    for (int step = 0; step < 5000; ++step) {
        if (freq == QLatin1String("DAILY")) {
            if (!emitDay(start.addDays(qint64(step) * interval))) break;
        } else if (freq == QLatin1String("WEEKLY")) {
            const QDate weekStart = start.addDays(1 - start.dayOfWeek()).addDays(qint64(step) * 7 * interval);
            bool go = true;
            for (int day : byDay.isEmpty() ? QList<int>{start.dayOfWeek()} : byDay)
                if (!(go = emitDay(weekStart.addDays(day - 1)))) break;
            if (!go) break;
        } else if (freq == QLatin1String("MONTHLY")) {
            const QDate m = start.addMonths(step * interval);
            if (m.day() != start.day()) continue;   // the 31st in a short month: skipped, as calendars do
            if (!emitDay(m)) break;
        } else if (freq == QLatin1String("YEARLY")) {
            const QDate y = start.addYears(step * interval);
            if (!emitDay(y)) break;
        } else {
            break;
        }
    }
    return out;
}

void Calendar::rebuild() {
    const QDate from = m_month, to = m_month.addMonths(1).addDays(-1);
    QList<QVariantMap> list;
    for (const auto &h : m_holidays.value(from.year()))
        if (h.first >= from && h.first <= to)
            list.append({{QStringLiteral("date"), h.first}, {QStringLiteral("title"), h.second}, {QStringLiteral("time"), QString()},
                         {QStringLiteral("allDay"), true}, {QStringLiteral("holiday"), true}, {QStringLiteral("sort"), 0}});
    for (const Event &e : std::as_const(m_mine))
        for (const QDate &d : occurrences(e, from, to))
            list.append({{QStringLiteral("date"), d}, {QStringLiteral("title"), e.title},
                         {QStringLiteral("time"), e.allDay ? QString() : QLocale().toString(e.start.time(), QLocale::ShortFormat)},
                         {QStringLiteral("allDay"), e.allDay}, {QStringLiteral("holiday"), false},
                         {QStringLiteral("sort"), e.allDay ? 1 : 2 + e.start.time().msecsSinceStartOfDay()}});
    std::sort(list.begin(), list.end(), [](const QVariantMap &a, const QVariantMap &b) {
        const QDate da = a.value(QStringLiteral("date")).toDate(), db = b.value(QStringLiteral("date")).toDate();
        return da != db ? da < db : a.value(QStringLiteral("sort")).toLongLong() < b.value(QStringLiteral("sort")).toLongLong();
    });
    m_events.clear();
    for (const auto &m : list) m_events.append(m);
    emit eventsChanged();
}

QVariantList Calendar::eventsOn(const QDate &day) const {
    QVariantList out;
    for (const QVariant &v : m_events)
        if (v.toMap().value(QStringLiteral("date")).toDate() == day) out.append(v);
    return out;
}
