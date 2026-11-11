#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QStandardPaths>
#include <QTranslator>
#include <QDBusConnection>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>

#include "ActiveWindow.h"
#include "AgentHub.h"
#include "CloudSessions.h"
#include "ChatBackend.h"
#include "Dictation.h"
#include "HookInstaller.h"
#include "HookServer.h"
#include "Music.h"
#include "Beat.h"
#include "Calendar.h"
#include "Controls.h"
#include "Lyrics.h"
#include "Notifications.h"
#include "Power.h"
#include "Prefs.h"
#include "SystemStats.h"
#include "Tools.h"
#include "Weather.h"
#include "PlanUsage.h"
#include "Shell.h"
#include "Sounds.h"

namespace {
// This app was called Nook at first: bring its settings, history and
// downloaded model along, once.
void migrateFromNook() {
    const QString config = QDir::homePath() + QStringLiteral("/.config/");
    const QString data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1Char('/');
    if (!QDir(config + QStringLiteral("nook")).exists() || QFile::exists(config + QStringLiteral("lumo/lumo.conf"))) return;
    QDir().mkpath(config + QStringLiteral("lumo"));
    QFile::copy(config + QStringLiteral("nook/nook.conf"), config + QStringLiteral("lumo/lumo.conf"));
    QFile::copy(config + QStringLiteral("nook/statusline-previous"), config + QStringLiteral("lumo/statusline-previous"));
    QDir().rename(config + QStringLiteral("nook/sounds"), config + QStringLiteral("lumo/sounds"));
    QDir().mkpath(data + QStringLiteral("lumo"));
    QFile::copy(data + QStringLiteral("nook/dictations.json"), data + QStringLiteral("lumo/dictations.json"));
    QDir().rename(data + QStringLiteral("nook/models"), data + QStringLiteral("lumo/models"));
    // Launch at login follows.
    const QString oldAutostart = config + QStringLiteral("autostart/io.github.mobenlamine.nook.desktop");
    if (QFile::exists(oldAutostart)) {
        QFile::remove(oldAutostart);
        QFile f(config + QStringLiteral("autostart/io.github.mobenlamine.lumo.desktop"));
        if (f.open(QIODevice::WriteOnly))
            f.write(QStringLiteral("[Desktop Entry]\nType=Application\nName=Lumo\nExec=%1\nIcon=io.github.mobenlamine.lumo\n"
                                   "X-GNOME-Autostart-enabled=true\nNoDisplay=true\n").arg(QCoreApplication::applicationFilePath()).toUtf8());
    }
}
}

