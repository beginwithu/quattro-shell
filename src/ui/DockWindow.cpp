#include "DockWindow.h"
#include "BorderPainter.h"
#include "../wayland/LayerShellIntegration.h"

#include <QApplication>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QProcess>
#include <algorithm>

namespace quattro {

// ---------------- AppGridWindow ----------------

AppGridWindow::AppGridWindow(DesktopEntryStore *entries,
                             ShellConfig *config,
                             ThemeManager *theme,
                             QWidget *parent)
    : ShellPopupWindow(theme, config, QStringLiteral("quattro-dock-apps"), true, parent)
    , m_entries(entries)
{}

void AppGridWindow::onOpening() {
    m_entries->ensureScanned();
    m_allVisible = m_entries->visibleApps();
    m_query.clear();
    m_scrollOffset = 0;
    m_selected = 0;
    refilter();
}

void AppGridWindow::refilter() {
    m_filtered.clear();
    const QString needle = m_query.trimmed();
    for (int i = 0; i < m_allVisible.size(); ++i) {
        const auto &a = m_allVisible.at(i);
        if (needle.isEmpty() ||
            a.name.contains(needle, Qt::CaseInsensitive) ||
            a.genericName.contains(needle, Qt::CaseInsensitive) ||
            a.id.contains(needle, Qt::CaseInsensitive)) {
            m_filtered.push_back(i);
        }
    }
    if (m_filtered.isEmpty())
        m_selected = 0;
    else
        m_selected = std::clamp(m_selected, 0, static_cast<int>(m_filtered.size()) - 1);
}

void AppGridWindow::paintCardContent(QPainter &p, const QRect &card) {
    const SurfaceColors &sc = theme()->popups();
    const QColor muted = theme()->muted();
    const QColor accent = sc.accent.isValid() ? sc.accent : theme()->accent();
    const int pad = 18;

    const QRect searchRect(card.left() + pad, card.top() + pad, card.width() - pad * 2, 38);
    UiPaint::drawSearchHeader(p, searchRect, theme(), m_query,
                              QStringLiteral("Search applications..."),
                              sc.text, muted, accent);

    m_gridRect = QRect(card.left() + pad, searchRect.bottom() + 12,
                       card.width() - pad * 2, card.bottom() - pad - 24 - (searchRect.bottom() + 12));

    constexpr int cols = 7;
    const int cellW = m_gridRect.width() / cols;
    const int cellH = 98;
    const int rows = (m_filtered.size() + cols - 1) / cols;
    m_maxScroll = std::max(0, rows * cellH - m_gridRect.height());
    m_scrollOffset = std::clamp(m_scrollOffset, 0, m_maxScroll);

    m_cellRects.clear();
    p.save();
    p.setClipRect(m_gridRect);

    for (int i = 0; i < m_filtered.size(); ++i) {
        const int r = i / cols;
        const int c = i % cols;
        const int y = m_gridRect.top() + r * cellH - m_scrollOffset;
        if (y + cellH < m_gridRect.top() || y > m_gridRect.bottom())
            continue;
        const QRect cell(m_gridRect.left() + c * cellW + 4, y + 4, cellW - 8, cellH - 8);
        m_cellRects.push_back({i, cell});

        const bool sel = (i == m_selected);
        const bool hov = cell.contains(m_hoverPos);
        if (sel) {
            p.setBrush(ThemeManager::withAlpha(accent, 0.22));
            p.setPen(QPen(accent, 1.5));
            p.drawRoundedRect(cell, 10, 10);
        } else if (hov) {
            p.setBrush(ThemeManager::withAlpha(sc.text, 0.08));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(cell, 10, 10);
        }

        const auto &app = m_allVisible.at(m_filtered.at(i));
        QIcon ico = app.icon.isNull() ? m_entries->iconForName(app.iconName) : app.icon;
        if (!ico.isNull()) {
            const QPixmap pm = ico.pixmap(42, 42);
            p.drawPixmap(cell.center().x() - pm.width() / 2, cell.top() + 10, pm);
        } else {
            p.setBrush(ThemeManager::withAlpha(accent, 0.3));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(QRect(cell.center().x() - 21, cell.top() + 10, 42, 42), 10, 10);
        }

        p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontCaption()));
        p.setPen(sc.text);
        const QRect lbl(cell.left() + 4, cell.top() + 58, cell.width() - 8, 28);
        p.drawText(lbl, Qt::AlignHCenter | Qt::AlignTop,
                   p.fontMetrics().elidedText(app.name, Qt::ElideRight, lbl.width()));
    }
    p.restore();

    p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontCaption()));
    p.setPen(muted);
    p.drawText(QRect(card.left() + pad, card.bottom() - pad - 18, card.width() - pad * 2, 18),
               Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("Left click / Enter: launch  •  Right click: pin to dock or uninstall"));
}

