#pragma once

#include "../core/ThemeManager.h"
#include <QTimer>
#include <QVariantAnimation>
#include <QWidget>

namespace quattro {

class OsdWindow : public QWidget {
    Q_OBJECT
public:
    explicit OsdWindow(ThemeManager *theme, QWidget *parent = nullptr);

    void openJson(const QString &payloadJson);
    void showOsd(const QString &iconName,
                 const QString &message,
                 int value,
                 int maxValue,
                 bool hasProgress,
                 const QString &progressText,
                 int durationMs);
    void closeOsd();
    bool isOpened() const { return m_opened; }

    static QString iconFor(const QString &name, int percent);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void updateGeometryForContent();

    ThemeManager *m_theme;
    bool m_opened = false;
    QString m_iconKey;
    QString m_icon = QStringLiteral("");
    QString m_message;
    int m_value = 0;
    int m_maxValue = 100;
    double m_animatedRatio = 0.0;
    bool m_hasProgress = true;
    int m_durationMs = 1200;

    QTimer m_hideTimer;
    QVariantAnimation m_progressAnim;
};

} // namespace quattro
