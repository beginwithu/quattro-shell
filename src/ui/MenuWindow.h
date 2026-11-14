#pragma once

#include "../core/ShellConfig.h"
#include "../core/ThemeManager.h"
#include <QHash>
#include <QIcon>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

namespace quattro {

struct MenuItem {
    QString id;
    QString parentId;
    QString icon;
    QString label;
    QString subtitle;
    QString action;
    QString provider;
    QString whenCmd;
    QString title;
    QStringList aliases;
    QString desktopExec;
    QIcon appIcon;
    bool hasChildren = false;
    bool terminal = false;
};

class MenuWindow : public QWidget {
    Q_OBJECT
public:
    MenuWindow(ThemeManager *theme, ShellConfig *config, QWidget *parent = nullptr);

    void openJson(const QString &payloadJson);
    void toggleJson(const QString &payloadJson);
    void closeMenu();
    void refreshMenuData();
    bool isOpened() const { return m_opened; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void loadMenuFiles();
    void loadDesktopApps();
    void openRoute(const QString &route);
    void openDmenu(const QJsonObject &payload);
    void rebuildVisibleItems();
    void activateRow(int index);
    QRect cardRect() const;

    ThemeManager *m_theme;
    ShellConfig *m_config;

    bool m_opened = false;
    QString m_mode = QStringLiteral("menu"); // menu | select | input
    QString m_activeRoute = QStringLiteral("root");
    QStringList m_navStack;
    QString m_filterText;
    int m_selectedIndex = 0;
    int m_scrollOffset = 0;

    // Dmenu state
    QString m_dmenuPrompt;
    QStringList m_dmenuOptions;
    QString m_selectionFile;
    QString m_doneFile;

    QHash<QString, MenuItem> m_items;
    QStringList m_itemOrder;
    QVector<MenuItem> m_apps;
    bool m_appsLoaded = false;

    QVector<MenuItem> m_visibleItems;
    QVector<QRect> m_rowRects;
};

} // namespace quattro
