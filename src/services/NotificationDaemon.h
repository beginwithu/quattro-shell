#pragma once

#include <QDBusAbstractAdaptor>
#include <QDateTime>
#include <QImage>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <QVector>

namespace quattro {

struct NotificationAction {
    QString key;
    QString label;
};

struct NotificationItem {
    uint id = 0;
    QString appName;
    QString summary;
    QString body;
    QString appIcon;
    QImage image;
    int urgency = 1; // 0=low, 1=normal, 2=critical
    int timeoutMs = 5000;
    qint64 createdMs = 0;
    qint64 remainingMs = 5000;
    bool paused = false;
    QVector<NotificationAction> actions;
};

class NotificationDaemon;

class NotificationsDbusAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")
public:
    explicit NotificationsDbusAdaptor(NotificationDaemon *parent);

public slots:
    uint Notify(const QString &appName,
                uint replacesId,
                const QString &appIcon,
                const QString &summary,
                const QString &body,
                const QStringList &actions,
                const QVariantMap &hints,
                int expireTimeout);
    void CloseNotification(uint id);
    QStringList GetCapabilities();
    QString GetServerInformation(QString &vendor, QString &version, QString &specVersion);

signals:
    void NotificationClosed(uint id, uint reason);
    void ActionInvoked(uint id, const QString &actionKey);

private:
    NotificationDaemon *m_daemon;
};

class NotificationDaemon : public QObject {
    Q_OBJECT
public:
    explicit NotificationDaemon(QObject *parent = nullptr);

    bool registerDbus();

    const QVector<NotificationItem> &active() const { return m_active; }
    int count() const { return m_active.size(); }
    bool doNotDisturb() const { return m_dnd; }
    void setDoNotDisturb(bool dnd);
    bool toggleDoNotDisturb();

    void setPaused(bool paused);
    void dismissId(uint id, uint reason = 2);
    bool dismissNewest();
    void dismissAll();
    int dismissBySummary(const QString &needle);
    bool invokeAction(uint id, const QString &actionKey);
    bool invokeNewestAction(int indexZeroBased);
    bool actOnNewest();
    void replayHistory();

    // Called by adaptor
    uint handleNotify(const QString &appName,
                      uint replacesId,
                      const QString &appIcon,
                      const QString &summary,
                      const QString &body,
                      const QStringList &actions,
                      const QVariantMap &hints,
                      int expireTimeout);
    void handleCloseNotification(uint id);

signals:
    void notificationsChanged();
    void dndChanged(bool dnd);

private slots:
    void onTick();

private:
    static QImage decodeHintImage(const QVariantMap &hints);

    NotificationsDbusAdaptor *m_adaptor = nullptr;
    QVector<NotificationItem> m_active;
    QVector<NotificationItem> m_history;
    QTimer m_tickTimer;
    uint m_nextId = 1;
    bool m_dnd = false;
    bool m_paused = false;
    qint64 m_lastTickMs = 0;
    QString m_dndFilePath;
};

} // namespace quattro
