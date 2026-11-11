#include "Controls.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusVariant>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

#include <algorithm>
#include <QDBusConnection>
#include <cmath>
#include <functional>
#include <memory>

namespace {
const QString kProps = QStringLiteral("org.freedesktop.DBus.Properties");
const QString kNM = QStringLiteral("org.freedesktop.NetworkManager");
const QString kNMPath = QStringLiteral("/org/freedesktop/NetworkManager");
const QString kBluez = QStringLiteral("org.bluez");

using ManagedObjects = QMap<QDBusObjectPath, QMap<QString, QVariantMap>>;

QDBusConnection bus(bool system) { return system ? QDBusConnection::systemBus() : QDBusConnection::sessionBus(); }

// An asynchronous D-Bus call: the island never waits for a slow service.
void call(bool system, const QString &service, const QString &path, const QString &iface, const QString &method,
          const QVariantList &args, QObject *context, std::function<void(const QDBusMessage &)> done = {}) {
    QDBusMessage msg = QDBusMessage::createMethodCall(service, path, iface, method);
    msg.setArguments(args);
    auto *w = new QDBusPendingCallWatcher(bus(system).asyncCall(msg, 3000), context);
    QObject::connect(w, &QDBusPendingCallWatcher::finished, context, [w, done] {
        w->deleteLater();
        if (done) done(w->reply());
    });
}

QVariant unwrap(const QVariant &v) {
    return v.canConvert<QDBusVariant>() ? v.value<QDBusVariant>().variant() : v;
}

void setDBusProperty(bool system, const QString &service, const QString &path, const QString &iface, const QString &name,
                 const QVariant &value, QObject *context, std::function<void(const QDBusMessage &)> done = {}) {
    call(system, service, path, kProps, QStringLiteral("Set"), {iface, name, QVariant::fromValue(QDBusVariant(value))}, context,
         std::move(done));
}

// "45%" of the first channel of a pactl JSON volume object.
int percentOf(const QJsonObject &volume) {
    for (auto it = volume.begin(); it != volume.end(); ++it) {
        const QString p = it.value().toObject().value(QStringLiteral("value_percent")).toString();
        return p.left(p.size() - 1).toInt();
    }
    return 0;
}
}

Controls::Controls(QObject *parent) : QObject(parent) {
    m_soon.setSingleShot(true);
    m_soon.setInterval(120);
    connect(&m_soon, &QTimer::timeout, this, &Controls::readAudio);
    qDBusRegisterMetaType<ManagedObjects>();
    qDBusRegisterMetaType<QMap<QString, QVariantMap>>();
    qDBusRegisterMetaType<QList<QDBusObjectPath>>();
}

Controls::~Controls() { stopSubscribe(); }

void Controls::setLive(bool on) {
    if (on == m_live) return;
    m_live = on;
    if (on) { refresh(); startSubscribe(); }
    else stopSubscribe();
    emit liveChanged();
}

void Controls::refresh() {
    readAudio();
    readBrightness();
    readRadios();
}

// ------------------------------------------------------------ sound

void Controls::pactl(const QStringList &args) {
    auto *p = new QProcess(this);
    connect(p, &QProcess::finished, p, &QObject::deleteLater);
    connect(p, &QProcess::errorOccurred, p, &QObject::deleteLater);
    p->start(QStringLiteral("pactl"), args);
}

