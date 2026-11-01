#pragma once

#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QObject>
#include <QVariantMap>
#include <functional>

// xdg-desktop-portal plumbing for Wayland, where an app may neither watch the
// keyboard nor type into another app:
//  * HoldShortcut — a GlobalShortcuts binding that reports press AND release,
//    which is all hold-to-talk needs. KDE asks once which keys to use.
//  * KeyboardRemote — a keyboard-only RemoteDesktop session, to press Ctrl+V
//    in the app in front. KDE asks once; the restore token remembers it.
namespace Portal {

using Reply = std::function<void(uint code, const QVariantMap &results)>;

// Calls a portal method that answers through an org.freedesktop.portal.Request.
// `options` is the trailing a{sv}; a handle_token is added to it.
void request(const QString &iface, const QString &method, QVariantList args, QVariantMap options, QObject *context,
             Reply reply);

}

class HoldShortcut : public QObject {
    Q_OBJECT
public:
    explicit HoldShortcut(const QString &id, const QString &description, const QString &trigger, QObject *parent = nullptr);
    void start();
    bool ready() const { return m_ready; }
    // Opens KDE's dialog to change the keys (GlobalShortcuts v2).
    void configure();
    QString trigger() const { return m_trigger; }

signals:
    void pressed();
    void released();
    void readyChanged(bool ready, const QString &why);

private slots:
    void onActivated(const QDBusObjectPath &session, const QString &id, qulonglong ts, const QVariantMap &options);
    void onDeactivated(const QDBusObjectPath &session, const QString &id, qulonglong ts, const QVariantMap &options);
    void onChanged(const QDBusMessage &message);

private:
    void bind();

    QString m_id, m_description, m_trigger;
    QString m_session;
    bool m_ready = false;
};

class KeyboardRemote : public QObject {
    Q_OBJECT
public:
    explicit KeyboardRemote(QObject *parent = nullptr);
    // Presses the evdev key codes in order, then releases them in reverse.
    // Opens the session first if needed (KDE may ask the user the first time).
    void chord(const QList<int> &keycodes);
    bool ready() const { return !m_session.isEmpty() && m_started; }

signals:
    void failed(const QString &why);

private slots:
    void onClosed() { m_started = false; m_session.clear(); m_queue.clear(); m_sending = false; }

private:
    void open(std::function<void()> then);
    void send(const QList<int> &keycodes);
    void next();

    QString m_session;
    bool m_started = false, m_opening = false, m_sending = false;
    QList<QPair<int, uint>> m_queue;
    QList<std::function<void()>> m_waiting;
};
