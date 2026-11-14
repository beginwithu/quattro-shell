#include "BorderPainter.h"

#include <QLinearGradient>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

namespace quattro {

void BorderPainter::paintCard(QPainter &p,
                              const QRectF &rect,
                              double radius,
                              const QColor &fill,
                              const BorderSpec &border) {
    if (rect.width() <= 0 || rect.height() <= 0)
        return;

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    const double maxR = std::min(rect.width(), rect.height()) / 2.0;
    const double r = std::clamp(radius, 0.0, maxR);

    QPainterPath outerPath;
    if (r > 0.0)
        outerPath.addRoundedRect(rect, r, r);
    else
        outerPath.addRect(rect);

    if (border.isNone()) {
        p.fillPath(outerPath, fill);
        p.restore();
        return;
    }

    const QRectF innerRect = rect.adjusted(
        border.widths.left, border.widths.top,
        -border.widths.right, -border.widths.bottom);

    // Paint border fill across outerPath minus innerPath, or fill outer then inner
    QBrush borderBrush(border.color);
    if (border.gradient.enabled && border.gradient.colors.size() >= 2) {
        const double rad = border.gradient.angleDeg * M_PI / 180.0;
        const double dx = std::cos(rad) * rect.width() * 0.5;
        const double dy = std::sin(rad) * rect.height() * 0.5;
        const QPointF center = rect.center();
        QLinearGradient grad(center - QPointF(dx, dy), center + QPointF(dx, dy));
        const int count = border.gradient.colors.size();
        for (int i = 0; i < count; ++i)
            grad.setColorAt(static_cast<double>(i) / (count - 1), border.gradient.colors[i]);
        borderBrush = QBrush(grad);
    }

    if (innerRect.width() > 0 && innerRect.height() > 0) {
        QPainterPath innerPath;
        const double ir = std::max(0.0, r - border.widths.maxWidth() * 0.5);
        if (ir > 0.0)
            innerPath.addRoundedRect(innerRect, ir, ir);
        else
            innerPath.addRect(innerRect);

        QPainterPath ringPath = outerPath.subtracted(innerPath);
        p.fillPath(ringPath, borderBrush);
        p.fillPath(innerPath, fill);
    } else {
        p.fillPath(outerPath, borderBrush);
    }

    p.restore();
}

QImage BorderPainter::fastBlur(const QImage &src, int radius) {
    if (src.isNull() || radius <= 0)
        return src;

    // Downscale 4x for rapid, low-memory blur, then run 3-pass separable box blur
    const QSize smallSize(std::max(1, src.width() / 4), std::max(1, src.height() / 4));
    QImage img = src.scaled(smallSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                     .convertToFormat(QImage::Format_ARGB32_Premultiplied);

    const int w = img.width();
    const int h = img.height();
    const int r = std::max(1, radius / 4);
    const int div = r * 2 + 1;

    QImage tmp(w, h, QImage::Format_ARGB32_Premultiplied);
    for (int pass = 0; pass < 3; ++pass) {
        // Horizontal pass
        for (int y = 0; y < h; ++y) {
            const QRgb *in = reinterpret_cast<const QRgb *>(img.constScanLine(y));
            QRgb *out = reinterpret_cast<QRgb *>(tmp.scanLine(y));
            int sr = 0, sg = 0, sb = 0, sa = 0;
            for (int k = -r; k <= r; ++k) {
                const QRgb px = in[std::clamp(k, 0, w - 1)];
                sr += qRed(px); sg += qGreen(px); sb += qBlue(px); sa += qAlpha(px);
            }
            for (int x = 0; x < w; ++x) {
                out[x] = qRgba(sr / div, sg / div, sb / div, sa / div);
                const QRgb pOut = in[std::clamp(x - r, 0, w - 1)];
                const QRgb pIn = in[std::clamp(x + r + 1, 0, w - 1)];
                sr += qRed(pIn) - qRed(pOut);
                sg += qGreen(pIn) - qGreen(pOut);
                sb += qBlue(pIn) - qBlue(pOut);
                sa += qAlpha(pIn) - qAlpha(pOut);
            }
        }
        // Vertical pass
        for (int x = 0; x < w; ++x) {
            int sr = 0, sg = 0, sb = 0, sa = 0;
            for (int k = -r; k <= r; ++k) {
                const QRgb px = reinterpret_cast<const QRgb *>(tmp.constScanLine(std::clamp(k, 0, h - 1)))[x];
                sr += qRed(px); sg += qGreen(px); sb += qBlue(px); sa += qAlpha(px);
            }
            for (int y = 0; y < h; ++y) {
                reinterpret_cast<QRgb *>(img.scanLine(y))[x] = qRgba(sr / div, sg / div, sb / div, sa / div);
                const QRgb pOut = reinterpret_cast<const QRgb *>(tmp.constScanLine(std::clamp(y - r, 0, h - 1)))[x];
                const QRgb pIn = reinterpret_cast<const QRgb *>(tmp.constScanLine(std::clamp(y + r + 1, 0, h - 1)))[x];
                sr += qRed(pIn) - qRed(pOut);
                sg += qGreen(pIn) - qGreen(pOut);
                sb += qBlue(pIn) - qBlue(pOut);
                sa += qAlpha(pIn) - qAlpha(pOut);
            }
        }
    }
    return img.scaled(src.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

} // namespace quattro
