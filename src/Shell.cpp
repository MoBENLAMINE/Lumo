#include "Shell.h"

#include <QAction>
#include <algorithm>
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QProcess>
#include <QUrlQuery>
#include <QFile>
#include <QGuiApplication>
#include <QLinearGradient>
#include <QMenu>
#include <QPainter>
#include <QQuickWindow>
#include <QRegion>
#include <QScreen>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QTimer>

#include <KGlobalAccel>
#ifdef LUMO_HAVE_KWINDOWSYSTEM
#include <KWindowEffects>
#endif
#include <LayerShellQt/Window>

namespace {
QString autostartFile() { return QDir::homePath() + QStringLiteral("/.config/autostart/io.github.mobenlamine.lumo.desktop"); }

// Lumo, drawn small for the tray: the blocky terracotta body with its arms and
// four legs, and the dark face screen with two eyes of light.
QIcon lumoIcon() {
    QIcon icon;
    for (int size : {16, 22, 24, 32, 48, 64}) {
        QPixmap pm(size, size);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        const qreal s = size;
        const QColor body(0xD9, 0x77, 0x57), shade(0xB2, 0x5E, 0x40);
        // legs and arms, behind the body
        p.setBrush(shade);
        for (qreal x : {0.22, 0.36, 0.55, 0.69}) p.drawRoundedRect(QRectF(s * x, s * 0.72, s * 0.09, s * 0.2), s * 0.03, s * 0.03);
        p.drawRoundedRect(QRectF(0, s * 0.45, s * 0.16, s * 0.15), s * 0.04, s * 0.04);
        p.drawRoundedRect(QRectF(s * 0.84, s * 0.45, s * 0.16, s * 0.15), s * 0.04, s * 0.04);
        // the body
        QLinearGradient g(0, 0, 0, s);
        g.setColorAt(0, QColor(0xF0, 0x8F, 0x6E));
        g.setColorAt(1, body);
        p.setBrush(g);
        p.drawRoundedRect(QRectF(s * 0.07, s * 0.04, s * 0.86, s * 0.76), s * 0.22, s * 0.22);
        // the face screen
        p.setBrush(QColor(0x14, 0x14, 0x1C));
        p.drawRoundedRect(QRectF(s * 0.16, s * 0.24, s * 0.68, s * 0.4), s * 0.17, s * 0.17);
        // two eyes of light
        p.setBrush(QColor(0xEE, 0xF8, 0xFF));
        const qreal ew = std::max(1.5, s * 0.12), eh = std::max(2.5, s * 0.2);
        p.drawRoundedRect(QRectF(s * 0.35 - ew / 2, s * 0.44 - eh / 2, ew, eh), ew * 0.35, ew * 0.35);
        p.drawRoundedRect(QRectF(s * 0.65 - ew / 2, s * 0.44 - eh / 2, ew, eh), ew * 0.35, ew * 0.35);
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
    }
    if (QScreen *screen = targetScreen()) {
        island->setScreen(screen);
        if (!m_layerShell) island->setPosition(screen->geometry().center().x() - island->width() / 2, screen->geometry().top());
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
    connect(qApp, &QGuiApplication::screenRemoved, this, [this, reshow] {
        emit screensChanged();
        reshow();
        // The chosen screen went away: the primary one takes the island meanwhile.
        QTimer::singleShot(500, this, &Shell::moveToScreen);
    });
    // The chosen screen is back, or the primary one changed: follow it.
    connect(qApp, &QGuiApplication::screenAdded, this, [this] {
        emit screensChanged();
        QTimer::singleShot(500, this, &Shell::moveToScreen);
    });
    connect(qApp, &QGuiApplication::primaryScreenChanged, this, [this] {
        emit screensChanged();
        moveToScreen();
    });
    setInputRect(QRectF(island->width() / 2.0 - 130, 0, 260, 4));
    island->show();
}

void Shell::setInputRect(const QRectF &rect) {
    if (!m_island) return;
    const QRect r = rect.toAlignedRect();
    if (r == m_lastMask) return;
    m_lastMask = r;
    m_island->setMask(QRegion(r));
    if (m_blur) applyBlur();
}

void Shell::setBlur(bool on) {
    if (on == m_blur) return;
    m_blur = on;
    applyBlur();
    emit blurChanged();
}

// Only the island's own shape is blurred, not the window around it.
void Shell::applyBlur() {
#ifdef LUMO_HAVE_KWINDOWSYSTEM
    if (!m_island) return;
    KWindowEffects::enableBlurBehind(m_island, m_blur, QRegion(m_lastMask, QRegion::Rectangle));
#endif
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
    QAction *open = menu->addAction(QString(), this, &Shell::toggleRequested);
    // The way back when the island was hidden with its eye button.
    QAction *always = menu->addAction(QString());
    always->setCheckable(true);
    always->setChecked(alwaysShow());
    connect(always, &QAction::toggled, this, &Shell::setAlwaysShow);
    connect(this, &Shell::alwaysShowChanged, always, [this, always] { always->setChecked(alwaysShow()); });
    QAction *chat = menu->addAction(QString(), this, &Shell::chatRequested);
    QAction *dashboard = menu->addAction(QString(), this, &Shell::dashboardRequested);
    QAction *prefs = menu->addAction(QString(), this, &Shell::settingsRequested);
    menu->addSeparator();
    QAction *quit = menu->addAction(QString(), this, &Shell::quit);
    auto label = [=, this] {
        open->setText(tr("Open island"));
        always->setText(tr("Always show island"));
        chat->setText(tr("Chat"));
        dashboard->setText(tr("Dashboard"));
        prefs->setText(tr("Settings…"));
        quit->setText(tr("Quit Lumo"));
    };
    label();
    // Queued, so the new translation is loaded first.
    connect(this, &Shell::languageChanged, menu, label, Qt::QueuedConnection);
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

QString Shell::language() const { return QSettings().value(QStringLiteral("general/language")).toString(); }

void Shell::setLanguage(const QString &language) {
    if (language == this->language()) return;
    QSettings().setValue(QStringLiteral("general/language"), language);
    emit languageChanged();
}

QString Shell::screenName() const { return QSettings().value(QStringLiteral("island/screen")).toString(); }

void Shell::setScreenName(const QString &name) {
    if (name == screenName()) return;
    QSettings().setValue(QStringLiteral("island/screen"), name);
    emit screenChanged();
    moveToScreen();
}

QVariantList Shell::screens() const {
    QVariantList out;
    QScreen *primary = QGuiApplication::primaryScreen();
    QList<QScreen *> all = QGuiApplication::screens();
    std::stable_partition(all.begin(), all.end(), [primary](QScreen *s) { return s == primary; });
    for (QScreen *s : std::as_const(all)) {
        // "Dell U2723QE · HDMI-A-1": the monitor first, then the connector.
        const QString what = (s->manufacturer() + QLatin1Char(' ') + s->model()).trimmed();
        out << QVariantMap{{QStringLiteral("name"), s->name()},
                           {QStringLiteral("label"), what.isEmpty() || what == s->name() ? s->name() : what + QStringLiteral(" · ") + s->name()}};
    }
    return out;
}

QScreen *Shell::targetScreen() const {
    const QString name = screenName();
    if (!name.isEmpty())
        for (QScreen *s : QGuiApplication::screens())
            if (s->name() == name) return s;
    return QGuiApplication::primaryScreen();
}

void Shell::moveToScreen() {
    if (!m_island) return;
    QScreen *screen = targetScreen();
    if (!screen || m_island->screen() == screen) return;
    const bool shown = m_island->isVisible();
    if (shown) m_island->hide();
    m_island->setScreen(screen);
    if (!m_layerShell) m_island->setPosition(screen->geometry().center().x() - m_island->width() / 2, screen->geometry().top());
    if (shown) {
        m_lastMask = {};
        m_island->show();
        emit resurfaced();
    }
}

bool Shell::greeting() const { return QSettings().value(QStringLiteral("island/greeting"), true).toBool(); }

void Shell::setGreeting(bool on) {
    if (on == greeting()) return;
    QSettings().setValue(QStringLiteral("island/greeting"), on);
    emit greetingChanged();
}

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

void Shell::quit() {
    if (m_leaving) {   // asked twice (Quit clicked again): don't wait
        quitNow();
        return;
    }
    m_leaving = true;
    emit farewellRequested();
    QTimer::singleShot(4500, this, &Shell::quitNow);
}

void Shell::quitNow() { QCoreApplication::quit(); }