bool AppGridWindow::handleKey(QKeyEvent *event) {
    const int k = event->key();
    constexpr int cols = 7;
    if (k == Qt::Key_Right && m_selected + 1 < m_filtered.size()) { ++m_selected; update(); return true; }
    if (k == Qt::Key_Left && m_selected > 0) { --m_selected; update(); return true; }
    if (k == Qt::Key_Down && m_selected + cols < m_filtered.size()) { m_selected += cols; update(); return true; }
    if (k == Qt::Key_Up && m_selected - cols >= 0) { m_selected -= cols; update(); return true; }
    if (k == Qt::Key_Return || k == Qt::Key_Enter) {
        if (!m_filtered.isEmpty()) {
            DesktopEntryStore::launch(m_allVisible.at(m_filtered.at(m_selected)));
            closePopup();
        }
        return true;
    }
    if (k == Qt::Key_Backspace) {
        if (!m_query.isEmpty()) { m_query.chop(1); refilter(); update(); }
        return true;
    }
    const QString t = event->text();
    if (!t.isEmpty() && t.at(0).isPrint()) {
        m_query += t;
        m_scrollOffset = 0;
        refilter();
        update();
        return true;
    }
    return false;
}

void AppGridWindow::handleCardMousePress(QMouseEvent *event, const QRect & /*card*/) {
    for (const auto &pair : m_cellRects) {
        if (pair.second.contains(event->pos())) {
            const auto &app = m_allVisible.at(m_filtered.at(pair.first));
            if (event->button() == Qt::RightButton) {
                QMenu menu(this);
                QStringList pinned = config()->dockPinned();
                const bool isPinned = pinned.contains(app.id);
                menu.addAction(isPinned ? QStringLiteral("Unpin from Dock") : QStringLiteral("Pin to Dock"), [this, app, isPinned]() {
                    QStringList list = config()->dockPinned();
                    if (isPinned) list.removeAll(app.id);
                    else list.push_back(app.id);
                    config()->setDockPinned(list);
                });
                if (app.isUserOwned) {
                    menu.addAction(QStringLiteral("Uninstall..."), [this, app]() {
                        askConfirm(QStringLiteral("Remove \"%1\"?").arg(app.name),
                                   QStringLiteral("Deletes %1").arg(app.filePath),
                                   [this, id = app.id]() {
                            m_entries->uninstall(id);
                            onOpening();
                            update();
                        });
                    });
                }
                menu.exec(event->globalPosition().toPoint());
                return;
            }
            DesktopEntryStore::launch(app);
            closePopup();
            return;
        }
    }
}

// ---------------- DockWindow ----------------

DockWindow::DockWindow(QScreen *screen,
                       DockManager *manager,
                       ThemeManager *theme,
                       ShellConfig *config,
                       QWidget *parent)
    : QWidget(parent)
    , m_screen(screen)
    , m_manager(manager)
    , m_theme(theme)
    , m_config(config)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMouseTracking(true);
    setWindowTitle(QStringLiteral("quattro-dock"));
    winId();

    LayerSpec spec;
    spec.layer = LayerType::Top;
    spec.keyboard = KeyboardFocus::None;
    spec.scope = QStringLiteral("quattro-dock");
    LayerShellIntegration::configureWindow(windowHandle(), spec);

    m_hideDelay.setSingleShot(true);
    m_hideDelay.setInterval(450);
    connect(&m_hideDelay, &QTimer::timeout, this, [this]() {
        if (!m_hovered) {
            m_peeking = false;
            syncPlacement();
        }
    });

    connect(m_theme, &ThemeManager::themeChanged, this, QOverload<>::of(&QWidget::update));
}

void DockWindow::refreshFromManager() {
    syncPlacement();
    update();
}

