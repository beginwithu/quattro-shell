#include "LockWindow.h"

#include "../wayland/LayerShellIntegration.h"
#include "../wayland/SessionLockManager.h"
#include "BackgroundWindow.h"
#include "BorderPainter.h"
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QPainter>
#include <QProcess>
#include <algorithm>
#include <cmath>

namespace quattro {

LockController::LockController(ThemeManager *theme, BackgroundManager *bg, QObject *parent)
    : QObject(parent)
    , m_theme(theme)
    , m_bg(bg)
{
    m_shakeAnim.setDuration(140);
    m_shakeAnim.setKeyValueAt(0.0, 0.0);
    m_shakeAnim.setKeyValueAt(0.25, -8.0);
    m_shakeAnim.setKeyValueAt(0.65, 8.0);
    m_shakeAnim.setKeyValueAt(1.0, 0.0);
    connect(&m_shakeAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_shakeOffset = v.toDouble();
        emit viewStateChanged();
    });

    connect(&m_pam, &PamAuth::authenticationFinished, this, [this](bool ok, const QString &msg) {
        if (ok) {
            m_fprintd.stopLockSession();
            m_password.clear();
            m_failureMessage.clear();
            if (m_previewVisible) {
                hidePreview();
                return;
            }
            SessionLockManager::instance().unlock();
            m_locked = false;
            destroyWindows();
            QProcess::startDetached(QStringLiteral("quattro-system-wake"), {});
            emit lockStateChanged(false);
        } else {
            m_password.clear();
            m_failureMessage = msg.isEmpty() ? QStringLiteral("Incorrect password") : msg;
            triggerShake();
            emit viewStateChanged();
        }
    });

    connect(&m_fprintd, &FprintdService::fingerprintUnlocked, this, [this]() {
        m_fprintd.stopLockSession();
        m_password.clear();
        m_failureMessage.clear();
        if (m_previewVisible) {
            hidePreview();
            return;
        }
        SessionLockManager::instance().unlock();
        m_locked = false;
        destroyWindows();
        QProcess::startDetached(QStringLiteral("quattro-system-wake"), {});
        emit lockStateChanged(false);
    });
    connect(&m_fprintd, &FprintdService::fingerprintError, this, [this](const QString &msg) {
        if (!msg.isEmpty())
            m_failureMessage = msg;
        emit viewStateChanged();
    });
    connect(&m_fprintd, &FprintdService::stateChanged, this, &LockController::viewStateChanged);

    connect(&SessionLockManager::instance(), &SessionLockManager::finished, this, [this]() {
        m_fprintd.stopLockSession();
        m_locked = false;
        destroyWindows();
        emit lockStateChanged(false);
    });

    connect(qGuiApp, &QGuiApplication::screenAdded, this, [this](QScreen *) {
        if (m_locked || m_previewVisible)
            syncLockScreens();
    });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this](QScreen *) {
        if (m_locked || m_previewVisible)
            syncLockScreens();
    });
}

LockController::~LockController() {
    destroyWindows();
}

QImage LockController::blurredWallpaperFor(const QSize &pixelSize) {
    const QString bgPath = m_bg ? m_bg->currentBackground() : QString();
    if (bgPath.isEmpty())
        return {};
    if (bgPath == m_cachedBgPath && !m_cachedBlur.isNull())
        return m_cachedBlur;

    if (BackgroundManager::isVideoPath(bgPath)) {
        const QString cached = FprintdService::cachedPosterPathFor(bgPath);
        if (!cached.isEmpty() && QFileInfo::exists(cached)) {
            const QImage raw = BackgroundManager::loadScaledCover(cached, pixelSize);
            if (!raw.isNull()) {
                m_cachedBgPath = bgPath;
                m_cachedBlur = BorderPainter::fastBlur(raw, 48);
                return m_cachedBlur;
            }
        }
        if (!m_posterRequested) {
            m_posterRequested = true;
            FprintdService::generateLockPosterAsync(bgPath, this, [this, bgPath, pixelSize](const QString &poster) {
                m_posterRequested = false;
                if (!poster.isEmpty() && QFileInfo::exists(poster)) {
                    const QImage raw = BackgroundManager::loadScaledCover(poster, pixelSize);
                    if (!raw.isNull()) {
                        m_cachedBgPath = bgPath;
                        m_cachedBlur = BorderPainter::fastBlur(raw, 48);
                        emit viewStateChanged();
                    }
                }
            });
        }
        return {};
    }

    const QImage raw = BackgroundManager::loadScaledCover(bgPath, pixelSize);
    if (raw.isNull())
        return {};
    m_cachedBgPath = bgPath;
    m_cachedBlur = BorderPainter::fastBlur(raw, 48);
    return m_cachedBlur;
}

QString LockController::requestLock() {
    if (!PamAuth::isPamConfigured())
        return QStringLiteral("missing-pam");
    if (m_locked)
        return QStringLiteral("ok");

    hidePreview();
    if (!SessionLockManager::instance().beginLock())
        return QStringLiteral("failed");

    m_locked = true;
    m_password.clear();
    m_failureMessage.clear();
    syncLockScreens();
    m_fprintd.startLockSession();
    emit lockStateChanged(true);
    return QStringLiteral("ok");
}