void Controls::readAudio() {
    // One `pactl -f json` per list; `info` names the defaults.
    struct Read { QJsonDocument info, sinks, sources, inputs; int left = 4; };
    auto read = std::make_shared<Read>();
    auto finish = [this, read] {
        if (--read->left) return;
        const QJsonObject info = read->info.object();
        m_sink = info.value(QStringLiteral("default_sink_name")).toString();
        m_source = info.value(QStringLiteral("default_source_name")).toString();
        m_outputs.clear();
        for (const QJsonValue &v : read->sinks.array()) {
            const QJsonObject s = v.toObject();
            const QString name = s.value(QStringLiteral("name")).toString();
            const bool current = name == m_sink;
            if (current) {
                m_volume = percentOf(s.value(QStringLiteral("volume")).toObject());
                m_muted = s.value(QStringLiteral("mute")).toBool();
            }
            m_outputs.append(QVariantMap{{QStringLiteral("name"), name},
                                         {QStringLiteral("label"), s.value(QStringLiteral("description")).toString()},
                                         {QStringLiteral("current"), current}});
        }
        m_inputs.clear();
        for (const QJsonValue &v : read->sources.array()) {
            const QJsonObject s = v.toObject();
            const QString name = s.value(QStringLiteral("name")).toString();
            if (name.endsWith(QLatin1String(".monitor"))) continue;   // what a speaker plays, not a microphone
            const bool current = name == m_source;
            if (current) {
                m_micVolume = percentOf(s.value(QStringLiteral("volume")).toObject());
                m_micMuted = s.value(QStringLiteral("mute")).toBool();
            }
            m_inputs.append(QVariantMap{{QStringLiteral("name"), name},
                                        {QStringLiteral("label"), s.value(QStringLiteral("description")).toString()},
                                        {QStringLiteral("current"), current}});
        }
        m_apps.clear();
        for (const QJsonValue &v : read->inputs.array()) {
            const QJsonObject s = v.toObject();
            const QJsonObject props = s.value(QStringLiteral("properties")).toObject();
            QString name = props.value(QStringLiteral("application.name")).toString();
            if (name.isEmpty()) name = props.value(QStringLiteral("media.name")).toString();
            m_apps.append(QVariantMap{{QStringLiteral("id"), s.value(QStringLiteral("index")).toInt()},
                                      {QStringLiteral("name"), name},
                                      {QStringLiteral("icon"), props.value(QStringLiteral("application.icon_name")).toString()},
                                      {QStringLiteral("volume"), percentOf(s.value(QStringLiteral("volume")).toObject())},
                                      {QStringLiteral("muted"), s.value(QStringLiteral("mute")).toBool()}});
        }
        emit audioChanged();
    };
    auto run = [this, finish](const QStringList &args, QJsonDocument *into) {
        auto *p = new QProcess(this);
        connect(p, &QProcess::finished, this, [p, into, finish] {
            *into = QJsonDocument::fromJson(p->readAllStandardOutput());
            p->deleteLater();
            finish();
        });
        connect(p, &QProcess::errorOccurred, this, [p, finish](QProcess::ProcessError e) {
            if (e != QProcess::FailedToStart) return;
            p->deleteLater();
            finish();
        });
        p->start(QStringLiteral("pactl"), QStringList{QStringLiteral("-f"), QStringLiteral("json")} + args);
    };
    run({QStringLiteral("info")}, &read->info);
    run({QStringLiteral("list"), QStringLiteral("sinks")}, &read->sinks);
    run({QStringLiteral("list"), QStringLiteral("sources")}, &read->sources);
    run({QStringLiteral("list"), QStringLiteral("sink-inputs")}, &read->inputs);
}

void Controls::startSubscribe() {
    if (m_subscribe) return;
    m_subscribe = new QProcess(this);
    connect(m_subscribe, &QProcess::readyReadStandardOutput, this, [this] {
        m_subscribe->readAllStandardOutput();
        m_soon.start();
    });
    m_subscribe->start(QStringLiteral("pactl"), {QStringLiteral("subscribe")});
}

void Controls::stopSubscribe() {
    if (!m_subscribe) return;
    m_subscribe->disconnect(this);
    m_subscribe->kill();
    m_subscribe->waitForFinished(200);
    delete m_subscribe;
    m_subscribe = nullptr;
}

void Controls::setVolume(int percent) {
    m_volume = std::clamp(percent, 0, 150);
    pactl({QStringLiteral("set-sink-volume"), QStringLiteral("@DEFAULT_SINK@"), QStringLiteral("%1%").arg(m_volume)});
    emit audioChanged();
}

void Controls::toggleMute() {
    m_muted = !m_muted;
    pactl({QStringLiteral("set-sink-mute"), QStringLiteral("@DEFAULT_SINK@"), m_muted ? QStringLiteral("1") : QStringLiteral("0")});
    emit audioChanged();
}

