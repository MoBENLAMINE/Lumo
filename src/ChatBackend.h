#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QProcess>

#include "MapListModel.h"

class QNetworkReply;

// The island's chat. Two ways to reach a model, neither with an API key:
//  * "claude" — through the Claude Code CLI you're signed in to, so it runs on
//               your Claude subscription
//  * "notion" — the same, with your claude.ai Notion connector (read-only
//               unless you allow edits)
//  * "ollama" — a local Ollama server
class ChatBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(MapListModel *messages READ messages CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString provider READ provider WRITE setProvider NOTIFY providerChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QStringList models READ models NOTIFY modelsChanged)
    Q_PROPERTY(QStringList attachments READ attachments NOTIFY attachmentsChanged)
    Q_PROPERTY(bool notionEdits READ notionEdits WRITE setNotionEdits NOTIFY notionEditsChanged)

public:
    explicit ChatBackend(QObject *parent = nullptr);
    ~ChatBackend() override;

    MapListModel *messages() { return &m_messages; }
    bool busy() const { return m_busy; }
    QString provider() const { return m_provider; }
    void setProvider(const QString &p);
    QString model() const { return m_model; }
    void setModel(const QString &m);
    QStringList models() const { return m_models; }
    QStringList attachments() const { return m_attachments; }
    bool notionEdits() const { return m_notionEdits; }
    void setNotionEdits(bool on);

    Q_INVOKABLE void send(const QString &text);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void clear();
    Q_INVOKABLE void refreshModels();
    Q_INVOKABLE void attach(const QUrl &file);
    Q_INVOKABLE void detach(int index);

signals:
    void busyChanged();
    void providerChanged();
    void modelChanged();
    void modelsChanged();
    void attachmentsChanged();
    void notionEditsChanged();
    void usage(double five, qint64 fiveReset, double seven, qint64 sevenReset);
    void replied();

private:
    void appendDelta(const QString &delta);
    void addStep(const QString &text);
    void finish(const QString &error = {});
    void setBusy(bool b);
    QString withAttachments(const QString &text);
    void runClaude(const QString &prompt);
    void runOllama();
    void readClaudeLine(const QByteArray &line);
    QJsonArray history() const;

    MapListModel m_messages;
    QString m_provider, m_model;
    QStringList m_models, m_attachments;
    QString m_claudeSession, m_current, m_lastPrompt;
    int m_notionTries = 0;
    bool m_retrying = false;
    bool m_busy = false, m_streamed = false, m_afterTool = false, m_notionEdits = false;
    int m_nextId = 1;
    QPointer<QProcess> m_proc;
    QPointer<QNetworkReply> m_reply;
    QByteArray m_buffer;
    QNetworkAccessManager m_net;
};
