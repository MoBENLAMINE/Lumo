#include "HookInstaller.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

#include <functional>

#include "Diff.h"

namespace {
const QStringList kEvents = {
    QStringLiteral("SessionStart"),      QStringLiteral("SessionEnd"),        QStringLiteral("UserPromptSubmit"),
    QStringLiteral("PreToolUse"),        QStringLiteral("PostToolUse"),       QStringLiteral("PostToolUseFailure"),
    QStringLiteral("PermissionRequest"), QStringLiteral("Notification"),      QStringLiteral("Stop"),
    QStringLiteral("StopFailure"),       QStringLiteral("SubagentStart"),     QStringLiteral("SubagentStop"),
};

// "nook-hook" is this app's hook from before it was renamed Lumo.
bool isLegacy(const QString &command) { return command.contains(QLatin1String("nook-hook")); }
bool isOurs(const QString &command) { return command.contains(QLatin1String("lumo-hook")) || isLegacy(command); }
bool isCoucou(const QString &command) { return command.contains(QLatin1String("coucou-hook")); }

QString quoted(const QString &path) { return QLatin1Char('\'') + QString(path).replace(u'\'', QStringLiteral("'\\''")) + QLatin1Char('\''); }

// Drops the hook commands `drop` matches; a group left with no hook goes too.
QJsonArray without(const QJsonArray &groups, const std::function<bool(const QString &)> &drop) {
    QJsonArray out;
    for (const auto &g : groups) {
        QJsonObject group = g.toObject();
        QJsonArray hooks;
        for (const auto &h : group.value(QStringLiteral("hooks")).toArray())
            if (!drop(h.toObject().value(QStringLiteral("command")).toString())) hooks.append(h);
        if (hooks.isEmpty()) continue;
        group.insert(QStringLiteral("hooks"), hooks);
        out.append(group);
    }
    return out;
}
}

