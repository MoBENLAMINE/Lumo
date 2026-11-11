#include "SystemStats.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QProcess>
#include <QSettings>
#include <QTextStream>

#include <algorithm>
#include <unistd.h>

namespace {
constexpr int kHistory = 40;

QByteArray readFile(const QString &path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

void push(QVariantList &list, double value) {
    list.append(value);
    while (list.size() > kHistory) list.removeFirst();
}
}

SystemStats::SystemStats(QObject *parent) : QObject(parent) {
    connect(&m_timer, &QTimer::timeout, this, &SystemStats::sample);
    m_timer.setTimerType(Qt::CoarseTimer);   // lets the kernel batch the wakeups
}

void SystemStats::setLive(bool on) {
    if (on == m_live) return;
    m_live = on;
    if (on) refreshAddresses();
    else { m_procs.clear(); m_ticks.clear(); }
    schedule();
    emit liveChanged();
}

void SystemStats::setLight(bool on) {
    if (on == m_light) return;
    m_light = on;
    schedule();
    emit liveChanged();
}

void SystemStats::schedule() {
    if (!m_live && !m_light) {
        m_timer.stop();
        return;
    }
    const int interval = m_live ? 1000 : 5000;
    if (m_timer.isActive() && m_timer.interval() == interval) return;
    const bool wasRunning = m_timer.isActive();
    m_timer.start(interval);
    if (!wasRunning) {
        m_since.invalidate();
        sample();   // a first reading now; rates need a second one
    }
}

void SystemStats::sample() {
    const double seconds = m_since.isValid() ? std::max(0.2, m_since.restart() / 1000.0) : 0;
    if (!m_since.isValid()) m_since.start();
    sampleCpu();
    sampleMemory();
    sampleNetwork(seconds);
    if (m_live) {
        sampleGpu();
        sampleProcesses(seconds);
    }
    push(m_cpuHist, m_cpu);
    push(m_memHist, memory());
    push(m_downHist, m_down);
    push(m_upHist, m_up);
    emit sampled();
}

void SystemStats::sampleCpu() {
    const QByteArray stat = readFile(QStringLiteral("/proc/stat"));
    const int nl = stat.indexOf('\n');
    const QList<QByteArray> f = stat.left(nl).simplified().split(' ');
    if (f.size() < 5) return;
    quint64 total = 0;
    for (int i = 1; i < f.size(); ++i) total += f[i].toULongLong();
    const quint64 idle = f[4].toULongLong() + (f.size() > 5 ? f[5].toULongLong() : 0);   // idle + iowait
    if (m_cpuTotal && total > m_cpuTotal)
        m_cpu = 100.0 * (1.0 - double(idle - m_cpuIdle) / double(total - m_cpuTotal));
    m_cpu = std::clamp(m_cpu, 0.0, 100.0);
    m_cpuIdle = idle;
    m_cpuTotal = total;
}

void SystemStats::sampleMemory() {
    const QByteArray info = readFile(QStringLiteral("/proc/meminfo"));
    qint64 total = 0, available = 0;
    for (const QByteArray &line : info.split('\n')) {
        if (line.startsWith("MemTotal:")) total = line.mid(9).trimmed().split(' ').first().toLongLong() * 1024;
        else if (line.startsWith("MemAvailable:")) available = line.mid(13).trimmed().split(' ').first().toLongLong() * 1024;
    }
    m_memTotal = total;
    m_memUsed = std::max<qint64>(0, total - available);
}

void SystemStats::sampleNetwork(double seconds) {
    const QByteArray dev = readFile(QStringLiteral("/proc/net/dev"));
    quint64 rx = 0, tx = 0;
    for (const QByteArray &line : dev.split('\n')) {
        const int colon = line.indexOf(':');
        if (colon < 0) continue;
        const QByteArray name = line.left(colon).trimmed();
        // only real links: not loopback, nor the bridges and tunnels of VMs and containers
        if (name == "lo" || name.startsWith("veth") || name.startsWith("docker") || name.startsWith("br-")
            || name.startsWith("virbr")) continue;
        const QList<QByteArray> f = line.mid(colon + 1).simplified().split(' ');
        if (f.size() < 9) continue;
        rx += f[0].toULongLong();
        tx += f[8].toULongLong();
    }
    if (seconds > 0 && m_rx && rx >= m_rx && tx >= m_tx) {
        m_down = (rx - m_rx) / seconds;
        m_up = (tx - m_tx) / seconds;
    }
    m_rx = rx;
    m_tx = tx;
}

void SystemStats::sampleGpu() {
    if (!m_gpuProbed) {
        m_gpuProbed = true;
        // AMD and recent Intel expose a busy percentage in sysfs.
        const QDir drm(QStringLiteral("/sys/class/drm"));
        for (const QString &card : drm.entryList({QStringLiteral("card?")}, QDir::Dirs)) {
            const QString busy = drm.filePath(card + QStringLiteral("/device/gpu_busy_percent"));
            if (QFile::exists(busy)) {
                m_gpuBusyPath = busy;
                const QByteArray vendor = readFile(drm.filePath(card + QStringLiteral("/device/vendor"))).trimmed();
                m_gpuName = vendor == "0x1002" ? QStringLiteral("AMD") : vendor == "0x8086" ? QStringLiteral("Intel") : QStringLiteral("GPU");
                break;
            }
        }
        // NVIDIA: ask nvidia-smi, only while the page is open.
        if (m_gpuBusyPath.isEmpty() && QFile::exists(QStringLiteral("/proc/driver/nvidia/version"))) {
            m_nvidia = true;
            m_gpuName = QStringLiteral("NVIDIA");
        }
    }
    if (!m_gpuBusyPath.isEmpty()) {
        m_gpu = readFile(m_gpuBusyPath).trimmed().toDouble();
    } else if (m_nvidia) {
        auto *p = new QProcess(this);
        connect(p, &QProcess::finished, this, [this, p] {
            const double v = p->readAllStandardOutput().trimmed().split('\n').value(0).toDouble();
            m_gpu = std::clamp(v, 0.0, 100.0);
            p->deleteLater();
        });
        connect(p, &QProcess::errorOccurred, p, [this, p] { m_nvidia = false; m_gpu = -1; p->deleteLater(); });
        p->start(QStringLiteral("nvidia-smi"), {QStringLiteral("--query-gpu=utilization.gpu"), QStringLiteral("--format=csv,noheader,nounits")});
    }
}

void SystemStats::sampleProcesses(double seconds) {
    static const double hz = double(sysconf(_SC_CLK_TCK));
    static const long page = sysconf(_SC_PAGESIZE);
    const QDir proc(QStringLiteral("/proc"));
    QHash<int, quint64> ticks;
    struct Row { QString name; double cpu; qint64 mem; };
    QHash<QString, Row> byName;   // one row per program: a browser's many processes add up
    const uint uid = getuid();
    for (const QString &entry : proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool ok = false;
        const int pid = entry.toInt(&ok);
        if (!ok) continue;
        const QByteArray stat = readFile(QStringLiteral("/proc/%1/stat").arg(pid));
        const int open = stat.indexOf('('), close = stat.lastIndexOf(')');
        if (open < 0 || close < 0) continue;
        const QString name = QString::fromUtf8(stat.mid(open + 1, close - open - 1));
        const QList<QByteArray> f = stat.mid(close + 2).split(' ');
        if (f.size() < 22) continue;
        const quint64 t = f[11].toULongLong() + f[12].toULongLong();   // utime + stime
        ticks.insert(pid, t);
        if (seconds <= 0 || !m_ticks.contains(pid)) continue;
        // only this user's programs: the system's own daemons aren't yours to watch
        QFileInfo owner(QStringLiteral("/proc/%1").arg(pid));
        if (owner.ownerId() != uid) continue;
        const double cpu = 100.0 * double(t - m_ticks.value(pid)) / hz / seconds;
        const qint64 rss = f[21].toLongLong() * page;
        Row &row = byName[name];
        row.name = name;
        row.cpu += cpu;
        row.mem += rss;
    }
    m_ticks = ticks;
    if (seconds <= 0) return;
    QList<Row> rows = byName.values();
    std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.cpu != b.cpu ? a.cpu > b.cpu : a.mem > b.mem; });
    m_procs.clear();
    for (int i = 0; i < std::min<qsizetype>(5, rows.size()); ++i)
        m_procs.append(QVariantMap{{QStringLiteral("name"), rows[i].name}, {QStringLiteral("cpu"), rows[i].cpu},
                                   {QStringLiteral("memory"), double(rows[i].mem)}});
}

