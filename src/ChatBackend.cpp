#include "ChatBackend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QNetworkReply>
#include <QSettings>
#include <QUrl>

namespace {
constexpr qint64 kMaxAttachment = 200 * 1024;

const QString kSystemPrompt = QStringLiteral(
    "You are Lumo, a small friendly assistant living at the top of the user's screen on Linux "
    "(KDE Plasma, Wayland). Answer briefly and clearly. Use Markdown only when it helps.");

const QString kNotionPrompt = QStringLiteral(
    "You are Lumo, a small assistant at the top of the user's screen, with access to the user's Notion workspace "
    "through the Notion tools. Search and read Notion to answer; cite page titles. Answer briefly, in Markdown when it helps. "
    "Before you create, change, move or delete anything in Notion, say exactly what you will do and wait for the user's yes "
    "in their next message.");

// The Notion connector's tools that only read. Edits need the switch.
const QStringList kNotionReadTools = {
    QStringLiteral("notion-search"), QStringLiteral("notion-fetch"), QStringLiteral("notion-list-recent-pages"),
    QStringLiteral("notion-list-favorite-pages"), QStringLiteral("notion-list-private-pages"),
    QStringLiteral("notion-list-shared-pages"), QStringLiteral("notion-list-page-backlinks"), QStringLiteral("notion-get-comments"),
    QStringLiteral("notion-get-self"), QStringLiteral("notion-get-users"), QStringLiteral("notion-get-teams"),
    QStringLiteral("notion-query-data-sources"), QStringLiteral("notion-query-multiple-data-sources"),
    QStringLiteral("notion-query-meeting-notes"), QStringLiteral("notion-memory-search"), QStringLiteral("notion-view-version"),
    QStringLiteral("notion-get-version-changes"), QStringLiteral("notion-calendar-list-events"),
    QStringLiteral("notion-calendar-list-calendars"), QStringLiteral("notion-get-async-task"),
};
const QString kNotionPrefix = QStringLiteral("mcp__claude_ai_Notion__");

// "notion-create-pages" → "Creating pages in Notion", for the chat's step lines.
QString describeNotionTool(const QString &tool, const QJsonObject &input) {
    const QString t = QString(tool).remove(kNotionPrefix);
    const QString q = input.value(QStringLiteral("query")).toString();
    if (t == QLatin1String("notion-search")) return q.isEmpty() ? QObject::tr("Searching Notion") : QObject::tr("Searching Notion for “%1”").arg(q);
    if (t == QLatin1String("notion-fetch")) return QObject::tr("Reading a Notion page");
    if (t.startsWith(QLatin1String("notion-list-"))) return QObject::tr("Listing pages");
    if (t.startsWith(QLatin1String("notion-query"))) return QObject::tr("Querying a database");
    if (t.startsWith(QLatin1String("notion-create"))) return QObject::tr("Creating in Notion");
    if (t.startsWith(QLatin1String("notion-update"))) return QObject::tr("Updating Notion");
    if (t.startsWith(QLatin1String("notion-delete"))) return QObject::tr("Deleting in Notion");
    if (t.startsWith(QLatin1String("notion-calendar"))) return QObject::tr("Checking your calendar");
    return t.mid(7).replace(u'-', u' ');
}

QStringList defaultModels(const QString &provider) {
    if (provider == QLatin1String("claude") || provider == QLatin1String("notion"))
        return {QStringLiteral("sonnet"), QStringLiteral("opus"), QStringLiteral("haiku")};
    return {};
}
}

ChatBackend::ChatBackend(QObject *parent)
    : QObject(parent),
      m_messages({QStringLiteral("msgId"), QStringLiteral("role"), QStringLiteral("text"), QStringLiteral("streaming"),
                  QStringLiteral("error")},
                 QStringLiteral("msgId")) {
    QSettings s;
    m_provider = s.value(QStringLiteral("chat/provider"), QStringLiteral("claude")).toString();
    if (m_provider != QLatin1String("ollama") && m_provider != QLatin1String("notion")) m_provider = QStringLiteral("claude");
    m_notionEdits = s.value(QStringLiteral("chat/notionEdits"), false).toBool();
    m_models = defaultModels(m_provider);
    m_model = s.value(QStringLiteral("chat/model/") + m_provider, m_models.value(0)).toString();
    if (m_provider == QLatin1String("ollama")) refreshModels();
}

