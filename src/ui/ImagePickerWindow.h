#pragma once

#include "CommonUi.h"
#include <QHash>
#include <QPixmap>
#include <QVector>

namespace quattro {

struct PickerItem {
    QString path;
    QString previewPath;
    QString label;
    bool isTheme = false;
    bool isStockTheme = false;
};

class ImagePickerWindow : public ShellPopupWindow {
    Q_OBJECT
public:
    ImagePickerWindow(ThemeManager *theme, ShellConfig *config, QWidget *parent = nullptr);

    // Low-level positional API matching `quattro-shell image-selector ...`
    QString openSelector(const QString &dirs,
                         const QString &imageRowsB64,
                         const QString &selectedImage,
                         const QString &selectionFile,
                         const QString &doneFile,
                         bool showLabels,
                         bool filterable);
    QString preloadRows(const QString &imageRowsB64,
                        const QString &selectedImage,
                        bool showLabels,
                        bool filterable);
    void cancel();

    // High-level JSON payload API matching `shell summon image-picker ...`
    void openPayload(const QString &payloadJson);
    void openThemes();

protected:
    QSize desiredCardSize() const override;
    QColor scrimColor() const override { return QColor(0, 0, 0, 120); }
    void paintEvent(QPaintEvent *event) override;
    void paintCardContent(QPainter &p, const QRect &card) override;
    bool handleKey(QKeyEvent *event) override;
    void handleCardMousePress(QMouseEvent *event, const QRect &card) override;
    void handleCardWheel(QWheelEvent *event, const QRect &card) override;
    void onClosing() override;

private:
    void parseTsvRows(const QString &tsv, bool themeRows);
    void applyFilter();
    int indexForPath(const QString &path) const;
    QPixmap pixmapFor(const PickerItem &item, const QSize &targetSize);
    void applySelected();
    void releasePendingRequest(const QString &selectedResult);
    void startDirectoryScan(const QString &dirs);
    void refreshThemesAsync();
    static bool isVideoPath(const QString &path);

    QVector<PickerItem> m_allItems;
    QVector<int> m_filteredIndices;
    QHash<QString, QPixmap> m_pixCache;
    QHash<QString, bool> m_thumbPending;
    QVector<QPair<int, QRect>> m_sliceHitRects;

    QString m_selectedTarget;
    QString m_selectionFile;
    QString m_doneFile;
    QString m_lastRowsTsv;
    QString m_filterText;
    int m_selectedPos = 0;
    bool m_showLabels = false;
    bool m_filterable = false;
    bool m_themeMode = false;
    bool m_requestActive = false;
    int m_requestSerial = 0;
};

} // namespace quattro
