#include "Shell.h"

#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QProcess>
#include <QUrlQuery>
#include <QFile>
#include <QGuiApplication>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QQuickWindow>
#include <QRegion>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QTimer>

#include <KGlobalAccel>
#include <LayerShellQt/Window>

namespace {
QString autostartFile() { return QDir::homePath() + QStringLiteral("/.config/autostart/io.github.mobenlamine.lumo.desktop"); }

// Lumo, drawn small for the tray.
QIcon lumoIcon() {
    QIcon icon;
    for (int size : {16, 22, 24, 32, 48, 64}) {
        QPixmap pm(size, size);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal s = size;
        QPainterPath body;
        body.addRoundedRect(QRectF(s * .06, s * .1, s * .88, s * .82), s * .34, s * .34);
        p.fillPath(body, QColor(0xE8, 0x87, 0x5F));
        p.setBrush(Qt::white);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QRectF(s * .24, s * .32, s * .2, s * .26));
        p.drawEllipse(QRectF(s * .56, s * .32, s * .2, s * .26));
        p.setBrush(QColor(0x1A, 0x1A, 0x1A));
        p.drawEllipse(QRectF(s * .29, s * .4, s * .11, s * .14));
        p.drawEllipse(QRectF(s * .61, s * .4, s * .11, s * .14));
        icon.addPixmap(pm);
    }
    return icon;
}
}

Shell::Shell(QObject *parent) : QObject(parent) {}

void Shell::attach(QQuickWindow *island) {
    m_island = island;
    m_layerShell = QGuiApplication::platformName().startsWith(QLatin1String("wayland"));
    if (m_layerShell) {
        auto *ls = LayerShellQt::Window::get(island);
        ls->setScope(QStringLiteral("lumo-island"));
        ls->setLayer(LayerShellQt::Window::LayerOverlay);
        ls->setAnchors(LayerShellQt::Window::AnchorTop);
        ls->setExclusiveZone(-1); // sit on the very edge, over any top panel
        ls->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
        ls->setActivateOnShow(false);
        // A screen going away must not close the island for good.
        ls->setCloseOnDismissed(false);
    } else {
        // X11 fallback: a frameless always-on-top tool window at the top centre.
        island->setFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::WindowDoesNotAcceptFocus);
        if (QScreen *screen = QGuiApplication::primaryScreen())
            island->setPosition(screen->geometry().center().x() - island->width() / 2, screen->geometry().top());
    }
    island->setColor(Qt::transparent);
    // If the compositor dropped the surface (its screen was unplugged or
    // disabled), map it again on whatever screen is left.
    auto reshow = [this] {
        QTimer::singleShot(400, this, [this] {
            if (m_island && !m_island->isVisible()) {
                m_lastMask = {};
                m_island->show();
                emit resurfaced();
            }
        });
    };
    connect(island, &QWindow::visibleChanged, this, [reshow](bool visible) { if (!visible) reshow(); });
    connect(qApp, &QGuiApplication::screenRemoved, this, reshow);
    setInputRect(QRectF(island->width() / 2.0 - 130, 0, 260, 4));
    island->show();
}

void Shell::setInputRect(const QRectF &rect) {
    if (!m_island) return;
    const QRect r = rect.toAlignedRect();
    if (r == m_lastMask) return;
    m_lastMask = r;
    m_island->setMask(QRegion(r));
}

void Shell::setKeyboard(bool on) {
    if (!m_island) return;
    if (m_layerShell)
        LayerShellQt::Window::get(m_island)->setKeyboardInteractivity(
            on ? LayerShellQt::Window::KeyboardInteractivityOnDemand : LayerShellQt::Window::KeyboardInteractivityNone);
    if (on) m_island->requestActivate();
}

void Shell::setupTray() {
    m_tray = new QSystemTrayIcon(lumoIcon(), this);
    m_tray->setToolTip(QStringLiteral("Lumo"));
    auto *menu = new QMenu();
    menu->addAction(tr("Open island"), this, &Shell::toggleRequested);
    // The way back when the island was hidden with its eye button.
    QAction *always = menu->addAction(tr("Always show island"));
    always->setCheckable(true);
    always->setChecked(alwaysShow());
    connect(always, &QAction::toggled, this, &Shell::setAlwaysShow);
    connect(this, &Shell::alwaysShowChanged, always, [this, always] { always->setChecked(alwaysShow()); });
    menu->addAction(tr("Chat"), this, &Shell::chatRequested);
    menu->addAction(tr("Settings…"), this, &Shell::settingsRequested);
    menu->addSeparator();
    menu->addAction(tr("Quit Lumo"), this, &Shell::quit);
    m_tray->setContextMenu(menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger) emit toggleRequested();
    });
    m_tray->show();
    QApplication::setWindowIcon(lumoIcon());
}

