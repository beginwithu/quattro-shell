#include "NotificationDaemon.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>

namespace quattro {

NotificationsDbusAdaptor::NotificationsDbusAdaptor(NotificationDaemon *parent)
    : QDBusAbstractAdaptor(parent)
    , m_daemon(parent)
{
}

uint NotificationsDbusAdaptor::Notify(const QString &appName,
                                      uint replacesId,
                                      const QString &appIcon,
                                      const QString &summary,
                                      const QString &body,
                                      const QStringList &actions,
                                      const QVariantMap &hints,
                                      int expireTimeout) {
    return m_daemon->handleNotify(appName, replacesId, appIcon, summary, body, actions, hints, expireTimeout);
}

void NotificationsDbusAdaptor::CloseNotification(uint id) {
    m_daemon->handleCloseNotification(id);
}

QStringList NotificationsDbusAdaptor::GetCapabilities() {
    return {
        QStringLiteral("actions"),
        QStringLiteral("body"),
        QStringLiteral("body-markup"),
        QStringLiteral("persistence"),
        QStringLiteral("icon-static")
    };
}

QString NotificationsDbusAdaptor::GetServerInformation(QString &vendor, QString &version, QString &specVersion) {
    vendor = QStringLiteral("quattro");
    version = QStringLiteral("1.0");
    specVersion = QStringLiteral("1.2");
    return QStringLiteral("quattro-cpp");
}

NotificationDaemon::NotificationDaemon(QObject *parent)
    : QObject(parent)
    , m_adaptor(new NotificationsDbusAdaptor(this))
    , m_dndFilePath(qEnvironmentVariable("XDG_STATE_HOME", QDir::homePath() + QStringLiteral("/.local/state"))
                    + QStringLiteral("/quattro/toggles/dnd"))
{
    m_dnd = QFileInfo::exists(m_dndFilePath);
    m_tickTimer.setInterval(80);
    connect(&m_tickTimer, &QTimer::timeout, this, &NotificationDaemon::onTick);
}

bool NotificationDaemon::registerDbus() {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return false;
    if (!bus.registerService(QStringLiteral("org.freedesktop.Notifications")))
        return false;
    return bus.registerObject(QStringLiteral("/org/freedesktop/Notifications"), this);
}

void NotificationDaemon::setDoNotDisturb(bool dnd) {
    if (dnd == m_dnd)
        return;
    m_dnd = dnd;
    QDir().mkpath(QFileInfo(m_dndFilePath).absolutePath());
    if (dnd) {
        QFile f(m_dndFilePath);
        if (f.open(QIODevice::WriteOnly)) f.write("1\n");
    } else {
        QFile::remove(m_dndFilePath);
    }
    emit dndChanged(m_dnd);
}

bool NotificationDaemon::toggleDoNotDisturb() {
    setDoNotDisturb(!m_dnd);
    return m_dnd;
}

void NotificationDaemon::setPaused(bool paused) {
    m_paused = paused;
}

QImage NotificationDaemon::decodeHintImage(const QVariantMap &hints) {
    QVariant v = hints.value(QStringLiteral("image-data"));
    if (!v.isValid())
        v = hints.value(QStringLiteral("icon_data"));
    if (!v.isValid() || !v.canConvert<QDBusArgument>())
        return {};

    const QDBusArgument arg = v.value<QDBusArgument>();
    int width = 0, height = 0, rowstride = 0, bitsPerSample = 0, channels = 0;
    bool hasAlpha = false;
    QByteArray pixels;

    arg.beginStructure();
    arg >> width >> height >> rowstride >> hasAlpha >> bitsPerSample >> channels >> pixels;
    arg.endStructure();

    if (width <= 0 || height <= 0 || bitsPerSample != 8 || pixels.size() < rowstride * (height - 1) + width * channels)
        return {};

    const QImage::Format fmt = hasAlpha && channels == 4
                                   ? QImage::Format_RGBA8888
                                   : (channels == 3 ? QImage::Format_RGB888 : QImage::Format_Invalid);
    if (fmt == QImage::Format_Invalid)
        return {};

    QImage img(reinterpret_cast<const uchar *>(pixels.constData()), width, height, rowstride, fmt);
    return img.copy();
}

uint NotificationDaemon::handleNotify(const QString &appName,
                                      uint replacesId,
                                      const QString &appIcon,
                                      const QString &summary,
                                      const QString &body,
                                      const QStringList &actions,
                                      const QVariantMap &hints,
                                      int expireTimeout) {
    const uint id = replacesId > 0 ? replacesId : m_nextId++;
    if (m_nextId <= id)
        m_nextId = id + 1;

    NotificationItem item;
    item.id = id;
    item.appName = appName.isEmpty() ? QStringLiteral("Notification") : appName;
    item.summary = summary;
    item.body = body;
    item.appIcon = appIcon;
    item.image = decodeHintImage(hints);
    item.urgency = std::clamp(hints.value(QStringLiteral("urgency"), 1).toInt(), 0, 2);

    if (expireTimeout == 0 || item.urgency == 2) {
        item.timeoutMs = 0; // persistent
    } else if (expireTimeout > 0) {
        item.timeoutMs = std::max(1000, expireTimeout);
    } else {
        item.timeoutMs = item.urgency == 0 ? 4000 : 6000;
    }
    item.createdMs = QDateTime::currentMSecsSinceEpoch();
    item.remainingMs = item.timeoutMs;

    for (int i = 0; i + 1 < actions.size(); i += 2) {
        if (!actions[i].isEmpty() && !actions[i + 1].isEmpty())
            item.actions.push_back({actions[i], actions[i + 1]});
    }

    // Store in history (capped at 40)
    m_history.prepend(item);
    if (m_history.size() > 40)
        m_history.resize(40);

    if (m_dnd && item.urgency < 2) {
        return id;
    }

    int existingIdx = -1;
    for (int i = 0; i < m_active.size(); ++i) {
        if (m_active[i].id == id) {
            existingIdx = i;
            break;
        }
    }
    if (existingIdx >= 0) {
        m_active[existingIdx] = item;
    } else {
        m_active.prepend(item);
        if (m_active.size() > 6) {
            const uint droppedId = m_active.last().id;
            m_active.removeLast();
            emit m_adaptor->NotificationClosed(droppedId, 1);
        }
    }

    m_lastTickMs = QDateTime::currentMSecsSinceEpoch();
    if (!m_tickTimer.isActive())
        m_tickTimer.start();
    emit notificationsChanged();
    return id;
}

void NotificationDaemon::handleCloseNotification(uint id) {
    dismissId(id, 3);
}

void NotificationDaemon::dismissId(uint id, uint reason) {
    for (int i = 0; i < m_active.size(); ++i) {
        if (m_active[i].id == id) {
            m_active.removeAt(i);
            emit m_adaptor->NotificationClosed(id, reason);
            if (m_active.isEmpty())
                m_tickTimer.stop();
            emit notificationsChanged();
            return;
        }
    }
}

bool NotificationDaemon::dismissNewest() {
    if (m_active.isEmpty())
        return false;
    dismissId(m_active.first().id, 2);
    return true;
}

void NotificationDaemon::dismissAll() {
    if (m_active.isEmpty())
        return;
    const auto copy = m_active;
    m_active.clear();
    m_tickTimer.stop();
    for (const auto &it : copy)
        emit m_adaptor->NotificationClosed(it.id, 2);
    emit notificationsChanged();
}

int NotificationDaemon::dismissBySummary(const QString &needle) {
    if (needle.isEmpty())
        return 0;
    int removed = 0;
    for (int i = m_active.size() - 1; i >= 0; --i) {
        if (m_active[i].summary.contains(needle, Qt::CaseInsensitive)) {
            const uint id = m_active[i].id;
            m_active.removeAt(i);
            emit m_adaptor->NotificationClosed(id, 2);
            ++removed;
        }
    }
    if (removed > 0) {
        if (m_active.isEmpty())
            m_tickTimer.stop();
        emit notificationsChanged();
    }
    return removed;
}

bool NotificationDaemon::invokeAction(uint id, const QString &actionKey) {
    for (const auto &it : m_active) {
        if (it.id == id) {
            emit m_adaptor->ActionInvoked(id, actionKey);
            dismissId(id, 2);
            return true;
        }
    }
    return false;
}

bool NotificationDaemon::invokeNewestAction(int indexZeroBased) {
    if (m_active.isEmpty() || indexZeroBased < 0 || indexZeroBased >= m_active.first().actions.size())
        return false;
    return invokeAction(m_active.first().id, m_active.first().actions[indexZeroBased].key);
}

bool NotificationDaemon::actOnNewest() {
    if (m_active.isEmpty())
        return false;
    if (!m_active.first().actions.isEmpty())
        return invokeAction(m_active.first().id, m_active.first().actions.first().key);
    return dismissNewest();
}

void NotificationDaemon::replayHistory() {
    if (m_history.isEmpty())
        return;
    const int n = std::min<int>(3, m_history.size());
    for (int i = n - 1; i >= 0; --i) {
        NotificationItem item = m_history[i];
        item.remainingMs = item.timeoutMs > 0 ? item.timeoutMs : 5000;
        bool exists = false;
        for (const auto &a : m_active) {
            if (a.id == item.id) { exists = true; break; }
        }
        if (!exists)
            m_active.prepend(item);
    }
    m_lastTickMs = QDateTime::currentMSecsSinceEpoch();
    if (!m_active.isEmpty() && !m_tickTimer.isActive())
        m_tickTimer.start();
    emit notificationsChanged();
}

void NotificationDaemon::onTick() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 elapsed = std::max<qint64>(0, now - m_lastTickMs);
    m_lastTickMs = now;

    if (m_paused)
        return;

    bool changed = false;
    bool anyTimed = false;
    QVector<uint> expiredIds;

    for (int i = m_active.size() - 1; i >= 0; --i) {
        if (m_active[i].timeoutMs <= 0)
            continue;
        anyTimed = true;
        m_active[i].remainingMs -= elapsed;
        if (m_active[i].remainingMs <= 0) {
            expiredIds.push_back(m_active[i].id);
            m_active.removeAt(i);
            changed = true;
        }
    }

    for (uint id : expiredIds)
        emit m_adaptor->NotificationClosed(id, 1);

    if (!anyTimed && m_active.isEmpty())
        m_tickTimer.stop();

    if (changed || anyTimed)
        emit notificationsChanged();
}

} // namespace quattro
