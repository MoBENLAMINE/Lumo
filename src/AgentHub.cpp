#include "AgentHub.h"

#include <QDateTime>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QRegularExpression>

#include "Diff.h"
#include "protocol.h"

namespace {
constexpr int kDecisionTimeoutMs = LUMO_APP_DECISION_MS; // the hook gives up a little later
static_assert(kDecisionTimeoutMs < LUMO_DECISION_MS, "the app must answer before the hook gives up");
// A notice (an agent Lumo can't answer for) stays up this long, or until dismissed.
constexpr int kNoticeMs = 8000;
constexpr qint64 kForgetAfterMs = 20 * 60 * 1000;
constexpr qint64 kHappyMs = 6000;
constexpr int kMaxSteps = 40;
constexpr int kMaxDiffs = 12;

qint64 now() { return QDateTime::currentMSecsSinceEpoch(); }

QString shorten(QString s, int max) {
    s = s.simplified();
    return s.size() > max ? s.left(max - 1) + u'…' : s;
}

QString fileName(const QString &path) { return QFileInfo(path).fileName(); }

// What you typed, without the machinery the agent wraps around prompts:
// <system-reminder>, <task-notification>, slash-command markers and so on.
// Empty when the "prompt" was only machinery (a background task finishing…).
QString cleanPrompt(const QString &raw) {
    static const QRegularExpression block(
        QStringLiteral("<(system-reminder|task-notification|command-name|command-message|command-args|local-command-stdout|"
                       "local-command-stderr|bash-input|bash-stdout|bash-stderr|user-prompt-submit-hook|pasted_content)"
                       "(\\s[^>]*)?>.*?</\\1>"),
        QRegularExpression::DotMatchesEverythingOption);
    QString s = raw;
    s.remove(block);
    return s.simplified();
}

const QStringList kSessionRoles = {
    QStringLiteral("sessionId"), QStringLiteral("agent"),    QStringLiteral("agentName"),
    QStringLiteral("agentColor"),     QStringLiteral("project"),  QStringLiteral("cwd"),
    QStringLiteral("status"),    QStringLiteral("activity"), QStringLiteral("prompt"),
    QStringLiteral("added"),     QStringLiteral("removed"),  QStringLiteral("tools"),
    QStringLiteral("startedAt"), QStringLiteral("updatedAt"), QStringLiteral("lastMessage"),
    QStringLiteral("steps"),     QStringLiteral("diffs"),    QStringLiteral("model"),
};
const QStringList kRequestRoles = {
    QStringLiteral("requestId"), QStringLiteral("sessionId"), QStringLiteral("agent"),
    QStringLiteral("agentColor"),     QStringLiteral("project"),   QStringLiteral("kind"),
    QStringLiteral("tool"),      QStringLiteral("title"),     QStringLiteral("detail"),
    QStringLiteral("questions"), QStringLiteral("canAlways"), QStringLiteral("createdAt"),
};
}

