#include "PolkitWindow.h"
#include "BorderPainter.h"
#include "CommonUi.h"
#include "../wayland/LayerShellIntegration.h"

#include <QApplication>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QSequentialAnimationGroup>

namespace quattro {

PolkitWindow::PolkitWindow(PolkitService *service, ThemeManager *theme, QWidget *parent)
    : QWidget(parent)
    , m_service(service)
    , m_theme(theme)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    winId();

    LayerSpec spec;
    spec.layer = LayerType::Overlay;
    spec.anchors = static_cast<uint32_t>(AnchorEdge::Top)
                   | static_cast<uint32_t>(AnchorEdge::Bottom)
                   | static_cast<uint32_t>(AnchorEdge::Left)
                   | static_cast<uint32_t>(AnchorEdge::Right);
    spec.exclusiveZone = -1;
    spec.keyboard = KeyboardFocus::Exclusive;
    spec.scope = QStringLiteral("quattro-polkit");
    if (QScreen *scr = QGuiApplication::primaryScreen())
        spec.desiredSize = scr->size();
    LayerShellIntegration::configureWindow(windowHandle(), spec);

    auto *group = new QSequentialAnimationGroup(this);
    const int shifts[] = {-12, 12, -8, 8, -4, 0};
    for (int s : shifts) {
        auto *a = new QPropertyAnimation(this, "shakeOffset", group);
        a->setDuration(38);
        a->setEndValue(qreal(s));
        group->addAnimation(a);
    }
    connect(group, &QSequentialAnimationGroup::finished, this, [this]() {
        m_shakeOffset = 0.0;
        update();
    });
    m_shakeAnim = reinterpret_cast<QPropertyAnimation *>(group);

    connect(m_service, &PolkitService::requestOpened, this, &PolkitWindow::onRequestOpened);
    connect(m_service, &PolkitService::promptUpdated, this, [this](const QString &p, bool echo) {
        m_info.promptLabel = p;
        m_info.echoOn = echo;
        update();
    });
    connect(m_service, &PolkitService::statusMessage, this, [this](const QString &msg, bool err) {
        if (err)
            m_errorText = msg;
        else
            m_statusText = msg;
        update();
    });
    connect(m_service, &PolkitService::authenticationFailed, this, &PolkitWindow::onAuthenticationFailed);
    connect(m_service, &PolkitService::requestClosed, this, &PolkitWindow::onRequestClosed);
    connect(m_theme, &ThemeManager::themeChanged, this, QOverload<>::of(&QWidget::update));
}

void PolkitWindow::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    LayerSpec spec;
    spec.layer = LayerType::Overlay;
    spec.anchors = static_cast<uint32_t>(AnchorEdge::Top)
                   | static_cast<uint32_t>(AnchorEdge::Bottom)
                   | static_cast<uint32_t>(AnchorEdge::Left)
                   | static_cast<uint32_t>(AnchorEdge::Right);
    spec.exclusiveZone = -1;
    spec.keyboard = KeyboardFocus::Exclusive;
    spec.scope = QStringLiteral("quattro-polkit");
    spec.desiredSize = size();
    LayerShellIntegration::updateWindowSpec(windowHandle(), spec);
}

void PolkitWindow::onRequestOpened(const PolkitRequestInfo &info) {
    m_info = info;
    m_input.clear();
    m_showPassword = info.echoOn;
    m_errorText.clear();
    m_statusText.clear();

    QScreen *scr = QGuiApplication::screenAt(QCursor::pos());
    if (!scr)
        scr = QGuiApplication::primaryScreen();
    if (scr)
        setGeometry(scr->geometry());

    show();
    raise();
    activateWindow();
    setFocus(Qt::ActiveWindowFocusReason);
    update();
}

void PolkitWindow::onAuthenticationFailed(const QString &msg) {
    m_errorText = msg;
    m_input.clear();
    triggerShake();
}

