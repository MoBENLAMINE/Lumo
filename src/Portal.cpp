#include "Portal.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QSettings>
#include <QTimer>

namespace {
const QString kService = QStringLiteral("org.freedesktop.portal.Desktop");
const QString kPath = QStringLiteral("/org/freedesktop/portal/desktop");
const QString kShortcuts = QStringLiteral("org.freedesktop.portal.GlobalShortcuts");
const QString kRemote = QStringLiteral("org.freedesktop.portal.RemoteDesktop");

struct Shortcut {
    QString id;
    QVariantMap props;
};
}
Q_DECLARE_METATYPE(Shortcut)

namespace {
QDBusArgument &operator<<(QDBusArgument &a, const Shortcut &s) {
    a.beginStructure();
    a << s.id << s.props;
    a.endStructure();
    return a;
}
const QDBusArgument &operator>>(const QDBusArgument &a, Shortcut &s) {
    a.beginStructure();
    a >> s.id >> s.props;
    a.endStructure();
    return a;
}

void registerTypes() {
    static bool done = false;
    if (done) return;
    done = true;
    qDBusRegisterMetaType<Shortcut>();
    qDBusRegisterMetaType<QList<Shortcut>>();
}

QString shortcutTrigger(const QList<Shortcut> &list, const QString &id) {
    for (const auto &s : list)
        if (s.id == id) return s.props.value(QStringLiteral("trigger_description")).toString();
    return {};
}
}

// One portal Request: waits for its Response signal, then calls back once.
class PortalRequest : public QObject {
    Q_OBJECT
public:
    PortalRequest(Portal::Reply reply, QObject *parent) : QObject(parent), m_reply(std::move(reply)) {}
    void answer(uint code, const QVariantMap &results) {
        if (m_done) return;
        m_done = true;
        m_reply(code, results);
        deleteLater();
    }
public slots:
    void onResponse(uint code, const QVariantMap &results) { answer(code, results); }

private:
    Portal::Reply m_reply;
    bool m_done = false;
};

namespace Portal {

void request(const QString &iface, const QString &method, QVariantList args, QVariantMap options, QObject *context, Reply reply) {
    registerTypes();
    static int counter = 0;
    QDBusConnection bus = QDBusConnection::sessionBus();
    const QString token = QStringLiteral("lumo%1").arg(++counter);
    options.insert(QStringLiteral("handle_token"), token);
    const QString sender = bus.baseService().mid(1).replace(u'.', u'_');
    const QString path = kPath + QStringLiteral("/request/") + sender + QLatin1Char('/') + token;

    auto *rq = new PortalRequest(std::move(reply), context);
    // Subscribe before calling, so a fast Response can't be missed.
    bus.connect(kService, path, QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"), rq,
                SLOT(onResponse(uint, QVariantMap)));

    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, iface, method);
    args << options;
    msg.setArguments(args);
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(msg), rq);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, rq, [rq, watcher] {
        watcher->deleteLater();
        if (watcher->isError()) rq->answer(2, {{QStringLiteral("error"), watcher->error().message()}});
    });
}

}

// ---------------------------------------------------------------- GlobalShortcuts

HoldShortcut::HoldShortcut(const QString &id, const QString &description, const QString &trigger, QObject *parent)
    : QObject(parent), m_id(id), m_description(description), m_trigger(trigger) {}

void HoldShortcut::start() {
    registerTypes();
    Portal::request(kShortcuts, QStringLiteral("CreateSession"), {},
                    {{QStringLiteral("session_handle_token"), QStringLiteral("lumodictation")}}, this,
                    [this](uint code, const QVariantMap &r) {
                        m_session = r.value(QStringLiteral("session_handle")).toString();
                        if (code != 0 || m_session.isEmpty()) {
                            emit readyChanged(false, r.value(QStringLiteral("error"), tr("The desktop has no shortcut portal.")).toString());
                            return;
                        }
                        QDBusConnection bus = QDBusConnection::sessionBus();
                        bus.connect(kService, kPath, kShortcuts, QStringLiteral("Activated"), this,
                                    SLOT(onActivated(QDBusObjectPath, QString, qulonglong, QVariantMap)));
                        bus.connect(kService, kPath, kShortcuts, QStringLiteral("Deactivated"), this,
                                    SLOT(onDeactivated(QDBusObjectPath, QString, qulonglong, QVariantMap)));
                        bus.connect(kService, kPath, kShortcuts, QStringLiteral("ShortcutsChanged"), this,
                                    SLOT(onChanged(QDBusMessage)));
                        bind();
                    });
}

void HoldShortcut::bind() {
    const QList<Shortcut> wanted{{m_id, {{QStringLiteral("description"), m_description},
                                         {QStringLiteral("preferred_trigger"), m_trigger}}}};
    Portal::request(kShortcuts, QStringLiteral("BindShortcuts"),
                    {QVariant::fromValue(QDBusObjectPath(m_session)), QVariant::fromValue(wanted), QString()}, {}, this,
                    [this](uint code, const QVariantMap &r) {
                        m_ready = code == 0;
                        QList<Shortcut> bound;
                        if (r.contains(QStringLiteral("shortcuts")))
                            r.value(QStringLiteral("shortcuts")).value<QDBusArgument>() >> bound;
                        const QString t = shortcutTrigger(bound, m_id);
                        if (!t.isEmpty()) m_trigger = t;
                        qInfo("dictation shortcut: %s (%s)", m_ready ? "bound" : "refused", qPrintable(m_trigger));
                        emit readyChanged(m_ready, m_ready ? QString() : tr("The shortcut wasn't granted."));
                    });
}

