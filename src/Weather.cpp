#include "Weather.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QTimeZone>
#include <QUrlQuery>

#include <cmath>

namespace {
constexpr int kRefreshSecs = 30 * 60;
}

Weather::Weather(QObject *parent) : QObject(parent) {}

bool Weather::enabled() const { return QSettings().value(QStringLiteral("weather/enabled"), true).toBool(); }
void Weather::setEnabled(bool on) { QSettings().setValue(QStringLiteral("weather/enabled"), on); emit settingsChanged(); if (on) want(); }
QString Weather::city() const { return QSettings().value(QStringLiteral("weather/city")).toString(); }
bool Weather::fahrenheit() const {
    // Fahrenheit by default only where it is the custom
    const bool usual = QLocale().measurementSystem() == QLocale::ImperialUSSystem;
    return QSettings().value(QStringLiteral("weather/fahrenheit"), usual).toBool();
}
void Weather::setFahrenheit(bool on) {
    QSettings().setValue(QStringLiteral("weather/fahrenheit"), on);
    emit settingsChanged();
    m_fetchedAt = {};
    want();
}

void Weather::want() {
    if (!enabled() || m_busy) return;
    if (QSettings().value(QStringLiteral("weather/lat")).isNull()) {
        m_status = tr("Type your city in Settings → General → Weather.");
        emit changed();
        return;
    }
    if (m_fetchedAt.isValid() && m_fetchedAt.secsTo(QDateTime::currentDateTime()) < kRefreshSecs) return;
    fetch();
}

void Weather::setCity(const QString &query) {
    if (query.trimmed().isEmpty()) {
        QSettings s;
        s.remove(QStringLiteral("weather/city"));
        s.remove(QStringLiteral("weather/lat"));
        s.remove(QStringLiteral("weather/lon"));
        m_ready = false;
        emit settingsChanged();
        emit changed();
        return;
    }
    if (!m_net) m_net = new QNetworkAccessManager(this);
    // "Paris, Texas": the name goes to the search, the rest picks among the results
    const QString name = query.section(QLatin1Char(','), 0, 0).trimmed();
    const QString hint = query.section(QLatin1Char(','), 1).trimmed().toLower();
    QUrl url(QStringLiteral("https://geocoding-api.open-meteo.com/v1/search"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("name"), name);
    q.addQueryItem(QStringLiteral("count"), QStringLiteral("10"));
    q.addQueryItem(QStringLiteral("language"), QLocale().name().left(2));
    url.setQuery(q);
    QNetworkReply *reply = m_net->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, hint] {
        reply->deleteLater();
        const QJsonArray results = QJsonDocument::fromJson(reply->readAll()).object().value(QStringLiteral("results")).toArray();
        QJsonObject pick;
        for (const auto &v : results) {
            const QJsonObject r = v.toObject();
            const QString where = (r.value(QStringLiteral("admin1")).toString() + QLatin1Char(' ') + r.value(QStringLiteral("country")).toString()
                                   + QLatin1Char(' ') + r.value(QStringLiteral("country_code")).toString()).toLower();
            if (hint.isEmpty() || where.contains(hint)) { pick = r; break; }
        }
        if (pick.isEmpty()) { emit cityResult(false, QString()); return; }
        const QString found = pick.value(QStringLiteral("name")).toString() + QStringLiteral(", ") + pick.value(QStringLiteral("country")).toString();
        QSettings s;
        s.setValue(QStringLiteral("weather/city"), found);
        s.setValue(QStringLiteral("weather/lat"), pick.value(QStringLiteral("latitude")).toDouble());
        s.setValue(QStringLiteral("weather/lon"), pick.value(QStringLiteral("longitude")).toDouble());
        emit settingsChanged();
        emit cityResult(true, found);
        m_fetchedAt = {};
        fetch();
    });
}

