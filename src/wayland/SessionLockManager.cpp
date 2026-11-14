#include "SessionLockManager.h"

#include <QtWaylandClient/private/qwaylandscreen_p.h>
#include <QtWaylandClient/private/qwaylandwindow_p.h>

namespace quattro {

SessionLockManager &SessionLockManager::instance() {
    static SessionLockManager inst;
    return inst;
}

SessionLockManager::SessionLockManager()
    : QWaylandClientExtensionTemplate<SessionLockManager>(1)
{
}

SessionLockManager::~SessionLockManager() {
    if (m_lockObj) {
        delete m_lockObj;
        m_lockObj = nullptr;
    }
}

bool SessionLockManager::ensureInitialized() {
    if (!m_initialized) {
        m_initialized = true;
        QWaylandClientExtension::initialize();
    }
    return isActive();
}

bool SessionLockManager::beginLock() {
    if (!ensureInitialized())
        return false;
    if (m_lockObj)
        return true;

    struct ::ext_session_lock_v1 *raw = lock();
    if (!raw)
        return false;

    m_locked = false;
    m_lockObj = new SessionLockObject(raw, this);
    SessionLockShellIntegration::instance().setCurrentLock(m_lockObj);

    connect(m_lockObj, &SessionLockObject::locked, this, [this]() {
        m_locked = true;
        emit locked();
    });
    connect(m_lockObj, &SessionLockObject::finished, this, [this]() {
        m_locked = false;
        SessionLockShellIntegration::instance().setCurrentLock(nullptr);
        if (m_lockObj) {
            m_lockObj->deleteLater();
            m_lockObj = nullptr;
        }
        emit finished();
    });
    return true;
}

void SessionLockManager::unlock() {
    SessionLockShellIntegration::instance().setCurrentLock(nullptr);
    if (m_lockObj) {
        if (m_locked)
            m_lockObj->unlock_and_destroy();
        else
            m_lockObj->destroy();
        m_lockObj->deleteLater();
        m_lockObj = nullptr;
    }
    m_locked = false;
}

void SessionLockManager::attachWindow(QWindow *window) {
    if (!window)
        return;
    window->setFlag(Qt::FramelessWindowHint, true);
    if (!window->handle())
        window->create();
    if (auto *ww = dynamic_cast<QtWaylandClient::QWaylandWindow *>(window->handle()))
        ww->setShellIntegration(&SessionLockShellIntegration::instance());
}

SessionLockObject::SessionLockObject(struct ::ext_session_lock_v1 *obj, QObject *parent)
    : QObject(parent)
    , QtWayland::ext_session_lock_v1(obj)
{
}

SessionLockObject::~SessionLockObject() = default;

void SessionLockObject::ext_session_lock_v1_locked() {
    emit locked();
}

void SessionLockObject::ext_session_lock_v1_finished() {
    emit finished();
}

SessionLockShellIntegration &SessionLockShellIntegration::instance() {
    static SessionLockShellIntegration inst;
    return inst;
}

QtWaylandClient::QWaylandShellSurface *SessionLockShellIntegration::createShellSurface(QtWaylandClient::QWaylandWindow *window) {
    if (!m_lockObj)
        return nullptr;
    return new SessionLockSurface(m_lockObj, window);
}

SessionLockSurface::SessionLockSurface(SessionLockObject *lockObj, QtWaylandClient::QWaylandWindow *window)
    : QtWaylandClient::QWaylandShellSurface(window)
{
    ::wl_output *output = nullptr;
    if (auto *ws = window->waylandScreen())
        output = ws->output();
    init(lockObj->get_lock_surface(window->wlSurface(), output));
}

SessionLockSurface::~SessionLockSurface() {
    if (isInitialized())
        destroy();
}

void SessionLockSurface::applyConfigure() {
    if (!m_pendingConfigure)
        return;
    m_pendingConfigure = false;
    ack_configure(m_pendingSerial);
    if (m_pendingSize.width() > 0 && m_pendingSize.height() > 0)
        resizeFromApplyConfigure(m_pendingSize);
}

void SessionLockSurface::ext_session_lock_surface_v1_configure(uint32_t serial, uint32_t width, uint32_t height) {
    m_pendingSerial = serial;
    m_pendingSize = QSize(static_cast<int>(width), static_cast<int>(height));
    m_pendingConfigure = true;

    const bool firstExpose = !m_configured;
    m_configured = true;

    applyConfigureWhenPossible();
    if (firstExpose && window())
        window()->updateExposure();
}

} // namespace quattro
