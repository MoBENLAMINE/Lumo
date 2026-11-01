#include "Speech.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QSettings>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>

#include <sherpa-onnx/c-api/c-api.h>

#include <cstring>
#include <thread>

namespace {
constexpr int kIdleUnloadMs = 5 * 60 * 1000;
constexpr int kSampleRate = 16000;
const QString kModel = QStringLiteral("parakeet-tdt-0.6b-v3-int8");
const QStringList kFiles = {QStringLiteral("encoder.int8.onnx"), QStringLiteral("decoder.int8.onnx"),
                            QStringLiteral("joiner.int8.onnx"), QStringLiteral("tokens.txt")};

bool complete(const QString &dir) {
    for (const auto &f : kFiles)
        if (!QFile::exists(dir + QLatin1Char('/') + f)) return false;
    return true;
}

int threads() {
    const int forced = qEnvironmentVariableIntValue("LUMO_SPEECH_THREADS");
    // 4 measured fastest on a 2P+8E i5-1345U; more threads spill onto E-cores and slow down.
    return forced > 0 ? forced : std::clamp(QThread::idealThreadCount(), 1, 4);
}
}

Speech::Speech(QObject *parent) : QObject(parent) {
    m_idle.setSingleShot(true);
    m_idle.setInterval(kIdleUnloadMs);
    connect(&m_idle, &QTimer::timeout, this, &Speech::unload);
    refreshModel();
    m_keepLoaded = QSettings().value(QStringLiteral("dictation/keepLoaded"), false).toBool();
    if (m_keepLoaded && modelPresent()) QTimer::singleShot(3000, this, &Speech::warmUp);
}

Speech::~Speech() {
    std::lock_guard g(m_lock);
    if (m_recognizer) SherpaOnnxDestroyOfflineRecognizer(m_recognizer);
}

void Speech::refreshModel() {
    // Lumo's own copy first, then the one Coucou may already have downloaded.
    const QString data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QStringList candidates = {data + QStringLiteral("/lumo/models/") + kModel,
                                    data + QStringLiteral("/coucou/Models/") + kModel};
    QString found = candidates.first();
    for (const auto &c : candidates)
        if (complete(c)) {
            found = c;
            break;
        }
    m_dir = found;
    emit modelChanged();
}

bool Speech::modelPresent() const { return complete(m_dir); }

bool Speech::loaded() const { return m_loaded; }

bool Speech::ensureLoaded() {
    if (m_recognizer) return true;
    if (!complete(m_dir)) return false;
    const QByteArray enc = (m_dir + QStringLiteral("/encoder.int8.onnx")).toUtf8();
    const QByteArray dec = (m_dir + QStringLiteral("/decoder.int8.onnx")).toUtf8();
    const QByteArray join = (m_dir + QStringLiteral("/joiner.int8.onnx")).toUtf8();
    const QByteArray tokens = (m_dir + QStringLiteral("/tokens.txt")).toUtf8();

    SherpaOnnxOfflineRecognizerConfig config;
    std::memset(&config, 0, sizeof config);
    config.feat_config.sample_rate = kSampleRate;
    config.feat_config.feature_dim = 80;
    config.model_config.transducer.encoder = enc.constData();
    config.model_config.transducer.decoder = dec.constData();
    config.model_config.transducer.joiner = join.constData();
    config.model_config.tokens = tokens.constData();
    config.model_config.num_threads = threads();
    config.model_config.provider = "cpu";
    config.model_config.model_type = "nemo_transducer";
    config.decoding_method = "greedy_search";

    QElapsedTimer t;
    t.start();
    m_recognizer = SherpaOnnxCreateOfflineRecognizer(&config);
    qInfo("Parakeet %s in %lld ms", m_recognizer ? "loaded" : "failed to load", t.elapsed());
    m_loaded = m_recognizer != nullptr;
    QMetaObject::invokeMethod(this, &Speech::loadedChanged, Qt::QueuedConnection);
    return m_recognizer != nullptr;
}

void Speech::warmUp() {
    m_idle.start();
    std::thread([this] {
        std::lock_guard g(m_lock);
        ensureLoaded();
    }).detach();
}