void Controls::setMicVolume(int percent) {
    m_micVolume = std::clamp(percent, 0, 150);
    pactl({QStringLiteral("set-source-volume"), QStringLiteral("@DEFAULT_SOURCE@"), QStringLiteral("%1%").arg(m_micVolume)});
    emit audioChanged();
}

void Controls::toggleMicMute() {
    m_micMuted = !m_micMuted;
    pactl({QStringLiteral("set-source-mute"), QStringLiteral("@DEFAULT_SOURCE@"), m_micMuted ? QStringLiteral("1") : QStringLiteral("0")});
    emit audioChanged();
}

void Controls::setOutput(const QString &name) {
    pactl({QStringLiteral("set-default-sink"), name});
    m_soon.start();
}

void Controls::setInput(const QString &name) {
    pactl({QStringLiteral("set-default-source"), name});
    m_soon.start();
}

void Controls::setAppVolume(int id, int percent) {
    pactl({QStringLiteral("set-sink-input-volume"), QString::number(id), QStringLiteral("%1%").arg(std::clamp(percent, 0, 150))});
    for (QVariant &v : m_apps) {
        QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("id")).toInt() == id) { m[QStringLiteral("volume")] = percent; v = m; }
    }
    emit audioChanged();
}

void Controls::toggleAppMute(int id) {
    pactl({QStringLiteral("set-sink-input-mute"), QString::number(id), QStringLiteral("toggle")});
    m_soon.start();
}

// ------------------------------------------------------------ brightness

int Controls::brightness() const {
    return m_brightnessMax > 0 ? int(std::lround(100.0 * m_brightnessRaw / m_brightnessMax)) : 0;
}

void Controls::readBrightness() {
    // Plasma 6.2 and later: org.kde.ScreenBrightness, one object per display.
    call(false, QStringLiteral("org.kde.ScreenBrightness"), QStringLiteral("/org/kde/ScreenBrightness"), kProps,
         QStringLiteral("Get"), {QStringLiteral("org.kde.ScreenBrightness"), QStringLiteral("DisplaysDBusNames")}, this,
         [this](const QDBusMessage &r) {
             const QStringList displays = r.type() == QDBusMessage::ReplyMessage && !r.arguments().isEmpty()
                                              ? unwrap(r.arguments().first()).toStringList() : QStringList();
             if (!displays.isEmpty()) {
                 m_display = QStringLiteral("/org/kde/ScreenBrightness/") + displays.first();
                 call(false, QStringLiteral("org.kde.ScreenBrightness"), m_display, kProps, QStringLiteral("GetAll"),
                      {QStringLiteral("org.kde.ScreenBrightness.Display")}, this, [this](const QDBusMessage &r) {
                          if (r.type() != QDBusMessage::ReplyMessage || r.arguments().isEmpty()) return;
                          const QVariantMap p = qdbus_cast<QVariantMap>(r.arguments().first());
                          m_brightnessRaw = p.value(QStringLiteral("Brightness")).toInt();
                          m_brightnessMax = p.value(QStringLiteral("MaxBrightness")).toInt();
                          emit brightnessChanged();
                      });
                 return;
             }
             // Older Plasma: PowerDevil's BrightnessControl action.
             m_display.clear();
             const QString service = QStringLiteral("org.kde.Solid.PowerManagement");
             const QString path = QStringLiteral("/org/kde/Solid/PowerManagement/Actions/BrightnessControl");
             const QString iface = QStringLiteral("org.kde.Solid.PowerManagement.Actions.BrightnessControl");
             call(false, service, path, iface, QStringLiteral("brightnessMax"), {}, this, [=, this](const QDBusMessage &r) {
                 if (r.type() != QDBusMessage::ReplyMessage || r.arguments().isEmpty()) return;
                 m_brightnessMax = r.arguments().first().toInt();
                 call(false, service, path, iface, QStringLiteral("brightness"), {}, this, [this](const QDBusMessage &r) {
                     if (r.type() != QDBusMessage::ReplyMessage || r.arguments().isEmpty()) return;
                     m_brightnessRaw = r.arguments().first().toInt();
                     emit brightnessChanged();
                 });
             });
         });
}