ChatBackend::~ChatBackend() {
    if (m_proc) m_proc->kill();
}

void ChatBackend::setProvider(const QString &requested) {
    const QString p = requested == QLatin1String("ollama") || requested == QLatin1String("notion") ? requested : QStringLiteral("claude");
    if (p == m_provider) return;
    stop();
    m_provider = p;
    m_claudeSession.clear();
    QSettings().setValue(QStringLiteral("chat/provider"), p);
    m_models = defaultModels(p);
    emit modelsChanged();
    setModel(QSettings().value(QStringLiteral("chat/model/") + p, m_models.value(0)).toString());
    if (p == QLatin1String("ollama")) refreshModels();
    emit providerChanged();
}

void ChatBackend::setModel(const QString &m) {
    if (m == m_model) return;
    m_model = m;
    m_claudeSession.clear();
    QSettings().setValue(QStringLiteral("chat/model/") + m_provider, m);
    emit modelChanged();
}

void ChatBackend::refreshModels() {
    if (m_provider != QLatin1String("ollama")) return;
    QNetworkReply *reply = m_net.get(QNetworkRequest(QUrl(QStringLiteral("http://127.0.0.1:11434/api/tags"))));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        QStringList names;
        for (const auto &m : QJsonDocument::fromJson(reply->readAll()).object().value(QStringLiteral("models")).toArray())
            names << m.toObject().value(QStringLiteral("name")).toString();
        m_models = names;
        emit modelsChanged();
        if (!names.contains(m_model) && !names.isEmpty()) setModel(names.first());
    });
}

void ChatBackend::attach(const QUrl &file) {
    const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    if (!QFileInfo(path).isFile() || m_attachments.contains(path)) return;
    m_attachments << path;
    emit attachmentsChanged();
}

void ChatBackend::detach(int index) {
    if (index < 0 || index >= m_attachments.size()) return;
    m_attachments.removeAt(index);
    emit attachmentsChanged();
}

QString ChatBackend::withAttachments(const QString &text) {
    if (m_attachments.isEmpty()) return text;
    QString out;
    QMimeDatabase mime;
    for (const auto &path : std::as_const(m_attachments)) {
        QFile f(path);
        const QString name = QFileInfo(path).fileName();
        if (!mime.mimeTypeForFile(path).inherits(QStringLiteral("text/plain")) || !f.open(QIODevice::ReadOnly)) {
            out += QStringLiteral("[Attached file: %1 (%2), not readable as text]\n\n").arg(path, mime.mimeTypeForFile(path).name());
            continue;
        }
        const QByteArray data = f.read(kMaxAttachment);
        out += QStringLiteral("File `%1`:\n```\n%2\n```\n\n").arg(name, QString::fromUtf8(data));
    }
    m_attachments.clear();
    emit attachmentsChanged();
    return out + text;
}

void ChatBackend::send(const QString &raw) {
    const QString text = raw.trimmed();
    if (text.isEmpty() || m_busy) return;
    const QString shown = m_attachments.isEmpty()
                              ? text
                              : text + QStringLiteral("\n📎 ") + [this] {
                                    QStringList n;
                                    for (const auto &a : std::as_const(m_attachments)) n << QFileInfo(a).fileName();
                                    return n.join(QStringLiteral(", "));
                                }();
    const QString prompt = withAttachments(text);
    m_messages.upsert({{QStringLiteral("msgId"), QString::number(m_nextId++)},
                       {QStringLiteral("role"), QStringLiteral("user")},
                       {QStringLiteral("text"), shown},
                       {QStringLiteral("prompt"), prompt},
                       {QStringLiteral("streaming"), false},
                       {QStringLiteral("error"), false}});
    m_current = QString::number(m_nextId++);
    m_streamed = false;
    m_afterTool = false;
    m_messages.upsert({{QStringLiteral("msgId"), m_current},
                       {QStringLiteral("role"), QStringLiteral("assistant")},
                       {QStringLiteral("text"), QString()},
                       {QStringLiteral("streaming"), true},
                       {QStringLiteral("error"), false}});
    setBusy(true);
    if (m_provider == QLatin1String("ollama")) runOllama();
    else {
        m_notionTries = 0;
        runClaude(prompt);
    }
}

