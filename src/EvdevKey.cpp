#include "EvdevKey.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QSocketNotifier>

#include <cerrno>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {
bool hasBit(int fd, int type, int code, int max) {
    unsigned long bits[KEY_MAX / (8 * sizeof(unsigned long)) + 1] = {};
    if (ioctl(fd, EVIOCGBIT(type, sizeof bits), bits) < 0 || code > max) return false;
    return bits[code / (8 * sizeof(unsigned long))] & (1UL << (code % (8 * sizeof(unsigned long))));
}
}

EvdevKey::EvdevKey(const QList<int> &keycodes, QObject *parent)
    : QObject(parent), m_keys(keycodes), m_watcher(new QFileSystemWatcher({QStringLiteral("/dev/input")}, this)) {
    // A new device node needs a moment before its permissions are set.
    m_rescan.setSingleShot(true);
    m_rescan.setInterval(700);
    connect(&m_rescan, &QTimer::timeout, this, &EvdevKey::scan);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_rescan.start(); });
    scan();
}

EvdevKey::~EvdevKey() {
    for (const auto &d : std::as_const(m_devices)) {
        delete d.notifier;
        close(d.fd);
    }
}

void EvdevKey::scan() {
    const QStringList nodes = QDir(QStringLiteral("/dev/input")).entryList({QStringLiteral("event*")}, QDir::System);
    // Gone devices.
    for (const auto &node : m_devices.keys())
        if (!nodes.contains(QFileInfo(node).fileName())) drop(node);
    for (const auto &name : nodes) {
        const QString node = QStringLiteral("/dev/input/") + name;
        if (m_devices.contains(node)) continue;
        const int fd = open(node.toLatin1().constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        m_readable = true;
        const bool keyboard = hasBit(fd, EV_KEY, m_keys.value(0, KEY_LEFTCTRL), KEY_MAX) && hasBit(fd, EV_KEY, KEY_A, KEY_MAX);
        const bool pointer = hasBit(fd, EV_KEY, BTN_LEFT, KEY_MAX) || hasBit(fd, EV_REL, REL_WHEEL, REL_MAX);
        if (!keyboard && !pointer) {
            close(fd);
            continue;
        }
        watch(node, fd);
    }
}

void EvdevKey::watch(const QString &node, int fd) {
    auto *n = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(n, &QSocketNotifier::activated, this, [this, node, fd] { read(node, fd); });
    m_devices.insert(node, {fd, n});
}

void EvdevKey::drop(const QString &node) {
    const Device d = m_devices.take(node);
    if (!d.notifier) return;
    d.notifier->deleteLater();
    close(d.fd);
}

void EvdevKey::read(const QString &node, int fd) {
    input_event ev[64];
    ssize_t n;
    while ((n = ::read(fd, ev, sizeof ev)) > 0) {
        for (size_t i = 0; i < size_t(n) / sizeof(input_event); ++i) {
            const int type = ev[i].type, code = ev[i].code, value = ev[i].value;
            if (type == EV_REL && (code == REL_WHEEL || code == REL_HWHEEL) && m_down >= 0) {
                emit otherKey(-1);
                continue;
            }
            if (type != EV_KEY) continue;
            // value: 1 down, 0 up, 2 auto-repeat
            if (m_keys.contains(code)) {
                if (value == 1 && m_down < 0) m_down = code, emit pressed();
                else if (value == 0 && code == m_down) m_down = -1, emit released();
            } else if (value == 1 && (m_down >= 0 || code == KEY_ESC)) {
                emit otherKey(code);
            }
        }
    }
    // The device went away (keyboard unplugged, Bluetooth asleep).
    if (n < 0 && errno == ENODEV) {
        if (m_down >= 0) m_down = -1, emit released();
        drop(node);
    }
}