void Controls::setBrightness(int percent) {
    if (m_brightnessMax <= 0) return;
    m_brightnessRaw = int(std::lround(std::clamp(percent, 1, 100) / 100.0 * m_brightnessMax));
    if (!m_display.isEmpty())
        call(false, QStringLiteral("org.kde.ScreenBrightness"), m_display, QStringLiteral("org.kde.ScreenBrightness.Display"),
             QStringLiteral("SetBrightness"), {m_brightnessRaw, 0u}, this);
    else
        call(false, QStringLiteral("org.kde.Solid.PowerManagement"), QStringLiteral("/org/kde/Solid/PowerManagement/Actions/BrightnessControl"),
             QStringLiteral("org.kde.Solid.PowerManagement.Actions.BrightnessControl"), QStringLiteral("setBrightness"),
             {m_brightnessRaw}, this);
    emit brightnessChanged();
}

// ------------------------------------------------------------ radios and power

void Controls::readRadios() {
    static bool watching = false;
    if (!watching) {
        watching = true;
        QDBusConnection::systemBus().connect(kNM, kNMPath, kProps, QStringLiteral("PropertiesChanged"), this,
                                             SLOT(onRadioChanged(QDBusMessage)));
        QDBusConnection::systemBus().connect(kBluez, QString(), kProps, QStringLiteral("PropertiesChanged"), this,
                                             SLOT(onRadioChanged(QDBusMessage)));
    }
    call(true, kNM, kNMPath, kProps, QStringLiteral("GetAll"), {kNM}, this, [this](const QDBusMessage &r) {
        if (r.type() != QDBusMessage::ReplyMessage || r.arguments().isEmpty()) return;
        const QVariantMap p = qdbus_cast<QVariantMap>(r.arguments().first());
        m_hasWifi = p.value(QStringLiteral("WirelessHardwareEnabled")).toBool();
        m_wifi = p.value(QStringLiteral("WirelessEnabled")).toBool();
        emit radiosChanged();
    });
    call(true, kBluez, QStringLiteral("/"), QStringLiteral("org.freedesktop.DBus.ObjectManager"), QStringLiteral("GetManagedObjects"), {},
         this, [this](const QDBusMessage &r) {
             if (r.type() != QDBusMessage::ReplyMessage || r.arguments().isEmpty()) return;
             const ManagedObjects objects = qdbus_cast<ManagedObjects>(r.arguments().first());
             m_adapter.clear();
             for (auto it = objects.begin(); it != objects.end(); ++it) {
                 const auto adapter = it.value().find(QStringLiteral("org.bluez.Adapter1"));
                 if (adapter == it.value().end()) continue;
                 m_adapter = it.key().path();
                 m_bluetooth = adapter->value(QStringLiteral("Powered")).toBool();
                 break;
             }
             emit radiosChanged();
         });
    // power-profiles-daemon, under its new name or its old one
    auto readProfiles = [this](const QString &service, const QString &path, auto &&fallback) -> void {
        call(true, service, path, kProps, QStringLiteral("GetAll"), {service}, this, [=, this](const QDBusMessage &r) {
            if (r.type() != QDBusMessage::ReplyMessage || r.arguments().isEmpty()) { fallback(); return; }
            const QVariantMap p = qdbus_cast<QVariantMap>(r.arguments().first());
            m_profilesService = service;
            m_profilesPath = path;
            m_profile = p.value(QStringLiteral("ActiveProfile")).toString();
            m_profiles.clear();
            const QDBusArgument list = p.value(QStringLiteral("Profiles")).value<QDBusArgument>();
            list.beginArray();
            while (!list.atEnd()) {
                QVariantMap profile;
                list >> profile;
                m_profiles.append(profile.value(QStringLiteral("Profile")).toString());
            }
            list.endArray();
            emit radiosChanged();
        });
    };
    readProfiles(QStringLiteral("org.freedesktop.UPower.PowerProfiles"), QStringLiteral("/org/freedesktop/UPower/PowerProfiles"),
                 [this, readProfiles] {
                     readProfiles(QStringLiteral("net.hadess.PowerProfiles"), QStringLiteral("/net/hadess/PowerProfiles"), [] {});
                 });
}

