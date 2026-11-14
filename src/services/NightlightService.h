#pragma once

#include <QObject>
#include <QString>

namespace quattro {

class NightlightService : public QObject {
    Q_OBJECT
public:
    static constexpr int kNightTemp = 4000;
    static constexpr int kDayTemp = 6500;
    static constexpr int kIdentityThreshold = 6000;

    explicit NightlightService(QObject *parent = nullptr);

    bool stateLoaded() const { return m_stateLoaded; }
    bool enabled() const { return m_stateLoaded && isNightlightTemp(m_temperature); }
    int temperature() const { return m_temperature; }

    static bool isNightlightTemp(int temp) { return temp > 0 && temp < kIdentityThreshold; }

    void refresh();
    void applyTemperature(int temp);
    QString enable();
    QString disable();
    QString toggle();
    QString statusJson() const;

signals:
    void changed();

private:
    bool m_stateLoaded = false;
    int m_temperature = kDayTemp;
    bool m_applying = false;
    int m_pendingTemp = -1;
};

} // namespace quattro
