#pragma once

#include <QObject>

// Which app is in front, on KWin/Wayland: a tiny KWin script reports the
// active window's class over D-Bus whenever it changes. Used to paste with
// Ctrl+Shift+V in terminals and to note where you dictate.
class ActiveWindow : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.mobenlamine.lumo.ActiveWindow")
    Q_PROPERTY(bool fullscreen READ fullscreen NOTIFY changed)
public:
    explicit ActiveWindow(QObject *parent = nullptr);
    ~ActiveWindow() override;
    QString appClass() const { return m_class; }
    bool isTerminal() const;
    bool fullscreen() const { return m_fullscreen; }
    // Brings forward the window of the nearest process in `pids` (a session's
    // terminal or editor) through a one-shot KWin script.
    Q_INVOKABLE void focusProcess(const QVariantList &pids);

signals:
    void changed();

public slots:
    // Called by the KWin script.
    Q_SCRIPTABLE void report(const QString &resourceClass, bool fullscreen);
    Q_SCRIPTABLE QString current() const { return m_class; }

private:
    QString m_class;
    bool m_fullscreen = false;
};
