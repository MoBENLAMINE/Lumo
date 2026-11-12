#pragma once

#include <QJsonObject>
#include <QObject>
#include <QVariantList>

// Wires lumo-hook into ~/.claude/settings.json. Never writes blind: prepare()
// builds the merged file and its diff, apply() makes a dated backup first and
// only runs on an explicit click.
class HookInstaller : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool installed READ installed NOTIFY changed)
    Q_PROPERTY(bool coucouHooks READ coucouHooks NOTIFY changed)
    // Hooks still point at the pre-rename nook-hook path.
    Q_PROPERTY(bool needsUpdate READ needsUpdate NOTIFY changed)
    Q_PROPERTY(QVariantList preview READ preview NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString hookPath READ hookPath CONSTANT)

public:
    explicit HookInstaller(QObject *parent = nullptr);

    bool installed() const { return m_installed; }
    bool coucouHooks() const { return m_coucou; }
    bool needsUpdate() const { return m_legacy; }
    QVariantList preview() const { return m_preview; }
    QString message() const { return m_message; }
    QString hookPath() const;

    Q_INVOKABLE void refresh();
    // install=true: add our hooks (and take over Coucou's if replaceCoucou)
    Q_INVOKABLE void prepare(bool install, bool replaceCoucou, bool statusLine);
    Q_INVOKABLE bool apply();
    Q_INVOKABLE void cancel();

signals:
    void changed();

private:
    static QString settingsPath();
    // ok=false when the file exists but can't be read or isn't a JSON object.
    QJsonObject read(bool *ok = nullptr) const;
    bool deployHook();

    bool m_installed = false, m_coucou = false, m_legacy = false;
    QVariantList m_preview;
    QString m_message;
    QJsonObject m_next;
    QString m_previousStatusLine;
    bool m_hasNext = false;
};
