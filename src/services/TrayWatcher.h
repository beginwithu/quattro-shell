#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusContext>
#include <QDBusServiceWatcher>
#include <QIcon>
#include <QObject>
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <QVector>

namespace quattro {

struct TrayItem {
    QString key;        // "service/objectPath"
    QString service;
    QString objectPath;
    QString id;
    QString title;
    QString status;     // Passive, Active, NeedsAttention
    QString tooltip;
    QString iconName;
    QString iconThemePath;
    QPixmap pixmap;
    bool itemIsMenu = false;
};

class TrayWatcher;

class StatusNotifierWatcherAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierWatcher")
    Q_PROPERTY(QStringList RegisteredStatusNotifierItems READ registeredStatusNotifierItems)
    Q_PROPERTY(bool IsStatusNotifierHostRegistered READ isStatusNotifierHostRegistered)
    Q_PROPERTY(int ProtocolVersion READ protocolVersion)

public:
    explicit StatusNotifierWatcherAdaptor(TrayWatcher *parent);

    QStringList registeredStatusNotifierItems() const;
    bool isStatusNotifierHostRegistered() const { return true; }
    int protocolVersion() const { return 0; }

public slots:
    void RegisterStatusNotifierItem(const QString &service);
    void RegisterStatusNotifierHost(const QString &service);

signals:
    void StatusNotifierItemRegistered(const QString &service);
    void StatusNotifierItemUnregistered(const QString &service);
    void StatusNotifierHostRegistered();

private:
    TrayWatcher *m_watcher;
};

class TrayWatcher : public QObject, public QDBusContext {
    Q_OBJECT
public:
    explicit TrayWatcher(QObject *parent = nullptr);

    bool start();

    const QVector<TrayItem> &items() const { return m_items; }
    QStringList registeredKeys() const;

    void registerItem(const QString &serviceOrPath, const QString &senderService);
    void activateItem(const QString &key, int x, int y);
    void secondaryActivateItem(const QString &key, int x, int y);
    void contextMenuItem(const QString &key, int x, int y);

public slots:
    void onExternalItemRegistered(const QString &serviceOrPath);

signals:
    void itemsChanged();

private:
    void fetchItemProperties(const QString &service, const QString &path);
    void removeByService(const QString &service);
    static QPixmap decodePixmapList(const QVariant &v);

    StatusNotifierWatcherAdaptor *m_adaptor = nullptr;
    QDBusServiceWatcher m_dbusWatcher;
    QVector<TrayItem> m_items;
    bool m_ownsWatcher = false;
};

} // namespace quattro
