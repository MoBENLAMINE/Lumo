#pragma once

#include <QColor>
#include <QFileSystemWatcher>
#include <QObject>
#include <QStringList>
#include <QUrl>

// How Lumo looks and behaves, beyond the basics in Shell: its accent color
// (Claude's terracotta, KDE's accent, the wallpaper's or the album's), reduce
// motion, hiding in fullscreen, what the resting pill shows; plus exporting
// and importing all settings, and a problem report for the clipboard.
class Prefs : public QObject {
    Q_OBJECT
    // claude | kde | wallpaper | album | custom
    Q_PROPERTY(QString accentSource READ accentSource WRITE setAccentSource NOTIFY accentChanged)
    Q_PROPERTY(QColor customAccent READ customAccent WRITE setCustomAccent NOTIFY accentChanged)
    // The accent from the source, except "album", which the island blends in
    // from the music while something plays.
    Q_PROPERTY(QColor accent READ accent NOTIFY accentChanged)
    // the light behind the player that swells with the music
    Q_PROPERTY(bool visualizer READ visualizer WRITE setVisualizer NOTIFY changed)
    // the card takes the album art's colors
    Q_PROPERTY(bool albumColors READ albumColors WRITE setAlbumColors NOTIFY changed)
    Q_PROPERTY(bool reduceMotion READ reduceMotion WRITE setReduceMotion NOTIFY changed)
    Q_PROPERTY(bool hideOnFullscreen READ hideOnFullscreen WRITE setHideOnFullscreen NOTIFY changed)
    // The resting pill: a clock, and up to two small items
    // (weather | battery | cpu | memory | network | date | timer)
    Q_PROPERTY(bool pillClock READ pillClock WRITE setPillClock NOTIFY changed)
    Q_PROPERTY(QStringList pillItems READ pillItems WRITE setPillItems NOTIFY changed)
    // The long pill: a wider resting pill with more items, in your order.
    Q_PROPERTY(bool longPill READ longPill WRITE setLongPill NOTIFY changed)
    Q_PROPERTY(QStringList longPillItems READ longPillItems WRITE setLongPillItems NOTIFY changed)
    // light | dark | glass | custom | starfall
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY changed)
    Q_PROPERTY(QColor customBackground READ customBackground WRITE setCustomBackground NOTIFY changed)
    Q_PROPERTY(double opacity READ opacity WRITE setOpacity NOTIFY changed)
public:
    explicit Prefs(QObject *parent = nullptr);

    QString accentSource() const;
    void setAccentSource(const QString &source);
    QColor customAccent() const;
    void setCustomAccent(const QColor &color);
    QColor accent() const { return m_accent; }
    bool visualizer() const;
    void setVisualizer(bool on);
    bool albumColors() const;
    void setAlbumColors(bool on);
    bool reduceMotion() const;
    void setReduceMotion(bool on);
    bool hideOnFullscreen() const;
    void setHideOnFullscreen(bool on);
    bool pillClock() const;
    void setPillClock(bool on);
    QStringList pillItems() const;
    void setPillItems(const QStringList &items);
    bool longPill() const;
    void setLongPill(bool on);
    QStringList longPillItems() const;
    void setLongPillItems(const QStringList &items);
    QString theme() const;
    void setTheme(const QString &theme);
    QColor customBackground() const;
    void setCustomBackground(const QColor &color);
    double opacity() const;
    void setOpacity(double value);

    // Every setting, into one .ini file (Settings → General → Backup).
    Q_INVOKABLE bool exportSettings(const QUrl &file) const;
    Q_INVOKABLE bool importSettings(const QUrl &file);
    // Lumo's version, the system and the main settings, with your user name,
    // computer name and home folder taken out; copied to the clipboard.
    Q_INVOKABLE QString problemReport() const;
    Q_INVOKABLE void copyProblemReport() const;

    // The most vivid color of an image, softened to read well as an accent
    // (also used for album covers).
    static QColor vividColor(const QImage &image);

signals:
    void accentChanged();
    void changed();
    void imported();

private:
    void updateAccent();
    QColor kdeAccent() const;
    QColor wallpaperAccent() const;
    QString wallpaperPath() const;

    QColor m_accent;
    QFileSystemWatcher m_watcher;
};