void Controls::onRadioChanged(const QDBusMessage &message) {
    if (message.arguments().size() < 2) return;
    // InterfacesAdded(o path, a{sa{sv}}): its first argument is a path, not an interface
    const QString iface = message.member() == QLatin1String("InterfacesAdded") ? QString() : message.arguments().at(0).toString();
    const QVariantMap changed = iface.isEmpty() ? QVariantMap() : qdbus_cast<QVariantMap>(message.arguments().at(1));
    if (iface == kNM) {
        if (changed.contains(QStringLiteral("WirelessEnabled"))) m_wifi = changed.value(QStringLiteral("WirelessEnabled")).toBool();
        if (changed.contains(QStringLiteral("WirelessHardwareEnabled")))
            m_hasWifi = changed.value(QStringLiteral("WirelessHardwareEnabled")).toBool();
        emit radiosChanged();
    } else if (iface == QLatin1String("org.bluez.Adapter1") && changed.contains(QStringLiteral("Powered"))) {
        m_bluetooth = changed.value(QStringLiteral("Powered")).toBool();
        emit radiosChanged();
    } else if (m_live && (iface == QLatin1String("org.bluez.Device1") || iface == QLatin1String("org.bluez.Battery1")
                          || message.member() == QLatin1String("InterfacesAdded"))) {
        // a device connected, its battery moved, or a new one was found: read
        // them again, once for a burst of changes
        m_devicesSoon.setSingleShot(true);
        m_devicesSoon.setInterval(300);
        connect(&m_devicesSoon, &QTimer::timeout, this, &Controls::readDevices, Qt::UniqueConnection);
        m_devicesSoon.start();
    }
}

void Controls::setWifi(bool on) {
    m_wifi = on;
    setDBusProperty(true, kNM, kNMPath, kNM, QStringLiteral("WirelessEnabled"), on, this);
    emit radiosChanged();
}

void Controls::setBluetooth(bool on) {
    if (m_adapter.isEmpty()) return;
    m_bluetooth = on;
    setDBusProperty(true, kBluez, m_adapter, QStringLiteral("org.bluez.Adapter1"), QStringLiteral("Powered"), on, this);
    emit radiosChanged();
}

void Controls::setPowerProfile(const QString &profile) {
    if (m_profilesService.isEmpty() || !m_profiles.contains(profile)) return;
    m_profile = profile;
    setDBusProperty(true, m_profilesService, m_profilesPath, m_profilesService, QStringLiteral("ActiveProfile"), profile, this);
    emit radiosChanged();
}

// ------------------------------------------------------------ Bluetooth devices

void Controls::readDevices() {
    call(true, kBluez, QStringLiteral("/"), QStringLiteral("org.freedesktop.DBus.ObjectManager"), QStringLiteral("GetManagedObjects"), {},
         this, [this](const QDBusMessage &r) {
             if (r.type() != QDBusMessage::ReplyMessage || r.arguments().isEmpty()) return;
             const ManagedObjects objects = qdbus_cast<ManagedObjects>(r.arguments().first());
             QVariantList list;
             for (auto it = objects.begin(); it != objects.end(); ++it) {
                 const auto dev = it.value().find(QStringLiteral("org.bluez.Device1"));
                 if (dev == it.value().end()) continue;
                 const QVariantMap &d = *dev;
                 // unnamed strangers nearby aren't worth a line
                 const QString name = d.value(QStringLiteral("Alias"), d.value(QStringLiteral("Name"))).toString();
                 const bool paired = d.value(QStringLiteral("Paired")).toBool();
                 if (!paired && !d.contains(QStringLiteral("Name"))) continue;
                 const auto battery = it.value().find(QStringLiteral("org.bluez.Battery1"));
                 list.append(QVariantMap{{QStringLiteral("path"), it.key().path()},
                                         {QStringLiteral("name"), name},
                                         {QStringLiteral("icon"), d.value(QStringLiteral("Icon")).toString()},
                                         {QStringLiteral("paired"), paired},
                                         {QStringLiteral("connected"), d.value(QStringLiteral("Connected")).toBool()},
                                         {QStringLiteral("battery"), battery != it.value().end()
                                              ? int(battery->value(QStringLiteral("Percentage")).toUInt()) : -1}});
             }
             // connected first, then paired, then the new ones
             std::stable_sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) {
                 const auto rank = [](const QVariantMap &m) {
                     return m.value(QStringLiteral("connected")).toBool() ? 0 : m.value(QStringLiteral("paired")).toBool() ? 1 : 2;
                 };
                 return rank(a.toMap()) < rank(b.toMap());
             });
             m_devices = list;
             emit devicesChanged();
         });
}

