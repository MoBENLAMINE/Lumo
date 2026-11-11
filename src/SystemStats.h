#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVariantList>

class QNetworkAccessManager;

// CPU, GPU, memory, the busiest processes and network use, read from /proc and
// /sys. Nothing is sampled unless something shows it: every second while the
// System page is open (`live`), every 5 s while the resting pill shows a CPU,
// memory or network item (`light`), never otherwise.
class SystemStats : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool live READ live WRITE setLive NOTIFY liveChanged)
    Q_PROPERTY(bool light READ light WRITE setLight NOTIFY liveChanged)
    Q_PROPERTY(double cpu READ cpu NOTIFY sampled)            // 0..100
    Q_PROPERTY(double gpu READ gpu NOTIFY sampled)            // 0..100, -1 when unknown
    Q_PROPERTY(QString gpuName READ gpuName NOTIFY sampled)
    Q_PROPERTY(double memory READ memory NOTIFY sampled)      // 0..100
    Q_PROPERTY(qint64 memoryUsed READ memoryUsed NOTIFY sampled)   // bytes
    Q_PROPERTY(qint64 memoryTotal READ memoryTotal NOTIFY sampled)
    Q_PROPERTY(double download READ download NOTIFY sampled)  // bytes/s
    Q_PROPERTY(double upload READ upload NOTIFY sampled)
    Q_PROPERTY(QVariantList cpuHistory READ cpuHistory NOTIFY sampled)
    Q_PROPERTY(QVariantList memoryHistory READ memoryHistory NOTIFY sampled)
    Q_PROPERTY(QVariantList downloadHistory READ downloadHistory NOTIFY sampled)
    Q_PROPERTY(QVariantList uploadHistory READ uploadHistory NOTIFY sampled)
    // [{ name, cpu, memory }] — the five busiest, while live
    Q_PROPERTY(QVariantList processes READ processes NOTIFY sampled)
    Q_PROPERTY(QString localIp READ localIp NOTIFY addressesChanged)
    Q_PROPERTY(QString publicIp READ publicIp NOTIFY addressesChanged)
    Q_PROPERTY(bool showPublicIp READ showPublicIp WRITE setShowPublicIp NOTIFY addressesChanged)
    Q_PROPERTY(QString uptime READ uptime NOTIFY sampled)
public:
    explicit SystemStats(QObject *parent = nullptr);

    bool live() const { return m_live; }
    void setLive(bool on);
    bool light() const { return m_light; }
    void setLight(bool on);
    double cpu() const { return m_cpu; }
    double gpu() const { return m_gpu; }
    QString gpuName() const { return m_gpuName; }
    double memory() const { return m_memTotal ? 100.0 * m_memUsed / m_memTotal : 0; }
    qint64 memoryUsed() const { return m_memUsed; }
    qint64 memoryTotal() const { return m_memTotal; }
    double download() const { return m_down; }
    double upload() const { return m_up; }
    QVariantList cpuHistory() const { return m_cpuHist; }
    QVariantList memoryHistory() const { return m_memHist; }
    QVariantList downloadHistory() const { return m_downHist; }
    QVariantList uploadHistory() const { return m_upHist; }
    QVariantList processes() const { return m_procs; }
    QString localIp() const { return m_localIp; }
    QString publicIp() const { return m_publicIp; }
    bool showPublicIp() const;
    void setShowPublicIp(bool on);
    QString uptime() const;

    // "1.2 MB/s", "340 KB/s"
    Q_INVOKABLE QString rate(double bytesPerSecond) const;
    Q_INVOKABLE QString size(double bytes) const;

signals:
    void liveChanged();
    void sampled();
    void addressesChanged();

private:
    void schedule();
    void sample();
    void sampleCpu();
    void sampleMemory();
    void sampleNetwork(double seconds);
    void sampleGpu();
    void sampleProcesses(double seconds);
    void refreshAddresses();

    bool m_live = false, m_light = false;
    QTimer m_timer;
    QElapsedTimer m_since;
    // cpu
    quint64 m_cpuIdle = 0, m_cpuTotal = 0;
    double m_cpu = 0;
    // gpu
    QString m_gpuBusyPath, m_gpuName;
    double m_gpu = -1;
    bool m_gpuProbed = false, m_nvidia = false;
    // memory
    qint64 m_memUsed = 0, m_memTotal = 0;
    // network
    quint64 m_rx = 0, m_tx = 0;
    double m_down = 0, m_up = 0;
    // processes: pid → ticks last time
    QHash<int, quint64> m_ticks;
    QVariantList m_procs;
    QVariantList m_cpuHist, m_memHist, m_downHist, m_upHist;
    QString m_localIp, m_publicIp;
    QElapsedTimer m_publicAt;
    QNetworkAccessManager *m_net = nullptr;
};
