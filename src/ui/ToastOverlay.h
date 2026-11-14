#pragma once

#include "../core/ShellConfig.h"
#include "../core/ThemeManager.h"
#include "../services/NotificationDaemon.h"
#include <QRect>
#include <QVector>
#include <QWidget>

namespace quattro {

struct LayerSpec;

class ToastOverlay : public QWidget {
    Q_OBJECT
public:
    ToastOverlay(ThemeManager *theme, ShellConfig *config, NotificationDaemon *daemon, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    struct HitRegion {
        QRect rect;
        uint notifId = 0;
        QString actionKey;
    };

    void rebuildLayout();
    int cardHeightFor(const NotificationItem &it, int cardWidth) const;
    LayerSpec currentLayerSpec(const QSize &sz) const;

    ThemeManager *m_theme;
    ShellConfig *m_config;
    NotificationDaemon *m_daemon;

    QVector<QRect> m_cardRects;
    QVector<HitRegion> m_actionHits;
};

} // namespace quattro