void ChatBackend::stop() {
    if (m_proc) m_proc->kill();
    if (m_reply) m_reply->abort();
}

void ChatBackend::clear() {
    stop();
    for (int i = m_messages.count() - 1; i >= 0; --i) m_messages.remove(m_messages.row(i).value(QStringLiteral("msgId")).toString());
    m_claudeSession.clear();
}

void ChatBackend::setBusy(bool b) {
    if (b == m_busy) return;
    m_busy = b;
    emit busyChanged();
}

void ChatBackend::appendDelta(const QString &delta) {
    const int i = m_messages.indexOf(m_current);
    if (i < 0 || delta.isEmpty()) return;
    m_messages.upsert({{QStringLiteral("msgId"), m_current},
                       {QStringLiteral("text"), m_messages.row(i).value(QStringLiteral("text")).toString() + delta}});
}

// A grey "doing…" line above the answer being written.
void ChatBackend::addStep(const QString &text) {
    const int at = m_messages.indexOf(m_current);
    m_messages.upsert({{QStringLiteral("msgId"), QString::number(m_nextId++)},
                       {QStringLiteral("role"), QStringLiteral("tool")},
                       {QStringLiteral("text"), text},
                       {QStringLiteral("streaming"), false},
                       {QStringLiteral("error"), false}},
                      at < 0 ? -1 : at);
}

void ChatBackend::setNotionEdits(bool on) {
    if (on == m_notionEdits) return;
    m_notionEdits = on;
    QSettings().setValue(QStringLiteral("chat/notionEdits"), on);
    emit notionEditsChanged();
}

void ChatBackend::finish(const QString &error) {
    if (!m_busy) return;
    const int i = m_messages.indexOf(m_current);
    if (i >= 0) {
        QVariantMap v{{QStringLiteral("msgId"), m_current}, {QStringLiteral("streaming"), false}};
        if (!error.isEmpty() && m_messages.row(i).value(QStringLiteral("text")).toString().isEmpty()) {
            v.insert(QStringLiteral("text"), error);
            v.insert(QStringLiteral("error"), true);
        }
        m_messages.upsert(v);
    }
    setBusy(false);
    emit replied();
}

QJsonArray ChatBackend::history() const {
    QJsonArray out;
    for (const auto &r : m_messages.rows()) {
        if (r.value(QStringLiteral("msgId")).toString() == m_current || r.value(QStringLiteral("error")).toBool() ||
            r.value(QStringLiteral("role")).toString() == QLatin1String("tool"))
            continue;
        const bool user = r.value(QStringLiteral("role")).toString() == QLatin1String("user");
        const QString content = user ? r.value(QStringLiteral("prompt")).toString() : r.value(QStringLiteral("text")).toString();
        if (content.isEmpty()) continue;
        out.append(QJsonObject{{QStringLiteral("role"), user ? QStringLiteral("user") : QStringLiteral("assistant")},
                               {QStringLiteral("content"), content}});
    }
    return out;
}

// ---------------------------------------------------------------- Claude Code CLI

