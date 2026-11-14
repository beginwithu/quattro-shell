#pragma once

#include "SystemStatsService.h"
#include <QObject>
#include <QString>
#include <QTimer>

namespace quattro {

class BatteryService : public QObject {
    Q_OBJECT
public:
    static constexpr int kLowBatteryThreshold = 10;

    explicit BatteryService(SystemStatsService *stats, QObject *parent = nullptr);

    QString activePowerProfile() const { return m_activeProfile; }
    void refreshPowerProfile();
    void setPowerProfile(const QString &profile);

    static bool shouldWarnLowBattery(bool present, bool charging, int pct, bool alreadyNotified, bool &nextNotified);

signals:
    void profileChanged(const QString &profile);

private slots:
    void checkLowBattery();
    void onUPowerPropertiesChanged(const QString &iface, const QVariantMap &changed, const QStringList &invalidated);

private:
    SystemStatsService *m_stats;
    bool m_notifiedLow = false;
    bool m_lastOnBattery = false;
    bool m_haveOnBattery = false;
    QString m_activeProfile{QStringLiteral("balanced")};
    QTimer m_pollTimer;
};

} // namespace quattro