void DockWindow::syncPlacement() {
    if (!m_manager->enabled() || !m_screen) {
        hide();
        return;
    }
    const int fs = m_manager->activeFullscreenMode();
    if (fs >= 2) {
        hide();
        return;
    }

    const QString pos = m_config->dockPosition();
    const bool vert = (pos == QLatin1String("left") || pos == QLatin1String("right"));
    const bool autohide = m_config->dockAutohide() || !m_manager->workspaceFloating() || fs == 1;
    const bool collapsed = autohide && !m_hovered && !m_peeking;

    const int count = m_manager->items().size() + 1; // +1 for AppGrid launcher icon
    const int iconSlot = 48;
    const int pad = 10;
    const int thickness = collapsed ? 4 : 62;
    const int length = collapsed ? (vert ? std::min(360, count * iconSlot + pad * 2) : std::min(460, count * iconSlot + pad * 2))
                                 : std::max(84, count * iconSlot + pad * 2);

    const QSize scrSz = m_screen->size();
    const QSize targetSize = vert ? QSize(thickness, std::min(scrSz.height() - 40, length))
                                  : QSize(std::min(scrSz.width() - 40, length), thickness);

    resize(targetSize);

    LayerSpec spec;
    spec.layer = autohide ? LayerType::Overlay : LayerType::Top;
    spec.scope = QStringLiteral("quattro-dock");
    spec.desiredSize = targetSize;
    spec.keyboard = KeyboardFocus::None;
    spec.exclusiveZone = (autohide || collapsed) ? 0 : (thickness + 10);

    if (pos == QLatin1String("top")) {
        spec.anchors = static_cast<uint32_t>(AnchorEdge::Top);
        spec.margins = QMargins(0, collapsed ? 0 : 8, 0, 0);
    } else if (pos == QLatin1String("left")) {
        spec.anchors = static_cast<uint32_t>(AnchorEdge::Left);
        spec.margins = QMargins(collapsed ? 0 : 8, 0, 0, 0);
    } else if (pos == QLatin1String("right")) {
        spec.anchors = static_cast<uint32_t>(AnchorEdge::Right);
        spec.margins = QMargins(0, 0, collapsed ? 0 : 8, 0);
    } else {
        spec.anchors = static_cast<uint32_t>(AnchorEdge::Bottom);
        spec.margins = QMargins(0, 0, 0, collapsed ? 0 : 8);
    }

    if (windowHandle()) {
        windowHandle()->setScreen(m_screen);
        LayerShellIntegration::updateWindowSpec(windowHandle(), spec);
    }
    if (!isVisible())
        show();
    update();
}

void DockWindow::enterEvent(QEnterEvent *) {
    m_hovered = true;
    m_peeking = true;
    m_hideDelay.stop();
    syncPlacement();
}

void DockWindow::leaveEvent(QEvent *) {
    m_hovered = false;
    m_hoverPos = QPoint(-1, -1);
    m_hideDelay.start();
    update();
}

