#pragma once

#include <QObject>
#include <QTimer>
#include <QHash>
#include <QVariantList>

#include <algorithm>
#include <functional>

class QProcess;
class QDBusMessage;

// The quick controls of the island's Controls page, the way KDE's own applets
// reach them: sound through pactl (PipeWire's pulse layer), brightness through
// PowerDevil, Wi-Fi through NetworkManager, Bluetooth through BlueZ and the
// power mode through power-profiles-daemon.
//
// Nothing is read until the page opens (`live`); while it is open a single
// `pactl subscribe` reports changes, and it is stopped when the page closes.
class Controls : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool live READ live WRITE setLive NOTIFY liveChanged)
    // sound
    Q_PROPERTY(bool hasAudio READ hasAudio NOTIFY audioChanged)
    Q_PROPERTY(int volume READ volume NOTIFY audioChanged)             // 0..150 %
    Q_PROPERTY(bool muted READ muted NOTIFY audioChanged)
    Q_PROPERTY(int micVolume READ micVolume NOTIFY audioChanged)
    Q_PROPERTY(bool micMuted READ micMuted NOTIFY audioChanged)
    Q_PROPERTY(QVariantList outputs READ outputs NOTIFY audioChanged)  // [{ name, label, current }]
    Q_PROPERTY(QVariantList inputs READ inputs NOTIFY audioChanged)
    Q_PROPERTY(QVariantList apps READ apps NOTIFY audioChanged)        // [{ id, name, volume, muted }]
    // brightness
    Q_PROPERTY(bool hasBrightness READ hasBrightness NOTIFY brightnessChanged)
    Q_PROPERTY(int brightness READ brightness NOTIFY brightnessChanged) // 0..100 %
    // radios and power
    Q_PROPERTY(bool hasWifi READ hasWifi NOTIFY radiosChanged)
    Q_PROPERTY(bool wifi READ wifi NOTIFY radiosChanged)
    Q_PROPERTY(bool hasBluetooth READ hasBluetooth NOTIFY radiosChanged)
    Q_PROPERTY(bool bluetooth READ bluetooth NOTIFY radiosChanged)
    Q_PROPERTY(bool hasPowerProfiles READ hasPowerProfiles NOTIFY radiosChanged)
    Q_PROPERTY(QString powerProfile READ powerProfile NOTIFY radiosChanged) // power-saver | balanced | performance
    Q_PROPERTY(QStringList powerProfiles READ powerProfiles NOTIFY radiosChanged)
    // Bluetooth devices: [{ path, name, icon, paired, connected, battery }] (battery -1 unknown)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY devicesChanged)
    // Wi-Fi networks: [{ ssid, strength, secured, active, saved }], strongest first
    Q_PROPERTY(QVariantList networks READ networks NOTIFY networksChanged)
    Q_PROPERTY(QString wifiBusy READ wifiBusy NOTIFY networksChanged)   // the SSID being joined
    Q_PROPERTY(QString wifiError READ wifiError NOTIFY networksChanged)
public:
    explicit Controls(QObject *parent = nullptr);
    ~Controls() override;

    bool live() const { return m_live; }
    void setLive(bool on);

    bool hasAudio() const { return !m_sink.isEmpty(); }
    int volume() const { return m_volume; }
    bool muted() const { return m_muted; }
    int micVolume() const { return m_micVolume; }
    bool micMuted() const { return m_micMuted; }
    QVariantList outputs() const { return m_outputs; }
    QVariantList inputs() const { return m_inputs; }
    QVariantList apps() const { return m_apps; }

    bool hasBrightness() const { return m_brightnessMax > 0; }
    int brightness() const;

    bool hasWifi() const { return m_hasWifi; }
    bool wifi() const { return m_wifi; }
    bool hasBluetooth() const { return !m_adapter.isEmpty(); }
    bool bluetooth() const { return m_bluetooth; }
    bool hasPowerProfiles() const { return !m_profiles.isEmpty(); }
    QString powerProfile() const { return m_profile; }
    QStringList powerProfiles() const { return m_profiles; }
    QVariantList devices() const { return m_devices; }
    bool scanning() const { return m_scanning; }
    QVariantList networks() const { return m_networks; }
    QString wifiBusy() const { return m_joining; }
    QString wifiError() const { return m_wifiError; }

    // Bluetooth: read only while the Bluetooth sheet shows; scanning stops by itself.
    Q_INVOKABLE void readDevices();
    Q_INVOKABLE void setScanning(bool on);
    Q_INVOKABLE void connectDevice(const QString &path);
    Q_INVOKABLE void disconnectDevice(const QString &path);
    Q_INVOKABLE void forgetDevice(const QString &path);
    // Wi-Fi: a scan, then the networks; join (with a password for a new
    // secured one), disconnect, forget.
    Q_INVOKABLE void scanNetworks();
    Q_INVOKABLE void joinNetwork(const QString &ssid, const QString &password);
    Q_INVOKABLE void disconnectWifi();
    Q_INVOKABLE void forgetNetwork(const QString &ssid);

    // as SystemStats::hold: live while any view holds it
    Q_INVOKABLE void hold(bool on) { m_holders = std::max(0, m_holders + (on ? 1 : -1)); setLive(m_holders > 0); }
    Q_INVOKABLE void setVolume(int percent);
    Q_INVOKABLE void toggleMute();
    Q_INVOKABLE void setMicVolume(int percent);
    Q_INVOKABLE void toggleMicMute();
    Q_INVOKABLE void setOutput(const QString &name);
    Q_INVOKABLE void setInput(const QString &name);
    Q_INVOKABLE void setAppVolume(int id, int percent);
    Q_INVOKABLE void toggleAppMute(int id);
    Q_INVOKABLE void setBrightness(int percent);
    Q_INVOKABLE void setWifi(bool on);
    Q_INVOKABLE void setBluetooth(bool on);
    Q_INVOKABLE void setPowerProfile(const QString &profile);
    // Reads everything once (the page calls it as it opens).
    Q_INVOKABLE void refresh();

signals:
    void liveChanged();
    void audioChanged();
    void brightnessChanged();
    void radiosChanged();
    void devicesChanged();
    void networksChanged();

private slots:
    void onRadioChanged(const QDBusMessage &message);

private:
    void pactl(const QStringList &args);
    void readAudio();
    void readBrightness();
    void readRadios();
    void startSubscribe();
    void stopSubscribe();

    bool m_live = false;
    int m_holders = 0;
    QProcess *m_subscribe = nullptr;
    QTimer m_soon;   // gathers a burst of pactl events into one read
    int m_reading = 0;

    QString m_sink, m_source;
    int m_volume = 0, m_micVolume = 0;
    bool m_muted = false, m_micMuted = false;
    QVariantList m_outputs, m_inputs, m_apps;

    // brightness: Plasma 6's ScreenBrightness, or PowerDevil's older interface
    QString m_display;   // a ScreenBrightness display object path, if any
    int m_brightnessRaw = 0, m_brightnessMax = 0;

    bool m_hasWifi = false, m_wifi = false, m_bluetooth = false;
    QString m_adapter;   // BlueZ adapter object path
    QString m_profilesService, m_profilesPath, m_profile;
    QStringList m_profiles;

    QVariantList m_devices;
    bool m_scanning = false;
    QTimer m_scanStop, m_devicesSoon;
    QString m_wifiDevice;                 // NetworkManager's wireless device
    QVariantList m_networks;
    QHash<QString, QString> m_saved;      // SSID → saved connection path
    QString m_joining, m_wifiError;
    void readNetworks();
    void withWifiDevice(std::function<void()> then);
};
