#pragma once

#include "CommonUi.h"
#include <QImage>

namespace quattro {

class WifiQrOverlay : public ShellPopupWindow {
    Q_OBJECT
public:
    WifiQrOverlay(ThemeManager *theme, ShellConfig *config, QWidget *parent = nullptr);

protected:
    QSize desiredCardSize() const override { return QSize(360, 440); }
    void onOpening() override;
    void paintCardContent(QPainter &p, const QRect &card) override;
    void handleCardMousePress(QMouseEvent *event, const QRect &card) override;

private:
    void fetchActiveWifi();
    void rebuildQr();

    QString m_ssid;
    QString m_security = QStringLiteral("WPA");
    QString m_password;
    QString m_statusText = QStringLiteral("Loading Wi-Fi details...");
    bool m_showPassword = false;
    QImage m_qrImage;
    QRect m_togglePassRect;
    QRect m_copyPassRect;
};

} // namespace quattro
