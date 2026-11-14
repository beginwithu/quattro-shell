#pragma once

#include "CommonUi.h"
#include <QVector>

namespace quattro {

struct EmojiEntry {
    QString emoji;
    QString keywords;
};

class EmojiOverlay : public ShellPopupWindow {
    Q_OBJECT
public:
    EmojiOverlay(ThemeManager *theme, ShellConfig *config, QWidget *parent = nullptr);

protected:
    QSize desiredCardSize() const override { return QSize(540, 440); }
    void onOpening() override;
    void paintCardContent(QPainter &p, const QRect &card) override;
    bool handleKey(QKeyEvent *event) override;
    void handleCardMousePress(QMouseEvent *event, const QRect &card) override;

private:
    void ensureLoaded();
    void refilter();
    void insertEmoji(const QString &emoji);

    QVector<EmojiEntry> m_all;
    QVector<int> m_filtered;
    bool m_loaded = false;
    QString m_query;
    int m_selected = 0;
    int m_cols = 10;
    int m_cellSize = 48;
    QRect m_gridRect;
};

} // namespace quattro
