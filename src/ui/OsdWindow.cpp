#include "OsdWindow.h"

#include "../wayland/LayerShellIntegration.h"
#include "BorderPainter.h"
#include <QFontMetrics>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <algorithm>
#include <cmath>

namespace quattro {

OsdWindow::OsdWindow(ThemeManager *theme, QWidget *parent)
    : QWidget(parent)
    , m_theme(theme)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setWindowTitle(QStringLiteral("quattro-osd"));

    m_hideTimer.setSingleShot(true);
    connect(&m_hideTimer, &QTimer::timeout, this, &OsdWindow::closeOsd);

    m_progressAnim.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_progressAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_animatedRatio = v.toDouble();
        update();
    });
    connect(m_theme, &ThemeManager::themeChanged, this, [this]() {
        if (m_opened) {
            updateGeometryForContent();
            update();
        }
    });

    resize(260, 64);
    winId();

    LayerSpec spec;
    spec.layer = LayerType::Overlay;
    spec.anchors = static_cast<uint32_t>(AnchorEdge::Bottom);
    spec.exclusiveZone = 0;
    spec.margins = QMargins(0, 0, 0, m_theme->space(67));
    spec.keyboard = KeyboardFocus::None;
    spec.scope = QStringLiteral("quattro-osd");
    spec.desiredSize = size();
    spec.clickThrough = true;
    LayerShellIntegration::configureWindow(windowHandle(), spec);
}

QString OsdWindow::iconFor(const QString &name, int percent) {
    const QString n = name.trimmed().toLower();
    if (n == QLatin1String("volume-muted") || n == QLatin1String("volume-mute")
        || n == QLatin1String("muted") || n == QLatin1String("mute"))
        return QStringLiteral("");
    if (n == QLatin1String("volume-low")) return QStringLiteral("");
    if (n == QLatin1String("volume-medium")) return QStringLiteral("");
    if (n == QLatin1String("volume-high") || n == QLatin1String("volume")) return QStringLiteral("");
    if (n == QLatin1String("microphone-muted") || n == QLatin1String("microphone-off")
        || n == QLatin1String("mic-muted") || n == QLatin1String("mic-off"))
        return QStringLiteral("󰍭");
    if (n == QLatin1String("microphone") || n == QLatin1String("mic")) return QStringLiteral("󰍬");
    if (n == QLatin1String("keyboard")) return QStringLiteral("󰌌");
    if (n == QLatin1String("brightness") || n == QLatin1String("display")) return QStringLiteral("󰍹");
    if (n == QLatin1String("touchpad")) return QStringLiteral("󰟸");
    if (n == QLatin1String("touch") || n == QLatin1String("touchscreen")) return QStringLiteral("󰝁");
    if (n == QLatin1String("reboot") || n == QLatin1String("restart")) return QStringLiteral("󰜉");
    if (n == QLatin1String("shutdown") || n == QLatin1String("power") || n == QLatin1String("poweroff"))
        return QStringLiteral("󰐥");
    if (n == QLatin1String("logout") || n == QLatin1String("sign-out") || n == QLatin1String("leave"))
        return QStringLiteral("󰍃");
    if (n == QLatin1String("media") || n == QLatin1String("player")
        || n == QLatin1String("media-source") || n == QLatin1String("player-source"))
        return QStringLiteral("󰝚");
    if (n == QLatin1String("media-play") || n == QLatin1String("player-play")) return QStringLiteral("󰐊");
    if (n == QLatin1String("media-pause") || n == QLatin1String("player-pause")) return QStringLiteral("󰏤");
    if (n == QLatin1String("media-next") || n == QLatin1String("player-next")) return QStringLiteral("󰒭");
    if (n == QLatin1String("media-previous") || n == QLatin1String("player-previous")) return QStringLiteral("󰒮");
    if (!name.trimmed().isEmpty())
        return name;
    if (percent <= 0) return QStringLiteral("");
    if (percent <= 33) return QStringLiteral("");
    if (percent <= 66) return QStringLiteral("");
    return QStringLiteral("");
}

