#pragma once

#include "CommonUi.h"
#include <QVector>

namespace quattro {

class ReminderOverlay : public ShellPopupWindow {
    Q_OBJECT
public:
    ReminderOverlay(ThemeManager *theme, ShellConfig *config, QWidget *parent = nullptr);

protected:
    QSize desiredCardSize() const override { return QSize(460, 260); }
    void onOpening() override;
    void paintCardContent(QPainter &p, const QRect &card) override;
    bool handleKey(QKeyEvent *event) override;
    void handleCardMousePress(QMouseEvent *event, const QRect &card) override;

private:
    static int parseMinutes(const QString &raw);
    void submit();

    int m_step = 0; // 0 = minutes, 1 = message
    QString m_timeInput;
    QString m_messageInput;
    QString m_errorText;
    QVector<QPair<QString, QRect>> m_presetButtons;
    QRect m_nextBtnRect;
    QRect m_backBtnRect;
};

} // namespace quattro
