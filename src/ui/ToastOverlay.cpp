#include "ToastOverlay.h"

#include "../wayland/LayerShellIntegration.h"
#include "BorderPainter.h"
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>

namespace quattro {

ToastOverlay::ToastOverlay(ThemeManager *theme, ShellConfig *config, NotificationDaemon *daemon, QWidget *parent)
    : QWidget(parent)
    , m_theme(theme)
    , m_config(config)
    , m_daemon(daemon)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setWindowTitle(QStringLiteral("quattro-notifications"));
    setMouseTracking(true);

    resize(m_theme->space(360), 10);
    winId();
    LayerShellIntegration::configureWindow(windowHandle(), currentLayerSpec(size()));

    connect(m_daemon, &NotificationDaemon::notificationsChanged, this, &ToastOverlay::rebuildLayout);
    connect(m_theme, &ThemeManager::themeChanged, this, &ToastOverlay::rebuildLayout);
    connect(m_config, &ShellConfig::configChanged, this, &ToastOverlay::rebuildLayout);
    connect(m_config, &ShellConfig::barHiddenChanged, this, &ToastOverlay::rebuildLayout);

    rebuildLayout();
}

LayerSpec ToastOverlay::currentLayerSpec(const QSize &sz) const {
    const int gap = std::max(m_theme->space(8), m_theme->gapsOut());
    int topMargin = gap;
    if (!m_config->barHidden() && m_config->barPosition() == QLatin1String("top"))
        topMargin += m_theme->barSizeHorizontal();

    LayerSpec spec;
    spec.layer = LayerType::Overlay;
    spec.anchors = static_cast<uint32_t>(AnchorEdge::Top) | static_cast<uint32_t>(AnchorEdge::Right);
    spec.exclusiveZone = 0;
    spec.margins = QMargins(0, topMargin, gap, 0);
    spec.keyboard = KeyboardFocus::None;
    spec.scope = QStringLiteral("quattro-notifications");
    spec.desiredSize = sz;
    return spec;
}

int ToastOverlay::cardHeightFor(const NotificationItem &it, int cardWidth) const {
    const BorderSpec border = m_theme->surfaceBorderSpec(
        QStringLiteral("notifications"), QStringLiteral("border"),
        m_theme->notifications().border, std::max(1, m_theme->space(2)));
    const int pad = m_theme->space(12);
    const int innerW = std::max(60, cardWidth - border.widths.left - border.widths.right - pad * 2);

    QFont appFont(m_theme->fontFamily());
    appFont.setPixelSize(m_theme->fontCaption());
    const QFontMetrics appFm(appFont);

    QFont sumFont(m_theme->fontFamily());
    sumFont.setBold(true);
    sumFont.setPixelSize(m_theme->fontBody());
    const QFontMetrics sumFm(sumFont);

    QFont bodyFont(m_theme->fontFamily());
    bodyFont.setPixelSize(m_theme->fontBodySmall());
    const QFontMetrics bodyFm(bodyFont);

    int h = border.widths.top + pad + appFm.height() + m_theme->space(3);
    if (!it.summary.isEmpty())
        h += sumFm.boundingRect(QRect(0, 0, innerW, 200), Qt::TextWordWrap, it.summary).height() + m_theme->space(3);
    if (!it.body.isEmpty())
        h += std::min(bodyFm.height() * 4,
                      bodyFm.boundingRect(QRect(0, 0, innerW, 400), Qt::TextWordWrap, it.body).height()) + m_theme->space(4);
    if (!it.actions.isEmpty())
        h += m_theme->spacingToken(QStringLiteral("control-height"), 26) + m_theme->space(4);
    if (it.timeoutMs > 0)
        h += m_theme->space(3) + m_theme->space(4);
    h += pad + border.widths.bottom;
    return h;
}

void ToastOverlay::rebuildLayout() {
    const auto &items = m_daemon->active();
    m_cardRects.clear();
    m_actionHits.clear();

    if (items.isEmpty()) {
        hide();
        return;
    }

    const int cardW = m_theme->space(360);
    const int gap = m_theme->space(8);
    int y = 0;

    for (const NotificationItem &it : items) {
        const int h = cardHeightFor(it, cardW);
        m_cardRects.push_back(QRect(0, y, cardW, h));
        y += h + gap;
    }

    const int totalH = std::max(1, y - gap);
    resize(cardW, totalH);
    LayerShellIntegration::updateWindowSpec(windowHandle(), currentLayerSpec(QSize(cardW, totalH)));
    if (!isVisible())
        show();
    update();
}

void ToastOverlay::enterEvent(QEnterEvent *) {
    m_daemon->setPaused(true);
}

void ToastOverlay::leaveEvent(QEvent *) {
    m_daemon->setPaused(false);
}

void ToastOverlay::mousePressEvent(QMouseEvent *event) {
    const QPoint pos = event->pos();
    for (const HitRegion &hit : m_actionHits) {
        if (hit.rect.contains(pos)) {
            m_daemon->invokeAction(hit.notifId, hit.actionKey);
            return;
        }
    }
    const auto &items = m_daemon->active();
    for (int i = 0; i < m_cardRects.size() && i < items.size(); ++i) {
        if (m_cardRects[i].contains(pos)) {
            m_daemon->dismissId(items[i].id, 2);
            return;
        }
    }
}