void ChatBackend::runClaude(const QString &prompt) {
    m_lastPrompt = prompt;
    m_retrying = false;
    auto *proc = new QProcess(this);
    m_proc = proc;
    m_buffer.clear();
    proc->setWorkingDirectory(QDir::homePath());
    // Always the Pro/Max subscription: with an API key in the environment the
    // CLI would bill that key instead.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const char *var : {"ANTHROPIC_API_KEY", "ANTHROPIC_AUTH_TOKEN", "CLAUDE_CODE_USE_BEDROCK", "CLAUDE_CODE_USE_VERTEX"})
        env.remove(QString::fromLatin1(var));
    proc->setProcessEnvironment(env);
    const bool notion = m_provider == QLatin1String("notion");
    QStringList args{QStringLiteral("-p"), QStringLiteral("--output-format"), QStringLiteral("stream-json"),
                     QStringLiteral("--verbose"), QStringLiteral("--include-partial-messages"),
                     QStringLiteral("--model"), m_model, QStringLiteral("--system-prompt"), notion ? kNotionPrompt : kSystemPrompt,
                     // No built-in tools (files, shell…) in either chat.
                     QStringLiteral("--tools"), QString(), QStringLiteral("--disable-slash-commands")};
    if (notion) {
        // Your claude.ai Notion connector, on your Pro login. It connects lazily:
        // wait for it (and only it) before the first turn. Hooks off, or this
        // chat would show up as an agent session.
        env.insert(QStringLiteral("CLAUDE_CODE_MCP_CONNECTOR_PREWAIT_MS"), QStringLiteral("20000"));
        env.insert(QStringLiteral("CLAUDE_CODE_MCP_PREWAIT_SERVERS"), QStringLiteral("claude.ai Notion"));
        env.insert(QStringLiteral("CLAUDE_CODE_MCP_PREWAIT_SERVERS_MS"), QStringLiteral("20000"));
        proc->setProcessEnvironment(env);
        args << QStringLiteral("--settings") << QStringLiteral(R"({"disableAllHooks":true})");
        QStringList allowed;
        if (m_notionEdits) allowed << QStringLiteral("mcp__claude_ai_Notion");
        else
            for (const auto &t : kNotionReadTools) allowed << kNotionPrefix + t;
        args << QStringLiteral("--allowedTools") << allowed.join(u',');
    } else {
        // A plain chat: no hooks (they'd show up as a session), no MCP.
        args << QStringLiteral("--setting-sources") << QString() << QStringLiteral("--strict-mcp-config");
    }
    if (!m_claudeSession.isEmpty()) args << QStringLiteral("--resume") << m_claudeSession;
    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc] {
        m_buffer += proc->readAllStandardOutput();
        qsizetype nl;
        while ((nl = m_buffer.indexOf('\n')) >= 0) {
            readClaudeLine(m_buffer.left(nl));
            m_buffer.remove(0, nl + 1);
        }
    });
    connect(proc, &QProcess::finished, this, [this, proc](int code, QProcess::ExitStatus) {
        const QString err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
        proc->deleteLater();
        // The Notion connector wasn't up yet: start over, quietly.
        if (m_retrying) {
            runClaude(m_lastPrompt);
            return;
        }
        finish(code == 0 ? QString() : (err.isEmpty() ? tr("Claude Code stopped (exit %1).").arg(code) : err.left(400)));
    });
    connect(proc, &QProcess::errorOccurred, this, [this, proc](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            proc->deleteLater();
            finish(tr("Couldn't start `claude`. Install Claude Code and sign in, or pick another provider."));
        }
    });
    proc->start(QStringLiteral("claude"), args);
    proc->write(prompt.toUtf8());
    proc->closeWriteChannel();
}

