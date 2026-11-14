#pragma once

#include "../CommonUi.h"
#include "../../core/HyprlandClient.h"
#include "../../services/BrightnessService.h"
#include <QJsonArray>
#include <QProcess>
#include <QVector>

namespace quattro {

struct MonitorDetailedInfo {
    QString name;
    QString description;
    int width = 1920;
    int height = 1080;
    double refreshRate = 60.0;
    int x = 0;
    int y = 0;
    double scale = 1.0;
    int transform = 0;
    bool focused = false;
    QStringList availableModes;
};

class MonitorPanel : public ShellPopupWindow {
    Q_OBJECT
public:
    MonitorPanel(HyprlandClient *hypr,
                 BrightnessService *brightness,
                 ThemeManager *theme,
                 ShellConfig *config,
                 QWidget *parent = nullptr);

protected:
    QSize desiredCardSize() const override { return QSize(460, 520); }
    void onOpening() override;
    void paintCardContent(QPainter &p, const QRect &card) override;
    void handleCardMousePress(QMouseEvent *event, const QRect &card) override;
    void handleCardMouseMove(QMouseEvent *event, const QRect &card) override;
    void handleCardMouseRelease(QMouseEvent *event, const QRect &card) override;

private:
    void fetchMonitorsJson();
    void applyMonitorKeyword(const MonitorDetailedInfo &m);

    HyprlandClient *m_hypr;
    BrightnessService *m_brightness;
    QVector<MonitorDetailedInfo> m_monitors;
    int m_selectedIdx = 0;
    int m_brightnessPct = 100;
    bool m_draggingLayout = false;
    bool m_draggingBright = false;
    QPoint m_dragStartMouse;
    QPoint m_dragOrigPos;
    double m_mapScale = 0.05;

    QRect m_mapRect;
    QRect m_brightRect;
    QVector<QPair<int, QRect>> m_tabRects;
    QVector<QPair<double, QRect>> m_scaleRects;
    QVector<QPair<QString, QRect>> m_modeRects;
    QRect m_rotateRect;
};

struct WifiNetworkItem {
    bool inUse = false;
    QString ssid;
    QString security;
    int signal = 0;
};

class NetworkPanel : public ShellPopupWindow {
    Q_OBJECT
public:
    NetworkPanel(ThemeManager *theme,
                 ShellConfig *config,
                 QWidget *parent = nullptr);

signals:
    void openWifiQrRequested();
    void openSpeedTestRequested();

protected:
    QSize desiredCardSize() const override { return QSize(420, 520); }
    void onOpening() override;
    void paintCardContent(QPainter &p, const QRect &card) override;
    bool handleKey(QKeyEvent *event) override;
    void handleCardMousePress(QMouseEvent *event, const QRect &card) override;

private:
    void refreshNetwork();
    void toggleWifiRadio();
    void connectToSsid(const QString &ssid, const QString &password = QString());

    bool m_wifiEnabled = true;
    bool m_scanning = false;
    QString m_activeSsid;
    QString m_ipAddress;
    QString m_dnsServer;
    QString m_statusText;
    QVector<WifiNetworkItem> m_networks;

    // Inline password prompt for secured SSID
    QString m_promptSsid;
    QString m_passwordInput;
    QRect m_passwordConnectRect;
    QRect m_passwordCancelRect;

    QRect m_wifiToggleRect;
    QRect m_rescanRect;
    QRect m_qrRect;
    QRect m_speedRect;
    QRect m_listRect;
    QVector<QPair<int, QRect>> m_netRows;
};

} // namespace quattro
