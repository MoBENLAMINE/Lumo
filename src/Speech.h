#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <atomic>
#include <mutex>
#include <vector>

struct SherpaOnnxOfflineRecognizer;

// Offline speech recognition with NVIDIA's Parakeet TDT 0.6B v3 (25 languages)
// through sherpa-onnx, on the CPU. The model (~640 MB on disk, ~1 GB in RAM)
// loads on a worker thread when dictation starts and is dropped after a few
// idle minutes.
class Speech : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString modelDir READ modelDir NOTIFY modelChanged)
    Q_PROPERTY(bool modelPresent READ modelPresent NOTIFY modelChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    Q_PROPERTY(bool keepLoaded READ keepLoaded WRITE setKeepLoaded NOTIFY keepLoadedChanged)
    Q_PROPERTY(double downloadProgress READ downloadProgress NOTIFY downloadChanged)
    Q_PROPERTY(bool downloading READ downloading NOTIFY downloadChanged)
    Q_PROPERTY(QString downloadError READ downloadError NOTIFY downloadChanged)
public:
    explicit Speech(QObject *parent = nullptr);
    ~Speech() override;

    QString modelDir() const { return m_dir; }
    bool modelPresent() const;
    bool loaded() const;
    Q_INVOKABLE void refreshModel();
    bool keepLoaded() const { return m_keepLoaded; }
    void setKeepLoaded(bool on);
    // Fetches Parakeet (~640 MB) from the sherpa-onnx releases into Lumo's folder.
    Q_INVOKABLE void download();
    double downloadProgress() const { return m_progress; }
    bool downloading() const { return m_downloading; }
    QString downloadError() const { return m_downloadError; }

    // Starts loading in the background (no-op if already loaded).
    void warmUp();
    // Transcribes 16 kHz mono samples on a worker thread; emits `transcribed`.
    void transcribe(std::vector<float> samples, quint64 ticket);

signals:
    void modelChanged();
    void loadedChanged();
    void transcribed(quint64 ticket, const QString &text, const QString &error);
    void downloadChanged();
    void keepLoadedChanged();

private:
    bool ensureLoaded(); // worker thread, holds m_lock
    void unload();

    QString m_dir;
    mutable std::mutex m_lock;
    const SherpaOnnxOfflineRecognizer *m_recognizer = nullptr;
    std::atomic<bool> m_loaded{false}; // readable without the lock (the GUI never waits on a load)
    QTimer m_idle;
    double m_progress = 0;
    bool m_downloading = false, m_keepLoaded = false;
    QString m_downloadError;
};