void HoldShortcut::configure() {
    if (m_session.isEmpty()) return;
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kShortcuts, QStringLiteral("ConfigureShortcuts"));
    msg.setArguments({QVariant::fromValue(QDBusObjectPath(m_session)), QString(), QVariantMap()});
    QDBusConnection::sessionBus().asyncCall(msg);
}

void HoldShortcut::onActivated(const QDBusObjectPath &session, const QString &id, qulonglong, const QVariantMap &) {
    if (session.path() == m_session && id == m_id) emit pressed();
}

void HoldShortcut::onDeactivated(const QDBusObjectPath &session, const QString &id, qulonglong, const QVariantMap &) {
    if (session.path() == m_session && id == m_id) emit released();
}

void HoldShortcut::onChanged(const QDBusMessage &message) {
    const QVariantList args = message.arguments();
    if (args.size() < 2 || args.at(0).value<QDBusObjectPath>().path() != m_session) return;
    QList<Shortcut> list;
    args.at(1).value<QDBusArgument>() >> list;
    const QString t = shortcutTrigger(list, m_id);
    if (!t.isEmpty()) m_trigger = t;
    emit readyChanged(m_ready, QString());
}

// ---------------------------------------------------------------- RemoteDesktop

KeyboardRemote::KeyboardRemote(QObject *parent) : QObject(parent) {}

void KeyboardRemote::chord(const QList<int> &keycodes) {
    if (ready()) {
        send(keycodes);
        return;
    }
    open([this, keycodes] { send(keycodes); });
}

void KeyboardRemote::open(std::function<void()> then) {
    m_waiting << std::move(then);
    if (m_opening) return;
    m_opening = true;
    auto fail = [this](const QString &why) {
        m_opening = false;
        m_waiting.clear();
        m_session.clear();
        emit failed(why);
    };
    Portal::request(kRemote, QStringLiteral("CreateSession"), {},
                    {{QStringLiteral("session_handle_token"), QStringLiteral("lumopaste")}}, this,
                    [this, fail](uint code, const QVariantMap &r) {
        m_session = r.value(QStringLiteral("session_handle")).toString();
        if (code != 0 || m_session.isEmpty()) return fail(tr("No remote-desktop portal."));
        QVariantMap select{{QStringLiteral("types"), uint(1)}, // keyboard only
                           {QStringLiteral("persist_mode"), uint(2)}};
        const QString token = QSettings().value(QStringLiteral("dictation/remoteToken")).toString();
        if (!token.isEmpty()) select.insert(QStringLiteral("restore_token"), token);
        Portal::request(kRemote, QStringLiteral("SelectDevices"), {QVariant::fromValue(QDBusObjectPath(m_session))}, select, this,
                        [this, fail](uint code, const QVariantMap &) {
            if (code != 0) return fail(tr("Keyboard access was refused."));
            Portal::request(kRemote, QStringLiteral("Start"), {QVariant::fromValue(QDBusObjectPath(m_session)), QString()}, {}, this,
                            [this, fail](uint code, const QVariantMap &r) {
                if (code != 0) return fail(tr("Keyboard access was refused."));
                const QString token = r.value(QStringLiteral("restore_token")).toString();
                if (!token.isEmpty()) QSettings().setValue(QStringLiteral("dictation/remoteToken"), token);
                m_started = true;
                m_opening = false;
                QDBusConnection::sessionBus().connect(kService, m_session, QStringLiteral("org.freedesktop.portal.Session"),
                                                      QStringLiteral("Closed"), this, SLOT(onClosed()));
                const auto waiting = std::exchange(m_waiting, {});
                for (const auto &f : waiting) f();
            });
        });
    });
}

void KeyboardRemote::send(const QList<int> &keycodes) {
    // One event at a time, each sent only once the portal acknowledged the
    // previous one: fired together they can arrive out of order, and a lost
    // key-up leaves the key auto-repeating in the app in front.
    QList<QPair<int, uint>> events;
    for (int k : keycodes) events << qMakePair(k, 1u);
    for (auto it = keycodes.rbegin(); it != keycodes.rend(); ++it) events << qMakePair(*it, 0u);
    // A second round of key-ups a moment later (-1: pause), whatever happened.
    events << qMakePair(-1, 0u);
    for (auto it = keycodes.rbegin(); it != keycodes.rend(); ++it) events << qMakePair(*it, 0u);
    m_queue += events;
    if (!m_sending) next();
}

void KeyboardRemote::next() {
    if (m_queue.isEmpty() || m_session.isEmpty()) {
        m_sending = false;
        m_queue.clear();
        return;
    }
    m_sending = true;
    const auto [code, state] = m_queue.takeFirst();
    if (code < 0) {
        QTimer::singleShot(120, this, &KeyboardRemote::next);
        return;
    }
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kRemote, QStringLiteral("NotifyKeyboardKeycode"));
    msg.setArguments({QVariant::fromValue(QDBusObjectPath(m_session)), QVariantMap(), code, state});
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        watcher->deleteLater();
        QTimer::singleShot(12, this, &KeyboardRemote::next);
    });
}

#include "Portal.moc"
