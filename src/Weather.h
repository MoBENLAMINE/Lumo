#pragma once

#include <QDateTime>
#include <QObject>
#include <QVariantList>

class QNetworkAccessManager;

// The weather where you are, from Open-Meteo (free, no key, no account). Your
// city is what you type in Settings: Lumo never looks you up by IP. Fetched
// only when something shows it, at most every 30 minutes.
class Weather : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY settingsChanged)
    Q_PROPERTY(QString city READ city NOTIFY settingsChanged)            // as found, "Casablanca, Morocco"
    Q_PROPERTY(bool fahrenheit READ fahrenheit WRITE setFahrenheit NOTIFY settingsChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)                // why there is nothing to show
    Q_PROPERTY(int temperature READ temperature NOTIFY changed)
    Q_PROPERTY(int code READ code NOTIFY changed)                        // WMO weather code
    Q_PROPERTY(bool day READ day NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    Q_PROPERTY(int high READ high NOTIFY changed)
    Q_PROPERTY(int low READ low NOTIFY changed)
    // [{ time, temperature, code }] the next 12 hours; [{ date, high, low, code }] 6 days
    Q_PROPERTY(QVariantList hours READ hours NOTIFY changed)
    Q_PROPERTY(QVariantList days READ days NOTIFY changed)
public:
    explicit Weather(QObject *parent = nullptr);

    bool enabled() const;
    void setEnabled(bool on);
    QString city() const;
    bool fahrenheit() const;
    void setFahrenheit(bool on);
    bool ready() const { return m_ready; }
    QString status() const { return m_status; }
    int temperature() const { return m_temp; }
    int code() const { return m_code; }
    bool day() const { return m_day; }
    QString summary() const { return describe(m_code); }
    int high() const { return m_high; }
    int low() const { return m_low; }
    QVariantList hours() const { return m_hours; }
    QVariantList days() const { return m_days; }

    // Something is about to show the weather: fetch it if it's older than 30 min.
    Q_INVOKABLE void want();
    // Finds a city ("Paris, Texas") and keeps it; the forecast follows.
    Q_INVOKABLE void setCity(const QString &query);
    // "Clear", "Light rain"…
    Q_INVOKABLE QString describe(int code) const;
    // An icon name for Ui.icons: sun, moon, cloudSun, cloud, fog, rain, snow, storm
    Q_INVOKABLE QString icon(int code, bool day = true) const;

signals:
    void settingsChanged();
    void changed();
    void cityResult(bool found, const QString &name);

private:
    void fetch();

    QNetworkAccessManager *m_net = nullptr;
    QDateTime m_fetchedAt;
    bool m_ready = false, m_busy = false, m_day = true;
    QString m_status;
    int m_temp = 0, m_code = 0, m_high = 0, m_low = 0;
    QVariantList m_hours, m_days;
};