void Controls::setScanning(bool on) {
    if (m_adapter.isEmpty() || on == m_scanning) return;
    m_scanning = on;
    call(true, kBluez, m_adapter, QStringLiteral("org.bluez.Adapter1"), on ? QStringLiteral("StartDiscovery") : QStringLiteral("StopDiscovery"),
         {}, this);
    if (on) {
        // a scan costs radio time and battery: half a minute is plenty
        m_scanStop.setSingleShot(true);
        m_scanStop.setInterval(30000);
        connect(&m_scanStop, &QTimer::timeout, this, [this] { setScanning(false); }, Qt::UniqueConnection);
        m_scanStop.start();
        // new finds show up as they come
        QDBusConnection::systemBus().connect(kBluez, QStringLiteral("/"), QStringLiteral("org.freedesktop.DBus.ObjectManager"),
                                             QStringLiteral("InterfacesAdded"), this, SLOT(onRadioChanged(QDBusMessage)));
    } else {
        m_scanStop.stop();
    }
    emit devicesChanged();
}

void Controls::connectDevice(const QString &path) {
    // a device not paired yet is paired first, and trusted so it comes back by itself
    bool paired = false;
    for (const QVariant &v : std::as_const(m_devices))
        if (v.toMap().value(QStringLiteral("path")) == path) paired = v.toMap().value(QStringLiteral("paired")).toBool();
    const QString dev = QStringLiteral("org.bluez.Device1");
    auto connectIt = [this, path, dev] {
        call(true, kBluez, path, dev, QStringLiteral("Connect"), {}, this, [this](const QDBusMessage &) { readDevices(); });
    };
    if (paired) { connectIt(); return; }
    call(true, kBluez, path, dev, QStringLiteral("Pair"), {}, this, [this, path, dev, connectIt](const QDBusMessage &r) {
        if (r.type() != QDBusMessage::ReplyMessage) { readDevices(); return; }
        setDBusProperty(true, kBluez, path, dev, QStringLiteral("Trusted"), true, this);
        connectIt();
    });
}

void Controls::disconnectDevice(const QString &path) {
    call(true, kBluez, path, QStringLiteral("org.bluez.Device1"), QStringLiteral("Disconnect"), {}, this,
         [this](const QDBusMessage &) { readDevices(); });
}

void Controls::forgetDevice(const QString &path) {
    if (m_adapter.isEmpty()) return;
    call(true, kBluez, m_adapter, QStringLiteral("org.bluez.Adapter1"), QStringLiteral("RemoveDevice"),
         {QVariant::fromValue(QDBusObjectPath(path))}, this, [this](const QDBusMessage &) { readDevices(); });
}

// ------------------------------------------------------------ Wi-Fi networks

void Controls::withWifiDevice(std::function<void()> then) {
    if (!m_wifiDevice.isEmpty()) { then(); return; }
    call(true, kNM, kNMPath, kNM, QStringLiteral("GetDevices"), {}, this, [this, then](const QDBusMessage &r) {
        if (r.type() != QDBusMessage::ReplyMessage || r.arguments().isEmpty()) return;
        const auto devices = qdbus_cast<QList<QDBusObjectPath>>(r.arguments().first());
        auto left = std::make_shared<int>(devices.size());
        for (const QDBusObjectPath &d : devices) {
            call(true, kNM, d.path(), kProps, QStringLiteral("Get"), {QStringLiteral("org.freedesktop.NetworkManager.Device"), QStringLiteral("DeviceType")},
                 this, [this, d, then, left](const QDBusMessage &r) {
                     --*left;
                     if (m_wifiDevice.isEmpty() && r.type() == QDBusMessage::ReplyMessage && !r.arguments().isEmpty()
                         && unwrap(r.arguments().first()).toUInt() == 2) {   // NM_DEVICE_TYPE_WIFI
                         m_wifiDevice = d.path();
                         then();
                     }
                 });
        }
    });
}

