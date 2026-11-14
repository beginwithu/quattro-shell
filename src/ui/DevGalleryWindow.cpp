#include "DevGalleryWindow.h"

#include <QMouseEvent>
#include <QPainter>
#include <algorithm>

namespace quattro {

DevGalleryWindow::DevGalleryWindow(ThemeManager *theme, ShellConfig *config, QWidget *parent)
    : ShellPopupWindow(theme, config, QStringLiteral("quattro-dev-gallery"), true, parent)
{}

void DevGalleryWindow::paintCardContent(QPainter &p, const QRect &card) {
    const SurfaceColors &sc = theme()->popups();
    const QColor muted = theme()->muted();
    const QColor accent = sc.accent.isValid() ? sc.accent : theme()->accent();

    const int pad = 20;
    const int w = card.width() - pad * 2;
    int y = card.top() + pad;

    UiPaint::drawHero(p, QRect(card.left() + pad, y, w, 48), theme(),
                      QStringLiteral("󰏘"),
                      QStringLiteral("Quattro UI Gallery"),
                      QStringLiteral("Live C++ theme tokens, primitives, and interactive controls"),
                      sc.text, muted, accent);
    y += 58;

    // Color swatches
    UiPaint::drawSectionHeader(p, QRect(card.left() + pad, y, w, 22), theme(), QStringLiteral("Palette"), muted);
    y += 26;
    struct Swatch { QString name; QColor c; };
    const QVector<Swatch> swatches = {
        {QStringLiteral("Bg"), theme()->background()},
        {QStringLiteral("Fg"), theme()->foreground()},
        {QStringLiteral("Accent"), theme()->accent()},
        {QStringLiteral("Urgent"), theme()->urgent()},
        {QStringLiteral("Muted"), theme()->muted()},
        {QStringLiteral("Popup"), sc.background}
    };
    const int sw = (w - (swatches.size() - 1) * 8) / swatches.size();
    for (int i = 0; i < swatches.size(); ++i) {
        const QRect sr(card.left() + pad + i * (sw + 8), y, sw, 44);
        p.setBrush(swatches.at(i).c);
        p.setPen(QPen(ThemeManager::withAlpha(sc.text, 0.2), 1));
        p.drawRoundedRect(sr.adjusted(0, 0, 0, -16), 6, 6);
        p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontCaption()));
        p.setPen(muted);
        p.drawText(QRect(sr.left(), sr.bottom() - 14, sr.width(), 14), Qt::AlignCenter, swatches.at(i).name);
    }
    y += 56;

    // Controls
    UiPaint::drawSectionHeader(p, QRect(card.left() + pad, y, w, 22), theme(), QStringLiteral("Interactive Controls"), muted);
    y += 28;

    m_toggleARect = QRect(card.left() + pad, y, 42, 22);
    UiPaint::drawToggleSwitch(p, m_toggleARect, theme(), m_toggleA, m_toggleARect.contains(m_hoverPos));
    p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBody()));
    p.setPen(sc.text);
    p.drawText(QRect(m_toggleARect.right() + 10, y, 140, 22), Qt::AlignVCenter | Qt::AlignLeft,
               QStringLiteral("Primary Toggle"));

    m_toggleBRect = QRect(card.left() + pad + 210, y, 42, 22);
    UiPaint::drawToggleSwitch(p, m_toggleBRect, theme(), m_toggleB, m_toggleBRect.contains(m_hoverPos), true);
    p.drawText(QRect(m_toggleBRect.right() + 10, y, 140, 22), Qt::AlignVCenter | Qt::AlignLeft,
               QStringLiteral("Busy Switch"));
    y += 36;

    m_sliderRect = QRect(card.left() + pad, y, 260, 24);
    UiPaint::drawSlider(p, m_sliderRect, theme(), m_sliderVal, m_sliderRect.contains(m_hoverPos) || m_draggingSlider);
    p.setPen(muted);
    p.drawText(QRect(m_sliderRect.right() + 12, y, 80, 24), Qt::AlignVCenter | Qt::AlignLeft,
               QStringLiteral("%1%").arg(qRound(m_sliderVal * 100.0)));

    m_pillRect = QRect(m_sliderRect.right() + 90, y - 2, 110, 28);
    UiPaint::drawPillButton(p, m_pillRect, theme(), QStringLiteral("Pill Toggle"),
                            m_pillActive, m_pillRect.contains(m_hoverPos));

    m_confirmTestRect = QRect(m_pillRect.right() + 10, y - 2, 120, 28);
    UiPaint::drawPillButton(p, m_confirmTestRect, theme(), QStringLiteral("Test Modal"),
                            false, m_confirmTestRect.contains(m_hoverPos), true);
    y += 40;

    UiPaint::drawProgressBar(p, QRect(card.left() + pad, y, w, 8), 4, m_sliderVal,
                             ThemeManager::withAlpha(sc.text, 0.14), accent);
    y += 22;

    // Dial + Typography row
    UiPaint::drawDialGauge(p, QRect(card.left() + pad, y, 160, 150), theme(),
                           QStringLiteral("Sample Gauge"), m_sliderVal * 100.0, 100.0,
                           QStringLiteral("%"), accent, sc.text, muted);

    const int tx = card.left() + pad + 184;
    const int tw = w - 184;
    p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontDisplay(), QFont::Bold));
    p.setPen(sc.text);
    p.drawText(QRect(tx, y + 6, tw, 28), Qt::AlignVCenter | Qt::AlignLeft,
               QStringLiteral("Display (%1 px)").arg(theme()->fontDisplay()));
    p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontHeading(), QFont::DemiBold));
    p.drawText(QRect(tx, y + 36, tw, 24), Qt::AlignVCenter | Qt::AlignLeft,
               QStringLiteral("Heading • %1").arg(theme()->resolvedFontFamily()));
    p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBody()));
    p.drawText(QRect(tx, y + 64, tw, 22), Qt::AlignVCenter | Qt::AlignLeft,
               QStringLiteral("Body text sample (border radius %1px)").arg(theme()->cornerRadius()));
    p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontCaption()));
    p.setPen(muted);
    p.drawText(QRect(tx, y + 90, tw, 20), Qt::AlignVCenter | Qt::AlignLeft,
               QStringLiteral("Caption / metadata label • Esc to close"));
}

