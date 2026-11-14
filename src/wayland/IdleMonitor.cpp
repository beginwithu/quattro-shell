#include "IdleMonitor.h"

#include <QGuiApplication>
#include <QtGui/qguiapplication_platform.h>

namespace quattro {

IdleNotification::IdleNotification(struct ::ext_idle_notification_v1 *obj, QObject *parent)
    : QObject(parent)
    , QtWayland::ext_idle_notification_v1(obj)
{
}

IdleNotification::~IdleNotification() {
    if (isInitialized())
        destroy();
}

IdleMonitor::IdleMonitor(QObject *parent)
    : QWaylandClientExtensionTemplate<IdleMonitor>(1, parent)
{
    connect(this, &QWaylandClientExtension::activeChanged, this, [this]() {
        if (isActive())
            rebuildNotifications();
    });
    initialize();
}

IdleMonitor::~IdleMonitor() {
    delete m_screensaverNotif;
    delete m_lockNotif;
    if (isActive())
        destroy();
}

void IdleMonitor::setTimeouts(int screensaverSec, int lockSec) {
    if (screensaverSec == m_screensaverSec && lockSec == m_lockSec)
        return;
    m_screensaverSec = screensaverSec;
    m_lockSec = lockSec;
    if (isActive())
        rebuildNotifications();
}

void IdleMonitor::rebuildNotifications() {
    delete m_screensaverNotif;
    m_screensaverNotif = nullptr;
    delete m_lockNotif;
    m_lockNotif = nullptr;

    auto *wlApp = qGuiApp ? qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>() : nullptr;
    struct ::wl_seat *seat = wlApp ? wlApp->seat() : nullptr;
    if (!seat || !isActive())
        return;

    if (m_screensaverSec > 0) {
        auto *obj = get_idle_notification(static_cast<uint32_t>(m_screensaverSec) * 1000u, seat);
        if (obj) {
            m_screensaverNotif = new IdleNotification(obj, this);
            connect(m_screensaverNotif, &IdleNotification::idled, this, &IdleMonitor::screensaverTriggered);
            connect(m_screensaverNotif, &IdleNotification::resumed, this, &IdleMonitor::activityResumed);
        }
    }

    if (m_lockSec > 0) {
        auto *obj = get_idle_notification(static_cast<uint32_t>(m_lockSec) * 1000u, seat);
        if (obj) {
            m_lockNotif = new IdleNotification(obj, this);
            connect(m_lockNotif, &IdleNotification::idled, this, &IdleMonitor::lockTriggered);
            connect(m_lockNotif, &IdleNotification::resumed, this, &IdleMonitor::activityResumed);
        }
    }
}

} // namespace quattro