void DockWindow::paintEvent(QPaintEvent *) {
    if (width() <= 6 || height() <= 6) {
        // Subtle 4px autohide trigger indicator
        QPainter p(this);
        p.fillRect(rect(), ThemeManager::withAlpha(m_theme->accent(), 0.28));
        return;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const SurfaceColors &sc = m_theme->bar();
    const QColor accent = sc.accent.isValid() ? sc.accent : m_theme->accent();
    const int radius = m_theme->cornerRadius() > 0 ? m_theme->cornerRadius() : 14;

    UiPaint::drawCard(p, rect().adjusted(1, 1, -1, -1), m_theme,
                      QStringLiteral("bar"), sc.background, radius, 1);

    const QString pos = m_config->dockPosition();
    const bool vert = (pos == QLatin1String("left") || pos == QLatin1String("right"));
    const auto &items = m_manager->items();
    const int iconSlot = 48;
    const int pad = 10;

    m_itemRects.clear();

    // App grid launcher button first
    m_launcherRect = vert ? QRect(7, pad, width() - 14, iconSlot - 4)
                          : QRect(pad, 7, iconSlot - 4, height() - 14);
    const bool lHov = m_launcherRect.contains(m_hoverPos);
    if (lHov) {
        p.setBrush(ThemeManager::withAlpha(accent, 0.2));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(m_launcherRect, 10, 10);
    }
    p.setFont(QFont(m_theme->resolvedFontFamily(), 18));
    p.setPen(accent);
    p.drawText(m_launcherRect, Qt::AlignCenter, QStringLiteral("󰀻"));

    // Pinned / running apps
    for (int i = 0; i < items.size(); ++i) {
        const DockGroupItem &item = items.at(i);
        const int off = pad + (i + 1) * iconSlot;
        const QRect slot = vert ? QRect(7, off, width() - 14, iconSlot - 4)
                                : QRect(off, 7, iconSlot - 4, height() - 14);
        m_itemRects.push_back({i, slot});

        const bool hov = slot.contains(m_hoverPos);
        if (item.focused) {
            p.setBrush(ThemeManager::withAlpha(accent, 0.24));
            p.setPen(QPen(accent, 1));
            p.drawRoundedRect(slot, 10, 10);
        } else if (hov) {
            p.setBrush(ThemeManager::withAlpha(sc.text, 0.12));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(slot, 10, 10);
        }

        const QRect icoR(slot.center().x() - 16, slot.center().y() - 16, 32, 32);
        if (!item.icon.isNull()) {
            p.save();
            if (item.minimized) p.setOpacity(0.55);
            p.drawPixmap(icoR.topLeft(), item.icon.pixmap(32, 32));
            p.restore();
        } else {
            p.setFont(QFont(m_theme->resolvedFontFamily(), 14, QFont::Bold));
            p.setPen(sc.text);
            p.drawText(icoR, Qt::AlignCenter, item.title.left(1).toUpper());
        }

        // Active window indicator dot(s)
        if (!item.addresses.isEmpty()) {
            p.setPen(Qt::NoPen);
            p.setBrush(item.focused ? accent : sc.text);
            const int dots = std::min(3, static_cast<int>(item.addresses.size()));
            for (int d = 0; d < dots; ++d) {
                const int cx = slot.center().x() + (d - (dots - 1) / 2.0) * 6;
                const int cy = (pos == QLatin1String("top")) ? slot.top() + 2
                                                             : slot.bottom() - 2;
                p.drawEllipse(QPoint(cx, cy), 2, 2);
            }
        }
    }
}

void DockWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::RightButton) {
        for (const auto &pair : m_itemRects) {
            if (pair.second.contains(event->pos()) && pair.first < m_manager->items().size()) {
                showItemMenu(m_manager->items().at(pair.first), event->globalPosition().toPoint());
                return;
            }
        }
        showDockMenu(event->globalPosition().toPoint());
        return;
    }

    if (event->button() == Qt::LeftButton) {
        if (m_launcherRect.contains(event->pos())) {
            m_manager->showAppGrid(m_screen);
            return;
        }
        for (const auto &pair : m_itemRects) {
            if (pair.second.contains(event->pos())) {
                m_dragFromIdx = pair.first;
                m_dragStartPos = event->pos();
                m_dragActive = false;
                return;
            }
        }
    }
}

void DockWindow::mouseMoveEvent(QMouseEvent *event) {
    m_hoverPos = event->pos();
    if (m_dragFromIdx >= 0 && (event->pos() - m_dragStartPos).manhattanLength() > 14)
        m_dragActive = true;
    update();
}

void DockWindow::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton || m_dragFromIdx < 0)
        return;

    const auto &items = m_manager->items();
    if (m_dragActive && m_dragFromIdx < items.size() && items.at(m_dragFromIdx).pinned) {
        for (const auto &pair : m_itemRects) {
            if (pair.second.contains(event->pos()) && pair.first != m_dragFromIdx) {
                m_manager->reorderPinned(items.at(m_dragFromIdx).id, pair.first);
                break;
            }
        }
    } else if (m_dragFromIdx < items.size()) {
        m_manager->activateGroup(items.at(m_dragFromIdx));
    }

    m_dragFromIdx = -1;
    m_dragActive = false;
}

void DockWindow::showItemMenu(const DockGroupItem &item, const QPoint &globalPos) {
    QMenu menu(this);
    if (item.entry) {
        menu.addAction(QStringLiteral("New Window"), [entry = *item.entry]() {
            DesktopEntryStore::launch(entry);
        });
    }
    menu.addAction(item.pinned ? QStringLiteral("Unpin from Dock") : QStringLiteral("Pin to Dock"),
                   [this, id = item.id]() { m_manager->togglePin(id); });
    if (!item.addresses.isEmpty()) {
        menu.addAction(item.minimized ? QStringLiteral("Restore Window") : QStringLiteral("Minimize"),
                       [this, item]() { m_manager->minimizeGroup(item); });
        menu.addAction(QStringLiteral("Close All"), [this, item]() { m_manager->closeGroup(item); });
    }
    menu.addSeparator();
    populateDockMenu(menu);
    menu.exec(globalPos);
}