void Shell::setupShortcuts() {
    auto *chat = new QAction(tr("Open Lumo chat"), this);
    chat->setObjectName(QStringLiteral("open-chat"));
    chat->setProperty("componentName", QStringLiteral("lumo"));
    chat->setProperty("componentDisplayName", QStringLiteral("Lumo"));
    KGlobalAccel::setGlobalShortcut(chat, QKeySequence(Qt::META | Qt::ALT | Qt::Key_N));
    connect(chat, &QAction::triggered, this, &Shell::chatRequested);

    auto *toggle = new QAction(tr("Show or hide the Lumo island"), this);
    toggle->setObjectName(QStringLiteral("toggle-island"));
    toggle->setProperty("componentName", QStringLiteral("lumo"));
    toggle->setProperty("componentDisplayName", QStringLiteral("Lumo"));
    KGlobalAccel::setGlobalShortcut(toggle, QKeySequence(Qt::META | Qt::ALT | Qt::Key_I));
    connect(toggle, &QAction::triggered, this, &Shell::toggleRequested);

    // Answer the top request without reaching for the mouse.
    auto *allow = new QAction(tr("Allow Lumo's top request"), this);
    allow->setObjectName(QStringLiteral("allow-request"));
    allow->setProperty("componentName", QStringLiteral("lumo"));
    allow->setProperty("componentDisplayName", QStringLiteral("Lumo"));
    KGlobalAccel::setGlobalShortcut(allow, QKeySequence(Qt::META | Qt::ALT | Qt::Key_Y));
    connect(allow, &QAction::triggered, this, &Shell::allowTopRequested);

    auto *deny = new QAction(tr("Deny Lumo's top request"), this);
    deny->setObjectName(QStringLiteral("deny-request"));
    deny->setProperty("componentName", QStringLiteral("lumo"));
    deny->setProperty("componentDisplayName", QStringLiteral("Lumo"));
    KGlobalAccel::setGlobalShortcut(deny, QKeySequence(Qt::META | Qt::ALT | Qt::Key_D));
    connect(deny, &QAction::triggered, this, &Shell::denyTopRequested);
}

bool Shell::autostart() const { return QFile::exists(autostartFile()); }

void Shell::setAutostart(bool on) {
    if (on == autostart()) return;
    if (on) {
        QDir().mkpath(QDir::homePath() + QStringLiteral("/.config/autostart"));
        QFile f(autostartFile());
        if (f.open(QIODevice::WriteOnly))
            f.write(QStringLiteral("[Desktop Entry]\nType=Application\nName=Lumo\nExec=%1\nIcon=io.github.mobenlamine.lumo\n"
                                   "X-GNOME-Autostart-enabled=true\nNoDisplay=true\n")
                        .arg(QCoreApplication::applicationFilePath())
                        .toUtf8());
    } else {
        QFile::remove(autostartFile());
    }
    emit autostartChanged();
}

bool Shell::alwaysShow() const { return QSettings().value(QStringLiteral("island/alwaysShow"), true).toBool(); }

void Shell::setAlwaysShow(bool on) {
    if (on == alwaysShow()) return;
    QSettings().setValue(QStringLiteral("island/alwaysShow"), on);
    emit alwaysShowChanged();
}

bool Shell::showUsage() const { return QSettings().value(QStringLiteral("island/showUsage"), true).toBool(); }

void Shell::setShowUsage(bool on) {
    if (on == showUsage()) return;
    QSettings().setValue(QStringLiteral("island/showUsage"), on);
    emit showUsageChanged();
}

bool Shell::flag(const QString &key) const { return QSettings().value(key, true).toBool(); }

void Shell::setFlag(const QString &key, bool on, void (Shell::*changed)()) {
    if (on == flag(key)) return;
    QSettings().setValue(key, on);
    emit (this->*changed)();
}

QString Shell::outfit() const { return QSettings().value(QStringLiteral("island/outfit"), QStringLiteral("auto")).toString(); }

void Shell::setOutfit(const QString &outfit) {
    if (outfit == this->outfit()) return;
    QSettings().setValue(QStringLiteral("island/outfit"), outfit);
    emit outfitChanged();
}

QString Shell::instinctNumber() const { return QSettings().value(QStringLiteral("instinct/whatsapp")).toString(); }

void Shell::setInstinctNumber(const QString &number) {
    // Digits only, as wa.me wants them (country code first, no + or spaces).
    QString digits;
    for (const QChar c : number)
        if (c.isDigit()) digits += c;
    if (digits == instinctNumber()) return;
    QSettings().setValue(QStringLiteral("instinct/whatsapp"), digits);
    emit instinctNumberChanged();
}

void Shell::copyText(const QString &text) {
    QProcess p;
    p.start(QStringLiteral("wl-copy"), {QStringLiteral("--type"), QStringLiteral("text/plain;charset=utf-8")});
    if (!p.waitForStarted(500)) return;
    p.write(text.toUtf8());
    p.closeWriteChannel();
    p.waitForFinished(800);
}

QString Shell::sendToInstinct(const QString &text) {
    const QString message = tr("From Claude, via Lumo:") + QStringLiteral("\n\n") + text.trimmed();
    copyText(message);
    const QString number = instinctNumber();
    if (number.isEmpty()) return tr("Copied — paste it to Instinct on WhatsApp.");
    QUrl url(QStringLiteral("https://wa.me/") + number);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("text"), message);
    url.setQuery(query);
    QDesktopServices::openUrl(url);
    return tr("WhatsApp is open with the text — review it and press Send.");
}

void Shell::quit() { QCoreApplication::quit(); }