AgentHub::AgentHub(QObject *parent)
    : QObject(parent), m_sessions(kSessionRoles, QStringLiteral("sessionId")),
      m_requests(kRequestRoles, QStringLiteral("requestId")) {
    m_pruneTimer.setInterval(15'000);
    connect(&m_pruneTimer, &QTimer::timeout, this, &AgentHub::prune);
    m_pruneTimer.start();
}

QString AgentHub::agentColor(const QString &agent) const {
    static const QHash<QString, QString> colors = {
        {QString(), QStringLiteral("#E8875F")},       {QStringLiteral("claude"), QStringLiteral("#E8875F")},
        {QStringLiteral("claude-desktop"), QStringLiteral("#E8875F")},
        {QStringLiteral("codex"), QStringLiteral("#3DD68C")}, {QStringLiteral("gemini"), QStringLiteral("#6EA8FE")},
        {QStringLiteral("antigravity"), QStringLiteral("#8AB4F8")},
        {QStringLiteral("cursor"), QStringLiteral("#C9CDD4")}, {QStringLiteral("copilot"), QStringLiteral("#B48CFF")},
        {QStringLiteral("opencode"), QStringLiteral("#F5C451")}, {QStringLiteral("amp"), QStringLiteral("#FF6B9A")},
    };
    return colors.value(agent, QStringLiteral("#7DD3FC"));
}

QString AgentHub::agentName(const QString &agent) const {
    static const QHash<QString, QString> names = {
        {QString(), QStringLiteral("Claude Code")},  {QStringLiteral("claude-desktop"), QStringLiteral("Claude")},
        {QStringLiteral("codex"), QStringLiteral("Codex")}, {QStringLiteral("gemini"), QStringLiteral("Gemini CLI")},
        {QStringLiteral("antigravity"), QStringLiteral("Antigravity")},
        {QStringLiteral("cursor"), QStringLiteral("Cursor")}, {QStringLiteral("copilot"), QStringLiteral("Copilot CLI")},
        {QStringLiteral("opencode"), QStringLiteral("OpenCode")}, {QStringLiteral("amp"), QStringLiteral("Amp")},
    };
    if (names.contains(agent)) return names.value(agent);
    QString n = agent;
    if (!n.isEmpty()) n[0] = n[0].toUpper();
    return n;
}

// ---------------------------------------------------------------- state

int AgentHub::workingCount() const {
    int n = 0;
    for (const auto &r : m_sessions.rows())
        if (r.value(QStringLiteral("status")).toString() == QLatin1String("working")) ++n;
    return n;
}

QString AgentHub::mood() const {
    if (m_requests.count()) return QStringLiteral("alert");
    if (workingCount()) return QStringLiteral("working");
    const qint64 t = now();
    for (const auto &r : m_sessions.rows()) {
        const QString s = r.value(QStringLiteral("status")).toString();
        const bool recent = t - r.value(QStringLiteral("updatedAt")).toLongLong() < kHappyMs;
        if (recent && s == QLatin1String("done")) return QStringLiteral("happy");
        if (recent && s == QLatin1String("error")) return QStringLiteral("sad");
    }
    for (const auto &r : m_sessions.rows())
        if (r.value(QStringLiteral("status")).toString() == QLatin1String("idle")) return QStringLiteral("waiting");
    return QStringLiteral("idle");
}

namespace {
// The session that best stands for "what's going on": a waiting one, then the
// freshest working one, then the freshest of all.
int headlineRow(const MapListModel &sessions) {
    int best = -1, rank = -1;
    qint64 at = 0;
    for (int i = 0; i < sessions.count(); ++i) {
        const QVariantMap r = sessions.row(i);
        const QString s = r.value(QStringLiteral("status")).toString();
        const int k = s == QLatin1String("waiting") ? 3 : s == QLatin1String("working") ? 2 : s == QLatin1String("idle") ? 1 : 0;
        const qint64 t = r.value(QStringLiteral("updatedAt")).toLongLong();
        if (k > rank || (k == rank && t > at)) best = i, rank = k, at = t;
    }
    return best;
}
}

QString AgentHub::headline() const {
    if (m_requests.count()) return m_requests.row(0).value(QStringLiteral("title")).toString();
    const int i = headlineRow(m_sessions);
    if (i < 0) return {};
    const QVariantMap r = m_sessions.row(i);
    const QString status = r.value(QStringLiteral("status")).toString();
    QString what = r.value(QStringLiteral("activity")).toString();
    if (status == QLatin1String("done")) what = tr("Done");
    else if (status == QLatin1String("idle")) what = tr("Waiting for you");
    else if (status == QLatin1String("error")) what = tr("Stopped with an error");
    return what;
}

QString AgentHub::headlineAgent() const {
    const int i = headlineRow(m_sessions);
    if (m_requests.count()) return m_requests.row(0).value(QStringLiteral("project")).toString();
    return i < 0 ? QString() : m_sessions.row(i).value(QStringLiteral("project")).toString();
}

QString AgentHub::headlineColor() const {
    if (m_requests.count()) return m_requests.row(0).value(QStringLiteral("agentColor")).toString();
    const int i = headlineRow(m_sessions);
    return i < 0 ? agentColor(QString()) : m_sessions.row(i).value(QStringLiteral("agentColor")).toString();
}

// ---------------------------------------------------------------- events

QString AgentHub::describeTool(const QString &tool, const QJsonObject &in, bool past) const {
    const QString file = fileName(in.value(QStringLiteral("file_path")).toString(in.value(QStringLiteral("path")).toString()));
    if (tool == QLatin1String("Bash")) {
        const QString d = in.value(QStringLiteral("description")).toString();
        return d.isEmpty() ? tr("Running %1").arg(shorten(in.value(QStringLiteral("command")).toString(), 48)) : shorten(d, 60);
    }
    if (tool == QLatin1String("Read")) return (past ? tr("Read %1") : tr("Reading %1")).arg(file);
    if (tool == QLatin1String("Edit") || tool == QLatin1String("MultiEdit")) return (past ? tr("Edited %1") : tr("Editing %1")).arg(file);
    if (tool == QLatin1String("Write")) return (past ? tr("Wrote %1") : tr("Writing %1")).arg(file);
    if (tool == QLatin1String("NotebookEdit")) return tr("Editing notebook %1").arg(fileName(in.value(QStringLiteral("notebook_path")).toString()));
    if (tool == QLatin1String("Grep") || tool == QLatin1String("Glob"))
        return tr("Searching %1").arg(shorten(in.value(QStringLiteral("pattern")).toString(), 40));
    if (tool == QLatin1String("WebFetch")) return tr("Reading %1").arg(shorten(in.value(QStringLiteral("url")).toString(), 44));
    if (tool == QLatin1String("WebSearch")) return tr("Searching the web: %1").arg(shorten(in.value(QStringLiteral("query")).toString(), 40));
    if (tool == QLatin1String("Task") || tool == QLatin1String("Agent"))
        return tr("Delegating: %1").arg(shorten(in.value(QStringLiteral("description")).toString(), 44));
    if (tool == QLatin1String("TodoWrite")) return tr("Updating the plan");
    if (tool == QLatin1String("AskUserQuestion")) return tr("Has a question");
    if (tool.startsWith(QLatin1String("mcp__"))) return tr("Using %1").arg(tool.section(QStringLiteral("__"), 2).replace(u'_', u' '));
    return tool;
}

void AgentHub::touchSession(const QString &id, const QString &agent, const QJsonObject &p, QVariantMap values) {
    const QString cwd = p.value(QStringLiteral("cwd")).toString();
    values.insert(QStringLiteral("sessionId"), id);
    values.insert(QStringLiteral("updatedAt"), now());
    if (m_sessions.indexOf(id) < 0) {
        values.insert(QStringLiteral("agent"), agent);
        values.insert(QStringLiteral("agentName"), agentName(agent));
        values.insert(QStringLiteral("agentColor"), agentColor(agent));
        values.insert(QStringLiteral("startedAt"), now());
        values.insert(QStringLiteral("added"), 0);
        values.insert(QStringLiteral("removed"), 0);
        values.insert(QStringLiteral("tools"), 0);
        values.insert(QStringLiteral("steps"), QVariantList());
        values.insert(QStringLiteral("diffs"), QVariantList());
        values.insert(QStringLiteral("model"), p.value(QStringLiteral("model")).toString());
        if (!values.contains(QStringLiteral("status"))) values.insert(QStringLiteral("status"), QStringLiteral("idle"));
        if (!values.contains(QStringLiteral("activity"))) values.insert(QStringLiteral("activity"), tr("Ready"));
        m_sessions.upsert(values, 0);
    } else {
        m_sessions.upsert(values);
    }
    if (!cwd.isEmpty())
        m_sessions.upsert({{QStringLiteral("sessionId"), id}, {QStringLiteral("cwd"), cwd},
                           {QStringLiteral("project"), QFileInfo(cwd).fileName().isEmpty() ? cwd : QFileInfo(cwd).fileName()}});
}

void AgentHub::addStep(const QString &id, const QString &icon, const QString &text, const QString &kind) {
    const int i = m_sessions.indexOf(id);
    if (i < 0) return;
    QVariantList steps = m_sessions.row(i).value(QStringLiteral("steps")).toList();
    steps.prepend(QVariantMap{{QStringLiteral("icon"), icon}, {QStringLiteral("text"), text},
                              {QStringLiteral("kind"), kind}, {QStringLiteral("at"), now()}});
    while (steps.size() > kMaxSteps) steps.removeLast();
    m_sessions.upsert({{QStringLiteral("sessionId"), id}, {QStringLiteral("steps"), steps}});
}

void AgentHub::recordDiff(const QString &id, const QJsonObject &p) {
    const QString tool = p.value(QStringLiteral("tool_name")).toString();
    const QJsonObject in = p.value(QStringLiteral("tool_input")).toObject();
    const QString path = in.value(QStringLiteral("file_path")).toString();
    Diff::Result d;
    if (tool == QLatin1String("Edit")) {
        d = Diff::compute(in.value(QStringLiteral("old_string")).toString(), in.value(QStringLiteral("new_string")).toString());
    } else if (tool == QLatin1String("MultiEdit")) {
        for (const auto &e : in.value(QStringLiteral("edits")).toArray()) {
            const QJsonObject o = e.toObject();
            Diff::Result part = Diff::compute(o.value(QStringLiteral("old_string")).toString(), o.value(QStringLiteral("new_string")).toString());
            d.added += part.added, d.removed += part.removed;
            if (!d.lines.isEmpty()) d.lines.append(QVariantMap{{QStringLiteral("t"), QStringLiteral("…")}, {QStringLiteral("s"), QString()}});
            d.lines.append(part.lines);
        }
    } else if (tool == QLatin1String("Write")) {
        d = Diff::compute(QString(), in.value(QStringLiteral("content")).toString());
    } else {
        return;
    }
    const int i = m_sessions.indexOf(id);
    if (i < 0) return;
    const QVariantMap row = m_sessions.row(i);
    QVariantList diffs = row.value(QStringLiteral("diffs")).toList();
    diffs.prepend(QVariantMap{{QStringLiteral("file"), fileName(path)}, {QStringLiteral("path"), path},
                              {QStringLiteral("added"), d.added}, {QStringLiteral("removed"), d.removed},
                              {QStringLiteral("lines"), d.lines}, {QStringLiteral("at"), now()}});
    while (diffs.size() > kMaxDiffs) diffs.removeLast();
    m_sessions.upsert({{QStringLiteral("sessionId"), id},
                       {QStringLiteral("diffs"), diffs},
                       {QStringLiteral("added"), row.value(QStringLiteral("added")).toInt() + d.added},
                       {QStringLiteral("removed"), row.value(QStringLiteral("removed")).toInt() + d.removed},
                       {QStringLiteral("activity"), tr("%1  +%2 −%3").arg(fileName(path)).arg(d.added).arg(d.removed)}});
}

void AgentHub::onHookEvent(const QJsonObject &header, const QJsonObject &p, QLocalSocket *socket) {
    const QString agent = header.value(QStringLiteral("agent")).toString();
    QString event = p.value(QStringLiteral("hook_event_name")).toString();
    if (event.isEmpty()) event = header.value(QStringLiteral("event")).toString();
    QString sid = p.value(QStringLiteral("session_id")).toString();
    if (sid.isEmpty()) sid = agent + QLatin1Char(':') + header.value(QStringLiteral("ppid")).toVariant().toString();

    if (const QJsonArray pids = header.value(QStringLiteral("pids")).toArray(); !pids.isEmpty())
        m_pids.insert(sid, pids.toVariantList());

    if (event == QLatin1String("StatusLine")) {
        readStatusLine(p);
        socket->disconnectFromServer();
        return;
    }

    const QString tool = p.value(QStringLiteral("tool_name")).toString();
    const QJsonObject in = p.value(QStringLiteral("tool_input")).toObject();

    if (event == QLatin1String("PermissionRequest")) {
        touchSession(sid, agent, p, {{QStringLiteral("status"), QStringLiteral("waiting")},
                                     {QStringLiteral("activity"), tool == QLatin1String("AskUserQuestion") ? tr("Has a question")
                                                                  : tool == QLatin1String("ExitPlanMode") ? tr("Plan ready for your review")
                                                                  : tr("Needs your OK")}});
        addRequest(sid, agent, p, socket);
        emit stateChanged();
        return; // the socket stays open for the decision
    }
    socket->disconnectFromServer();

    if (event == QLatin1String("SessionStart")) {
        touchSession(sid, agent, p, {{QStringLiteral("status"), QStringLiteral("idle")}, {QStringLiteral("activity"), tr("Ready")}});
    } else if (event == QLatin1String("UserPromptSubmit")) {
        const QString raw = p.value(QStringLiteral("prompt")).toString();
        const QString prompt = cleanPrompt(raw);
        QVariantMap values{{QStringLiteral("status"), QStringLiteral("working")},
                           {QStringLiteral("activity"), tr("Thinking…")},
                           {QStringLiteral("lastMessage"), QString()}};
        if (!prompt.isEmpty()) values.insert(QStringLiteral("prompt"), shorten(prompt, 600));
        touchSession(sid, agent, p, values);
        if (!prompt.isEmpty()) addStep(sid, QStringLiteral("prompt"), shorten(prompt, 160), QStringLiteral("prompt"));
        else if (raw.contains(QLatin1String("<task-notification>")))
            addStep(sid, QStringLiteral("Task"), tr("A background task finished"), QStringLiteral("ok"));
        m_sessions.moveToTop(sid);
        emit promptSubmitted();
    } else if (event == QLatin1String("PreToolUse")) {
        const int i = m_sessions.indexOf(sid);
        const int tools = i < 0 ? 0 : m_sessions.row(i).value(QStringLiteral("tools")).toInt();
        touchSession(sid, agent, p, {{QStringLiteral("status"), QStringLiteral("working")},
                                     {QStringLiteral("activity"), describeTool(tool, in, false)},
                                     {QStringLiteral("tools"), tools + 1}});
    } else if (event == QLatin1String("PostToolUse")) {
        touchSession(sid, agent, p, {{QStringLiteral("status"), QStringLiteral("working")}});
        addStep(sid, tool, describeTool(tool, in, true), QStringLiteral("ok"));
        recordDiff(sid, p);
    } else if (event == QLatin1String("PostToolUseFailure")) {
        touchSession(sid, agent, p, {{QStringLiteral("status"), QStringLiteral("working")}});
        addStep(sid, tool, describeTool(tool, in, true), QStringLiteral("fail"));
    } else if (event == QLatin1String("Notification")) {
        const QString msg = p.value(QStringLiteral("message")).toString();
        if (msg.contains(QLatin1String("waiting for your input"), Qt::CaseInsensitive)) {
            touchSession(sid, agent, p, {{QStringLiteral("status"), QStringLiteral("idle")}});
            emit waitingForYou();
        }
    } else if (event == QLatin1String("Stop") || event == QLatin1String("SubagentStop")) {
        if (event == QLatin1String("Stop")) {
            touchSession(sid, agent, p, {{QStringLiteral("status"), QStringLiteral("done")},
                                         {QStringLiteral("activity"), tr("Done")},
                                         {QStringLiteral("lastMessage"), shorten(p.value(QStringLiteral("last_assistant_message")).toString(), 600)}});
            addStep(sid, QStringLiteral("done"), tr("Finished"), QStringLiteral("done"));
            emit sessionFinished(agent);
            QTimer::singleShot(kHappyMs + 50, this, &AgentHub::stateChanged);
        }
    } else if (event == QLatin1String("StopFailure")) {
        touchSession(sid, agent, p, {{QStringLiteral("status"), QStringLiteral("error")},
                                     {QStringLiteral("activity"), tr("Stopped with an error")}});
        emit sessionFailed();
        QTimer::singleShot(kHappyMs + 50, this, &AgentHub::stateChanged);
    } else if (event == QLatin1String("SessionEnd")) {
        m_sessions.remove(sid);
        m_pids.remove(sid);
        // A request still open belongs to a session that no longer exists.
        for (const auto &r : m_requests.rows())
            if (r.value(QStringLiteral("sessionId")).toString() == sid) replyInTerminal(r.value(QStringLiteral("requestId")).toString());
    } else if (event == QLatin1String("SubagentStart")) {
        addStep(sid, QStringLiteral("Task"), tr("Started a sub-agent"), QStringLiteral("ok"));
    }
    emit stateChanged();
}

// ---------------------------------------------------------------- requests

void AgentHub::addRequest(const QString &sid, const QString &agent, const QJsonObject &p, QLocalSocket *socket) {
    const QString id = QString::number(m_nextRequest++);
    const QString tool = p.value(QStringLiteral("tool_name")).toString();
    const QJsonObject in = p.value(QStringLiteral("tool_input")).toObject();
    const bool question = tool == QLatin1String("AskUserQuestion") && agent.isEmpty();
    // Only Claude Code and Codex read a decision; everyone else just gets notified.
    const bool decides = agent.isEmpty() || agent == QLatin1String("codex");

    QString title, detail;
    // Claude Code leaving plan mode: the plan, for you to approve or send back.
    const bool plan = tool == QLatin1String("ExitPlanMode") && agent.isEmpty();
    if (question) {
        title = tr("Question");
    } else if (plan) {
        title = tr("Approve the plan?");
        detail = in.value(QStringLiteral("plan")).toString();
    } else if (tool == QLatin1String("Bash")) {
        title = tr("Run a command?");
        detail = in.value(QStringLiteral("command")).toString();
        const QString d = in.value(QStringLiteral("description")).toString();
        if (!d.isEmpty()) detail = d + QStringLiteral("\n\n$ ") + detail;
    } else if (tool == QLatin1String("Edit") || tool == QLatin1String("MultiEdit") || tool == QLatin1String("Write")) {
        title = tool == QLatin1String("Write") ? tr("Write %1?") : tr("Edit %1?");
        title = title.arg(fileName(in.value(QStringLiteral("file_path")).toString()));
        if (tool == QLatin1String("Edit")) {
            const auto d = Diff::compute(in.value(QStringLiteral("old_string")).toString(), in.value(QStringLiteral("new_string")).toString());
            QStringList out;
            for (const auto &l : d.lines) {
                const QVariantMap m = l.toMap();
                out << m.value(QStringLiteral("t")).toString() + QLatin1Char(' ') + m.value(QStringLiteral("s")).toString();
            }
            detail = out.join(u'\n');
        } else {
            detail = shorten(in.value(QStringLiteral("content")).toString(), 1200);
        }
    } else if (tool == QLatin1String("WebFetch")) {
        title = tr("Open a web page?");
        detail = in.value(QStringLiteral("url")).toString();
    } else {
        title = tr("Use %1?").arg(tool.startsWith(QLatin1String("mcp__")) ? tool.section(QStringLiteral("__"), 2) : tool);
        detail = QString::fromUtf8(QJsonDocument(in).toJson(QJsonDocument::Indented)).left(1500);
    }

    QVariantList questions;
    if (question) {
        for (const auto &q : in.value(QStringLiteral("questions")).toArray()) {
            const QJsonObject o = q.toObject();
            QVariantList options;
            for (const auto &opt : o.value(QStringLiteral("options")).toArray())
                options << QVariantMap{{QStringLiteral("label"), opt.toObject().value(QStringLiteral("label")).toString()},
                                       {QStringLiteral("description"), opt.toObject().value(QStringLiteral("description")).toString()}};
            questions << QVariantMap{{QStringLiteral("question"), o.value(QStringLiteral("question")).toString()},
                                     {QStringLiteral("header"), o.value(QStringLiteral("header")).toString()},
                                     {QStringLiteral("multiSelect"), o.value(QStringLiteral("multiSelect")).toBool()},
                                     {QStringLiteral("options"), options}};
        }
    }

    const int si = m_sessions.indexOf(sid);
    // A notice's hook doesn't wait for an answer: let it go now.
    if (!decides) socket->disconnectFromServer();
    m_pending.insert(id, Pending{decides ? socket : nullptr, in, p.value(QStringLiteral("permission_suggestions")).toArray(), sid, agent});
    m_requests.upsert({{QStringLiteral("requestId"), id},
                       {QStringLiteral("sessionId"), sid},
                       {QStringLiteral("agent"), agent},
                       {QStringLiteral("agentColor"), agentColor(agent)},
                       {QStringLiteral("project"), si < 0 ? QString() : m_sessions.row(si).value(QStringLiteral("project"))},
                       {QStringLiteral("kind"), question ? QStringLiteral("question") : plan ? QStringLiteral("plan") : decides ? QStringLiteral("permission") : QStringLiteral("notice")},
                       {QStringLiteral("tool"), tool},
                       {QStringLiteral("title"), title},
                       {QStringLiteral("detail"), detail},
                       {QStringLiteral("questions"), questions},
                       {QStringLiteral("canAlways"), agent.isEmpty() && !p.value(QStringLiteral("permission_suggestions")).toArray().isEmpty()},
                       {QStringLiteral("createdAt"), now()}});

    if (decides) {
        // The hook died: the agent got its answer in the terminal, or gave up.
        connect(socket, &QLocalSocket::disconnected, this, [this, id] {
            if (m_pending.contains(id)) release(id, {});
        });
    }
    QTimer::singleShot(decides ? kDecisionTimeoutMs : kNoticeMs, this, [this, id] { replyInTerminal(id); });
    emit attention();
}

void AgentHub::release(const QString &requestId, const QByteArray &line) {
    const Pending pending = m_pending.take(requestId);
    m_requests.remove(requestId);
    if (pending.socket) {
        if (!line.isEmpty()) {
            pending.socket->write(line + '\n');
            pending.socket->flush();
        }
        pending.socket->disconnectFromServer();
    }
    if (const int i = m_sessions.indexOf(pending.sessionId); i >= 0) {
        bool other = false;
        for (const auto &p : std::as_const(m_pending)) other |= p.sessionId == pending.sessionId;
        if (!other)
            m_sessions.upsert({{QStringLiteral("sessionId"), pending.sessionId},
                               {QStringLiteral("status"), QStringLiteral("working")},
                               {QStringLiteral("activity"), line.isEmpty() ? tr("Asking in the terminal") : tr("Carrying on…")}});
    }
    emit stateChanged();
}

namespace {
QByteArray decisionLine(const QJsonObject &decision) {
    const QJsonObject out{{QStringLiteral("hookSpecificOutput"),
                           QJsonObject{{QStringLiteral("hookEventName"), QStringLiteral("PermissionRequest")},
                                       {QStringLiteral("decision"), decision}}}};
    return QJsonDocument(out).toJson(QJsonDocument::Compact);
}
}

void AgentHub::allow(const QString &requestId) {
    if (!m_pending.contains(requestId)) return;
    release(requestId, decisionLine({{QStringLiteral("behavior"), QStringLiteral("allow")}}));
}

void AgentHub::always(const QString &requestId) {
    if (!m_pending.contains(requestId)) return;
    // Claude Code's own "don't ask again": apply the rules it suggested.
    QJsonObject decision{{QStringLiteral("behavior"), QStringLiteral("allow")}};
    const QJsonArray suggestions = m_pending.value(requestId).suggestions;
    if (!suggestions.isEmpty()) decision.insert(QStringLiteral("updatedPermissions"), suggestions);
    release(requestId, decisionLine(decision));
}

void AgentHub::keepPlanning(const QString &requestId, const QString &feedback) {
    if (!m_pending.contains(requestId)) return;
    const QString f = feedback.trimmed();
    release(requestId, decisionLine({{QStringLiteral("behavior"), QStringLiteral("deny")},
                                     {QStringLiteral("message"), f.isEmpty() ? QStringLiteral("The user wants to keep planning before you start.")
                                                                             : QStringLiteral("The user wants changes to the plan: ") + f}}));
}

void AgentHub::deny(const QString &requestId) {
    if (!m_pending.contains(requestId)) return;
    release(requestId, decisionLine({{QStringLiteral("behavior"), QStringLiteral("deny")},
                                     {QStringLiteral("message"), QStringLiteral("Denied from Lumo")}}));
}

void AgentHub::answer(const QString &requestId, const QVariantMap &answers) {
    if (!m_pending.contains(requestId)) return;
    const QJsonObject input = m_pending.value(requestId).toolInput;
    // Every question needs an answer that is one of its own labels.
    const QJsonArray qs = input.value(QStringLiteral("questions")).toArray();
    if (qs.isEmpty() || qs.size() != answers.size()) return;
    for (const auto &q : qs) {
        const QJsonObject o = q.toObject();
        QStringList labels;
        for (const auto &opt : o.value(QStringLiteral("options")).toArray()) labels << opt.toObject().value(QStringLiteral("label")).toString();
        const QVariant a = answers.value(o.value(QStringLiteral("question")).toString());
        const QStringList picks = a.typeId() == QMetaType::QString ? QStringList{a.toString()} : a.toStringList();
        if (picks.isEmpty()) return;
        for (const auto &pick : picks)
            if (!labels.contains(pick)) return;
    }
    QJsonObject updated = input;
    updated.insert(QStringLiteral("answers"), QJsonObject::fromVariantMap(answers));
    release(requestId, decisionLine({{QStringLiteral("behavior"), QStringLiteral("allow")},
                                     {QStringLiteral("updatedInput"), updated}}));
}

void AgentHub::replyInTerminal(const QString &requestId) {
    if (!m_pending.contains(requestId)) return;
    release(requestId, {});
}

void AgentHub::dismissSession(const QString &sessionId) {
    m_sessions.remove(sessionId);
    m_pids.remove(sessionId);
    emit stateChanged();
}

// ---------------------------------------------------------------- usage

void AgentHub::readStatusLine(const QJsonObject &p) {
    const QJsonObject limits = p.value(QStringLiteral("rate_limits")).toObject();
    const QJsonObject five = limits.value(QStringLiteral("five_hour")).toObject();
    const QJsonObject seven = limits.value(QStringLiteral("seven_day")).toObject();
    auto reset = [](const QJsonValue &v) -> qint64 {
        if (v.isDouble()) return qint64(v.toDouble());
        return QDateTime::fromString(v.toString(), Qt::ISODateWithMs).toSecsSinceEpoch();
    };
    if (five.isEmpty() && seven.isEmpty()) return;
    setUsage(five.value(QStringLiteral("used_percentage")).toDouble(m_five), reset(five.value(QStringLiteral("resets_at"))),
             seven.value(QStringLiteral("used_percentage")).toDouble(m_seven), reset(seven.value(QStringLiteral("resets_at"))));
}

void AgentHub::setUsage(double five, qint64 fiveReset, double seven, qint64 sevenReset) {
    m_five = five, m_seven = seven;
    if (fiveReset > 0) m_fiveReset = fiveReset;
    if (sevenReset > 0) m_sevenReset = sevenReset;
    emit usageChanged();
}

void AgentHub::prune() {
    const qint64 t = now();
    QStringList stale;
    for (const auto &r : m_sessions.rows()) {
        const QString s = r.value(QStringLiteral("status")).toString();
        if (s != QLatin1String("working") && s != QLatin1String("waiting") &&
            t - r.value(QStringLiteral("updatedAt")).toLongLong() > kForgetAfterMs)
            stale << r.value(QStringLiteral("sessionId")).toString();
    }
    for (const auto &id : stale) {
        m_sessions.remove(id);
        m_pids.remove(id);
    }
    // Process chains of sessions that never showed up as a row (status-line only…).
    for (auto it = m_pids.begin(); it != m_pids.end();) {
        if (m_sessions.indexOf(it.key()) < 0) it = m_pids.erase(it);
        else ++it;
    }
    // Moods like "happy" expire with time alone.
    emit stateChanged();
}
