#include "CommonUi.h"

#include "../wayland/LayerShellIntegration.h"
#include "BorderPainter.h"
#include <QFontMetrics>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace quattro {

void UiPaint::drawCard(QPainter &p,
                       const QRect &rect,
                       ThemeManager *theme,
                       const QString &section,
                       const QColor &bg,
                       int radius,
                       int fallbackBorderWidth) {
    const QColor borderCol = (section == QLatin1String("menu"))
                                 ? theme->menu().border
                                 : theme->popups().border;
    const BorderSpec spec = theme->surfaceBorderSpec(section, QStringLiteral("border"), borderCol, fallbackBorderWidth);
    BorderPainter::paintCard(p, QRectF(rect), static_cast<double>(radius), bg, spec);
}

void UiPaint::drawHero(QPainter &p,
                       const QRect &rect,
                       ThemeManager *theme,
                       const QString &glyph,
                       const QString &title,
                       const QString &subtitle,
                       const QColor &textColor,
                       const QColor &mutedColor,
                       const QColor &accentColor) {
    const int iconSize = rect.height();
    QFont iconFont(theme->resolvedFontFamily(), std::max(16, theme->fontDisplay()));
    p.setFont(iconFont);
    p.setPen(accentColor);
    p.drawText(QRect(rect.left(), rect.top(), iconSize, rect.height()), Qt::AlignCenter, glyph);

    const int tx = rect.left() + iconSize + theme->space(12);
    const int tw = std::max(40, rect.right() - tx);
    QFont titleFont(theme->resolvedFontFamily(), theme->fontSubtitle(), QFont::DemiBold);
    p.setFont(titleFont);
    p.setPen(textColor);
    p.drawText(QRect(tx, rect.top(), tw, rect.height() / 2 + 2), Qt::AlignVCenter | Qt::AlignLeft,
               QFontMetrics(titleFont).elidedText(title, Qt::ElideRight, tw));

    QFont subFont(theme->resolvedFontFamily(), theme->fontCaption());
    p.setFont(subFont);
    p.setPen(mutedColor);
    p.drawText(QRect(tx, rect.top() + rect.height() / 2, tw, rect.height() / 2), Qt::AlignVCenter | Qt::AlignLeft,
               QFontMetrics(subFont).elidedText(subtitle, Qt::ElideRight, tw));
}

void UiPaint::drawSectionHeader(QPainter &p,
                                const QRect &rect,
                                ThemeManager *theme,
                                const QString &title,
                                const QColor &color) {
    QFont f(theme->resolvedFontFamily(), theme->fontCaption(), QFont::DemiBold);
    f.setLetterSpacing(QFont::PercentageSpacing, 108);
    p.setFont(f);
    p.setPen(color);
    p.drawText(rect, Qt::AlignVCenter | Qt::AlignLeft, title.toUpper());
}

void UiPaint::drawToggleSwitch(QPainter &p,
                               const QRect &rect,
                               ThemeManager *theme,
                               bool checked,
                               bool hovered,
                               bool busy) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    const int h = std::min(rect.height(), 22);
    const int w = std::max(38, h * 2 - 4);
    const QRect track(rect.center().x() - w / 2, rect.center().y() - h / 2, w, h);
    const qreal rad = h / 2.0;

    QColor bg = checked ? ThemeManager::withAlpha(theme->accent(), busy ? 0.55 : 0.9)
                        : (hovered ? theme->hoverFill() : theme->normalFill());
    p.setBrush(bg);
    p.setPen(QPen(checked ? theme->accent() : theme->normalBorderColor(), 1));
    p.drawRoundedRect(track, rad, rad);

    const int knobD = h - 6;
    const int knobX = checked ? (track.right() - knobD - 3) : (track.left() + 3);
    const QRect knob(knobX, track.top() + 3, knobD, knobD);
    p.setBrush(checked ? theme->background() : theme->foreground());
    p.setPen(Qt::NoPen);
    p.drawEllipse(knob);
    p.restore();
}

