#pragma once

#include "../core/ShellConfig.h"
#include "../core/ThemeManager.h"
#include <QColor>
#include <QFont>
#include <QRect>
#include <QScreen>
#include <QString>
#include <QWidget>
#include <functional>

namespace quattro {

class UiPaint {
public:
    static void drawCard(QPainter &p,
                         const QRect &rect,
                         ThemeManager *theme,
                         const QString &section,
                         const QColor &bg,
                         int radius,
                         int fallbackBorderWidth = 1);

    static void drawHero(QPainter &p,
                         const QRect &rect,
                         ThemeManager *theme,
                         const QString &glyph,
                         const QString &title,
                         const QString &subtitle,
                         const QColor &textColor,
                         const QColor &mutedColor,
                         const QColor &accentColor);

    static void drawSectionHeader(QPainter &p,
                                  const QRect &rect,
                                  ThemeManager *theme,
                                  const QString &title,
                                  const QColor &color);

    static void drawToggleSwitch(QPainter &p,
                                 const QRect &rect,
                                 ThemeManager *theme,
                                 bool checked,
                                 bool hovered = false,
                                 bool busy = false);

    static void drawSlider(QPainter &p,
                           const QRect &rect,
                           ThemeManager *theme,
                           double fraction,
                           bool hovered = false);

    static void drawProgressBar(QPainter &p,
                                const QRect &rect,
                                int radius,
                                double fraction,
                                const QColor &track,
                                const QColor &fill);

    static void drawPillButton(QPainter &p,
                               const QRect &rect,
                               ThemeManager *theme,
                               const QString &text,
                               bool active,
                               bool hovered,
                               bool danger = false);

    static void drawSearchHeader(QPainter &p,
                                 const QRect &rect,
                                 ThemeManager *theme,
                                 const QString &text,
                                 const QString &placeholder,
                                 const QColor &textColor,
                                 const QColor &mutedColor,
                                 const QColor &accentColor);

    static void drawDialGauge(QPainter &p,
                              const QRect &rect,
                              ThemeManager *theme,
                              const QString &title,
                              double value,
                              double maxValue,
                              const QString &unit,
                              const QColor &arcColor,
                              const QColor &textColor,
                              const QColor &mutedColor);
};

// Base overlay/anchored LayerShell window using theme-aware BorderPainter card rendering,
// click-outside / Esc dismissal, optional confirmation modal, and smooth scroll support.
class ShellPopupWindow : public QWidget {
    Q_OBJECT
public:
    ShellPopupWindow(ThemeManager *theme,
                     ShellConfig *config,
                     const QString &scope,
                     bool centeredOverlay,
                     QWidget *parent = nullptr);

    void openAt(const QPoint &globalAnchor = QPoint(), QScreen *screen = nullptr);
    void closePopup();
    bool isOpened() const { return m_opened; }

    void askConfirm(const QString &title, const QString &body, std::function<void()> onConfirm);

signals:
    void closed();

protected:
    ThemeManager *theme() const { return m_theme; }
    ShellConfig *config() const { return m_config; }

    virtual QSize desiredCardSize() const = 0;
    virtual QColor scrimColor() const;
    virtual QString borderSection() const { return QStringLiteral("popups"); }
    virtual void paintCardContent(QPainter &p, const QRect &card) = 0;
    virtual bool handleKey(QKeyEvent *event) { Q_UNUSED(event); return false; }
    virtual void handleCardMousePress(QMouseEvent *event, const QRect &card) { Q_UNUSED(event); Q_UNUSED(card); }
    virtual void handleCardMouseMove(QMouseEvent *event, const QRect &card) { Q_UNUSED(event); Q_UNUSED(card); }
    virtual void handleCardMouseRelease(QMouseEvent *event, const QRect &card) { Q_UNUSED(event); Q_UNUSED(card); }
    virtual void handleCardWheel(QWheelEvent *event, const QRect &card);
    virtual void onOpening() {}
    virtual void onClosing() {}

    QRect computeCardRect() const;
    void syncScreenGeometry();

    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void showEvent(QShowEvent *event) override;

    int m_scrollOffset = 0;
    int m_maxScroll = 0;
    QPoint m_hoverPos;

private:
    ThemeManager *m_theme;
    ShellConfig *m_config;
    QString m_scope;
    bool m_centered;
    bool m_opened = false;
    QPoint m_anchorGlobal;
    QScreen *m_targetScreen = nullptr;

    // Optional confirmation dialog modal inside overlay
    bool m_confirmOpen = false;
    QString m_confirmTitle;
    QString m_confirmBody;
    int m_confirmIdx = 1; // 0 = confirm, 1 = cancel
    QRect m_confirmYesRect;
    QRect m_confirmNoRect;
    std::function<void()> m_onConfirm;
};

} // namespace quattro