void Weather::fetch() {
    if (!m_net) m_net = new QNetworkAccessManager(this);
    QSettings s;
    QUrl url(QStringLiteral("https://api.open-meteo.com/v1/forecast"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("latitude"), QString::number(s.value(QStringLiteral("weather/lat")).toDouble(), 'f', 3));
    q.addQueryItem(QStringLiteral("longitude"), QString::number(s.value(QStringLiteral("weather/lon")).toDouble(), 'f', 3));
    q.addQueryItem(QStringLiteral("current"), QStringLiteral("temperature_2m,weather_code,is_day"));
    q.addQueryItem(QStringLiteral("hourly"), QStringLiteral("temperature_2m,weather_code"));
    q.addQueryItem(QStringLiteral("daily"), QStringLiteral("temperature_2m_max,temperature_2m_min,weather_code"));
    q.addQueryItem(QStringLiteral("forecast_days"), QStringLiteral("6"));
    q.addQueryItem(QStringLiteral("timezone"), QStringLiteral("auto"));
    if (fahrenheit()) q.addQueryItem(QStringLiteral("temperature_unit"), QStringLiteral("fahrenheit"));
    url.setQuery(q);
    m_busy = true;
    QNetworkReply *reply = m_net->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_busy = false;
        if (reply->error() != QNetworkReply::NoError) {
            m_status = tr("The weather can't be reached right now.");
            emit changed();
            return;
        }
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonObject cur = o.value(QStringLiteral("current")).toObject();
        m_temp = int(std::lround(cur.value(QStringLiteral("temperature_2m")).toDouble()));
        m_code = cur.value(QStringLiteral("weather_code")).toInt();
        m_day = cur.value(QStringLiteral("is_day")).toInt(1) == 1;
        const QJsonObject daily = o.value(QStringLiteral("daily")).toObject();
        const QJsonArray dDates = daily.value(QStringLiteral("time")).toArray(), dMax = daily.value(QStringLiteral("temperature_2m_max")).toArray(),
                         dMin = daily.value(QStringLiteral("temperature_2m_min")).toArray(), dCode = daily.value(QStringLiteral("weather_code")).toArray();
        m_days.clear();
        for (int i = 0; i < dDates.size(); ++i)
            m_days.append(QVariantMap{{QStringLiteral("date"), QDate::fromString(dDates[i].toString(), Qt::ISODate)},
                                      {QStringLiteral("high"), int(std::lround(dMax[i].toDouble()))},
                                      {QStringLiteral("low"), int(std::lround(dMin[i].toDouble()))},
                                      {QStringLiteral("code"), dCode[i].toInt()}});
        if (!m_days.isEmpty()) {
            m_high = m_days.first().toMap().value(QStringLiteral("high")).toInt();
            m_low = m_days.first().toMap().value(QStringLiteral("low")).toInt();
        }
        // the next 12 hours, from now (the times are the city's own)
        const QJsonObject hourly = o.value(QStringLiteral("hourly")).toObject();
        const QJsonArray hTimes = hourly.value(QStringLiteral("time")).toArray(), hTemp = hourly.value(QStringLiteral("temperature_2m")).toArray(),
                         hCode = hourly.value(QStringLiteral("weather_code")).toArray();
        const QDateTime cityNow = QDateTime::currentDateTimeUtc().addSecs(o.value(QStringLiteral("utc_offset_seconds")).toInt());
        m_hours.clear();
        for (int i = 0; i < hTimes.size() && m_hours.size() < 12; ++i) {
            QDateTime t = QDateTime::fromString(hTimes[i].toString(), QStringLiteral("yyyy-MM-ddTHH:mm"));
            t.setTimeZone(QTimeZone::utc());
            if (t.addSecs(3600) <= cityNow) continue;
            m_hours.append(QVariantMap{{QStringLiteral("time"), t.toString(QStringLiteral("HH:mm"))},
                                       {QStringLiteral("temperature"), int(std::lround(hTemp[i].toDouble()))},
                                       {QStringLiteral("code"), hCode[i].toInt()}});
        }
        m_ready = true;
        m_status.clear();
        m_fetchedAt = QDateTime::currentDateTime();
        emit changed();
    });
}

QString Weather::describe(int code) const {
    switch (code) {
    case 0: return tr("Clear");
    case 1: return tr("Mostly clear");
    case 2: return tr("Partly cloudy");
    case 3: return tr("Cloudy");
    case 45: case 48: return tr("Fog");
    case 51: case 53: case 55: case 56: case 57: return tr("Drizzle");
    case 61: case 80: return tr("Light rain");
    case 63: case 81: return tr("Rain");
    case 65: case 82: case 66: case 67: return tr("Heavy rain");
    case 71: case 73: case 75: case 77: case 85: case 86: return tr("Snow");
    case 95: case 96: case 99: return tr("Thunderstorm");
    default: return QString();
    }
}

QString Weather::icon(int code, bool day) const {
    if (code == 0 || code == 1) return day ? QStringLiteral("sun") : QStringLiteral("moon");
    if (code == 2) return QStringLiteral("cloudSun");
    if (code == 3) return QStringLiteral("cloud");
    if (code == 45 || code == 48) return QStringLiteral("fog");
    if (code >= 71 && code <= 86 && code != 80 && code != 81 && code != 82) return QStringLiteral("snow");
    if (code >= 95) return QStringLiteral("storm");
    if (code >= 51) return QStringLiteral("rain");
    return QStringLiteral("cloud");
}