void LockController::showPreview() {
    if (m_locked || m_previewVisible)
        return;
    m_previewVisible = true;
    m_password.clear();
    m_failureMessage.clear();
    syncLockScreens();
}

void LockController::hidePreview() {
    if (!m_previewVisible)
        return;
    m_previewVisible = false;
    destroyWindows();
}

void LockController::syncLockScreens() {
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (int i = m_windows.size() - 1; i >= 0; --i) {
        if (!screens.contains(m_windows[i]->targetScreen())) {
            m_windows[i]->close();
            m_windows[i]->deleteLater();
            m_windows.removeAt(i);
        }
    }
    for (QScreen *s : screens) {
        bool exists = false;
        for (LockSurfaceWidget *w : m_windows) {
            if (w->targetScreen() == s) { exists = true; break; }
        }
        if (!exists) {
            auto *w = new LockSurfaceWidget(s, m_theme, this, m_previewVisible);
            w->show();
            w->activateWindow();
            w->setFocus();
            m_windows.push_back(w);
        }
    }
}

void LockController::destroyWindows() {
    for (LockSurfaceWidget *w : m_windows) {
        w->close();
        w->deleteLater();
    }
    m_windows.clear();
}

QString LockController::statusJson() const {
    QJsonObject obj;
    obj[QStringLiteral("locked")] = m_locked;
    obj[QStringLiteral("sessionLocked")] = SessionLockManager::instance().isLocked();
    obj[QStringLiteral("realScreens")] = QGuiApplication::screens().size();
    obj[QStringLiteral("passwordPam")] = PamAuth::isPamConfigured();
    obj[QStringLiteral("authenticating")] = m_pam.isAuthenticating();
    obj[QStringLiteral("previewVisible")] = m_previewVisible;
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void LockController::appendPasswordText(const QString &text) {
    if (m_pam.isAuthenticating() || text.isEmpty())
        return;
    m_failureMessage.clear();
    m_password.append(text);
    emit viewStateChanged();
}

void LockController::backspacePassword() {
    if (m_pam.isAuthenticating() || m_password.isEmpty())
        return;
    m_failureMessage.clear();
    m_password.chop(1);
    emit viewStateChanged();
}

void LockController::clearPassword() {
    if (m_pam.isAuthenticating())
        return;
    m_password.clear();
    m_failureMessage.clear();
    emit viewStateChanged();
}

void LockController::submitPassword() {
    if (m_pam.isAuthenticating() || m_password.isEmpty())
        return;
    m_failureMessage.clear();
    emit viewStateChanged();
    m_pam.authenticateAsync(m_password);
}

void LockController::triggerShake() {
    if (m_theme->duration(140) <= 0)
        return;
    m_shakeAnim.stop();
    m_shakeAnim.start();
}

LockSurfaceWidget::LockSurfaceWidget(QScreen *screen, ThemeManager *theme, LockController *ctrl, bool previewMode)
    : QWidget(nullptr)
    , m_screen(screen)
    , m_theme(theme)
    , m_ctrl(ctrl)
    , m_previewMode(previewMode)
{
    setAttribute(Qt::WA_TranslucentBackground, false);
    setFocusPolicy(Qt::StrongFocus);
    setWindowTitle(previewMode ? QStringLiteral("quattro-lock-preview") : QStringLiteral("quattro-lock"));

    const QSize sz = screen ? screen->size() : QSize(1920, 1080);
    resize(sz);
    winId();
    if (screen)
        windowHandle()->setScreen(screen);

    if (previewMode) {
        LayerSpec spec;
        spec.layer = LayerType::Overlay;
        spec.anchors = static_cast<uint32_t>(AnchorEdge::Top)
                       | static_cast<uint32_t>(AnchorEdge::Bottom)
                       | static_cast<uint32_t>(AnchorEdge::Left)
                       | static_cast<uint32_t>(AnchorEdge::Right);
        spec.exclusiveZone = -1;
        spec.keyboard = KeyboardFocus::Exclusive;
        spec.scope = QStringLiteral("quattro-lock-preview");
        spec.desiredSize = sz;
        LayerShellIntegration::configureWindow(windowHandle(), spec);
    } else {
        SessionLockManager::instance().attachWindow(windowHandle());
    }

    connect(m_ctrl, &LockController::viewStateChanged, this, [this]() { update(); });
    connect(m_theme, &ThemeManager::themeChanged, this, [this]() { update(); });
}

void LockSurfaceWidget::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    setFocus(Qt::ActiveWindowFocusReason);
}

void LockSurfaceWidget::mousePressEvent(QMouseEvent *) {
    setFocus(Qt::MouseFocusReason);
}