void DockWindow::populateDockMenu(QMenu &menu) {
    menu.addAction(QStringLiteral("Open App Grid"), [this]() { m_manager->showAppGrid(m_screen); });
    menu.addSeparator();
    QAction *autoAct = menu.addAction(QStringLiteral("Auto-hide Dock"));
    autoAct->setCheckable(true);
    autoAct->setChecked(m_config->dockAutohide());
    connect(autoAct, &QAction::triggered, this, [this](bool chk) { m_manager->setAutohide(chk); });

    QMenu *posMenu = menu.addMenu(QStringLiteral("Position"));
    for (const QString &p : {QStringLiteral("bottom"), QStringLiteral("top"), QStringLiteral("left"), QStringLiteral("right")}) {
        QAction *a = posMenu->addAction(p);
        a->setCheckable(true);
        a->setChecked(m_config->dockPosition() == p);
        connect(a, &QAction::triggered, this, [this, p]() { m_manager->setPosition(p); });
    }
}

void DockWindow::showDockMenu(const QPoint &globalPos) {
    QMenu menu(this);
    populateDockMenu(menu);
    menu.exec(globalPos);
}

// ---------------- DockManager ----------------

DockManager::DockManager(HyprlandClient *hypr,
                         DesktopEntryStore *entries,
                         ShellConfig *config,
                         ThemeManager *theme,
                         QObject *parent)
    : QObject(parent)
    , m_hypr(hypr)
    , m_entries(entries)
    , m_config(config)
    , m_theme(theme)
{
    m_appGrid = new AppGridWindow(m_entries, m_config, m_theme);

    connect(m_hypr, &HyprlandClient::stateChanged, this, [this]() {
        if (m_enabled) {
            probeWorkspaceLayout();
            rebuildItems();
        }
    });
    connect(m_hypr, &HyprlandClient::fullscreenChanged, this, [this]() {
        int fs = 0;
        const int activeWs = m_hypr->activeWorkspaceForMonitor(m_hypr->focusedMonitorName());
        for (const auto &c : m_hypr->clients()) {
            if (c.workspaceId == activeWs && c.fullscreen > fs)
                fs = c.fullscreen;
        }
        m_fullscreenMode = fs;
        for (auto *w : m_windows) w->refreshFromManager();
    });
    connect(m_config, &ShellConfig::configChanged, this, [this]() {
        if (m_enabled) rebuildItems();
    });
    connect(qGuiApp, &QGuiApplication::screenAdded, this, [this]() {
        if (m_enabled) syncScreens();
    });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this]() {
        if (m_enabled) syncScreens();
    });
}

DockManager::~DockManager() {
    qDeleteAll(m_windows);
    delete m_appGrid;
}

void DockManager::setEnabled(bool enabled) {
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    if (enabled) {
        m_entries->ensureScanned();
        syncScreens();
        probeWorkspaceLayout();
        rebuildItems();
    } else {
        for (auto *w : m_windows) w->hide();
    }
}

void DockManager::toggleVisibility() {
    m_hiddenOverride = !m_hiddenOverride;
    setEnabled(!m_hiddenOverride);
}

void DockManager::showAppGrid(QScreen *screen) {
    m_appGrid->openAt(QPoint(), screen);
}

void DockManager::syncScreens() {
    const auto screens = qGuiApp->screens();
    for (QScreen *s : screens) {
        bool exists = false;
        for (auto *w : m_windows) {
            if (w->targetScreen() == s) { exists = true; break; }
        }
        if (!exists)
            m_windows.push_back(new DockWindow(s, this, m_theme, m_config));
    }
    for (int i = m_windows.size() - 1; i >= 0; --i) {
        if (!screens.contains(m_windows.at(i)->targetScreen())) {
            DockWindow *w = m_windows.takeAt(i);
            w->deleteLater();
        }
    }
}

void DockManager::probeWorkspaceLayout() {
    int wsId = 1;
    for (const auto &m : m_hypr->monitors()) {
        if (m.focused && m.activeWorkspaceId >= 1) {
            wsId = m.activeWorkspaceId;
            break;
        }
    }
    if (wsId == m_lastWsProbed)
        return;
    m_lastWsProbed = wsId;

    auto *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed().toLower();
        proc->deleteLater();
        const bool next = (out == QLatin1String("float") || out == QLatin1String("floating"));
        if (m_wsFloating != next) {
            m_wsFloating = next;
            for (auto *w : m_windows) w->syncPlacement();
        }
    });
    proc->start(QStringLiteral("quattro-hyprland-workspace-layout-current"), {QString::number(wsId)});
}