void OsdWindow::openJson(const QString &payloadJson) {
    const QJsonDocument doc = QJsonDocument::fromJson(payloadJson.toUtf8());
    const QJsonObject p = doc.object();

    const QString icon = p.value(QStringLiteral("icon")).toString();
    const QString message = p.value(QStringLiteral("message")).toString();
    const bool hasVal = p.contains(QStringLiteral("value")) && !p.value(QStringLiteral("value")).isNull()
                        && p.value(QStringLiteral("value")).toVariant().toString() != QString();
    const int val = p.value(QStringLiteral("value")).toVariant().toInt();
    const int maxV = std::max(1, p.value(QStringLiteral("max")).toVariant().toInt() > 0
                                     ? p.value(QStringLiteral("max")).toVariant().toInt()
                                     : 100);
    const QString progressText = p.value(QStringLiteral("progressText")).toString();
    const int dur = p.contains(QStringLiteral("duration"))
                        ? std::max(0, p.value(QStringLiteral("duration")).toVariant().toInt())
                        : 1200;

    const bool hasProgress = hasVal && message.isEmpty();
    showOsd(icon, message, val, maxV, hasProgress, progressText, dur);
}

void OsdWindow::showOsd(const QString &iconName,
                        const QString &message,
                        int value,
                        int maxValue,
                        bool hasProgress,
                        const QString &progressText,
                        int durationMs) {
    const bool wasOpened = m_opened;
    m_iconKey = iconName.trimmed().toLower();
    m_maxValue = std::max(1, maxValue);
    m_hasProgress = hasProgress;
    m_value = hasProgress ? std::clamp(value, 0, m_maxValue) : 0;
    const int percent = hasProgress ? static_cast<int>(std::lround(m_value * 100.0 / m_maxValue)) : -1;
    m_message = !message.isEmpty()
                    ? message
                    : (hasProgress ? (!progressText.isEmpty() ? progressText : QString::number(percent) + QLatin1Char('%'))
                                   : QString());
    m_icon = iconFor(iconName, percent);
    m_durationMs = std::max(0, durationMs);

    const double targetRatio = m_hasProgress ? static_cast<double>(m_value) / m_maxValue : 0.0;
    m_progressAnim.stop();
    if (wasOpened && m_theme->duration(140) > 0) {
        m_progressAnim.setDuration(m_theme->duration(140));
        m_progressAnim.setStartValue(m_animatedRatio);
        m_progressAnim.setEndValue(targetRatio);
        m_progressAnim.start();
    } else {
        m_animatedRatio = targetRatio;
    }

    m_opened = true;
    updateGeometryForContent();
    show();
    update();

    if (m_durationMs > 0)
        m_hideTimer.start(m_durationMs);
    else
        m_hideTimer.stop();
}

void OsdWindow::closeOsd() {
    m_hideTimer.stop();
    m_opened = false;
    hide();
}

void OsdWindow::updateGeometryForContent() {
    const int pad = m_theme->space(16);
    const int gap = m_theme->space(16);
    const int messageGap = static_cast<int>(std::lround(gap * 2.0 / 3.0));
    const int barWidth = m_theme->space(142);
    const bool mediaOsd = m_iconKey.startsWith(QLatin1String("media")) || m_iconKey.startsWith(QLatin1String("player"));
    const int maxMsgW = mediaOsd ? m_theme->space(325) : m_theme->space(190);

    QFont iconFont(m_theme->fontFamily());
    iconFont.setPixelSize(m_theme->fontDisplayLarge());
    const QFontMetrics iconFm(iconFont);
    const int iconInkW = iconFm.tightBoundingRect(m_icon).width();
    const int widestInkW = iconFm.tightBoundingRect(QStringLiteral("")).width();
    const int iconW = m_hasProgress ? std::max(iconInkW, widestInkW) : std::max(m_theme->fontDisplayLarge(), iconInkW);

    QFont msgFont(m_theme->fontFamily());
    msgFont.setBold(true);
    msgFont.setPixelSize(m_theme->fontTitle());
    const QFontMetrics msgFm(msgFont);
    const int valueW = std::max(msgFm.horizontalAdvance(QStringLiteral("100%")),
                                msgFm.horizontalAdvance(m_message));
    const int msgW = std::min(msgFm.horizontalAdvance(m_message), maxMsgW);

    const int contentW = m_hasProgress
                             ? iconW + gap + barWidth + gap + valueW
                             : (m_message.isEmpty() ? iconW : iconW + messageGap + msgW);

    const BorderSpec border = m_theme->surfaceBorderSpec(
        QStringLiteral("popups"), QStringLiteral("border"),
        m_theme->popups().border, std::max(1, m_theme->space(2)));

    const int totalW = border.widths.left + pad + contentW + pad + border.widths.right;
    const int totalH = border.widths.top + pad + m_theme->fontDisplayLarge() + pad + border.widths.bottom;

    resize(totalW, totalH);
    LayerSpec spec;
    spec.layer = LayerType::Overlay;
    spec.anchors = static_cast<uint32_t>(AnchorEdge::Bottom);
    spec.exclusiveZone = 0;
    spec.margins = QMargins(0, 0, 0, m_theme->space(67));
    spec.keyboard = KeyboardFocus::None;
    spec.scope = QStringLiteral("quattro-osd");
    spec.desiredSize = QSize(totalW, totalH);
    spec.clickThrough = true;
    LayerShellIntegration::updateWindowSpec(windowHandle(), spec);
}

