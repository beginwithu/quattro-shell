#pragma once

#include <QObject>
#include <QtWaylandClient/qwaylandclientextension.h>
#include "qwayland-ext-idle-notify-v1.h"

namespace quattro {

class IdleNotification : public QObject, public QtWayland::ext_idle_notification_v1 {
    Q_OBJECT
public:
    explicit IdleNotification(struct ::ext_idle_notification_v1 *obj, QObject *parent = nullptr);
    ~IdleNotification() override;

signals:
    void idled();
    void resumed();

protected:
    void ext_idle_notification_v1_idled() override { emit idled(); }
    void ext_idle_notification_v1_resumed() override { emit resumed(); }
};

class IdleMonitor : public QWaylandClientExtensionTemplate<IdleMonitor>,
                    public QtWayland::ext_idle_notifier_v1 {
    Q_OBJECT
public:
    explicit IdleMonitor(QObject *parent = nullptr);
    ~IdleMonitor() override;

    void setTimeouts(int screensaverSec, int lockSec);

signals:
    void screensaverTriggered();
    void lockTriggered();
    void activityResumed();

private:
    void rebuildNotifications();

    int m_screensaverSec = 150;
    int m_lockSec = 300;
    IdleNotification *m_screensaverNotif = nullptr;
    IdleNotification *m_lockNotif = nullptr;
};

} // namespace quattro