void LockSurfaceWidget::keyPressEvent(QKeyEvent *event) {
    if (event->isAutoRepeat() && event->key() != Qt::Key_Backspace && event->key() != Qt::Key_Delete)
        return;

    if (m_previewMode && event->key() == Qt::Key_Escape) {
        m_ctrl->hidePreview();
        return;
    }

    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        m_ctrl->submitPassword();
        return;
    case Qt::Key_Backspace:
        m_ctrl->backspacePassword();
        return;
    case Qt::Key_Escape:
        m_ctrl->clearPassword();
        return;
    default:
        break;
    }

    if ((event->modifiers() & Qt::ControlModifier) && (event->key() == Qt::Key_U || event->key() == Qt::Key_Backspace)) {
        m_ctrl->clearPassword();
        return;
    }

    const QString t = event->text();
    if (!t.isEmpty() && t[0].isPrint()) {
        m_ctrl->nudgeFingerprint();
        m_ctrl->appendPasswordText(t);
    }
}

void LockSurfaceWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), m_theme->background());

    const qreal dpr = m_screen ? m_screen->devicePixelRatio() : 1.0;
    const QImage blur = m_ctrl->blurredWallpaperFor(size() * dpr);
    if (!blur.isNull()) {
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const QSizeF targetSz = size();
        const QSizeF imgSz = blur.size();
        const double scale = std::max(targetSz.width() / imgSz.width(),
                                      targetSz.height() / imgSz.height());
        const double drawW = imgSz.width() * scale;
        const double drawH = imgSz.height() * scale;
        p.drawImage(QRectF((targetSz.width() - drawW) * 0.5,
                           (targetSz.height() - drawH) * 0.5,
                           drawW, drawH),
                    blur);
        // Subtle contrast dim like MultiEffect contrast: -0.08
        p.fillRect(rect(), QColor(0, 0, 0, 56));
    }

    const int fieldW = 381;
    const int fieldH = 67;
    const int outline = 3;
    const QRectF cardRect(
        (width() - fieldW) * 0.5 + m_ctrl->shakeOffset(),
        (height() - fieldH) * 0.5,
        fieldW,
        fieldH);

    const bool hasErr = !m_ctrl->failureMessage().isEmpty();
    BorderSpec border = m_theme->surfaceBorderSpec(
        QStringLiteral("lock"),
        hasErr ? QStringLiteral("border-error") : QStringLiteral("border-active"),
        hasErr ? m_theme->lock().borderError : m_theme->lock().borderActive,
        outline,
        QStringLiteral("border-alpha"));

    BorderPainter::paintCard(p, cardRect, m_theme->cornerRadius(), m_theme->lock().background, border);

    const QRectF inner = cardRect.adjusted(outline + 18, outline, -(outline + 18), -outline);
    const QString &pw = m_ctrl->passwordBuffer();

    if (m_ctrl->isAuthenticating()) {
        QFont f(m_theme->fontFamily());
        f.setPixelSize(static_cast<int>(std::lround(m_theme->fontHeading() * 1.125)));
        p.setFont(f);
        p.setPen(m_theme->lock().placeholder);
        p.drawText(inner, Qt::AlignCenter, QStringLiteral("Unlocking..."));
    } else if (pw.isEmpty()) {
        QFont f(m_theme->fontFamily());
        f.setPixelSize(static_cast<int>(std::lround(m_theme->fontHeading() * 1.125)));
        p.setFont(f);
        p.setPen(hasErr ? m_theme->lock().textError : m_theme->lock().placeholder);
        p.drawText(inner, Qt::AlignCenter, hasErr ? m_ctrl->failureMessage() : QStringLiteral("Enter password..."));
    } else {
        QFont f(m_theme->fontFamily());
        const int dotSize = static_cast<int>(std::lround(m_theme->fontHeading() * 1.33));
        f.setPixelSize(dotSize);
        f.setLetterSpacing(QFont::AbsoluteSpacing, std::lround(m_theme->fontHeading() * 0.19));
        const QString dots(pw.size(), QChar(0x25CF));
        const QFontMetrics fm(f);
        const int adv = fm.horizontalAdvance(dots);
        if (adv > inner.width() - 4 && adv > 0) {
            const double scale = (inner.width() - 4) / adv;
            f.setPixelSize(std::max(6, static_cast<int>(std::floor(dotSize * scale))));
        }
        p.setFont(f);
        p.setPen(m_theme->lock().text);
        p.drawText(inner, Qt::AlignCenter, dots);
    }

    if (m_ctrl->fingerprintAvailable() || !m_ctrl->fingerprintStatus().isEmpty()) {
        QFont fpFont(m_theme->fontFamily(), m_theme->fontBodySmall());
        p.setFont(fpFont);
        p.setPen(m_theme->lock().placeholder);
        const QString fpMsg = m_ctrl->fingerprintStatus().isEmpty()
                                  ? QStringLiteral("󰈷 Touch sensor to unlock")
                                  : QStringLiteral("󰈷 ") + m_ctrl->fingerprintStatus();
        const QRectF fpRect(cardRect.left(), cardRect.bottom() + 14, cardRect.width(), 24);
        p.drawText(fpRect, Qt::AlignCenter, fpMsg);
    }
}

} // namespace quattro