void Controls::scanNetworks() {
    withWifiDevice([this] {
        call(true, kNM, m_wifiDevice, QStringLiteral("org.freedesktop.NetworkManager.Device.Wireless"), QStringLiteral("RequestScan"),
             {QVariantMap()}, this);
        readNetworks();
        // the scan takes a few seconds: read again once it has had time
        QTimer::singleShot(4000, this, &Controls::readNetworks);
    });
    // which networks are saved (to join them without a password, or forget them)
    call(true, kNM, QStringLiteral("/org/freedesktop/NetworkManager/Settings"), QStringLiteral("org.freedesktop.NetworkManager.Settings"),
         QStringLiteral("ListConnections"), {}, this, [this](const QDBusMessage &r) {
             if (r.type() != QDBusMessage::ReplyMessage || r.arguments().isEmpty()) return;
             m_saved.clear();
             for (const QDBusObjectPath &c : qdbus_cast<QList<QDBusObjectPath>>(r.arguments().first())) {
                 call(true, kNM, c.path(), QStringLiteral("org.freedesktop.NetworkManager.Settings.Connection"), QStringLiteral("GetSettings"), {},
                      this, [this, c](const QDBusMessage &r) {
                          if (r.type() != QDBusMessage::ReplyMessage || r.arguments().isEmpty()) return;
                          const auto settings = qdbus_cast<QMap<QString, QVariantMap>>(r.arguments().first());
                          const QByteArray ssid = settings.value(QStringLiteral("802-11-wireless")).value(QStringLiteral("ssid")).toByteArray();
                          if (ssid.isEmpty()) return;
                          m_saved.insert(QString::fromUtf8(ssid), c.path());
                          for (QVariant &v : m_networks) {
                              QVariantMap m = v.toMap();
                              if (m.value(QStringLiteral("ssid")) == QString::fromUtf8(ssid)) { m[QStringLiteral("saved")] = true; v = m; }
                          }
                          emit networksChanged();
                      });
             }
         });
}

void Controls::readNetworks() {
    if (m_wifiDevice.isEmpty()) return;
    const QString wireless = QStringLiteral("org.freedesktop.NetworkManager.Device.Wireless");
    call(true, kNM, m_wifiDevice, kProps, QStringLiteral("Get"), {wireless, QStringLiteral("ActiveAccessPoint")}, this,
         [this, wireless](const QDBusMessage &r) {
             const QString active = r.type() == QDBusMessage::ReplyMessage && !r.arguments().isEmpty()
                                        ? unwrap(r.arguments().first()).value<QDBusObjectPath>().path() : QString();
             call(true, kNM, m_wifiDevice, wireless, QStringLiteral("GetAllAccessPoints"), {}, this, [this, active](const QDBusMessage &r) {
                 if (r.type() != QDBusMessage::ReplyMessage || r.arguments().isEmpty()) return;
                 const auto aps = qdbus_cast<QList<QDBusObjectPath>>(r.arguments().first());
                 auto found = std::make_shared<QHash<QString, QVariantMap>>();
                 auto left = std::make_shared<int>(aps.size());
                 if (aps.isEmpty()) { m_networks.clear(); emit networksChanged(); return; }
                 for (const QDBusObjectPath &ap : aps) {
                     call(true, kNM, ap.path(), kProps, QStringLiteral("GetAll"), {QStringLiteral("org.freedesktop.NetworkManager.AccessPoint")}, this,
                          [this, ap, active, found, left](const QDBusMessage &r) {
                              if (r.type() == QDBusMessage::ReplyMessage && !r.arguments().isEmpty()) {
                                  const QVariantMap p = qdbus_cast<QVariantMap>(r.arguments().first());
                                  const QString ssid = QString::fromUtf8(p.value(QStringLiteral("Ssid")).toByteArray());
                                  const int strength = p.value(QStringLiteral("Strength")).toInt();
                                  const bool secured = p.value(QStringLiteral("WpaFlags")).toUInt() || p.value(QStringLiteral("RsnFlags")).toUInt()
                                                       || (p.value(QStringLiteral("Flags")).toUInt() & 1);
                                  // one line per network: its strongest access point
                                  if (!ssid.isEmpty() && (!found->contains(ssid) || found->value(ssid).value(QStringLiteral("strength")).toInt() < strength))
                                      found->insert(ssid, {{QStringLiteral("ssid"), ssid}, {QStringLiteral("strength"), strength},
                                                           {QStringLiteral("secured"), secured},
                                                           {QStringLiteral("active"), ap.path() == active || found->value(ssid).value(QStringLiteral("active")).toBool()},
                                                           {QStringLiteral("saved"), m_saved.contains(ssid)}});
                              }
                              if (--*left) return;
                              QVariantList list;
                              for (const QVariantMap &m : found->values()) list.append(m);
                              std::sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) {
                                  const QVariantMap x = a.toMap(), y = b.toMap();
                                  if (x.value(QStringLiteral("active")) != y.value(QStringLiteral("active"))) return x.value(QStringLiteral("active")).toBool();
                                  return x.value(QStringLiteral("strength")).toInt() > y.value(QStringLiteral("strength")).toInt();
                              });
                              m_networks = list;
                              emit networksChanged();
                          });
                 }
             });
         });
}