void ChatBackend::readClaudeLine(const QByteArray &line) {
    const QJsonObject o = QJsonDocument::fromJson(line).object();
    const QString type = o.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("stream_event")) {
        const QJsonObject ev = o.value(QStringLiteral("event")).toObject();
        const QJsonObject delta = ev.value(QStringLiteral("delta")).toObject();
        if (ev.value(QStringLiteral("type")).toString() == QLatin1String("content_block_delta") &&
            delta.value(QStringLiteral("type")).toString() == QLatin1String("text_delta")) {
            m_streamed = true;
            // Text after a tool call is a new paragraph.
            appendDelta((std::exchange(m_afterTool, false) ? QStringLiteral("\n\n") : QString()) + delta.value(QStringLiteral("text")).toString());
        }
    } else if (type == QLatin1String("assistant")) {
        for (const auto &c : o.value(QStringLiteral("message")).toObject().value(QStringLiteral("content")).toArray()) {
            const QJsonObject block = c.toObject();
            const QString kind = block.value(QStringLiteral("type")).toString();
            if (kind == QLatin1String("text") && !m_streamed) appendDelta(block.value(QStringLiteral("text")).toString());
            if (kind == QLatin1String("tool_use")) {
                const QString name = block.value(QStringLiteral("name")).toString();
                addStep(name.startsWith(kNotionPrefix) ? describeNotionTool(name, block.value(QStringLiteral("input")).toObject())
                                                        : name.section(QStringLiteral("__"), -1));
                m_afterTool = true;
            }
        }
    } else if (type == QLatin1String("result")) {
        m_claudeSession = o.value(QStringLiteral("session_id")).toString();
        if (o.value(QStringLiteral("is_error")).toBool()) appendDelta(o.value(QStringLiteral("result")).toString());
    } else if (type == QLatin1String("system")) {
        if (m_provider == QLatin1String("notion") && o.value(QStringLiteral("subtype")).toString() == QLatin1String("init")) {
            QString status;
            for (const auto &s : o.value(QStringLiteral("mcp_servers")).toArray())
                if (s.toObject().value(QStringLiteral("name")).toString() == QLatin1String("claude.ai Notion"))
                    status = s.toObject().value(QStringLiteral("status")).toString();
            if (status != QLatin1String("connected")) {
                if (status.isEmpty() || ++m_notionTries > 2) {
                    appendDelta(status.isEmpty() ? tr("Your Claude account has no Notion connector. Add it at claude.ai → Settings → Connectors.")
                                                 : tr("Notion isn't answering right now (its connector is %1). Try again in a moment.").arg(status));
                    if (m_proc) m_proc->kill();
                    return;
                }
                if (m_notionTries == 1) addStep(tr("Connecting to Notion…"));
                m_retrying = true;
                if (m_proc) m_proc->kill();
                return;
            }
        }
        if (m_claudeSession.isEmpty()) m_claudeSession = o.value(QStringLiteral("session_id")).toString();
    } else if (type == QLatin1String("rate_limit_event")) {
        const QJsonObject w = o.value(QStringLiteral("rate_limit_info")).toObject().value(QStringLiteral("unifiedWindows")).toObject();
        const QJsonObject five = w.value(QStringLiteral("five_hour")).toObject();
        const QJsonObject seven = w.value(QStringLiteral("seven_day")).toObject();
        if (!five.isEmpty() || !seven.isEmpty())
            emit usage(five.value(QStringLiteral("utilization")).toDouble() * 100, qint64(five.value(QStringLiteral("resetsAt")).toDouble()),
                       seven.value(QStringLiteral("utilization")).toDouble() * 100, qint64(seven.value(QStringLiteral("resetsAt")).toDouble()));
    }
}

// ---------------------------------------------------------------- Ollama

void ChatBackend::runOllama() {
    QNetworkRequest req(QUrl(QStringLiteral("http://127.0.0.1:11434/api/chat")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QJsonArray messages{QJsonObject{{QStringLiteral("role"), QStringLiteral("system")}, {QStringLiteral("content"), kSystemPrompt}}};
    for (const auto &m : history()) messages.append(m);
    const QJsonObject body{{QStringLiteral("model"), m_model}, {QStringLiteral("stream"), true}, {QStringLiteral("messages"), messages}};
    m_buffer.clear();
    QNetworkReply *reply = m_net.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_reply = reply;
    connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
        m_buffer += reply->readAll();
        qsizetype nl;
        while ((nl = m_buffer.indexOf('\n')) >= 0) {
            const QJsonObject o = QJsonDocument::fromJson(m_buffer.left(nl)).object();
            m_buffer.remove(0, nl + 1);
            appendDelta(o.value(QStringLiteral("message")).toObject().value(QStringLiteral("content")).toString());
            if (o.contains(QStringLiteral("error"))) appendDelta(o.value(QStringLiteral("error")).toString());
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const bool failed = reply->error() != QNetworkReply::NoError && reply->error() != QNetworkReply::OperationCanceledError;
        finish(failed ? tr("Ollama isn't answering on 127.0.0.1:11434 (%1).").arg(reply->errorString()) : QString());
    });
}