void SystemStats::refreshAddresses() {
    QString local;
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp) || (iface.flags() & QNetworkInterface::IsLoopBack)) continue;
        if (iface.name().startsWith(QLatin1String("docker")) || iface.name().startsWith(QLatin1String("veth"))
            || iface.name().startsWith(QLatin1String("virbr"))) continue;
        for (const QNetworkAddressEntry &e : iface.addressEntries())
            if (e.ip().protocol() == QAbstractSocket::IPv4Protocol) { local = e.ip().toString(); break; }
        if (!local.isEmpty()) break;
    }
    m_localIp = local;
    emit addressesChanged();
    // The public address: one request, at most every 10 minutes, if wanted.
    if (!showPublicIp() || (m_publicAt.isValid() && m_publicAt.elapsed() < 10 * 60 * 1000)) return;
    m_publicAt.start();
    if (!m_net) m_net = new QNetworkAccessManager(this);
    QNetworkReply *reply = m_net->get(QNetworkRequest(QUrl(QStringLiteral("https://api.ipify.org"))));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) { m_publicAt.invalidate(); return; }
        const QString ip = QString::fromLatin1(reply->readAll()).trimmed();
        if (QHostAddress(ip).isNull()) return;
        m_publicIp = ip;
        emit addressesChanged();
    });
}

bool SystemStats::showPublicIp() const { return QSettings().value(QStringLiteral("system/publicIp"), true).toBool(); }

void SystemStats::setShowPublicIp(bool on) {
    QSettings().setValue(QStringLiteral("system/publicIp"), on);
    if (!on) m_publicIp.clear();
    else if (m_live) refreshAddresses();
    emit addressesChanged();
}

QString SystemStats::uptime() const {
    const double s = readFile(QStringLiteral("/proc/uptime")).split(' ').value(0).toDouble();
    const int days = int(s / 86400), hours = int(s / 3600) % 24, minutes = int(s / 60) % 60;
    if (days > 0) return tr("%1 d %2 h").arg(days).arg(hours);
    if (hours > 0) return tr("%1 h %2 min").arg(hours).arg(minutes);
    return tr("%1 min").arg(minutes);
}

QString SystemStats::rate(double b) const {
    return size(b) + tr("/s");
}

QString SystemStats::size(double b) const {
    if (b >= 1e9) return QStringLiteral("%1 GB").arg(b / 1e9, 0, 'f', 1);
    if (b >= 1e6) return QStringLiteral("%1 MB").arg(b / 1e6, 0, 'f', 1);
    if (b >= 1e3) return QStringLiteral("%1 KB").arg(b / 1e3, 0, 'f', 0);
    return QStringLiteral("%1 B").arg(qint64(b));
}
