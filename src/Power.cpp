#include "Power.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <functional>

namespace {
const QString kUPower = QStringLiteral("org.freedesktop.UPower");
const QString kDisplay = QStringLiteral("/org/freedesktop/UPower/devices/DisplayDevice");
const QString kDevice = QStringLiteral("org.freedesktop.UPower.Device");
const QString kProps = QStringLiteral("org.freedesktop.DBus.Properties");

void call(bool system, const QString &service, const QString &path, const QString &iface, const QString &method,
          const QVariantList &args, QObject *context, std::function<void(const QDBusMessage &)> done = {}) {
    QDBusMessage msg = QDBusMessage::createMethodCall(service, path, iface, method);
    msg.setArguments(args);
    const QDBusConnection bus = system ? QDBusConnection::systemBus() : QDBusConnection::sessionBus();
    auto *w = new QDBusPendingCallWatcher(bus.asyncCall(msg, 5000), context);
    QObject::connect(w, &QDBusPendingCallWatcher::finished, context, [w, done] {
        w->deleteLater();
        if (done) done(w->reply());
    });
}

bool failed(const QDBusMessage &r) { return r.type() != QDBusMessage::ReplyMessage; }
}

Power::Power(QObject *parent) : QObject(parent) {
    QDBusConnection::systemBus().connect(kUPower, kDisplay, kProps, QStringLiteral("PropertiesChanged"), this,
                                         SLOT(onChanged(QDBusMessage)));
    read();
    readHealth();
}

void Power::read() {
    call(true, kUPower, kDisplay, kProps, QStringLiteral("GetAll"), {kDevice}, this, [this](const QDBusMessage &r) {
        if (failed(r) || r.arguments().isEmpty()) return;
        apply(qdbus_cast<QVariantMap>(r.arguments().first()), true);
    });
}

void Power::onChanged(const QDBusMessage &message) {
    if (message.arguments().size() < 2) return;
    apply(qdbus_cast<QVariantMap>(message.arguments().at(1)), false);
}

void Power::apply(const QVariantMap &p, bool initial) {
    const int oldState = m_state;
    const bool wasPlugged = pluggedIn();
    if (p.contains(QStringLiteral("IsPresent")))
        m_hasBattery = p.value(QStringLiteral("IsPresent")).toBool() && p.value(QStringLiteral("Type"), 2).toUInt() == 2;
    if (p.contains(QStringLiteral("Percentage"))) m_percent = int(std::lround(p.value(QStringLiteral("Percentage")).toDouble()));
    if (p.contains(QStringLiteral("State"))) m_state = int(p.value(QStringLiteral("State")).toUInt());
    if (p.contains(QStringLiteral("EnergyRate"))) m_watts = p.value(QStringLiteral("EnergyRate")).toDouble();
    const qint64 toEmpty = p.value(QStringLiteral("TimeToEmpty"), -1).toLongLong();
    const qint64 toFull = p.value(QStringLiteral("TimeToFull"), -1).toLongLong();
    if (charging() && toFull >= 0) m_minutes = int(toFull / 60);
    else if (!pluggedIn() && toEmpty >= 0) m_minutes = int(toEmpty / 60);
    emit batteryChanged();
    if (initial || !m_hasBattery || !batteryPills()) return;

    if (oldState != m_state && wasPlugged != pluggedIn()) {
        if (pluggedIn()) { m_warnedLow = false; emit plugged(m_percent); }
        else emit unplugged(m_percent, m_minutes);
    }
    // Low battery: once per discharge, even in Do Not Disturb.
    if (!pluggedIn() && m_percent <= lowThreshold() && !m_warnedLow) {
        m_warnedLow = true;
        emit low(m_percent);
    }
}

void Power::readHealth() {
    // The display device has no health: ask the battery itself.
    call(true, kUPower, QStringLiteral("/org/freedesktop/UPower"), kUPower, QStringLiteral("EnumerateDevices"), {}, this,
         [this](const QDBusMessage &r) {
             if (failed(r) || r.arguments().isEmpty()) return;
             const QList<QDBusObjectPath> devices = qdbus_cast<QList<QDBusObjectPath>>(r.arguments().first());
             for (const QDBusObjectPath &d : devices) {
                 call(true, kUPower, d.path(), kProps, QStringLiteral("GetAll"), {kDevice}, this, [this](const QDBusMessage &r) {
                     if (failed(r) || r.arguments().isEmpty()) return;
                     const QVariantMap p = qdbus_cast<QVariantMap>(r.arguments().first());
                     if (p.value(QStringLiteral("Type")).toUInt() != 2 || !p.value(QStringLiteral("PowerSupply")).toBool()) return;
                     m_capacity = int(std::lround(p.value(QStringLiteral("Capacity")).toDouble()));
                     emit batteryChanged();
                 });
             }
         });
}

