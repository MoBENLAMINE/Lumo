#pragma once

#include <QObject>
#include <QPointer>
#include <QRectF>

class QQuickWindow;
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
    Q_PROPERTY(bool showMusic READ showMusic WRITE setShowMusic NOTIFY showMusicChanged)
    Q_PROPERTY(bool dance READ dance WRITE setDance NOTIFY danceChanged)
    // Lumo's outfit: "auto" follows the seasons, "" is none.
    Q_PROPERTY(QString outfit READ outfit WRITE setOutfit NOTIFY outfitChanged)
    // Instinct's WhatsApp number, for "Send to Instinct" (empty: copy only).
    Q_PROPERTY(QString instinctNumber READ instinctNumber WRITE setInstinctNumber NOTIFY instinctNumberChanged)
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
    bool showMusic() const { return flag(QStringLiteral("island/showMusic")); }
    void setShowMusic(bool on) { setFlag(QStringLiteral("island/showMusic"), on, &Shell::showMusicChanged); }
    bool dance() const { return flag(QStringLiteral("island/dance")); }
    void setDance(bool on) { setFlag(QStringLiteral("island/dance"), on, &Shell::danceChanged); }
    QString outfit() const;
    void setOutfit(const QString &outfit);
    QString instinctNumber() const;
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
    Q_INVOKABLE void quit();

signals:
    void autostartChanged();
    void alwaysShowChanged();
    void showUsageChanged();
    void showMusicChanged();
    void danceChanged();
    void outfitChanged();
    void instinctNumberChanged();
    void toggleRequested();
    void chatRequested();
    void settingsRequested();
    // The global shortcuts for the request at the top of the island.
    void allowTopRequested();
    void denyTopRequested();
    void resurfaced();

private:
    // On/off preferences, on by default.
    bool flag(const QString &key) const;
    void setFlag(const QString &key, bool on, void (Shell::*changed)());

    QPointer<QQuickWindow> m_island;
    QSystemTrayIcon *m_tray = nullptr;
    QRect m_lastMask;
    bool m_layerShell = false;
};
