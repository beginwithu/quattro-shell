#pragma once

#include <QObject>
#include <QSize>
#include <QWindow>

#include <QtWaylandClient/qwaylandclientextension.h>
#include <QtWaylandClient/private/qwaylandshellintegration_p.h>
#include <QtWaylandClient/private/qwaylandshellsurface_p.h>
#include "qwayland-ext-session-lock-v1.h"

namespace quattro {

class SessionLockObject;
class SessionLockSurface;

class SessionLockManager : public QWaylandClientExtensionTemplate<SessionLockManager>,
                           public QtWayland::ext_session_lock_manager_v1 {
    Q_OBJECT
public:
    static SessionLockManager &instance();

    bool ensureInitialized();
    bool isAvailable() const { return isActive(); }

    bool beginLock();
    void unlock();
    bool isLocked() const { return m_locked; }
    bool isLockActive() const { return m_lockObj != nullptr; }

    void attachWindow(QWindow *window);

signals:
    void locked();
    void finished();

private:
    friend class SessionLockShellIntegration;
    SessionLockManager();
    ~SessionLockManager() override;

    bool m_initialized = false;
    bool m_locked = false;
    SessionLockObject *m_lockObj = nullptr;
};

class SessionLockObject : public QObject, public QtWayland::ext_session_lock_v1 {
    Q_OBJECT
public:
    explicit SessionLockObject(struct ::ext_session_lock_v1 *obj, QObject *parent = nullptr);
    ~SessionLockObject() override;

signals:
    void locked();
    void finished();

protected:
    void ext_session_lock_v1_locked() override;
    void ext_session_lock_v1_finished() override;
};

class SessionLockShellIntegration : public QtWaylandClient::QWaylandShellIntegration {
public:
    static SessionLockShellIntegration &instance();

    bool initialize(QtWaylandClient::QWaylandDisplay *) override { return true; }
    QtWaylandClient::QWaylandShellSurface *createShellSurface(QtWaylandClient::QWaylandWindow *window) override;

    void setCurrentLock(SessionLockObject *lockObj) { m_lockObj = lockObj; }

private:
    SessionLockShellIntegration() = default;
    SessionLockObject *m_lockObj = nullptr;
};

class SessionLockSurface : public QtWaylandClient::QWaylandShellSurface,
                           public QtWayland::ext_session_lock_surface_v1 {
    Q_OBJECT
public:
    SessionLockSurface(SessionLockObject *lockObj, QtWaylandClient::QWaylandWindow *window);
    ~SessionLockSurface() override;

    bool isExposed() const override { return m_configured; }
    void applyConfigure() override;

protected:
    void ext_session_lock_surface_v1_configure(uint32_t serial, uint32_t width, uint32_t height) override;

private:
    bool m_configured = false;
    bool m_pendingConfigure = false;
    uint32_t m_pendingSerial = 0;
    QSize m_pendingSize;
};

} // namespace quattro