int Power::lowThreshold() const { return std::clamp(QSettings().value(QStringLiteral("power/low"), 20).toInt(), 5, 50); }
void Power::setLowThreshold(int percent) { QSettings().setValue(QStringLiteral("power/low"), std::clamp(percent, 5, 50)); emit settingsChanged(); }
bool Power::batteryPills() const { return QSettings().value(QStringLiteral("power/pills"), true).toBool(); }
void Power::setBatteryPills(bool on) { QSettings().setValue(QStringLiteral("power/pills"), on); emit settingsChanged(); }

// ------------------------------------------------------------ keep awake

void Power::setKeepAwake(bool on) {
    if (on == keepAwake()) return;
    QDBusConnection session = QDBusConnection::sessionBus();
    if (on) {
        // The screen stays on (no blanking, no lock) and the PC doesn't sleep.
        QDBusReply<uint> screen = session.call(QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("/ScreenSaver"), QStringLiteral("org.freedesktop.ScreenSaver"),
            QStringLiteral("Inhibit")) << QStringLiteral("Lumo") << tr("Keep awake is on"), QDBus::Block, 2000);
        QDBusReply<uint> sleep = session.call(QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.PowerManagement"), QStringLiteral("/org/freedesktop/PowerManagement/Inhibit"),
            QStringLiteral("org.freedesktop.PowerManagement.Inhibit"), QStringLiteral("Inhibit"))
            << QStringLiteral("Lumo") << tr("Keep awake is on"), QDBus::Block, 2000);
        m_cookie = screen.isValid() ? std::max(1u, screen.value()) : 0;
        m_sleepCookie = sleep.isValid() ? sleep.value() : 0;
        if (!m_cookie && m_sleepCookie) m_cookie = 1;   // at least sleep is held off
    } else {
        if (m_cookie)
            session.call(QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("/ScreenSaver"),
                                                        QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("UnInhibit"))
                             << m_cookie, QDBus::NoBlock);
        if (m_sleepCookie)
            session.call(QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.PowerManagement"),
                                                        QStringLiteral("/org/freedesktop/PowerManagement/Inhibit"),
                                                        QStringLiteral("org.freedesktop.PowerManagement.Inhibit"), QStringLiteral("UnInhibit"))
                             << m_sleepCookie, QDBus::NoBlock);
        m_cookie = m_sleepCookie = 0;
    }
    emit keepAwakeChanged();
}

// ------------------------------------------------------------ session actions

void Power::lock() {
    call(false, QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("/ScreenSaver"), QStringLiteral("org.freedesktop.ScreenSaver"),
         QStringLiteral("Lock"), {}, this);
}

void Power::suspend() {
    call(true, QStringLiteral("org.freedesktop.login1"), QStringLiteral("/org/freedesktop/login1"),
         QStringLiteral("org.freedesktop.login1.Manager"), QStringLiteral("Suspend"), {false}, this);
}

// KDE's own logout (it saves the session and lets apps ask about unsaved work);
// logind directly if ksmserver isn't there.
void Power::logout() {
    call(false, QStringLiteral("org.kde.Shutdown"), QStringLiteral("/Shutdown"), QStringLiteral("org.kde.Shutdown"),
         QStringLiteral("logout"), {}, this, [this](const QDBusMessage &r) {
             if (!failed(r)) return;
             call(true, QStringLiteral("org.freedesktop.login1"), QStringLiteral("/org/freedesktop/login1/session/auto"),
                  QStringLiteral("org.freedesktop.login1.Session"), QStringLiteral("Terminate"), {}, this);
         });
}

void Power::reboot() {
    call(false, QStringLiteral("org.kde.Shutdown"), QStringLiteral("/Shutdown"), QStringLiteral("org.kde.Shutdown"),
         QStringLiteral("logoutAndReboot"), {}, this, [this](const QDBusMessage &r) {
             if (!failed(r)) return;
             call(true, QStringLiteral("org.freedesktop.login1"), QStringLiteral("/org/freedesktop/login1"),
                  QStringLiteral("org.freedesktop.login1.Manager"), QStringLiteral("Reboot"), {false}, this);
         });
}

void Power::shutdown() {
    call(false, QStringLiteral("org.kde.Shutdown"), QStringLiteral("/Shutdown"), QStringLiteral("org.kde.Shutdown"),
         QStringLiteral("logoutAndShutdown"), {}, this, [this](const QDBusMessage &r) {
             if (!failed(r)) return;
             call(true, QStringLiteral("org.freedesktop.login1"), QStringLiteral("/org/freedesktop/login1"),
                  QStringLiteral("org.freedesktop.login1.Manager"), QStringLiteral("PowerOff"), {false}, this);
         });
}
