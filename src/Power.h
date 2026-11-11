#pragma once

#include <QObject>

class QDBusMessage;

// The battery (UPower), the session's power actions (KDE's ksmserver, logind)
// and keep-awake (screensaver and sleep inhibitors). UPower tells Lumo when
// anything changes: nothing polls.
class Power : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hasBattery READ hasBattery NOTIFY batteryChanged)
    Q_PROPERTY(int percent READ percent NOTIFY batteryChanged)
    Q_PROPERTY(bool charging READ charging NOTIFY batteryChanged)
    Q_PROPERTY(bool pluggedIn READ pluggedIn NOTIFY batteryChanged)
    // minutes to empty while discharging, to full while charging; 0 unknown
    Q_PROPERTY(int minutesLeft READ minutesLeft NOTIFY batteryChanged)
    Q_PROPERTY(int capacity READ capacity NOTIFY batteryChanged)   // health, % of design
    Q_PROPERTY(double watts READ watts NOTIFY batteryChanged)
    Q_PROPERTY(int lowThreshold READ lowThreshold WRITE setLowThreshold NOTIFY settingsChanged)
    Q_PROPERTY(bool batteryPills READ batteryPills WRITE setBatteryPills NOTIFY settingsChanged)
    Q_PROPERTY(bool keepAwake READ keepAwake WRITE setKeepAwake NOTIFY keepAwakeChanged)
public:
    explicit Power(QObject *parent = nullptr);

    bool hasBattery() const { return m_hasBattery; }
    int percent() const { return m_percent; }
    bool charging() const { return m_state == 1 || m_state == 5; }   // charging, pending charge
    bool pluggedIn() const { return m_state != 2 && m_state != 6; }  // not discharging
    int minutesLeft() const { return m_minutes; }
    int capacity() const { return m_capacity; }
    double watts() const { return m_watts; }
    int lowThreshold() const;
    void setLowThreshold(int percent);
    bool batteryPills() const;
    void setBatteryPills(bool on);
    bool keepAwake() const { return m_cookie != 0; }
    void setKeepAwake(bool on);

    Q_INVOKABLE void lock();
    Q_INVOKABLE void suspend();
    Q_INVOKABLE void logout();
    Q_INVOKABLE void reboot();
    Q_INVOKABLE void shutdown();

signals:
    void batteryChanged();
    void settingsChanged();
    void keepAwakeChanged();
    // For the island's pills.
    void plugged(int percent);
    void unplugged(int percent, int minutesLeft);
    void low(int percent);

private slots:
    void onChanged(const QDBusMessage &message);

private:
    void read();
    void apply(const QVariantMap &props, bool initial);
    void readHealth();

    bool m_hasBattery = false;
    int m_percent = 0, m_state = 0, m_minutes = 0, m_capacity = 0;
    double m_watts = 0;
    bool m_warnedLow = false;
    uint m_cookie = 0, m_sleepCookie = 0;
};
