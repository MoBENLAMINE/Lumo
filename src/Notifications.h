#pragma once

#include <QObject>

class QProcess;

// Your desktop's notifications, shown as pills in the island. Plasma keeps its
// own popups; Lumo only watches the Notify calls on your session bus (with
// dbus-monitor, while this is on) and never answers them. Nothing leaves the PC.
class Notifications : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool doNotDisturb READ doNotDisturb WRITE setDoNotDisturb NOTIFY settingsChanged)
public:
    explicit Notifications(QObject *parent = nullptr);
    ~Notifications() override;

    bool enabled() const;
    void setEnabled(bool on);
    bool doNotDisturb() const;
    void setDoNotDisturb(bool on);

    // Splits dbus-monitor output into notifications (public for the tests).
    struct Parsed { QString app, summary, body; };
    static QList<Parsed> parse(QByteArray &buffer);

signals:
    void settingsChanged();
    void notified(const QString &app, const QString &summary, const QString &body);

private:
    void start();
    void stop();

    QProcess *m_monitor = nullptr;
    QByteArray m_buffer;
};
