#pragma once

#include "../core/DesktopEntryStore.h"
#include "../core/HyprlandClient.h"
#include "../core/ShellConfig.h"
#include "../core/ThemeManager.h"
#include "CommonUi.h"
#include <QHash>
#include <QRect>
#include <QScreen>
#include <QTimer>
#include <QVector>
#include <QWidget>

class QMenu;

namespace quattro {

class DockManager;

struct DockGroupItem {
    QString id;        // desktopId or raw class
    QString title;
    QIcon icon;
    bool pinned = false;
    bool focused = false;
    bool minimized = false;
    QStringList addresses;
    const DesktopAppEntry *entry = nullptr;
};

class AppGridWindow : public ShellPopupWindow {
    Q_OBJECT
public:
    AppGridWindow(DesktopEntryStore *entries,
                  ShellConfig *config,
                  ThemeManager *theme,
                  QWidget *parent = nullptr);

protected:
    QSize desiredCardSize() const override { return QSize(780, 500); }
    void onOpening() override;
    void paintCardContent(QPainter &p, const QRect &card) override;
    bool handleKey(QKeyEvent *event) override;
    void handleCardMousePress(QMouseEvent *event, const QRect &card) override;

private:
    void refilter();

    DesktopEntryStore *m_entries;
    QVector<DesktopAppEntry> m_allVisible;
    QVector<int> m_filtered;
    QString m_query;
    int m_selected = 0;
    QVector<QPair<int, QRect>> m_cellRects;
    QRect m_gridRect;
};

class DockWindow : public QWidget {
    Q_OBJECT
public:
    DockWindow(QScreen *screen,
               DockManager *manager,
               ThemeManager *theme,
               ShellConfig *config,
               QWidget *parent = nullptr);

    QScreen *targetScreen() const { return m_screen; }
    void syncPlacement();
    void refreshFromManager();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void showItemMenu(const DockGroupItem &item, const QPoint &globalPos);
    void populateDockMenu(QMenu &menu);
    void showDockMenu(const QPoint &globalPos);

    QScreen *m_screen;
    DockManager *m_manager;
    ThemeManager *m_theme;
    ShellConfig *m_config;

    bool m_hovered = false;
    bool m_peeking = false;
    QTimer m_hideDelay;
    QPoint m_hoverPos;

    QRect m_launcherRect;
    QVector<QPair<int, QRect>> m_itemRects;
    int m_dragFromIdx = -1;
    QPoint m_dragStartPos;
    bool m_dragActive = false;
};

class DockManager : public QObject {
    Q_OBJECT
public:
    DockManager(HyprlandClient *hypr,
                DesktopEntryStore *entries,
                ShellConfig *config,
                ThemeManager *theme,
                QObject *parent = nullptr);
    ~DockManager() override;

    void setEnabled(bool enabled);
    bool enabled() const { return m_enabled; }
    void toggleVisibility();
    void showAppGrid(QScreen *screen = nullptr);

    const QVector<DockGroupItem> &items() const { return m_items; }
    bool workspaceFloating() const { return m_wsFloating; }
    int activeFullscreenMode() const { return m_fullscreenMode; } // 0=none, 1=maximize, 2+=fullscreen

    void activateGroup(const DockGroupItem &item);
    void togglePin(const QString &id);
    void reorderPinned(const QString &id, int toPinnedIndex);
    void minimizeGroup(const DockGroupItem &item);
    void closeGroup(const DockGroupItem &item);
    void setPosition(const QString &pos);
    void setAutohide(bool enabled);

private:
    void syncScreens();
    void rebuildItems();
    void probeWorkspaceLayout();

    HyprlandClient *m_hypr;
    DesktopEntryStore *m_entries;
    ShellConfig *m_config;
    ThemeManager *m_theme;

    bool m_enabled = false;
    bool m_hiddenOverride = false;
    bool m_wsFloating = false;
    int m_fullscreenMode = 0;
    int m_lastWsProbed = -1;

    QVector<DockGroupItem> m_items;
    QVector<DockWindow *> m_windows;
    AppGridWindow *m_appGrid = nullptr;
};

} // namespace quattro
