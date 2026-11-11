#include "Notifications.h"

#include <QProcess>
#include <QSettings>

Notifications::Notifications(QObject *parent) : QObject(parent) {
    if (enabled()) start();
}

Notifications::~Notifications() { stop(); }

bool Notifications::enabled() const { return QSettings().value(QStringLiteral("notifications/pills"), false).toBool(); }

void Notifications::setEnabled(bool on) {
    QSettings().setValue(QStringLiteral("notifications/pills"), on);
    if (on) start(); else stop();
    emit settingsChanged();
}

bool Notifications::doNotDisturb() const { return QSettings().value(QStringLiteral("notifications/dnd"), false).toBool(); }

void Notifications::setDoNotDisturb(bool on) {
    QSettings().setValue(QStringLiteral("notifications/dnd"), on);
    emit settingsChanged();
}

void Notifications::start() {
    if (m_monitor) return;
    m_monitor = new QProcess(this);
    m_monitor->setStandardErrorFile(QProcess::nullDevice());
    connect(m_monitor, &QProcess::readyReadStandardOutput, this, [this] {
        m_buffer += m_monitor->readAllStandardOutput();
        if (m_buffer.size() > 1 << 20) m_buffer.clear();   // never grow without bound
        for (const Parsed &n : parse(m_buffer)) {
            if (doNotDisturb() || n.app == QLatin1String("Lumo")) continue;
            emit notified(n.app, n.summary, n.body);
        }
    });
    m_monitor->start(QStringLiteral("dbus-monitor"),
                     {QStringLiteral("--session"),
                      QStringLiteral("type='method_call',interface='org.freedesktop.Notifications',member='Notify'")});
}

void Notifications::stop() {
    if (!m_monitor) return;
    m_monitor->disconnect(this);
    m_monitor->kill();
    m_monitor->waitForFinished(200);
    delete m_monitor;
    m_monitor = nullptr;
    m_buffer.clear();
}

// A Notify call as dbus-monitor prints it:
//   method call time=… member=Notify
//      string "app name"
//      uint32 0
//      string "icon"
//      string "summary"
//      string "body, maybe on
//   several lines"
//      array [ …
QList<Notifications::Parsed> Notifications::parse(QByteArray &buffer) {
    QList<Parsed> out;
    for (;;) {
        const int header = buffer.indexOf("member=Notify");
        if (header < 0) {
            // keep only a possible partial header line
            const int nl = buffer.lastIndexOf('\n');
            if (nl >= 0) buffer.remove(0, nl + 1);
            return out;
        }
        int at = buffer.indexOf('\n', header);
        if (at < 0) { buffer.remove(0, header); return out; }
        QStringList strings;
        bool complete = true;
        while (strings.size() < 4) {
            const int open = buffer.indexOf("string \"", at);
            if (open < 0) { complete = false; break; }
            const int start = open + 8;
            const int close = buffer.indexOf("\"\n", start);
            if (close < 0) { complete = false; break; }
            strings.append(QString::fromUtf8(buffer.mid(start, close - start)));
            at = close + 2;
        }
        if (!complete) { buffer.remove(0, header); return out; }
        out.append({strings[0], strings[2], strings[3]});
        buffer.remove(0, at);
    }
}