void PolkitWindow::onRequestClosed(bool /*gained*/) {
    m_input.clear();
    m_errorText.clear();
    m_statusText.clear();
    hide();
}

void PolkitWindow::triggerShake() {
    auto *group = reinterpret_cast<QSequentialAnimationGroup *>(m_shakeAnim);
    if (group) {
        group->stop();
        m_shakeOffset = 0.0;
        group->start();
    }
}

void PolkitWindow::submitCurrent() {
    if (!m_info.active || m_service->isAuthenticating())
        return;
    m_errorText.clear();
    m_service->submitResponse(m_input);
    update();
}

void PolkitWindow::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const SurfaceColors &sc = m_theme->polkit();
    QColor scrim = sc.scrim;
    if (!scrim.isValid() || scrim.alpha() == 0)
        scrim = QColor(0, 0, 0, 145);
    p.fillRect(rect(), scrim);

    const int cardW = 460;
    const int cardH = 310 + (m_errorText.isEmpty() ? 0 : 22) + (m_info.fingerprintAvailable ? 24 : 0);
    m_cardRect = QRect((width() - cardW) / 2 + qRound(m_shakeOffset),
                       (height() - cardH) / 2,
                       cardW,
                       cardH);

    const int radius = m_theme->cornerRadius() > 0 ? m_theme->cornerRadius() : 14;
    UiPaint::drawCard(p, m_cardRect, m_theme, QStringLiteral("polkit"), sc.background, radius, 1);

    int y = m_cardRect.top() + 24;
    const int left = m_cardRect.left() + 24;
    const int innerW = m_cardRect.width() - 48;

    // Header icon + title + user badge
    QColor accent = sc.accent.isValid() ? sc.accent : m_theme->accent();
    QColor muted = m_theme->muted();

    QRect hero(left, y, innerW, 56);
    UiPaint::drawHero(p, hero, m_theme,
                      QStringLiteral("󰒃"),
                      QStringLiteral("Authentication Required"),
                      m_info.identityName.isEmpty() ? QStringLiteral("Administrator") : m_info.identityName,
                      sc.text, muted, accent);
    y += 66;

    // Action message
    QFont bodyFont(m_theme->resolvedFontFamily(), 11);
    p.setFont(bodyFont);
    p.setPen(sc.text);
    const QRect msgRect(left, y, innerW, 44);
    p.drawText(msgRect, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
               m_info.message.isEmpty() ? QStringLiteral("Privileged action requested.") : m_info.message);
    y += 48;

    // Justification pill (action_id)
    if (!m_info.actionId.isEmpty()) {
        QFont smallFont(m_theme->resolvedFontFamily(), 9);
        p.setFont(smallFont);
        QColor pillBg = sc.text;
        pillBg.setAlpha(18);
        const QRect pill(left, y, innerW, 24);
        p.setPen(Qt::NoPen);
        p.setBrush(pillBg);
        p.drawRoundedRect(pill, 6, 6);
        p.setPen(muted);
        p.drawText(pill.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft,
                   p.fontMetrics().elidedText(m_info.actionId, Qt::ElideMiddle, pill.width() - 16));
        y += 32;
    }

    // Input box
    const QRect inputRect(left, y, innerW, 42);
    QColor inputBg = sc.text;
    inputBg.setAlpha(20);
    QColor inputBorder = !m_errorText.isEmpty()
                             ? (sc.borderError.isValid() ? sc.borderError : m_theme->urgent())
                             : (sc.borderActive.isValid() ? sc.borderActive : accent);
    p.setBrush(inputBg);
    p.setPen(QPen(inputBorder, 1.5));
    p.drawRoundedRect(inputRect, 8, 8);

    m_eyeRect = QRect(inputRect.right() - 36, inputRect.top() + 5, 32, 32);
    const QRect textRect(inputRect.left() + 12, inputRect.top(), inputRect.width() - 52, inputRect.height());
    p.setFont(QFont(m_theme->resolvedFontFamily(), 11));
    if (m_input.isEmpty()) {
        p.setPen(sc.placeholder.isValid() ? sc.placeholder : muted);
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, m_info.promptLabel);
    } else {
        p.setPen(sc.text);
        const QString shown = m_showPassword ? m_input : QString(m_input.size(), QChar(0x2022));
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, shown);
    }

    // Eye toggle
    p.setPen(m_eyeRect.contains(m_hoverPos) ? accent : muted);
    p.drawText(m_eyeRect, Qt::AlignCenter, m_showPassword ? QStringLiteral("󰈈") : QStringLiteral("󰈉"));
    y += 50;

    if (!m_errorText.isEmpty()) {
        p.setFont(QFont(m_theme->resolvedFontFamily(), 10));
        p.setPen(sc.textError.isValid() ? sc.textError : m_theme->urgent());
        p.drawText(QRect(left, y, innerW, 20), Qt::AlignVCenter | Qt::AlignLeft, m_errorText);
        y += 24;
    } else if (!m_statusText.isEmpty()) {
        p.setFont(QFont(m_theme->resolvedFontFamily(), 10));
        p.setPen(muted);
        p.drawText(QRect(left, y, innerW, 20), Qt::AlignVCenter | Qt::AlignLeft, m_statusText);
        y += 24;
    }

    if (m_info.fingerprintAvailable) {
        p.setFont(QFont(m_theme->resolvedFontFamily(), 10));
        p.setPen(accent);
        p.drawText(QRect(left, y, innerW, 22), Qt::AlignVCenter | Qt::AlignLeft,
                   QStringLiteral("󰈷 Touch fingerprint sensor or enter password"));
        y += 26;
    }

    // Footer buttons
    const int btnW = 116;
    const int btnH = 34;
    const int btnY = m_cardRect.bottom() - 24 - btnH;
    m_authRect = QRect(m_cardRect.right() - 24 - btnW, btnY, btnW, btnH);
    m_cancelRect = QRect(m_authRect.left() - 10 - btnW, btnY, btnW, btnH);

    UiPaint::drawPillButton(p, m_cancelRect, m_theme, QStringLiteral("Cancel"),
                            false, m_cancelRect.contains(m_hoverPos));
    const QString authLabel = m_service->isAuthenticating() ? QStringLiteral("Verifying...")
                                                            : QStringLiteral("Authenticate");
    UiPaint::drawPillButton(p, m_authRect, m_theme, authLabel,
                            true, m_authRect.contains(m_hoverPos));
}