void OsdWindow::paintEvent(QPaintEvent *) {
    if (!m_opened)
        return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const BorderSpec border = m_theme->surfaceBorderSpec(
        QStringLiteral("popups"), QStringLiteral("border"),
        m_theme->popups().border, std::max(1, m_theme->space(2)));
    const QColor bg = ThemeManager::withAlpha(m_theme->background(), 0.97);
    BorderPainter::paintCard(p, rect(), m_theme->cornerRadius(), bg, border);

    const int pad = m_theme->space(16);
    const int gap = m_theme->space(16);
    const int messageGap = static_cast<int>(std::lround(gap * 2.0 / 3.0));
    const int barWidth = m_theme->space(142);

    const QRect contentRect = rect().adjusted(
        border.widths.left + pad, border.widths.top + pad,
        -(border.widths.right + pad), -(border.widths.bottom + pad));

    QFont iconFont(m_theme->fontFamily());
    iconFont.setPixelSize(m_theme->fontDisplayLarge());
    const QFontMetrics iconFm(iconFont);
    const int iconInkW = iconFm.tightBoundingRect(m_icon).width();
    const int widestInkW = iconFm.tightBoundingRect(QStringLiteral("")).width();
    const int iconW = m_hasProgress ? std::max(iconInkW, widestInkW) : std::max(m_theme->fontDisplayLarge(), iconInkW);

    p.setFont(iconFont);
    p.setPen(m_theme->popups().text);
    const QRect iconRect(contentRect.left(), contentRect.top(), iconW, contentRect.height());
    p.drawText(iconRect, Qt::AlignCenter, m_icon);

    QFont msgFont(m_theme->fontFamily());
    msgFont.setBold(true);
    msgFont.setPixelSize(m_theme->fontTitle());
    const QFontMetrics msgFm(msgFont);

    if (m_hasProgress) {
        const int barX = iconRect.right() + 1 + gap;
        const int barH = std::max(m_theme->space(6), m_theme->spacingToken(QStringLiteral("sm"), 4));
        const int barY = contentRect.center().y() - barH / 2;
        const QRect barRect(barX, barY, barWidth, barH);
        p.fillRect(barRect, ThemeManager::withAlpha(m_theme->popups().text, 0.45));

        const int fillW = static_cast<int>(std::lround(barWidth * std::clamp(m_animatedRatio, 0.0, 1.0)));
        if (fillW > 0)
            p.fillRect(QRect(barX, barY, fillW, barH), m_theme->accent());

        const int valX = barRect.right() + 1 + gap;
        const QRect valRect(valX, contentRect.top(), contentRect.right() - valX + 1, contentRect.height());
        p.setFont(msgFont);
        p.setPen(m_theme->popups().text);
        p.drawText(valRect, Qt::AlignRight | Qt::AlignVCenter, m_message);
    } else if (!m_message.isEmpty()) {
        const int msgX = iconRect.right() + 1 + messageGap;
        const QRect msgRect(msgX, contentRect.top(), contentRect.right() - msgX + 1, contentRect.height());
        p.setFont(msgFont);
        p.setPen(m_theme->popups().text);
        const QString elided = msgFm.elidedText(m_message, Qt::ElideRight, msgRect.width());
        p.drawText(msgRect, Qt::AlignLeft | Qt::AlignVCenter, elided);
    }
}

} // namespace quattro
