#include "Tools.h"

#include "Portal.h"

#include <QClipboard>
#include <QDBusArgument>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMimeData>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

#include <cmath>

namespace {
constexpr int kShelfMax = 10;
const QString kShot = QStringLiteral("org.freedesktop.portal.Screenshot");
}

Tools::Tools(QObject *parent) : QObject(parent) {
    if (keepShelf()) {
        const QJsonArray saved = QJsonDocument::fromJson(QSettings().value(QStringLiteral("shelf/items")).toByteArray()).array();
        for (const auto &v : saved) {
            const QVariantMap item = v.toObject().toVariantMap();
            // a file that has gone since is dropped from the shelf
            if (item.value(QStringLiteral("kind")) == QLatin1String("file")
                && !QFileInfo::exists(QUrl(item.value(QStringLiteral("value")).toString()).toLocalFile())) continue;
            m_shelf.append(item);
        }
    }
}

bool Tools::keepShelf() const { return QSettings().value(QStringLiteral("shelf/keep"), false).toBool(); }

void Tools::setKeepShelf(bool on) {
    QSettings().setValue(QStringLiteral("shelf/keep"), on);
    save();
    emit shelfChanged();
}

void Tools::save() {
    QSettings s;
    if (!keepShelf()) { s.remove(QStringLiteral("shelf/items")); return; }
    s.setValue(QStringLiteral("shelf/items"), QJsonDocument(QJsonArray::fromVariantList(m_shelf)).toJson(QJsonDocument::Compact));
}

void Tools::add(const QVariantMap &item) {
    // the same thing dropped again moves to the front
    for (int i = 0; i < m_shelf.size(); ++i)
        if (m_shelf[i].toMap().value(QStringLiteral("value")) == item.value(QStringLiteral("value"))) { m_shelf.removeAt(i); break; }
    m_shelf.prepend(item);
    while (m_shelf.size() > kShelfMax) m_shelf.removeLast();
}

int Tools::addToShelf(const QVariantList &urls, const QString &text) {
    int added = 0;
    for (const QVariant &v : urls) {
        const QUrl url = v.toUrl().isValid() ? v.toUrl() : QUrl(v.toString());
        if (!url.isLocalFile()) continue;
        const QFileInfo info(url.toLocalFile());
        if (!info.exists()) continue;
        const bool image = !QImageReader::imageFormat(info.filePath()).isEmpty();
        add({{QStringLiteral("kind"), QStringLiteral("file")}, {QStringLiteral("value"), url.toString()},
             {QStringLiteral("name"), info.fileName()}, {QStringLiteral("isImage"), image},
             {QStringLiteral("isDir"), info.isDir()}});
        ++added;
    }
    if (!added && !text.trimmed().isEmpty()) {
        add({{QStringLiteral("kind"), QStringLiteral("text")}, {QStringLiteral("value"), text},
             {QStringLiteral("name"), text.simplified().left(80)}, {QStringLiteral("isImage"), false}});
        ++added;
    }
    if (added) { save(); emit shelfChanged(); }
    return added;
}

void Tools::removeFromShelf(int index) {
    if (index < 0 || index >= m_shelf.size()) return;
    m_shelf.removeAt(index);
    save();
    emit shelfChanged();
}

void Tools::clearShelf() {
    m_shelf.clear();
    save();
    emit shelfChanged();
}

void Tools::copyShelfItem(int index) {
    if (index < 0 || index >= m_shelf.size()) return;
    const QVariantMap item = m_shelf[index].toMap();
    auto *mime = new QMimeData;
    if (item.value(QStringLiteral("kind")) == QLatin1String("file")) {
        const QUrl url(item.value(QStringLiteral("value")).toString());
        mime->setUrls({url});
        // Dolphin and file managers paste a "copy" of what this names
        mime->setData(QStringLiteral("x-special/gnome-copied-files"), "copy\n" + url.toEncoded());
        mime->setText(url.toLocalFile());
        emit notice(tr("Copied the file"));
    } else {
        mime->setText(item.value(QStringLiteral("value")).toString());
        emit notice(tr("Copied the text"));
    }
    QGuiApplication::clipboard()->setMimeData(mime);
}

void Tools::openShelfItem(int index) {
    if (index < 0 || index >= m_shelf.size()) return;
    const QVariantMap item = m_shelf[index].toMap();
    if (item.value(QStringLiteral("kind")) == QLatin1String("file"))
        QDesktopServices::openUrl(QUrl(item.value(QStringLiteral("value")).toString()));
}

void Tools::pickColor() {
    Portal::request(kShot, QStringLiteral("PickColor"), {QString()}, {}, this, [this](uint code, const QVariantMap &results) {
        if (code != 0) return;   // cancelled
        const QDBusArgument arg = results.value(QStringLiteral("color")).value<QDBusArgument>();
        double r = 0, g = 0, b = 0;
        arg.beginStructure();
        arg >> r >> g >> b;
        arg.endStructure();
        auto hex = [](double c) { return QStringLiteral("%1").arg(int(std::lround(c * 255)), 2, 16, QLatin1Char('0')); };
        m_color = (QLatin1Char('#') + hex(r) + hex(g) + hex(b)).toUpper();
        QGuiApplication::clipboard()->setText(m_color);
        emit colorPicked(m_color);
        emit notice(tr("%1 copied").arg(m_color));
    });
}

void Tools::screenshot() {
    if (!QStandardPaths::findExecutable(QStringLiteral("spectacle")).isEmpty()) {
        QProcess::startDetached(QStringLiteral("spectacle"), {QStringLiteral("--region")});
        return;
    }
    Portal::request(kShot, QStringLiteral("Screenshot"), {QString()}, {{QStringLiteral("interactive"), true}}, this,
                    [this](uint code, const QVariantMap &results) {
                        if (code != 0) return;
                        const QUrl uri(results.value(QStringLiteral("uri")).toString());
                        if (uri.isValid()) {
                            addToShelf({uri}, QString());
                            emit notice(tr("Screenshot on the shelf"));
                        }
                    });
}
