#include "Prefs.h"
#include "Vivid.h"

#include <QClipboard>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QCoreApplication>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTextStream>

#include <algorithm>
#include <cmath>

namespace {
const QColor kClaude(0xD9, 0x77, 0x57);

QString configFile(const QString &name) {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QLatin1Char('/') + name;
}
}

Prefs::Prefs(QObject *parent) : QObject(parent) {
    // KDE's accent and the wallpaper change in these files: follow them live.
    for (const QString &f : {configFile(QStringLiteral("kdeglobals")), configFile(QStringLiteral("plasma-org.kde.plasma.desktop-appletsrc"))})
        if (QFile::exists(f)) m_watcher.addPath(f);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path) {
        // editors save by replacing the file: watch the new one
        if (!m_watcher.files().contains(path) && QFile::exists(path)) m_watcher.addPath(path);
        updateAccent();
    });
    updateAccent();
}

QString Prefs::accentSource() const { return QSettings().value(QStringLiteral("look/accent"), QStringLiteral("claude")).toString(); }
void Prefs::setAccentSource(const QString &source) { QSettings().setValue(QStringLiteral("look/accent"), source); updateAccent(); }
QColor Prefs::customAccent() const { return QColor(QSettings().value(QStringLiteral("look/customAccent"), kClaude.name()).toString()); }
void Prefs::setCustomAccent(const QColor &color) { QSettings().setValue(QStringLiteral("look/customAccent"), color.name()); updateAccent(); }

bool Prefs::visualizer() const { return QSettings().value(QStringLiteral("music/visualizer"), true).toBool(); }
void Prefs::setVisualizer(bool on) { QSettings().setValue(QStringLiteral("music/visualizer"), on); emit changed(); }
bool Prefs::albumColors() const { return QSettings().value(QStringLiteral("music/albumColors"), true).toBool(); }
void Prefs::setAlbumColors(bool on) { QSettings().setValue(QStringLiteral("music/albumColors"), on); emit changed(); }
bool Prefs::reduceMotion() const { return QSettings().value(QStringLiteral("look/reduceMotion"), false).toBool(); }
void Prefs::setReduceMotion(bool on) { QSettings().setValue(QStringLiteral("look/reduceMotion"), on); emit changed(); }
bool Prefs::hideOnFullscreen() const { return QSettings().value(QStringLiteral("island/hideOnFullscreen"), true).toBool(); }
void Prefs::setHideOnFullscreen(bool on) { QSettings().setValue(QStringLiteral("island/hideOnFullscreen"), on); emit changed(); }
bool Prefs::pillClock() const { return QSettings().value(QStringLiteral("pill/clock"), false).toBool(); }
void Prefs::setPillClock(bool on) { QSettings().setValue(QStringLiteral("pill/clock"), on); emit changed(); }
QStringList Prefs::pillItems() const { return QSettings().value(QStringLiteral("pill/items")).toStringList(); }
void Prefs::setPillItems(const QStringList &items) { QSettings().setValue(QStringLiteral("pill/items"), items.mid(0, 2)); emit changed(); }
bool Prefs::longPill() const { return QSettings().value(QStringLiteral("pill/long"), false).toBool(); }
void Prefs::setLongPill(bool on) { QSettings().setValue(QStringLiteral("pill/long"), on); emit changed(); }
QStringList Prefs::longPillItems() const {
    return QSettings().value(QStringLiteral("pill/longItems"), QStringList{QStringLiteral("clock"), QStringLiteral("date"),
                                                                         QStringLiteral("music"), QStringLiteral("weather")}).toStringList();
}
void Prefs::setLongPillItems(const QStringList &items) { QSettings().setValue(QStringLiteral("pill/longItems"), items); emit changed(); }
QString Prefs::theme() const { return QSettings().value(QStringLiteral("look/theme"), QStringLiteral("dark")).toString(); }
void Prefs::setTheme(const QString &theme) { QSettings().setValue(QStringLiteral("look/theme"), theme); emit changed(); }
QColor Prefs::customBackground() const { return QColor(QSettings().value(QStringLiteral("look/customBackground"), QStringLiteral("#101820")).toString()); }
void Prefs::setCustomBackground(const QColor &color) { QSettings().setValue(QStringLiteral("look/customBackground"), color.name()); emit changed(); }
double Prefs::opacity() const { return std::clamp(QSettings().value(QStringLiteral("look/opacity"), 1.0).toDouble(), 0.5, 1.0); }
void Prefs::setOpacity(double value) { QSettings().setValue(QStringLiteral("look/opacity"), std::clamp(value, 0.5, 1.0)); emit changed(); }

QStringList Prefs::dashboardTiles() const {
    return QSettings().value(QStringLiteral("dashboard/tiles"), QStringList{QStringLiteral("clock"), QStringLiteral("today"),
        QStringLiteral("media"), QStringLiteral("system"), QStringLiteral("controls"), QStringLiteral("tools")}).toStringList();
}
void Prefs::setDashboardTiles(const QStringList &tiles) { QSettings().setValue(QStringLiteral("dashboard/tiles"), tiles); emit changed(); }
QString Prefs::dashboardScreen() const { return QSettings().value(QStringLiteral("dashboard/screen")).toString(); }
void Prefs::setDashboardScreen(const QString &name) { QSettings().setValue(QStringLiteral("dashboard/screen"), name); emit changed(); }

// ------------------------------------------------------------ accent

