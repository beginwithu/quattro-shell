#pragma once

#include "../CommonUi.h"
#include "../../services/BatteryService.h"
#include "../../services/SystemStatsService.h"
#include <QDate>
#include <QTimer>

namespace quattro {

class ClockPanel : public ShellPopupWindow {
    Q_OBJECT
public:
    ClockPanel(ThemeManager *theme, ShellConfig *config, QWidget *parent = nullptr);

protected:
    QSize desiredCardSize() const override { return QSize(350, 430); }
    void onOpening() override;
    void onClosing() override;
    void paintCardContent(QPainter &p, const QRect &card) override;
    void handleCardMousePress(QMouseEvent *event, const QRect &card) override;

private:
    QDate m_displayMonth;
    QDate m_birthDate;
    int m_lifespanYears = 80;
    QTimer m_tick;
    QRect m_prevMonthRect;
    QRect m_todayRect;
    QRect m_nextMonthRect;
};

class PowerPanel : public ShellPopupWindow {
    Q_OBJECT
public:
    PowerPanel(BatteryService *battery,
               SystemStatsService *stats,
               ThemeManager *theme,
               ShellConfig *config,
               QWidget *parent = nullptr);

protected:
    QSize desiredCardSize() const override { return QSize(360, 420); }
    void onOpening() override;
    void paintCardContent(QPainter &p, const QRect &card) override;
    void handleCardMousePress(QMouseEvent *event, const QRect &card) override;

private:
    void probeDetailedBattery();

    BatteryService *m_battery;
    SystemStatsService *m_stats;
    double m_powerWatts = -1.0;
    double m_healthPercent = -1.0;
    int m_cycleCount = -1;
    QString m_timeRemaining;

    QVector<QPair<QString, QRect>> m_profileRects;
    QVector<QPair<QString, QRect>> m_actionRects;
};

} // namespace quattro