int main(int argc, char *argv[]) {
    QApplication::setOrganizationName(QStringLiteral("lumo"));
    QApplication::setApplicationName(QStringLiteral("lumo"));
    QApplication::setApplicationDisplayName(QStringLiteral("Lumo"));
    QApplication::setDesktopFileName(QStringLiteral("io.github.mobenlamine.lumo"));
    QApplication::setApplicationVersion(QStringLiteral(LUMO_VERSION));
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    migrateFromNook();

    if (HookServer::otherInstanceRunning()) {
        qWarning("Lumo is already running.");
        return 0;
    }

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    HookServer server;
    AgentHub hub;
    ChatBackend chat;
    HookInstaller installer;
    Sounds sounds;
    Shell shell;
    // Lumo's language: the one picked in Settings, or the desktop's. English
    // needs nothing: it is what the code is written in.
    QTranslator translator;
    auto useLanguage = [&] {
        QCoreApplication::removeTranslator(&translator);
        const QString language = shell.language();
        const QLocale locale = language.isEmpty() ? QLocale::system() : QLocale(language);
        if (translator.load(locale, QStringLiteral("lumo"), QStringLiteral("_"), QStringLiteral(":/i18n")))
            QCoreApplication::installTranslator(&translator);
    };
    useLanguage();
    ActiveWindow activeWindow;
    Dictation dictation(&activeWindow);
    PlanUsage planUsage;
    CloudSessions cloud;
    Music music;
    Beat beat;
    SystemStats systemStats;
    Controls controls;
    Power power;
    Tools tools;
    Weather weather;
    Notifications notifications;
    Prefs prefs;
    Lyrics lyrics;
    Calendar calendar;
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/Dictation"), &dictation, QDBusConnection::ExportScriptableSlots);

    QObject::connect(&server, &HookServer::hookEvent, &hub, &AgentHub::onHookEvent);
    QObject::connect(&chat, &ChatBackend::usage, &hub, &AgentHub::setUsage);
    QObject::connect(&planUsage, &PlanUsage::usage, &hub, &AgentHub::setUsage);
    QObject::connect(&cloud, &CloudSessions::sessionsChanged, &hub, &AgentHub::setCloudSessions);
    QObject::connect(&hub, &AgentHub::attention, &sounds, [&] { sounds.play(QStringLiteral("needs")); });
    QObject::connect(&hub, &AgentHub::waitingForYou, &sounds, [&] { sounds.play(QStringLiteral("needs")); });
    QObject::connect(&hub, &AgentHub::sessionFinished, &sounds, [&] { sounds.play(QStringLiteral("finish")); });
    QObject::connect(&hub, &AgentHub::sessionFailed, &sounds, [&] { sounds.play(QStringLiteral("error")); });
    QObject::connect(&dictation, &Dictation::sound, &sounds, &Sounds::play);
    QObject::connect(&chat, &ChatBackend::replied, &sounds, [&] { sounds.play(QStringLiteral("reply")); });

    if (!server.listen()) qWarning("Lumo couldn't listen on %s", qPrintable(HookServer::socketPath()));

    QQmlApplicationEngine engine;
    QObject::connect(&shell, &Shell::languageChanged, &engine, [&] {
        useLanguage();
        engine.retranslate();
    });
    engine.rootContext()->setContextProperty(QStringLiteral("hub"), &hub);
    engine.rootContext()->setContextProperty(QStringLiteral("chat"), &chat);
    engine.rootContext()->setContextProperty(QStringLiteral("installer"), &installer);
    engine.rootContext()->setContextProperty(QStringLiteral("sounds"), &sounds);
    engine.rootContext()->setContextProperty(QStringLiteral("shell"), &shell);
    engine.rootContext()->setContextProperty(QStringLiteral("dictation"), &dictation);
    engine.rootContext()->setContextProperty(QStringLiteral("activeWindow"), &activeWindow);
    engine.rootContext()->setContextProperty(QStringLiteral("planUsage"), &planUsage);
    engine.rootContext()->setContextProperty(QStringLiteral("music"), &music);
    engine.rootContext()->setContextProperty(QStringLiteral("beat"), &beat);
    engine.rootContext()->setContextProperty(QStringLiteral("systemStats"), &systemStats);
    engine.rootContext()->setContextProperty(QStringLiteral("controls"), &controls);
    engine.rootContext()->setContextProperty(QStringLiteral("power"), &power);
    engine.rootContext()->setContextProperty(QStringLiteral("tools"), &tools);
    engine.rootContext()->setContextProperty(QStringLiteral("weather"), &weather);
    engine.rootContext()->setContextProperty(QStringLiteral("notifications"), &notifications);
    engine.rootContext()->setContextProperty(QStringLiteral("prefs"), &prefs);
    engine.rootContext()->setContextProperty(QStringLiteral("lyrics"), &lyrics);
    engine.rootContext()->setContextProperty(QStringLiteral("calendar"), &calendar);
    engine.rootContext()->setContextProperty(QStringLiteral("cloud"), &cloud);
    engine.loadFromModule("Lumo", "Island");
    if (engine.rootObjects().isEmpty()) return 1;

    auto *island = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    shell.attach(island);
    shell.setupTray();
    shell.setupShortcuts();
    sounds.play(QStringLiteral("hello"));

    return app.exec();
}
