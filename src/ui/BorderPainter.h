#pragma once

#include "../core/ThemeManager.h"
#include <QImage>
#include <QPainter>
#include <QRectF>

namespace quattro {

class BorderPainter {
public:
    static void paintCard(QPainter &p,
                          const QRectF &rect,
                          double radius,
                          const QColor &fill,
                          const BorderSpec &border);

    static QImage fastBlur(const QImage &src, int radius);
};

} // namespace quattro