void ToastOverlay::paintEvent(QPaintEvent *) {
    const auto &items = m_daemon->active();
    if (items.isEmpty())
        return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    m_actionHits.clear();

    const BorderSpec baseBorder = m_theme->surfaceBorderSpec(
        QStringLiteral("notifications"), QStringLiteral("border"),
        m_theme->notifications().border, std::max(1, m_theme->space(2)));

    QFont appFont(m_theme->fontFamily());
    appFont.setPixelSize(m_theme->fontCaption());
    const QFontMetrics appFm(appFont);

    QFont sumFont(m_theme->fontFamily());
    sumFont.setBold(true);
    sumFont.setPixelSize(m_theme->fontBody());
    const QFontMetrics sumFm(sumFont);

    QFont bodyFont(m_theme->fontFamily());
    bodyFont.setPixelSize(m_theme->fontBodySmall());
    const QFontMetrics bodyFm(bodyFont);

    const int pad = m_theme->space(12);

    for (int i = 0; i < items.size() && i < m_cardRects.size(); ++i) {
        const NotificationItem &it = items[i];
        const QRect cardRect = m_cardRects[i];

        BorderSpec border = baseBorder;
        if (it.urgency == 2) {
            border.color = m_theme->urgent();
            border.gradient.enabled = false;
        }

        BorderPainter::paintCard(p, cardRect, m_theme->cornerRadius(),
                                 m_theme->notifications().background, border);

        const QRect inner = cardRect.adjusted(
            border.widths.left + pad, border.widths.top + pad,
            -(border.widths.right + pad), -(border.widths.bottom + pad));
        int y = inner.top();

        // App name
        p.setFont(appFont);
        p.setPen(ThemeManager::withAlpha(m_theme->notifications().text, 0.7));
        p.drawText(QRect(inner.left(), y, inner.width(), appFm.height()),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   appFm.elidedText(it.appName, Qt::ElideRight, inner.width()));
        y += appFm.height() + m_theme->space(3);

        // Summary
        if (!it.summary.isEmpty()) {
            p.setFont(sumFont);
            p.setPen(m_theme->notifications().text);
            const QRect sr = sumFm.boundingRect(QRect(inner.left(), y, inner.width(), 200),
                                                Qt::TextWordWrap, it.summary);
            p.drawText(sr, Qt::TextWordWrap, it.summary);
            y += sr.height() + m_theme->space(3);
        }

        // Body
        if (!it.body.isEmpty()) {
            p.setFont(bodyFont);
            p.setPen(ThemeManager::withAlpha(m_theme->notifications().text, 0.85));
            const int maxBodyH = bodyFm.height() * 4;
            const QRect br = bodyFm.boundingRect(QRect(inner.left(), y, inner.width(), maxBodyH),
                                                 Qt::TextWordWrap, it.body);
            p.drawText(QRect(inner.left(), y, inner.width(), std::min(maxBodyH, br.height())),
                       Qt::TextWordWrap, it.body);
            y += std::min(maxBodyH, br.height()) + m_theme->space(4);
        }

        // Action buttons
        if (!it.actions.isEmpty()) {
            const int btnH = m_theme->spacingToken(QStringLiteral("control-height"), 26);
            int bx = inner.left();
            p.setFont(bodyFont);
            for (const NotificationAction &act : it.actions) {
                const int bw = std::min(inner.width(), bodyFm.horizontalAdvance(act.label) + m_theme->space(16));
                if (bx + bw > inner.right())
                    break;
                const QRect bRect(bx, y, bw, btnH);
                BorderSpec btnBorder;
                btnBorder.color = m_theme->normalBorderColor();
                btnBorder.widths = {1, 1, 1, 1};
                BorderPainter::paintCard(p, bRect, std::min(4, m_theme->cornerRadius()),
                                         m_theme->normalFill(), btnBorder);
                p.setPen(m_theme->notifications().text);
                p.drawText(bRect, Qt::AlignCenter, act.label);
                m_actionHits.push_back({bRect, it.id, act.key});
                bx += bw + m_theme->space(6);
            }
            y += btnH + m_theme->space(4);
        }

        // Countdown bar
        if (it.timeoutMs > 0) {
            const double ratio = std::clamp(static_cast<double>(it.remainingMs) / it.timeoutMs, 0.0, 1.0);
            const int barH = m_theme->space(2);
            const QRect track(inner.left(), inner.bottom() - barH + 1, inner.width(), barH);
            p.fillRect(track, ThemeManager::withAlpha(m_theme->notifications().text, 0.15));
            const int fillW = static_cast<int>(track.width() * ratio);
            if (fillW > 0)
                p.fillRect(QRect(track.left(), track.top(), fillW, barH), m_theme->notifications().countdown);
        }
    }
}

} // namespace quattro
