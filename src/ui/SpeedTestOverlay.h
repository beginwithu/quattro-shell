#pragma once

#include "CommonUi.h"
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QVector>

namespace quattro {

class SpeedTestOverlay : public ShellPopupWindow {
    Q_OBJECT
public:
    enum class Mode {
        Network,
        Disk
    };

    SpeedTestOverlay(Mode mode,
                     ThemeManager *theme,
                     ShellConfig *config,
                     QWidget *parent = nullptr);

protected:
    QSize desiredCardSize() const override { return QSize(560, 390); }
    void onOpening() override;
    void onClosing() override;
    void paintCardContent(QPainter &p, const QRect &card) override;
    void handleCardMousePress(QMouseEvent *event, const QRect &card) override;

private:
    void startTest();
    void stopTest();
    void startNetworkSpeedtest();
    void runCurlFallbackNetwork();
    void startDiskSpeedtest();

    Mode m_mode;
    bool m_running = false;
    QString m_phase = QStringLiteral("Ready");
    QString m_detail;
    double m_primaryMbps = 0.0;   // Download Mbps or Seq Read MB/s
    double m_secondaryMbps = 0.0; // Upload Mbps or Seq Write MB/s
    double m_pingMs = 0.0;
    QString m_serverInfo;
    QVector<double> m_history;

    QProcess *m_proc = nullptr;
    QNetworkAccessManager m_nam;
    QNetworkReply *m_reply = nullptr;
    QElapsedTimer m_dlTimer;
    qint64 m_dlBytes = 0;

    QRect m_actionBtnRect;
    QRect m_modeToggleRect;
};

} // namespace quattro
