#include "BatteryService.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QProcess>

namespace quattro {

BatteryService::BatteryService(SystemStatsService *stats, QObject *parent)
    : QObject(parent)
    , m_stats(stats)
{
    m_pollTimer.setInterval(30000);
    connect(&m_pollTimer, &QTimer::timeout, this, &BatteryService::checkLowBattery);
    m_pollTimer.start();

    if (m_stats)
        connect(m_stats, &SystemStatsService::statsChanged, this, &BatteryService::checkLowBattery);

    QDBusConnection sys = QDBusConnection::systemBus();
    if (sys.isConnected()) {
        sys.connect(QStringLiteral("org.freedesktop.UPower"),
                    QStringLiteral("/org/freedesktop/UPower"),
                    QStringLiteral("org.freedesktop.DBus.Properties"),
                    QStringLiteral("PropertiesChanged"),
                    this, SLOT(onUPowerPropertiesChanged(QString,QVariantMap,QStringList)));
        sys.connect(QStringLiteral("net.hadess.PowerProfiles"),
                    QStringLiteral("/net/hadess/PowerProfiles"),
                    QStringLiteral("org.freedesktop.DBus.Properties"),
                    QStringLiteral("PropertiesChanged"),
                    this, SLOT(onUPowerPropertiesChanged(QString,QVariantMap,QStringList)));
    }

    refreshPowerProfile();
    checkLowBattery();
}

bool BatteryService::shouldWarnLowBattery(bool present, bool charging, int pct, bool alreadyNotified, bool &nextNotified) {
    const bool low = present && !charging && pct >= 0 && pct <= kLowBatteryThreshold;
    nextNotified = low;
    return low && !alreadyNotified;
}

void BatteryService::checkLowBattery() {
    if (!m_stats)
        return;

    bool nextNotified = m_notifiedLow;
    if (shouldWarnLowBattery(m_stats->batteryPresent(), m_stats->batteryCharging(),
                             m_stats->batteryPercent(), m_notifiedLow, nextNotified)) {
        QProcess::startDetached(QStringLiteral("quattro-battery-low"),
                                {QString::number(m_stats->batteryPercent())});
    }
    m_notifiedLow = nextNotified;

    const bool onBattery = m_stats->batteryPresent() && !m_stats->batteryCharging();
    if (!m_haveOnBattery || onBattery != m_lastOnBattery) {
        m_haveOnBattery = true;
        m_lastOnBattery = onBattery;
        QProcess::startDetached(QStringLiteral("quattro-powerprofiles-set"),
                                {onBattery ? QStringLiteral("battery") : QStringLiteral("ac")});
    }
}

void BatteryService::onUPowerPropertiesChanged(const QString &iface, const QVariantMap &changed, const QStringList &) {
    if (iface == QLatin1String("net.hadess.PowerProfiles") && changed.contains(QStringLiteral("ActiveProfile"))) {
        const QString p = changed.value(QStringLiteral("ActiveProfile")).toString();
        if (!p.isEmpty() && p != m_activeProfile) {
            m_activeProfile = p;
            emit profileChanged(m_activeProfile);
        }
    } else if (iface == QLatin1String("org.freedesktop.UPower") && changed.contains(QStringLiteral("OnBattery"))) {
        checkLowBattery();
        QTimer::singleShot(300, this, &BatteryService::refreshPowerProfile);
    }
}

void BatteryService::refreshPowerProfile() {
    QDBusInterface iface(QStringLiteral("net.hadess.PowerProfiles"),
                         QStringLiteral("/net/hadess/PowerProfiles"),
                         QStringLiteral("net.hadess.PowerProfiles"),
                         QDBusConnection::systemBus());
    if (!iface.isValid())
        return;
    const QString p = iface.property("ActiveProfile").toString();
    if (!p.isEmpty() && p != m_activeProfile) {
        m_activeProfile = p;
        emit profileChanged(m_activeProfile);
    }
}

void BatteryService::setPowerProfile(const QString &profile) {
    QDBusInterface iface(QStringLiteral("net.hadess.PowerProfiles"),
                         QStringLiteral("/net/hadess/PowerProfiles"),
                         QStringLiteral("net.hadess.PowerProfiles"),
                         QDBusConnection::systemBus());
    if (iface.isValid()) {
        iface.setProperty("ActiveProfile", profile);
        refreshPowerProfile();
    }
}

} // namespace quattro
