#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QTimer>

class QFileSystemWatcher;
class QSocketNotifier;

// A modifier held alone (Ctrl), read straight from the keyboards in
// /dev/input. Only works when the user is in the `input` group; Wayland gives
// apps no other way to see a lone modifier. Never grabs anything: every app
// still gets its keys. Mice are watched too, so Ctrl+click or Ctrl+scroll
// count as "something else was pressed".
class EvdevKey : public QObject {
    Q_OBJECT
public:
    // Any of `keycodes` is the key (e.g. KEY_LEFTCTRL).
    explicit EvdevKey(const QList<int> &keycodes, QObject *parent = nullptr);
    ~EvdevKey() override;
    // Whether keyboards can be read at all (the user is allowed to).
    bool available() const { return m_readable; }

signals:
    void pressed();
    void released();
    // Another key, a mouse button or the wheel while ours is held, or Esc.
    void otherKey(int keycode);

private:
    // Opens the keyboards and mice not open yet. Run at start and whenever
    // /dev/input changes: Bluetooth keyboards come and go while we run.
    void scan();
    void watch(const QString &node, int fd);
    void drop(const QString &node);
    void read(const QString &node, int fd);

    QList<int> m_keys;
    int m_down = -1;   // which of our keys is held
    bool m_readable = false;
    struct Device {
        int fd = -1;
        QSocketNotifier *notifier = nullptr;
    };
    QHash<QString, Device> m_devices;   // by /dev/input/eventN
    QFileSystemWatcher *m_watcher;
    QTimer m_rescan;
};