void UiPaint::drawSlider(QPainter &p,
                         const QRect &rect,
                         ThemeManager *theme,
                         double fraction,
                         bool hovered) {
    const double f = std::clamp(fraction, 0.0, 1.0);
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    const int trackH = 6;
    const int cy = rect.center().y();
    const QRect track(rect.left() + 6, cy - trackH / 2, std::max(10, rect.width() - 12), trackH);

    p.setBrush(ThemeManager::withAlpha(theme->foreground(), 0.16));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(track, 3, 3);

    const int fillW = static_cast<int>(std::lround(track.width() * f));
    if (fillW > 0) {
        p.setBrush(theme->accent());
        p.drawRoundedRect(QRect(track.left(), track.top(), fillW, trackH), 3, 3);
    }

    const int thumbR = hovered ? 7 : 6;
    const int thumbX = track.left() + fillW;
    p.setBrush(theme->foreground());
    p.setPen(QPen(theme->background(), 1.5));
    p.drawEllipse(QPoint(thumbX, cy), thumbR, thumbR);
    p.restore();
}

void UiPaint::drawProgressBar(QPainter &p,
                              const QRect &rect,
                              int radius,
                              double fraction,
                              const QColor &track,
                              const QColor &fill) {
    const double f = std::clamp(fraction, 0.0, 1.0);
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(track);
    p.drawRoundedRect(rect, radius, radius);
    const int w = static_cast<int>(std::lround(rect.width() * f));
    if (w > 0) {
        p.setBrush(fill);
        p.drawRoundedRect(QRect(rect.left(), rect.top(), w, rect.height()), radius, radius);
    }
    p.restore();
}

void UiPaint::drawPillButton(QPainter &p,
                             const QRect &rect,
                             ThemeManager *theme,
                             const QString &text,
                             bool active,
                             bool hovered,
                             bool danger) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor fill = active
                            ? ThemeManager::withAlpha(danger ? theme->urgent() : theme->accent(), 0.22)
                            : (hovered ? theme->hoverFill() : theme->normalFill());
    const QColor border = active
                              ? (danger ? theme->urgent() : theme->accent())
                              : (hovered ? theme->hoverBorderColor() : theme->normalBorderColor());
    const int r = std::min(theme->cornerRadius() > 0 ? theme->cornerRadius() : 8, rect.height() / 2);
    p.setBrush(fill);
    p.setPen(QPen(border, 1));
    p.drawRoundedRect(rect, r, r);

    QFont f(theme->resolvedFontFamily(), theme->fontBodySmall(), active ? QFont::DemiBold : QFont::Normal);
    p.setFont(f);
    p.setPen(active ? (danger ? theme->urgent() : theme->accent()) : theme->popups().text);
    p.drawText(rect.adjusted(6, 0, -6, 0), Qt::AlignCenter,
               QFontMetrics(f).elidedText(text, Qt::ElideRight, std::max(10, rect.width() - 12)));
    p.restore();
}

void UiPaint::drawSearchHeader(QPainter &p,
                               const QRect &rect,
                               ThemeManager *theme,
                               const QString &text,
                               const QString &placeholder,
                               const QColor &textColor,
                               const QColor &mutedColor,
                               const QColor &accentColor) {
    p.save();
    p.setPen(QPen(ThemeManager::withAlpha(textColor, 0.14), 1));
    p.drawLine(rect.left(), rect.bottom(), rect.right(), rect.bottom());

    QFont iconFont(theme->resolvedFontFamily(), theme->fontSubtitle());
    p.setFont(iconFont);
    p.setPen(accentColor);
    p.drawText(QRect(rect.left(), rect.top(), 34, rect.height()), Qt::AlignCenter, QStringLiteral(""));

    QFont inputFont(theme->resolvedFontFamily(), theme->fontBody());
    p.setFont(inputFont);
    const QRect textRect(rect.left() + 36, rect.top(), std::max(20, rect.width() - 44), rect.height());
    if (text.isEmpty()) {
        p.setPen(mutedColor);
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, placeholder);
    } else {
        p.setPen(textColor);
        const QString disp = QFontMetrics(inputFont).elidedText(text, Qt::ElideLeft, textRect.width());
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, disp);
        const int cw = std::min(textRect.width() - 2, QFontMetrics(inputFont).horizontalAdvance(disp));
        p.fillRect(QRect(textRect.left() + cw + 1, rect.center().y() - 9, 2, 18), accentColor);
    }
    p.restore();
}

