#pragma once

#include <QObject>
#include <QVariantList>

// Small everyday tools: a screen color picker, a screenshot, and the shelf,
// where files and text dropped on the island wait to be dragged out again.
// The shelf keeps paths, never copies, and stays in memory unless "Keep after
// restart" is on.
class Tools : public QObject {
    Q_OBJECT
    // [{ kind: "file" | "text", value, name, isImage }] newest first, up to 10
    Q_PROPERTY(QVariantList shelf READ shelf NOTIFY shelfChanged)
    Q_PROPERTY(bool keepShelf READ keepShelf WRITE setKeepShelf NOTIFY shelfChanged)
    Q_PROPERTY(QString lastColor READ lastColor NOTIFY colorPicked)
public:
    explicit Tools(QObject *parent = nullptr);

    QVariantList shelf() const { return m_shelf; }
    bool keepShelf() const;
    void setKeepShelf(bool on);
    QString lastColor() const { return m_color; }

    // A color from anywhere on screen (the desktop portal lets you click one);
    // its hex code goes to the clipboard.
    Q_INVOKABLE void pickColor();
    // Spectacle's region capture, or the portal's own screenshot dialog.
    Q_INVOKABLE void screenshot();

    // Adds dropped URLs (files) or text; returns how many were added.
    Q_INVOKABLE int addToShelf(const QVariantList &urls, const QString &text);
    Q_INVOKABLE void removeFromShelf(int index);
    Q_INVOKABLE void clearShelf();
    // Copies an item: a note's text, or the file itself (as a file, so a paste
    // in Dolphin copies the file).
    Q_INVOKABLE void copyShelfItem(int index);
    Q_INVOKABLE void openShelfItem(int index);

signals:
    void shelfChanged();
    void colorPicked(const QString &hex);
    void notice(const QString &text);

private:
    void add(const QVariantMap &item);
    void save();

    QVariantList m_shelf;
    QString m_color;
};