void Controls::joinNetwork(const QString &ssid, const QString &password) {
    m_joining = ssid;
    m_wifiError.clear();
    emit networksChanged();
    withWifiDevice([this, ssid, password] {
        auto done = [this](const QDBusMessage &r) {
            if (r.type() != QDBusMessage::ReplyMessage) m_wifiError = r.errorMessage();
            m_joining.clear();
            emit networksChanged();
            QTimer::singleShot(2500, this, &Controls::readNetworks);
        };
        if (m_saved.contains(ssid) && password.isEmpty()) {
            call(true, kNM, kNMPath, kNM, QStringLiteral("ActivateConnection"),
                 {QVariant::fromValue(QDBusObjectPath(m_saved.value(ssid))), QVariant::fromValue(QDBusObjectPath(m_wifiDevice)),
                  QVariant::fromValue(QDBusObjectPath(QStringLiteral("/")))}, this, done);
            return;
        }
        QMap<QString, QVariantMap> settings;
        settings[QStringLiteral("802-11-wireless")] = {{QStringLiteral("ssid"), ssid.toUtf8()}};
        if (!password.isEmpty())
            settings[QStringLiteral("802-11-wireless-security")] = {{QStringLiteral("key-mgmt"), QStringLiteral("wpa-psk")},
                                                                    {QStringLiteral("psk"), password}};
        call(true, kNM, kNMPath, kNM, QStringLiteral("AddAndActivateConnection"),
             {QVariant::fromValue(settings), QVariant::fromValue(QDBusObjectPath(m_wifiDevice)),
              QVariant::fromValue(QDBusObjectPath(QStringLiteral("/")))}, this, done);
    });
}

void Controls::disconnectWifi() {
    withWifiDevice([this] {
        call(true, kNM, m_wifiDevice, QStringLiteral("org.freedesktop.NetworkManager.Device"), QStringLiteral("Disconnect"), {}, this,
             [this](const QDBusMessage &) { QTimer::singleShot(1000, this, &Controls::readNetworks); });
    });
}

void Controls::forgetNetwork(const QString &ssid) {
    const QString path = m_saved.value(ssid);
    if (path.isEmpty()) return;
    call(true, kNM, path, QStringLiteral("org.freedesktop.NetworkManager.Settings.Connection"), QStringLiteral("Delete"), {}, this,
         [this, ssid](const QDBusMessage &) {
             m_saved.remove(ssid);
             QTimer::singleShot(800, this, &Controls::readNetworks);
         });
}
