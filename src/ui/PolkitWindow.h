#pragma once

#include "../core/ThemeManager.h"
#include "../services/PolkitService.h"
#include <QPropertyAnimation>
#include <QRect>
#include <QWidget>

namespace quattro {

class PolkitWindow : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal shakeOffset READ shakeOffset WRITE setShakeOffset)
public:
    PolkitWindow(PolkitService *service, ThemeManager *theme, QWidget *parent = nullptr);

    qreal shakeOffset() const { return m_shakeOffset; }
    void setShakeOffset(qreal v) {
        m_shakeOffset = v;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void onRequestOpened(const PolkitRequestInfo &info);
    void onAuthenticationFailed(const QString &msg);
    void onRequestClosed(bool gained);
    void triggerShake();
    void submitCurrent();

    PolkitService *m_service;
    ThemeManager *m_theme;
    PolkitRequestInfo m_info;

    QString m_input;
    bool m_showPassword = false;
    QString m_errorText;
    QString m_statusText;
    qreal m_shakeOffset = 0.0;
    QPropertyAnimation *m_shakeAnim = nullptr;

    QRect m_cardRect;
    QRect m_eyeRect;
    QRect m_cancelRect;
    QRect m_authRect;
    QPoint m_hoverPos;
};

} // namespace quattro
