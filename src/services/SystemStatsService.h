#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

namespace quattro {

class SystemStatsService : public QObject {
    Q_OBJECT
public:
    explicit SystemStatsService(QObject *parent = nullptr);

    int cpuPercent() const { return m_cpuPercent; }
    int ramPercent() const { return m_ramPercent; }

    bool batteryPresent() const { return m_batteryPresent; }
    int batteryPercent() const { return m_batteryPercent; }
    bool batteryCharging() const { return m_batteryCharging; }

    bool networkConnected() const { return m_networkConnected; }
    bool wifiConnected() const { return m_wifiConnected; }
    bool ethernetConnected() const { return m_ethernetConnected; }

    bool stayAwake() const { return m_stayAwake; }
    bool nightLight() const { return m_nightLight; }
    bool screenRecording() const { return m_screenRecording; }
    bool passwordlessSudo() const { return m_passwordlessSudo; }
    bool updateAvailable() const { return m_updateAvailable; }

    void setStayAwake(bool awake);
    void toggleStayAwake() { setStayAwake(!m_stayAwake); }
    void setNightLight(bool enabled);
    void toggleNightLight() { setNightLight(!m_nightLight); }
    void refreshIndicators();
    void checkUpdates();
    void clearUpdates();

signals:
    void statsChanged();
    void indicatorsChanged();
    void updateStatusChanged();

private:
    void sampleProc();
    void sampleBattery();
    void sampleNetwork();

    QTimer m_fastTimer;
    QTimer m_slowTimer;
    QTimer m_updateTimer;

    quint64 m_prevTotal = 0;
    quint64 m_prevIdle = 0;
    int m_cpuPercent = 0;
    int m_ramPercent = 0;

    bool m_batteryPresent = false;
    int m_batteryPercent = 100;
    bool m_batteryCharging = false;

    bool m_networkConnected = false;
    bool m_wifiConnected = false;
    bool m_ethernetConnected = false;

    bool m_stayAwake = false;
    bool m_nightLight = false;
    bool m_screenRecording = false;
    bool m_passwordlessSudo = false;
    bool m_updateAvailable = false;
    bool m_checkingUpdates = false;
    bool m_checkingSudo = false;
    bool m_checkingRec = false;

    QString m_stateHome;
};

} // namespace quattro
