#pragma once

#include "../core/HyprlandClient.h"
#include "../core/ShellConfig.h"
#include "../core/ThemeManager.h"
#include "../services/AudioService.h"
#include "../services/NotificationDaemon.h"
#include "../services/SystemStatsService.h"
#include "../services/TrayWatcher.h"
#include <QHash>
#include <QObject>
#include <QRect>
#include <QScreen>
#include <QTimer>
#include <QVector>
#include <QWidget>
#include <functional>

namespace quattro {

struct LayerSpec;
class DockManager;
class MediaService;
class MenuWindow;
class NightlightService;
class RemoteSessionService;
class BarWindow;

struct BarExtraServices {
    MediaService *media = nullptr;
    NightlightService *nightlight = nullptr;
    RemoteSessionService *remote = nullptr;
    DockManager *dock = nullptr;
};

class BarManager : public QObject {
    Q_OBJECT
public:
    BarManager(ThemeManager *theme,
               ShellConfig *config,
               HyprlandClient *hypr,
               AudioService *audio,
               SystemStatsService *stats,
               NotificationDaemon *notif,
               TrayWatcher *tray,
               MenuWindow *menu,
               QObject *parent = nullptr);
    ~BarManager() override;

    void setExtraServices(const BarExtraServices &extra);
    QString debugGeometryJson() const;
    void cycleClockFormat();

signals:
    void panelToggleRequested(const QString &widgetId, const QPoint &globalPos, QScreen *screen);

private:
    void syncScreens();
    void updateAllBars();
    void readWorkspaceBlocks();

    ThemeManager *m_theme;
    ShellConfig *m_config;
    HyprlandClient *m_hypr;
    AudioService *m_audio;
    SystemStatsService *m_stats;
    NotificationDaemon *m_notif;
    TrayWatcher *m_tray;
    MenuWindow *m_menu;
    BarExtraServices m_extra;

    QVector<BarWindow *> m_bars;
    QHash<QString, int> m_monitorBlocks;
    QTimer m_clockTimer;
    int m_clockFormatIdx = 0;
};

class BarWindow : public QWidget {
    Q_OBJECT
public:
    BarWindow(QScreen *screen,
              ThemeManager *theme,
              ShellConfig *config,
              HyprlandClient *hypr,
              AudioService *audio,
              SystemStatsService *stats,
              NotificationDaemon *notif,
              TrayWatcher *tray,
              MenuWindow *menu,
              const QHash<QString, int> *monitorBlocks);

    QScreen *targetScreen() const { return m_screen; }
    void setExtraServices(const BarExtraServices &extra) { m_extra = extra; update(); }
    void syncGeometryAndVisibility();
    void setClockFormatIndex(int idx);
    QJsonObject debugGeometry() const;

signals:
    void panelToggleRequested(const QString &widgetId, const QPoint &globalPos, QScreen *screen);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    struct HitAction {
        QRect rect;
        QString tooltip;
        std::function<void(Qt::MouseButton, QPoint)> onClick;
        std::function<void(int)> onWheel;
    };

    struct WidgetRender {
        QString id;
        QSize size;
        std::function<void(QPainter &, const QRect &, QVector<HitAction> &)> draw;
    };

    bool isVertical() const;
    QString monitorName() const;
    QVector<int> workspaceIdsForMonitor() const;
    QString formattedClock() const;
    QString clockFormatString() const;
    WidgetRender buildWidget(const BarWidgetEntry &entry, bool vertical);
    LayerSpec currentLayerSpec(const QSize &sz) const;

    QScreen *m_screen;
    ThemeManager *m_theme;
    ShellConfig *m_config;
    HyprlandClient *m_hypr;
    AudioService *m_audio;
    SystemStatsService *m_stats;
    NotificationDaemon *m_notif;
    TrayWatcher *m_tray;
    MenuWindow *m_menu;
    const QHash<QString, int> *m_monitorBlocks;
    BarExtraServices m_extra;

    QVector<HitAction> m_hits;
    int m_hoveredHitIndex = -1;
    bool m_indicatorsHovered = false;
    int m_clockFormatIdx = 0;
    int m_wheelAccum = 0;
};

} // namespace quattro