void UiPaint::drawDialGauge(QPainter &p,
                            const QRect &rect,
                            ThemeManager *theme,
                            const QString &title,
                            double value,
                            double maxValue,
                            const QString &unit,
                            const QColor &arcColor,
                            const QColor &textColor,
                            const QColor &mutedColor) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    const int size = std::min(rect.width(), rect.height() - 28);
    const QRect arcRect(rect.center().x() - size / 2 + 12, rect.top() + 8, size - 24, size - 24);
    const double frac = maxValue > 0 ? std::clamp(value / maxValue, 0.0, 1.0) : 0.0;

    // 270 deg sweep starting at 225 deg (bottom-left) clockwise (-270)
    const int penW = std::max(8, size / 14);
    p.setPen(QPen(ThemeManager::withAlpha(textColor, 0.14), penW, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(arcRect, 225 * 16, -270 * 16);

    if (frac > 0.0) {
        p.setPen(QPen(arcColor, penW, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(arcRect, 225 * 16, -static_cast<int>(std::lround(270.0 * frac * 16.0)));
    }

    QFont valFont(theme->resolvedFontFamily(), std::max(16, theme->fontHeading()), QFont::Bold);
    p.setFont(valFont);
    p.setPen(textColor);
    const QString valStr = value > 0.0 ? QString::number(value, 'f', value >= 100.0 ? 0 : 1) : QStringLiteral("—");
    p.drawText(arcRect.adjusted(0, -6, 0, -6), Qt::AlignCenter, valStr);

    QFont unitFont(theme->resolvedFontFamily(), theme->fontCaption());
    p.setFont(unitFont);
    p.setPen(mutedColor);
    p.drawText(arcRect.adjusted(0, 26, 0, 26), Qt::AlignCenter, unit);

    QFont titleFont(theme->resolvedFontFamily(), theme->fontBody(), QFont::DemiBold);
    p.setFont(titleFont);
    p.setPen(textColor);
    p.drawText(QRect(rect.left(), rect.bottom() - 24, rect.width(), 24), Qt::AlignCenter, title);
    p.restore();
}

ShellPopupWindow::ShellPopupWindow(ThemeManager *theme,
                                   ShellConfig *config,
                                   const QString &scope,
                                   bool centeredOverlay,
                                   QWidget *parent)
    : QWidget(parent)
    , m_theme(theme)
    , m_config(config)
    , m_scope(scope)
    , m_centered(centeredOverlay)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setWindowTitle(m_scope);

    const QSize sz = qGuiApp->primaryScreen() ? qGuiApp->primaryScreen()->size() : QSize(1920, 1080);
    resize(sz);
    winId();

    LayerSpec spec;
    spec.layer = LayerType::Overlay;
    spec.anchors = static_cast<uint32_t>(AnchorEdge::Top)
                   | static_cast<uint32_t>(AnchorEdge::Bottom)
                   | static_cast<uint32_t>(AnchorEdge::Left)
                   | static_cast<uint32_t>(AnchorEdge::Right);
    spec.exclusiveZone = -1;
    spec.keyboard = KeyboardFocus::Exclusive;
    spec.scope = m_scope;
    spec.desiredSize = sz;
    LayerShellIntegration::configureWindow(windowHandle(), spec);

    connect(m_theme, &ThemeManager::themeChanged, this, [this]() {
        if (m_opened) update();
    });
}

void ShellPopupWindow::syncScreenGeometry() {
    QScreen *scr = m_targetScreen;
    if (!scr && !m_anchorGlobal.isNull())
        scr = qGuiApp->screenAt(m_anchorGlobal);
    if (!scr)
        scr = qGuiApp->primaryScreen();
    if (!scr)
        return;

    const QSize sz = scr->size();
    if (size() != sz)
        resize(sz);

    LayerSpec spec;
    spec.layer = LayerType::Overlay;
    spec.anchors = static_cast<uint32_t>(AnchorEdge::Top)
                   | static_cast<uint32_t>(AnchorEdge::Bottom)
                   | static_cast<uint32_t>(AnchorEdge::Left)
                   | static_cast<uint32_t>(AnchorEdge::Right);
    spec.exclusiveZone = -1;
    spec.keyboard = KeyboardFocus::Exclusive;
    spec.scope = m_scope;
    spec.desiredSize = sz;
    LayerShellIntegration::updateWindowSpec(windowHandle(), spec);
}

void ShellPopupWindow::openAt(const QPoint &globalAnchor, QScreen *screen) {
    m_anchorGlobal = globalAnchor;
    m_targetScreen = screen;
    m_scrollOffset = 0;
    m_confirmOpen = false;
    m_opened = true;
    onOpening();
    syncScreenGeometry();
    show();
    raise();
    activateWindow();
    setFocus(Qt::OtherFocusReason);
    update();
}

void ShellPopupWindow::closePopup() {
    if (!m_opened)
        return;
    m_opened = false;
    m_confirmOpen = false;
    onClosing();
    hide();
    emit closed();
}

void ShellPopupWindow::askConfirm(const QString &title, const QString &body, std::function<void()> onConfirm) {
    m_confirmOpen = true;
    m_confirmTitle = title;
    m_confirmBody = body;
    m_confirmIdx = 1;
    m_onConfirm = std::move(onConfirm);
    update();
}

QColor ShellPopupWindow::scrimColor() const {
    return m_centered ? m_theme->menu().scrim : QColor(0, 0, 0, 1);
}

QRect ShellPopupWindow::computeCardRect() const {
    const QSize cs = desiredCardSize();
    const int margin = std::max(8, m_theme->gapsOut() + 6);
    const int maxW = std::max(160, width() - margin * 2);
    const int maxH = std::max(120, height() - margin * 2);
    const int w = std::min(cs.width(), maxW);
    const int h = std::min(cs.height(), maxH);

    if (m_centered || m_anchorGlobal.isNull())
        return QRect((width() - w) / 2, (height() - h) / 2, w, h);

    QScreen *scr = m_targetScreen ? m_targetScreen : qGuiApp->screenAt(m_anchorGlobal);
    const QPoint origin = scr ? scr->geometry().topLeft() : QPoint(0, 0);
    const QPoint localAnchor = m_anchorGlobal - origin;
    const QString barPos = m_config ? m_config->barPosition() : QStringLiteral("top");
    const int barThickness = (barPos == QLatin1String("left") || barPos == QLatin1String("right"))
                                 ? m_theme->barSizeVertical()
                                 : m_theme->barSizeHorizontal();

    int x = localAnchor.x() - w / 2;
    int y = barThickness + margin;
    if (barPos == QLatin1String("bottom")) {
        y = height() - barThickness - margin - h;
    } else if (barPos == QLatin1String("left")) {
        x = barThickness + margin;
        y = localAnchor.y() - h / 2;
    } else if (barPos == QLatin1String("right")) {
        x = width() - barThickness - margin - w;
        y = localAnchor.y() - h / 2;
    }

    x = std::clamp(x, margin, std::max(margin, width() - margin - w));
    y = std::clamp(y, margin, std::max(margin, height() - margin - h));
    return QRect(x, y, w, h);
}

void ShellPopupWindow::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), scrimColor());

    const QRect card = computeCardRect();
    paintCardContent(p, card);

    if (m_confirmOpen) {
        p.fillRect(rect(), ThemeManager::withAlpha(m_theme->background(), 0.55));
        const int cw = std::min(360, width() - 40);
        const int ch = 152;
        const QRect box((width() - cw) / 2, (height() - ch) / 2, cw, ch);
        UiPaint::drawCard(p, box, m_theme, QStringLiteral("popups"),
                          m_theme->popups().background,
                          m_theme->cornerRadius() > 0 ? m_theme->cornerRadius() : 10, 1);

        QFont tf(m_theme->resolvedFontFamily(), m_theme->fontSubtitle(), QFont::DemiBold);
        p.setFont(tf);
        p.setPen(m_theme->popups().text);
        p.drawText(QRect(box.left() + 18, box.top() + 16, cw - 36, 26), Qt::AlignLeft | Qt::AlignVCenter, m_confirmTitle);

        QFont bf(m_theme->resolvedFontFamily(), m_theme->fontBodySmall());
        p.setFont(bf);
        p.setPen(m_theme->muted());
        p.drawText(QRect(box.left() + 18, box.top() + 46, cw - 36, 46), Qt::TextWordWrap, m_confirmBody);

        const int bw = 96;
        const int bh = 32;
        m_confirmNoRect = QRect(box.right() - 18 - bw, box.bottom() - 16 - bh, bw, bh);
        m_confirmYesRect = QRect(m_confirmNoRect.left() - 10 - bw, box.bottom() - 16 - bh, bw, bh);

        UiPaint::drawPillButton(p, m_confirmYesRect, m_theme, QStringLiteral("Confirm"),
                                m_confirmIdx == 0, m_confirmYesRect.contains(m_hoverPos), true);
        UiPaint::drawPillButton(p, m_confirmNoRect, m_theme, QStringLiteral("Cancel"),
                                m_confirmIdx == 1, m_confirmNoRect.contains(m_hoverPos), false);
    }
}