void Speech::transcribe(std::vector<float> samples, quint64 ticket) {
    m_idle.start();
    std::thread([this, samples = std::move(samples), ticket] {
        QString text, error;
        {
            std::lock_guard g(m_lock);
            if (!ensureLoaded()) {
                error = modelPresent() ? tr("Parakeet couldn't be loaded.") : tr("The Parakeet model isn't installed.");
            } else {
                QElapsedTimer t;
                t.start();
                const SherpaOnnxOfflineStream *stream = SherpaOnnxCreateOfflineStream(m_recognizer);
                SherpaOnnxAcceptWaveformOffline(stream, kSampleRate, samples.data(), int32_t(samples.size()));
                SherpaOnnxDecodeOfflineStream(m_recognizer, stream);
                const SherpaOnnxOfflineRecognizerResult *r = SherpaOnnxGetOfflineStreamResult(stream);
                text = QString::fromUtf8(r && r->text ? r->text : "").trimmed();
                if (r) SherpaOnnxDestroyOfflineRecognizerResult(r);
                SherpaOnnxDestroyOfflineStream(stream);
                qInfo("Transcribed %.1f s of audio in %lld ms", samples.size() / double(kSampleRate), t.elapsed());
            }
        }
        QMetaObject::invokeMethod(this, [this, ticket, text, error] { emit transcribed(ticket, text, error); }, Qt::QueuedConnection);
    }).detach();
}

void Speech::setKeepLoaded(bool on) {
    if (on == m_keepLoaded) return;
    m_keepLoaded = on;
    QSettings().setValue(QStringLiteral("dictation/keepLoaded"), on);
    if (on && modelPresent()) warmUp();
    else if (!on) m_idle.start();
    emit keepLoadedChanged();
}

void Speech::unload() {
    if (m_keepLoaded) return;
    std::unique_lock g(m_lock, std::try_to_lock);
    if (!g.owns_lock()) {
        m_idle.start(); // busy: try again later
        return;
    }
    if (!m_recognizer) return;
    SherpaOnnxDestroyOfflineRecognizer(m_recognizer);
    m_recognizer = nullptr;
    m_loaded = false;
    qInfo("Parakeet unloaded after idling");
    emit loadedChanged();
}

void Speech::download() {
    if (m_downloading) return;
    const QString root = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/lumo/models");
    QDir().mkpath(root);
    const QString archive = root + QStringLiteral("/parakeet.tar.bz2.part");
    auto *file = new QFile(archive, this);
    if (!file->open(QIODevice::WriteOnly)) {
        m_downloadError = tr("Can't write to %1.").arg(root);
        emit downloadChanged();
        return;
    }
    m_downloading = true;
    m_progress = 0;
    m_downloadError.clear();
    emit downloadChanged();

    auto *net = new QNetworkAccessManager(this);
    QNetworkRequest req(QUrl(QStringLiteral(
        "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8.tar.bz2")));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = net->get(req);
    connect(reply, &QNetworkReply::readyRead, file, [reply, file] { file->write(reply->readAll()); });
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 got, qint64 total) {
        m_progress = total > 0 ? double(got) / total : 0;
        emit downloadChanged();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, net, root, archive] {
        file->write(reply->readAll());
        file->close();
        file->deleteLater();
        reply->deleteLater();
        net->deleteLater();
        auto fail = [this, archive](const QString &why) {
            QFile::remove(archive);
            m_downloading = false;
            m_downloadError = why;
            emit downloadChanged();
        };
        if (reply->error() != QNetworkReply::NoError) return fail(reply->errorString());
        auto *tar = new QProcess(this);
        tar->setWorkingDirectory(root);
        connect(tar, &QProcess::finished, this, [this, tar, root, archive, fail](int code) {
            tar->deleteLater();
            QFile::remove(archive);
            if (code != 0) return fail(tr("Couldn't unpack the model."));
            QDir(root).rename(QStringLiteral("sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8"), kModel);
            m_downloading = false;
            emit downloadChanged();
            refreshModel();
        });
        tar->start(QStringLiteral("tar"), {QStringLiteral("-xjf"), archive});
    });
}
