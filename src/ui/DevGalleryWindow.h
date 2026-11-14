#pragma once

#include "CommonUi.h"

namespace quattro {

class DevGalleryWindow : public ShellPopupWindow {
    Q_OBJECT
public:
    DevGalleryWindow(ThemeManager *theme, ShellConfig *config, QWidget *parent = nullptr);

protected:
    QSize desiredCardSize() const override { return QSize(680, 520); }
    void paintCardContent(QPainter &p, const QRect &card) override;
    void handleCardMousePress(QMouseEvent *event, const QRect &card) override;
    void handleCardMouseMove(QMouseEvent *event, const QRect &card) override;
    void handleCardMouseRelease(QMouseEvent *event, const QRect &card) override;

private:
    bool m_toggleA = true;
    bool m_toggleB = false;
    bool m_pillActive = true;
    double m_sliderVal = 0.65;
    bool m_draggingSlider = false;

    QRect m_toggleARect;
    QRect m_toggleBRect;
    QRect m_sliderRect;
    QRect m_pillRect;
    QRect m_confirmTestRect;
};

} // namespace quattro