void ShellPopupWindow::keyPressEvent(QKeyEvent *event) {
    if (m_confirmOpen) {
        if (event->key() == Qt::Key_Escape) {
            m_confirmOpen = false;
            update();
        } else if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right || event->key() == Qt::Key_Tab) {
            m_confirmIdx = 1 - m_confirmIdx;
            update();
        } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            const int chosen = m_confirmIdx;
            auto cb = std::move(m_onConfirm);
            m_confirmOpen = false;
            update();
            if (chosen == 0 && cb)
                cb();
        }
        return;
    }

    if (handleKey(event))
        return;

    if (event->key() == Qt::Key_Escape)
        closePopup();
}

void ShellPopupWindow::mousePressEvent(QMouseEvent *event) {
    if (m_confirmOpen) {
        const QPoint pt = event->pos();
        if (m_confirmYesRect.contains(pt)) {
            auto cb = std::move(m_onConfirm);
            m_confirmOpen = false;
            update();
            if (cb) cb();
        } else if (m_confirmNoRect.contains(pt)) {
            m_confirmOpen = false;
            update();
        }
        return;
    }

    const QRect card = computeCardRect();
    if (!card.contains(event->pos())) {
        closePopup();
        return;
    }
    handleCardMousePress(event, card);
}

void ShellPopupWindow::mouseMoveEvent(QMouseEvent *event) {
    m_hoverPos = event->pos();
    if (m_confirmOpen) {
        update();
        return;
    }
    handleCardMouseMove(event, computeCardRect());
}

void ShellPopupWindow::mouseReleaseEvent(QMouseEvent *event) {
    if (m_confirmOpen)
        return;
    handleCardMouseRelease(event, computeCardRect());
}

void ShellPopupWindow::wheelEvent(QWheelEvent *event) {
    if (m_confirmOpen)
        return;
    const QRect card = computeCardRect();
    if (card.contains(event->position().toPoint()))
        handleCardWheel(event, card);
}

void ShellPopupWindow::handleCardWheel(QWheelEvent *event, const QRect &) {
    if (m_maxScroll <= 0)
        return;
    const int delta = event->angleDelta().y() > 0 ? -36 : 36;
    m_scrollOffset = std::clamp(m_scrollOffset + delta, 0, m_maxScroll);
    update();
}

void ShellPopupWindow::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    raise();
    activateWindow();
    setFocus(Qt::OtherFocusReason);
}

} // namespace quattro