void DevGalleryWindow::handleCardMousePress(QMouseEvent *event, const QRect & /*card*/) {
    if (m_toggleARect.contains(event->pos())) {
        m_toggleA = !m_toggleA;
        update();
    } else if (m_toggleBRect.contains(event->pos())) {
        m_toggleB = !m_toggleB;
        update();
    } else if (m_pillRect.contains(event->pos())) {
        m_pillActive = !m_pillActive;
        update();
    } else if (m_confirmTestRect.contains(event->pos())) {
        askConfirm(QStringLiteral("Gallery Confirm Modal"),
                   QStringLiteral("Confirmation dialogs reuse ShellPopupWindow modal overlay."),
                   [this]() { m_pillActive = !m_pillActive; update(); });
    } else if (m_sliderRect.contains(event->pos())) {
        m_draggingSlider = true;
        m_sliderVal = std::clamp((event->pos().x() - m_sliderRect.left()) / double(m_sliderRect.width()), 0.0, 1.0);
        update();
    }
}

void DevGalleryWindow::handleCardMouseMove(QMouseEvent *event, const QRect &card) {
    ShellPopupWindow::handleCardMouseMove(event, card);
    if (m_draggingSlider) {
        m_sliderVal = std::clamp((event->pos().x() - m_sliderRect.left()) / double(m_sliderRect.width()), 0.0, 1.0);
        update();
    }
}

void DevGalleryWindow::handleCardMouseRelease(QMouseEvent *event, const QRect &card) {
    Q_UNUSED(event);
    Q_UNUSED(card);
    m_draggingSlider = false;
}

} // namespace quattro
