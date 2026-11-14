#include "TrayWatcher.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QImage>

namespace quattro {

StatusNotifierWatcherAdaptor::StatusNotifierWatcherAdaptor(TrayWatcher *parent)
    : QDBusAbstractAdaptor(parent)
    , m_watcher(parent)
{
}

QStringList StatusNotifierWatcherAdaptor::registeredStatusNotifierItems() const {
    return m_watcher->registeredKeys();
}

void StatusNotifierWatcherAdaptor::RegisterStatusNotifierItem(const QString &service) {
    const QString sender = m_watcher->calledFromDBus() ? m_watcher->message().service() : QString();
    m_watcher->registerItem(service, sender);
}

void StatusNotifierWatcherAdaptor::RegisterStatusNotifierHost(const QString &) {
    emit StatusNotifierHostRegistered();
}

TrayWatcher::TrayWatcher(QObject *parent)
    : QObject(parent)
    , m_adaptor(new StatusNotifierWatcherAdaptor(this))
{
    m_dbusWatcher.setConnection(QDBusConnection::sessionBus());
    m_dbusWatcher.setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    connect(&m_dbusWatcher, &QDBusServiceWatcher::serviceUnregistered,
            this, &TrayWatcher::removeByService);
}

bool TrayWatcher::start() {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return false;

    if (bus.registerService(QStringLiteral("org.kde.StatusNotifierWatcher"))) {
        m_ownsWatcher = true;
        bus.registerObject(QStringLiteral("/StatusNotifierWatcher"), this);
        emit m_adaptor->StatusNotifierHostRegistered();
        return true;
    }

    // Another watcher exists (e.g. during transition); pull its current items and listen
    QDBusInterface iface(QStringLiteral("org.kde.StatusNotifierWatcher"),
                         QStringLiteral("/StatusNotifierWatcher"),
                         QStringLiteral("org.kde.StatusNotifierWatcher"),
                         bus);
    if (iface.isValid()) {
        const QStringList existing = iface.property("RegisteredStatusNotifierItems").toStringList();
        for (const QString &entry : existing)
            registerItem(entry, QString());
        bus.connect(QStringLiteral("org.kde.StatusNotifierWatcher"),
                    QStringLiteral("/StatusNotifierWatcher"),
                    QStringLiteral("org.kde.StatusNotifierWatcher"),
                    QStringLiteral("StatusNotifierItemRegistered"),
                    this, SLOT(onExternalItemRegistered(QString)));
    }
    return false;
}

void TrayWatcher::onExternalItemRegistered(const QString &serviceOrPath) {
    registerItem(serviceOrPath, QString());
}

QStringList TrayWatcher::registeredKeys() const {
    QStringList out;
    out.reserve(m_items.size());
    for (const TrayItem &it : m_items)
        out.push_back(it.key);
    return out;
}

void TrayWatcher::registerItem(const QString &serviceOrPath, const QString &senderService) {
    QString service = serviceOrPath;
    QString path = QStringLiteral("/StatusNotifierItem");

    if (serviceOrPath.startsWith(QLatin1Char('/'))) {
        service = senderService;
        path = serviceOrPath;
    } else if (serviceOrPath.contains(QLatin1Char('/'))) {
        const int idx = serviceOrPath.indexOf(QLatin1Char('/'));
        service = serviceOrPath.left(idx);
        path = serviceOrPath.mid(idx);
    }

    if (service.isEmpty())
        return;

    const QString key = service + path;
    for (const TrayItem &it : m_items) {
        if (it.key == key)
            return;
    }

    m_dbusWatcher.addWatchedService(service);
    fetchItemProperties(service, path);

    if (m_ownsWatcher)
        emit m_adaptor->StatusNotifierItemRegistered(key);
}

void TrayWatcher::fetchItemProperties(const QString &service, const QString &path) {
    QDBusMessage msg = QDBusMessage::createMethodCall(
        service, path, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("GetAll"));
    msg << QStringLiteral("org.kde.StatusNotifierItem");

    auto *call = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(call, &QDBusPendingCallWatcher::finished, this, [this, call, service, path]() {
        call->deleteLater();
        QDBusPendingReply<QVariantMap> reply = *call;
        if (reply.isError())
            return;

        const QVariantMap props = reply.value();
        TrayItem item;
        item.service = service;
        item.objectPath = path;
        item.key = service + path;
        item.id = props.value(QStringLiteral("Id")).toString();
        item.title = props.value(QStringLiteral("Title")).toString();
        item.status = props.value(QStringLiteral("Status"), QStringLiteral("Active")).toString();
        item.iconName = props.value(QStringLiteral("IconName")).toString();
        item.iconThemePath = props.value(QStringLiteral("IconThemePath")).toString();
        item.itemIsMenu = props.value(QStringLiteral("ItemIsMenu")).toBool();
        item.pixmap = decodePixmapList(props.value(QStringLiteral("IconPixmap")));

        // ToolTip struct: (sa(iiay)ss) -> extract title (index 2)
        const QVariant ttVar = props.value(QStringLiteral("ToolTip"));
        if (ttVar.canConvert<QDBusArgument>()) {
            const QDBusArgument arg = ttVar.value<QDBusArgument>();
            QString ttIcon, ttTitle, ttSub;
            arg.beginStructure();
            arg >> ttIcon;
            arg.beginArray();
            while (!arg.atEnd()) {
                int w = 0, h = 0;
                QByteArray bytes;
                arg.beginStructure();
                arg >> w >> h >> bytes;
                arg.endStructure();
            }
            arg.endArray();
            arg >> ttTitle >> ttSub;
            arg.endStructure();
            item.tooltip = ttTitle.isEmpty() ? item.title : ttTitle;
        } else {
            item.tooltip = item.title;
        }

        if (item.pixmap.isNull() && !item.iconName.isEmpty()) {
            if (!item.iconThemePath.isEmpty()) {
                const QString cand = item.iconThemePath + QLatin1Char('/') + item.iconName + QStringLiteral(".png");
                if (QFile::exists(cand))
                    item.pixmap = QPixmap(cand);
            }
            if (item.pixmap.isNull()) {
                const QIcon icon = QIcon::fromTheme(item.iconName);
                if (!icon.isNull())
                    item.pixmap = icon.pixmap(24, 24);
            }
        }

        bool replaced = false;
        for (int i = 0; i < m_items.size(); ++i) {
            if (m_items[i].key == item.key) {
                m_items[i] = item;
                replaced = true;
                break;
            }
        }
        if (!replaced)
            m_items.push_back(item);

        emit itemsChanged();
    });
}

QPixmap TrayWatcher::decodePixmapList(const QVariant &v) {
    if (!v.isValid() || !v.canConvert<QDBusArgument>())
        return {};
    const QDBusArgument arg = v.value<QDBusArgument>();
    QPixmap best;
    int bestArea = 0;

    arg.beginArray();
    while (!arg.atEnd()) {
        int w = 0, h = 0;
        QByteArray argb;
        arg.beginStructure();
        arg >> w >> h >> argb;
        arg.endStructure();
        if (w > 0 && h > 0 && argb.size() >= w * h * 4) {
            // Network byte order ARGB32 -> host endianness QRgb
            QImage img(w, h, QImage::Format_ARGB32);
            const auto *src = reinterpret_cast<const uchar *>(argb.constData());
            for (int y = 0; y < h; ++y) {
                auto *dst = reinterpret_cast<QRgb *>(img.scanLine(y));
                for (int x = 0; x < w; ++x) {
                    const int off = (y * w + x) * 4;
                    dst[x] = qRgba(src[off + 1], src[off + 2], src[off + 3], src[off + 0]);
                }
            }
            if (w * h > bestArea) {
                bestArea = w * h;
                best = QPixmap::fromImage(img);
            }
        }
    }
    arg.endArray();
    return best;
}

void TrayWatcher::removeByService(const QString &service) {
    QStringList removedKeys;
    for (int i = m_items.size() - 1; i >= 0; --i) {
        if (m_items[i].service == service) {
            removedKeys.push_back(m_items[i].key);
            m_items.removeAt(i);
        }
    }
    if (!removedKeys.isEmpty()) {
        if (m_ownsWatcher) {
            for (const QString &k : removedKeys)
                emit m_adaptor->StatusNotifierItemUnregistered(k);
        }
        emit itemsChanged();
    }
}

void TrayWatcher::activateItem(const QString &key, int x, int y) {
    for (const TrayItem &it : m_items) {
        if (it.key != key) continue;
        const QString method = it.itemIsMenu ? QStringLiteral("ContextMenu") : QStringLiteral("Activate");
        QDBusMessage msg = QDBusMessage::createMethodCall(
            it.service, it.objectPath, QStringLiteral("org.kde.StatusNotifierItem"), method);
        msg << x << y;
        QDBusConnection::sessionBus().asyncCall(msg);
        return;
    }
}

void TrayWatcher::secondaryActivateItem(const QString &key, int x, int y) {
    for (const TrayItem &it : m_items) {
        if (it.key != key) continue;
        QDBusMessage msg = QDBusMessage::createMethodCall(
            it.service, it.objectPath, QStringLiteral("org.kde.StatusNotifierItem"), QStringLiteral("SecondaryActivate"));
        msg << x << y;
        QDBusConnection::sessionBus().asyncCall(msg);
        return;
    }
}

void TrayWatcher::contextMenuItem(const QString &key, int x, int y) {
    for (const TrayItem &it : m_items) {
        if (it.key != key) continue;
        QDBusMessage msg = QDBusMessage::createMethodCall(
            it.service, it.objectPath, QStringLiteral("org.kde.StatusNotifierItem"), QStringLiteral("ContextMenu"));
        msg << x << y;
        QDBusConnection::sessionBus().asyncCall(msg);
        return;
    }
}

} // namespace quattro
