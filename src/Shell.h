#pragma once

#include <QColor>
#include <QObject>
#include <QTimer>
#include <QPointer>
#include <QRectF>
#include <QVariantList>

class QQuickWindow;
class QScreen;
class QSystemTrayIcon;

// The island's window plumbing: a Wayland layer-shell surface at the top of
// the screen whose input region follows the island, so everything around it
// stays click-through. Also the tray icon, global shortcut and autostart.
class Shell : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool autostart READ autostart WRITE setAutostart NOTIFY autostartChanged)
    Q_PROPERTY(bool layerShell READ layerShell CONSTANT)
    // Keep a small resting pill on screen when nothing is going on.
    Q_PROPERTY(bool alwaysShow READ alwaysShow WRITE setAlwaysShow NOTIFY alwaysShowChanged)
    // The 5-hour / weekly Claude usage rings in the island.
    Q_PROPERTY(bool showUsage READ showUsage WRITE setShowUsage NOTIFY showUsageChanged)
    // The screen the island sits on, by name ("DP-1"…); "" is the primary screen.
    Q_PROPERTY(QString screen READ screenName WRITE setScreenName NOTIFY screenChanged)
    // The screens to pick from: [{ name, label }], the primary one first.
    Q_PROPERTY(QVariantList screens READ screens NOTIFY screensChanged)
    // Lumo's own language: "fr", "pt_BR"…; "" follows the desktop.
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    // Lumo says hello when it starts: the island opens and Lumo waves.
    Q_PROPERTY(bool greeting READ greeting WRITE setGreeting NOTIFY greetingChanged)
    Q_PROPERTY(bool showMusic READ showMusic WRITE setShowMusic NOTIFY showMusicChanged)
    Q_PROPERTY(bool dance READ dance WRITE setDance NOTIFY danceChanged)
    // Lumo's outfit: "auto" follows the seasons, "" is none.
    Q_PROPERTY(QString outfit READ outfit WRITE setOutfit NOTIFY outfitChanged)
    // Instinct's WhatsApp number, for "Send to Instinct" (empty: copy only).
    Q_PROPERTY(QString instinctNumber READ instinctNumber WRITE setInstinctNumber NOTIFY instinctNumberChanged)
    // KWin blurs what's behind the island (the glass theme), where it can.
    Q_PROPERTY(bool blur READ blur WRITE setBlur NOTIFY blurChanged)
    // Lumo's accent: its icon takes it, in the tray, the windows and the launcher.
    Q_PROPERTY(QColor accent READ accent WRITE setAccent NOTIFY accentChanged)
public:
    explicit Shell(QObject *parent = nullptr);

    void attach(QQuickWindow *island);
    void setupTray();
    void setupShortcuts();

    bool autostart() const;
    void setAutostart(bool on);
    bool layerShell() const { return m_layerShell; }
    bool alwaysShow() const;
    bool showUsage() const;
    bool greeting() const;
    void setGreeting(bool on);
    QString language() const;
    void setLanguage(const QString &language);
    QString screenName() const;
    void setScreenName(const QString &name);
    QVariantList screens() const;
    bool showMusic() const { return flag(QStringLiteral("island/showMusic")); }
    void setShowMusic(bool on) { setFlag(QStringLiteral("island/showMusic"), on, &Shell::showMusicChanged); }
    bool dance() const { return flag(QStringLiteral("island/dance")); }
    void setDance(bool on) { setFlag(QStringLiteral("island/dance"), on, &Shell::danceChanged); }
    QString outfit() const;
    void setOutfit(const QString &outfit);
    QString instinctNumber() const;
    bool blur() const { return m_blur; }
    QColor accent() const { return m_accent; }
    void setAccent(const QColor &color);
    void setBlur(bool on);
    void setInstinctNumber(const QString &number);

    // The relay Instinct's own brief describes: there is no direct link between
    // Claude and Instinct, so the text goes through you. It is copied, and your
    // WhatsApp chat with Instinct opens with it typed in; you review it and
    // press Send. Returns a line for the UI.
    Q_INVOKABLE QString sendToInstinct(const QString &text);
    Q_INVOKABLE void copyText(const QString &text);
    void setShowUsage(bool on);
    void setAlwaysShow(bool on);

    // Island-local rectangle that should take the pointer; the rest is click-through.
    Q_INVOKABLE void setInputRect(const QRectF &rect);
    // Lets the island take keyboard focus (for the chat field) or give it back.
    Q_INVOKABLE void setKeyboard(bool on);
    // Quit Lumo: the island says goodbye first (farewellRequested), then calls
    // quitNow(). If it doesn't within a few seconds, Lumo quits anyway.
    Q_INVOKABLE void quit();
    Q_INVOKABLE void quitNow();

signals:
    void farewellRequested();
    void blurChanged();
    void accentChanged();
    void autostartChanged();
    void alwaysShowChanged();
    void showUsageChanged();
    void greetingChanged();
    void screenChanged();
    void languageChanged();
    void screensChanged();
    void showMusicChanged();
    void danceChanged();
    void outfitChanged();
    void instinctNumberChanged();
    void toggleRequested();
    void chatRequested();
    // The global shortcuts for the request at the top of the island.
    void allowTopRequested();
    void denyTopRequested();
    void settingsRequested();
    void dashboardRequested();
    void resurfaced();

private:
    bool m_leaving = false;
    bool m_blur = false;
    QColor m_accent = QColor(0xD9, 0x77, 0x57);
    QTimer m_iconSoon;
    void writeLauncherIcon();
    void applyBlur();
    // On/off preferences, on by default.
    bool flag(const QString &key) const;
    void setFlag(const QString &key, bool on, void (Shell::*changed)());
    // The chosen screen if it is plugged in, else the primary one.
    QScreen *targetScreen() const;
    // Puts the island on targetScreen(). A layer-shell surface is bound to its
    // screen when it is mapped, so it is hidden, moved and shown again.
    void moveToScreen();

    QPointer<QQuickWindow> m_island;
    QSystemTrayIcon *m_tray = nullptr;
    QRect m_lastMask;
    bool m_layerShell = false;
};