void PolkitWindow::keyPressEvent(QKeyEvent *event) {
    const int k = event->key();
    if (k == Qt::Key_Escape) {
        m_service->cancel();
        return;
    }
    if (k == Qt::Key_Return || k == Qt::Key_Enter) {
        submitCurrent();
        return;
    }
    if (k == Qt::Key_Backspace) {
        if (!m_input.isEmpty()) {
            m_input.chop(1);
            update();
        }
        return;
    }
    if (event->matches(QKeySequence::Paste) ||
        ((event->modifiers() & Qt::ControlModifier) && k == Qt::Key_U)) {
        if (k == Qt::Key_U)
            m_input.clear();
        update();
        return;
    }
    const QString txt = event->text();
    if (!txt.isEmpty() && txt.at(0).isPrint()) {
        m_input += txt;
        m_errorText.clear();
        update();
    }
}

void PolkitWindow::mousePressEvent(QMouseEvent *event) {
    const QPoint pos = event->pos();
    if (m_eyeRect.contains(pos)) {
        m_showPassword = !m_showPassword;
        update();
        return;
    }
    if (m_cancelRect.contains(pos)) {
        m_service->cancel();
        return;
    }
    if (m_authRect.contains(pos)) {
        submitCurrent();
        return;
    }
}

void PolkitWindow::mouseMoveEvent(QMouseEvent *event) {
    m_hoverPos = event->pos();
    update();
}

} // namespace quattro