void DockManager::rebuildItems() {
    QVector<DockGroupItem> next;
    QHash<QString, int> byId;

    const QStringList pinned = m_config->dockPinned();
    for (const QString &pid : pinned) {
        const DesktopAppEntry *e = m_entries->findById(pid);
        DockGroupItem item;
        item.id = pid;
        item.entry = e;
        item.title = e ? e->name : pid;
        item.icon = e ? (e->icon.isNull() ? m_entries->iconForName(e->iconName) : e->icon)
                      : m_entries->iconForName(pid);
        item.pinned = true;
        byId.insert(pid.toLower(), next.size());
        next.push_back(item);
    }

    for (const HyprClientInfo &c : m_hypr->clients()) {
        const bool isMin = (c.workspaceName == QLatin1String("special:minimized"));
        if (!isMin && c.workspaceId < 1)
            continue;
        const DesktopAppEntry *e = m_entries->heuristicLookup(c.appClass, c.title);
        if (!e && !isMin)
            continue;

        const QString groupId = e ? e->id : c.appClass;
        const QString key = groupId.toLower();
        int idx = byId.value(key, -1);
        if (idx < 0) {
            DockGroupItem item;
            item.id = groupId;
            item.entry = e;
            item.title = e ? e->name : (c.title.isEmpty() ? c.appClass : c.title);
            item.icon = e ? (e->icon.isNull() ? m_entries->iconForName(e->iconName) : e->icon)
                          : m_entries->iconForName(c.appClass);
            byId.insert(key, next.size());
            next.push_back(item);
            idx = next.size() - 1;
        }
        next[idx].addresses.push_back(c.address);
        if (c.focusHistoryId == 0 && !c.hidden) next[idx].focused = true;
        if (isMin) next[idx].minimized = true;
    }

    m_items = next;
    for (auto *w : m_windows) w->refreshFromManager();
}

void DockManager::activateGroup(const DockGroupItem &item) {
    if (item.addresses.isEmpty()) {
        if (item.entry) DesktopEntryStore::launch(*item.entry);
        else DesktopEntryStore::launchById(*m_entries, item.id);
        return;
    }

    if (item.minimized) {
        int activeWs = 1;
        for (const auto &m : m_hypr->monitors()) {
            if (m.focused && m.activeWorkspaceId >= 1) { activeWs = m.activeWorkspaceId; break; }
        }
        for (const QString &addr : item.addresses) {
            m_hypr->dispatch(QStringLiteral("movetoworkspace %1,address:%2").arg(activeWs).arg(addr));
        }
        m_hypr->dispatch(QStringLiteral("focuswindow address:%1").arg(item.addresses.first()));
        return;
    }

    // Cycle to next window in group
    m_hypr->dispatch(QStringLiteral("focuswindow address:%1").arg(item.addresses.first()));
}

void DockManager::togglePin(const QString &id) {
    QStringList p = m_config->dockPinned();
    if (p.contains(id)) p.removeAll(id);
    else p.push_back(id);
    m_config->setDockPinned(p);
}

void DockManager::reorderPinned(const QString &id, int toIdx) {
    QStringList p = m_config->dockPinned();
    const int from = p.indexOf(id);
    if (from < 0 || p.isEmpty())
        return;
    p.removeAt(from);
    p.insert(std::clamp(toIdx, 0, static_cast<int>(p.size())), id);
    m_config->setDockPinned(p);
}

void DockManager::minimizeGroup(const DockGroupItem &item) {
    if (item.minimized) {
        activateGroup(item);
        return;
    }
    for (const QString &addr : item.addresses)
        m_hypr->dispatch(QStringLiteral("movetoworkspacesilent special:minimized,address:%1").arg(addr));
}

void DockManager::closeGroup(const DockGroupItem &item) {
    for (const QString &addr : item.addresses)
        m_hypr->dispatch(QStringLiteral("closewindow address:%1").arg(addr));
}

void DockManager::setPosition(const QString &pos) {
    m_config->setDockPosition(pos);
}

void DockManager::setAutohide(bool enabled) {
    m_config->setDockAutohide(enabled);
}

} // namespace quattro
