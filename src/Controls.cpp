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
             call(false, service, path, iface, QStringLiteral("brightnessMax"), {}, this, [=](const QDBusMessage &r) {
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
    const QString iface = message.arguments().at(0).toString();
    const QVariantMap changed = qdbus_cast<QVariantMap>(message.arguments().at(1));
    if (iface == kNM) {
        if (changed.contains(QStringLiteral("WirelessEnabled"))) m_wifi = changed.value(QStringLiteral("WirelessEnabled")).toBool();
        if (changed.contains(QStringLiteral("WirelessHardwareEnabled")))
            m_hasWifi = changed.value(QStringLiteral("WirelessHardwareEnabled")).toBool();
        emit radiosChanged();
    } else if (iface == QLatin1String("org.bluez.Adapter1") && changed.contains(QStringLiteral("Powered"))) {
        m_bluetooth = changed.value(QStringLiteral("Powered")).toBool();
        emit radiosChanged();
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
