#pragma once

#include <QHash>
#include <QLocalSocket>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

namespace quattro {

struct HyprMonitor {
    int id = -1;
    QString name;
    int activeWorkspaceId = -1;
    bool focused = false;
};

struct HyprWorkspace {
    int id = -1;
    QString name;
    QString monitorName;
    int windows = 0;
};

struct HyprClientInfo {
    QString address;
    QString appClass;
    QString initialClass;
    QString title;
    int workspaceId = -1;
    QString workspaceName;
    int monitorId = -1;
    int fullscreen = 0;
    bool floating = false;
    bool mapped = true;
    bool hidden = false;
    int focusHistoryId = 0;
};

class HyprlandClient : public QObject {
    Q_OBJECT
public:
    explicit HyprlandClient(QObject *parent = nullptr);

    bool isAvailable() const { return !m_socketPath.isEmpty(); }

    QByteArray request(const QByteArray &cmd) const;
    bool dispatch(const QString &expression);
    QByteArray eval(const QString &luaExpr) const;

    void refreshAll();
    void refreshState();
    void refreshOptions();

    QVector<HyprMonitor> monitors() const { return m_monitors; }
    QVector<HyprWorkspace> workspaces() const { return m_workspaces; }
    QVector<HyprClientInfo> clients() const;
    HyprMonitor monitorByName(const QString &name) const;
    QString focusedMonitorName() const;
    int activeWorkspaceForMonitor(const QString &name) const;
    bool workspaceHasWindows(int id) const;

    QString activeWindowTitle() const { return m_activeWindowTitle; }
    QString activeWindowClass() const { return m_activeWindowClass; }
    QString keyboardLayout() const { return m_keyboardLayout; }

    int rounding() const { return m_rounding; }
    int gapsOut() const { return m_gapsOut; }
    bool animationsEnabled() const { return m_animationsEnabled; }

signals:
    void stateChanged();
    void activeWindowChanged();
    void keyboardLayoutChanged(const QString &layout);
    void optionsChanged(int rounding, int gapsOut, bool animationsEnabled);
    void configReloaded();
    void fullscreenChanged();
    void screencastChanged();

private slots:
    void onEventReadyRead();
    void reconnectEventSocket();

private:
    void handleEventLine(const QString &line);

    QString m_socketPath;
    QString m_eventSocketPath;
    QLocalSocket m_eventSocket;
    QByteArray m_eventBuffer;
    QTimer m_reconnectTimer;
    QTimer m_refreshDebounce;

    QVector<HyprMonitor> m_monitors;
    QVector<HyprWorkspace> m_workspaces;
    QString m_activeWindowTitle;
    QString m_activeWindowClass;
    QString m_keyboardLayout;

    int m_rounding = 0;
    int m_gapsOut = 5;
    bool m_animationsEnabled = true;
};

} // namespace quattro
