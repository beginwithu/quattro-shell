#pragma once

#include "../core/ThemeManager.h"
#include "../services/FprintdService.h"
#include "../services/PamAuth.h"
#include <QImage>
#include <QObject>
#include <QScreen>
#include <QTimer>
#include <QVariantAnimation>
#include <QVector>
#include <QWidget>

namespace quattro {

class BackgroundManager;
class LockSurfaceWidget;

class LockController : public QObject {
    Q_OBJECT
public:
    LockController(ThemeManager *theme, BackgroundManager *bg, QObject *parent = nullptr);
    ~LockController() override;

    QString requestLock();
    bool isLocked() const { return m_locked; }
    bool isPreviewVisible() const { return m_previewVisible; }
    void showPreview();
    void hidePreview();
    QString statusJson() const;

    const QString &passwordBuffer() const { return m_password; }
    bool isAuthenticating() const { return m_pam.isAuthenticating(); }
    const QString &failureMessage() const { return m_failureMessage; }
    bool fingerprintAvailable() const { return m_fprintd.available(); }
    QString fingerprintStatus() const { return m_fprintd.statusText(); }
    double shakeOffset() const { return m_shakeOffset; }
    QImage blurredWallpaperFor(const QSize &pixelSize);

    void appendPasswordText(const QString &text);
    void backspacePassword();
    void clearPassword();
    void submitPassword();
    void nudgeFingerprint() { m_fprintd.nudge(); }

signals:
    void lockStateChanged(bool locked);
    void viewStateChanged();

private:
    void syncLockScreens();
    void destroyWindows();
    void triggerShake();

    ThemeManager *m_theme;
    BackgroundManager *m_bg;
    PamAuth m_pam;
    FprintdService m_fprintd;

    bool m_locked = false;
    bool m_previewVisible = false;
    bool m_posterRequested = false;
    QString m_password;
    QString m_failureMessage;
    double m_shakeOffset = 0.0;
    QVariantAnimation m_shakeAnim;

    QString m_cachedBgPath;
    QImage m_cachedBlur;

    QVector<LockSurfaceWidget *> m_windows;
};

class LockSurfaceWidget : public QWidget {
    Q_OBJECT
public:
    LockSurfaceWidget(QScreen *screen, ThemeManager *theme, LockController *ctrl, bool previewMode);

    QScreen *targetScreen() const { return m_screen; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    QScreen *m_screen;
    ThemeManager *m_theme;
    LockController *m_ctrl;
    bool m_previewMode;
};

} // namespace quattro