HookInstaller::HookInstaller(QObject *parent) : QObject(parent) {
    refresh();
    // Keep the deployed hook in step with this build (it speaks the app's protocol).
    if (m_installed) deployHook();
    // Until the user approves the new path, the old one keeps working: it gets
    // this build's hook, which talks to this app.
    if (m_legacy) {
        const QString old = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/nook/bin/nook-hook");
        if (QFile::exists(old)) {
            QFile::remove(old);
            QFile::copy(QCoreApplication::applicationDirPath() + QStringLiteral("/lumo-hook"), old);
            QFile::setPermissions(old, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        }
    }
}

QString HookInstaller::settingsPath() { return QDir::homePath() + QStringLiteral("/.claude/settings.json"); }

QString HookInstaller::hookPath() const {
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/lumo/bin/lumo-hook");
}

QJsonObject HookInstaller::read() const {
    QFile f(settingsPath());
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

void HookInstaller::refresh() {
    const QJsonObject hooks = read().value(QStringLiteral("hooks")).toObject();
    m_installed = m_coucou = m_legacy = false;
    for (auto it = hooks.begin(); it != hooks.end(); ++it)
        for (const auto &g : it.value().toArray())
            for (const auto &h : g.toObject().value(QStringLiteral("hooks")).toArray()) {
                const QString c = h.toObject().value(QStringLiteral("command")).toString();
                m_installed |= isOurs(c);
                m_legacy |= isLegacy(c);
                m_coucou |= isCoucou(c);
            }
    emit changed();
}

void HookInstaller::prepare(bool install, bool replaceCoucou, bool statusLine) {
    const QJsonObject current = read();
    QJsonObject next = current;
    QJsonObject hooks = next.value(QStringLiteral("hooks")).toObject();
    const QString hook = quoted(hookPath());
    m_previousStatusLine.clear();

    for (const auto &event : kEvents) {
        QJsonArray groups = without(hooks.value(event).toArray(), [&](const QString &c) {
            return isOurs(c) || (install && replaceCoucou && isCoucou(c));
        });
        if (install) {
            const int timeout = event == QLatin1String("PermissionRequest") ? 120 : 10;
            groups.append(QJsonObject{{QStringLiteral("hooks"),
                                       QJsonArray{QJsonObject{{QStringLiteral("type"), QStringLiteral("command")},
                                                              {QStringLiteral("command"), hook + QLatin1Char(' ') + event},
                                                              {QStringLiteral("timeout"), timeout}}}}});
        }
        if (groups.isEmpty()) hooks.remove(event);
        else hooks.insert(event, groups);
    }
    if (hooks.isEmpty()) next.remove(QStringLiteral("hooks"));
    else next.insert(QStringLiteral("hooks"), hooks);

    // The status line carries the plan limits. The user's own one keeps running behind ours.
    const QString oldLine = current.value(QStringLiteral("statusLine")).toObject().value(QStringLiteral("command")).toString();
    if (install && statusLine && (!isOurs(oldLine) || isLegacy(oldLine))) {
        if (isLegacy(oldLine)) {
            // Ours under the old name: the user's own status line is already
            // saved (and was carried over), so only the path changes.
        } else if (isCoucou(oldLine)) {
            QFile prev(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
                       QStringLiteral("/coucou/bin/statusline-previous.json"));
            if (prev.open(QIODevice::ReadOnly))
                m_previousStatusLine = QJsonDocument::fromJson(prev.readAll()).object().value(QStringLiteral("command")).toString();
        } else {
            m_previousStatusLine = oldLine;
        }
        next.insert(QStringLiteral("statusLine"), QJsonObject{{QStringLiteral("type"), QStringLiteral("command")},
                                                              {QStringLiteral("command"), hook + QStringLiteral(" --statusline")}});
    } else if (!install && isOurs(oldLine)) {
        QFile prev(QDir::homePath() + QStringLiteral("/.config/lumo/statusline-previous"));
        const QString previous = prev.open(QIODevice::ReadOnly) ? QString::fromUtf8(prev.readAll()).trimmed() : QString();
        if (previous.isEmpty()) next.remove(QStringLiteral("statusLine"));
        else next.insert(QStringLiteral("statusLine"), QJsonObject{{QStringLiteral("type"), QStringLiteral("command")},
                                                                   {QStringLiteral("command"), previous}});
    }

    const auto d = Diff::compute(QString::fromUtf8(QJsonDocument(current).toJson()), QString::fromUtf8(QJsonDocument(next).toJson()));
    m_preview = d.lines;
    m_next = next;
    m_hasNext = d.added || d.removed;
    m_message = m_hasNext ? tr("%1 lines added, %2 removed in ~/.claude/settings.json. A backup is made first.").arg(d.added).arg(d.removed)
                          : tr("Nothing to change.");
    emit changed();
}

bool HookInstaller::deployHook() {
    const QString src = QCoreApplication::applicationDirPath() + QStringLiteral("/lumo-hook");
    const QString dst = hookPath();
    QDir().mkpath(QFileInfo(dst).absolutePath());
    if (QFileInfo(src).canonicalFilePath() == QFileInfo(dst).canonicalFilePath()) return true;
    QFile a(src), b(dst);
    if (a.open(QIODevice::ReadOnly) && b.open(QIODevice::ReadOnly) && a.readAll() == b.readAll()) return true;
    a.close(), b.close();
    QFile::remove(dst + QStringLiteral(".new"));
    if (!QFile::copy(src, dst + QStringLiteral(".new"))) return false;
    QFile::remove(dst);
    return QFile::rename(dst + QStringLiteral(".new"), dst) &&
           QFile::setPermissions(dst, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
}

bool HookInstaller::apply() {
    if (!m_hasNext) return false;
    if (!deployHook()) {
        m_message = tr("Couldn't copy lumo-hook to %1.").arg(hookPath());
        emit changed();
        return false;
    }
    const QString path = settingsPath();
    if (QFile::exists(path)) {
        const QString backup = path + QStringLiteral(".lumo-backup-") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
        if (!QFile::copy(path, backup)) {
            m_message = tr("Couldn't back up settings.json; nothing was changed.");
            emit changed();
            return false;
        }
    }
    if (!m_previousStatusLine.isEmpty()) {
        QDir().mkpath(QDir::homePath() + QStringLiteral("/.config/lumo"));
        QSaveFile prev(QDir::homePath() + QStringLiteral("/.config/lumo/statusline-previous"));
        if (prev.open(QIODevice::WriteOnly)) {
            prev.write(m_previousStatusLine.toUtf8());
            prev.commit();
        }
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(QJsonDocument(m_next).toJson(QJsonDocument::Indented)) < 0 || !f.commit()) {
        m_message = tr("Couldn't write settings.json.");
        emit changed();
        return false;
    }
    m_hasNext = false;
    m_preview.clear();
    m_message = tr("Done. New Claude Code sessions will show up in Lumo.");
    refresh();
    return true;
}

void HookInstaller::cancel() {
    m_hasNext = false;
    m_preview.clear();
    m_message.clear();
    emit changed();
}
