#include "ActiveWindow.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QFile>
#include <QStandardPaths>

namespace {
const QString kService = QStringLiteral("io.github.mobenlamine.lumo");
const QString kScriptName = QStringLiteral("lumo-active-window");

const char *kScript = R"JS(
const watched = [];
function report() {
    const w = workspace.activeWindow;
    callDBus("io.github.mobenlamine.lumo", "/ActiveWindow", "io.github.mobenlamine.lumo.ActiveWindow", "report",
             w ? String(w.resourceClass || "") : "", w ? !!w.fullScreen : false);
}
function track(w) {
    if (w && watched.indexOf(w) < 0) {
        watched.push(w);
        w.fullScreenChanged.connect(report);
        w.closed.connect(() => watched.splice(watched.indexOf(w), 1));
    }
    report();
}
workspace.windowActivated.connect(track);
track(workspace.activeWindow);
)JS";

void kwin(const QString &method, const QVariantList &args) {
    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"), QStringLiteral("/Scripting"),
                                                      QStringLiteral("org.kde.kwin.Scripting"), method);
    msg.setArguments(args);
    QDBusConnection::sessionBus().call(msg, QDBus::Block, 2000);
}
}

ActiveWindow::ActiveWindow(QObject *parent) : QObject(parent) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(kService) ||
        !bus.registerObject(QStringLiteral("/ActiveWindow"), this, QDBusConnection::ExportScriptableSlots))
        return;
    const QString path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/active-window.js");
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(kScript);
    f.close();
    kwin(QStringLiteral("unloadScript"), {kScriptName});
    kwin(QStringLiteral("loadScript"), {path, kScriptName});
    kwin(QStringLiteral("start"), {});
}

ActiveWindow::~ActiveWindow() { kwin(QStringLiteral("unloadScript"), {kScriptName}); }

void ActiveWindow::report(const QString &resourceClass, bool fullscreen) {
    m_class = resourceClass.toLower();
    if (fullscreen == m_fullscreen) return;
    m_fullscreen = fullscreen;
    emit changed();
}

void ActiveWindow::focusProcess(const QVariantList &pids) {
    QStringList list;
    for (const auto &p : pids)
        if (p.toInt() > 1) list << QString::number(p.toInt());
    if (list.isEmpty()) return;
    const QString script = QStringLiteral(R"JS(
const pids = [%1];
let best = null, rank = 1e9;
for (const w of workspace.windowList()) {
    const i = pids.indexOf(w.pid);
    if (i >= 0 && i < rank && w.normalWindow) { best = w; rank = i; }
}
if (best) {
    if (best.minimized) best.minimized = false;
    workspace.currentDesktop = best.desktops.length ? best.desktops[0] : workspace.currentDesktop;
    workspace.activeWindow = best;
}
)JS").arg(list.join(QLatin1Char(',')));
    const QString path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/focus-window.js");
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(script.toUtf8());
    f.close();
    const QString name = QStringLiteral("lumo-focus-window");
    kwin(QStringLiteral("unloadScript"), {name});
    kwin(QStringLiteral("loadScript"), {path, name});
    kwin(QStringLiteral("start"), {});
}

bool ActiveWindow::isTerminal() const {
    static const QStringList terminals = {
        QStringLiteral("konsole"),  QStringLiteral("org.kde.konsole"), QStringLiteral("yakuake"),
        QStringLiteral("kitty"),    QStringLiteral("alacritty"),       QStringLiteral("foot"),
        QStringLiteral("footclient"), QStringLiteral("wezterm"),       QStringLiteral("org.wezfurlong.wezterm"),
        QStringLiteral("ghostty"),  QStringLiteral("com.mitchellh.ghostty"), QStringLiteral("xterm"),
        QStringLiteral("urxvt"),    QStringLiteral("tilix"),           QStringLiteral("terminator"),
        QStringLiteral("gnome-terminal-server"), QStringLiteral("org.gnome.console"), QStringLiteral("warp"),
        QStringLiteral("dev.warp.warp"), QStringLiteral("rio"),
    };
    return terminals.contains(m_class);
}
