#pragma once

#include "CommonUi.h"
#include "../services/ClipboardService.h"
#include <QHash>
#include <QPixmap>
#include <QVector>

namespace quattro {

class ClipboardOverlay : public ShellPopupWindow {
    Q_OBJECT
public:
    ClipboardOverlay(ClipboardService *service,
                     ThemeManager *theme,
                     ShellConfig *config,
                     QWidget *parent = nullptr);

protected:
    QSize desiredCardSize() const override { return QSize(780, 500); }
    void onOpening() override;
    void paintCardContent(QPainter &p, const QRect &card) override;
    bool handleKey(QKeyEvent *event) override;
    void handleCardMousePress(QMouseEvent *event, const QRect &card) override;

private:
    void refilter();
    QPixmap previewPixmapFor(const QString &path);

    ClipboardService *m_service;
    QString m_query;
    QVector<int> m_filteredIndices;
    int m_selectedPos = 0;
    QHash<QString, QPixmap> m_thumbCache;

    QRect m_listRect;
    QRect m_clearBtnRect;
    int m_rowHeight = 46;
};

} // namespace quattro