void Prefs::updateAccent() {
    const QString source = accentSource();
    QColor c = kClaude;
    if (source == QLatin1String("kde")) c = kdeAccent();
    else if (source == QLatin1String("wallpaper")) c = wallpaperAccent();
    else if (source == QLatin1String("custom")) c = customAccent();
    if (!c.isValid()) c = kClaude;
    m_accent = c;
    emit accentChanged();
}

QColor Prefs::kdeAccent() const {
    QFile f(configFile(QStringLiteral("kdeglobals")));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QString group;
    QColor accent, selection;
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.startsWith(QLatin1Char('['))) { group = line; continue; }
        const auto rgb = [&](const QString &value) {
            const QStringList p = value.split(QLatin1Char(','));
            return p.size() >= 3 ? QColor(p[0].toInt(), p[1].toInt(), p[2].toInt()) : QColor();
        };
        if (group == QLatin1String("[General]") && line.startsWith(QLatin1String("AccentColor=")))
            accent = rgb(line.mid(12));
        else if (group == QLatin1String("[Colors:Selection]") && line.startsWith(QLatin1String("BackgroundNormal=")))
            selection = rgb(line.mid(17));
    }
    return accent.isValid() ? accent : selection;
}

QString Prefs::wallpaperPath() const {
    QFile f(configFile(QStringLiteral("plasma-org.kde.plasma.desktop-appletsrc")));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QString group, image;
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.startsWith(QLatin1Char('['))) { group = line; continue; }
        if (group.contains(QLatin1String("[Wallpaper][org.kde.image][General]")) && line.startsWith(QLatin1String("Image="))) {
            image = line.mid(6);
            break;   // the first desktop's
        }
    }
    if (image.startsWith(QLatin1String("file://"))) image = QUrl(image).toLocalFile();
    // A wallpaper package (a folder): its largest picture.
    if (QFileInfo(image).isDir()) {
        QString best;
        qint64 size = 0;
        QDirIterator it(image + QStringLiteral("/contents/images"), QDir::Files);
        while (it.hasNext()) {
            it.next();
            if (it.fileInfo().size() > size) { size = it.fileInfo().size(); best = it.filePath(); }
        }
        image = best;
    }
    return image;
}

QColor Prefs::wallpaperAccent() const {
    const QString path = wallpaperPath();
    if (path.isEmpty()) return {};
    QImageReader reader(path);
    reader.setScaledSize(QSize(48, 48));   // the colors, not the pixels: cheap
    return vividColor(reader.read());
}

QColor Prefs::vividColor(const QImage &source) { return Vivid::color(source); }

// ------------------------------------------------------------ backup

bool Prefs::exportSettings(const QUrl &file) const {
    QSettings from;
    from.sync();
    const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    QFile::remove(path);
    QSettings to(path, QSettings::IniFormat);
    for (const QString &key : from.allKeys())
        if (!key.startsWith(QLatin1String("weather/la")) && !key.startsWith(QLatin1String("weather/lo"))) to.setValue(key, from.value(key));
    to.sync();
    return to.status() == QSettings::NoError;
}

bool Prefs::importSettings(const QUrl &file) {
    const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    if (!QFile::exists(path)) return false;
    QSettings from(path, QSettings::IniFormat);
    if (from.status() != QSettings::NoError || from.allKeys().isEmpty()) return false;
    QSettings to;
    for (const QString &key : from.allKeys()) to.setValue(key, from.value(key));
    to.sync();
    updateAccent();
    emit changed();
    emit imported();
    return true;
}

// ------------------------------------------------------------ problem report

QString Prefs::problemReport() const {
    QString out;
    QTextStream s(&out);
    s << "Lumo " << QCoreApplication::applicationVersion() << "\n";
    s << "Qt " << qVersion() << ", " << QSysInfo::prettyProductName() << ", kernel " << QSysInfo::kernelVersion() << "\n";
    s << "Desktop: " << qEnvironmentVariable("XDG_CURRENT_DESKTOP") << " " << qEnvironmentVariable("KDE_SESSION_VERSION")
      << ", session " << qEnvironmentVariable("XDG_SESSION_TYPE") << "\n";
    s << "Screens: " << QGuiApplication::screens().size() << "\n\nSettings:\n";
    QSettings settings;
    for (const QString &key : settings.allKeys()) {
        // plain choices only: nothing you typed (numbers, cities, words, snippets)
        if (key.contains(QLatin1String("instinct")) || key.startsWith(QLatin1String("weather/")) || key.startsWith(QLatin1String("dictation/"))
            || key.startsWith(QLatin1String("shelf/")) || key.startsWith(QLatin1String("calendar/")) || key.contains(QLatin1String("history")))
            continue;
        const QVariant v = settings.value(key);
        if (v.typeId() == QMetaType::QString && v.toString().size() > 40) continue;
        s << "  " << key << " = " << v.toString() << "\n";
    }
    // recent warnings Lumo wrote, if any
    QFile log(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/lumo.log"));
    if (log.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QStringList lines = QString::fromUtf8(log.readAll()).split(QLatin1Char('\n'));
        s << "\nRecent log:\n" << lines.mid(std::max<qsizetype>(0, lines.size() - 30)).join(QLatin1Char('\n')) << "\n";
    }
    // take out who and where you are
    const QString home = QDir::homePath(), user = qEnvironmentVariable("USER"), host = QSysInfo::machineHostName();
    out.replace(home, QStringLiteral("~"));
    if (!user.isEmpty()) out.replace(user, QStringLiteral("<user>"), Qt::CaseInsensitive);
    if (!host.isEmpty()) out.replace(host, QStringLiteral("<computer>"), Qt::CaseInsensitive);
    return out;
}

void Prefs::copyProblemReport() const { QGuiApplication::clipboard()->setText(problemReport()); }
