#include "BarWindow.h"

#include "../services/MediaService.h"
#include "../services/NightlightService.h"
#include "../services/RemoteSessionService.h"
#include "../wayland/LayerShellIntegration.h"
#include "BorderPainter.h"
#include "DockWindow.h"
#include "MenuWindow.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QProcess>
#include <QToolTip>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace quattro {

BarManager::BarManager(ThemeManager *theme,
                       ShellConfig *config,
                       HyprlandClient *hypr,
                       AudioService *audio,
                       SystemStatsService *stats,
                       NotificationDaemon *notif,
                       TrayWatcher *tray,
                       MenuWindow *menu,
                       QObject *parent)
    : QObject(parent)
    , m_theme(theme)
    , m_config(config)
    , m_hypr(hypr)
    , m_audio(audio)
    , m_stats(stats)
    , m_notif(notif)
    , m_tray(tray)
    , m_menu(menu)
{
    readWorkspaceBlocks();

    m_clockTimer.setInterval(1000);
    connect(&m_clockTimer, &QTimer::timeout, this, &BarManager::updateAllBars);
    m_clockTimer.start();

    connect(qGuiApp, &QGuiApplication::screenAdded, this, [this](QScreen *) { syncScreens(); });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this](QScreen *) { syncScreens(); });

    connect(m_theme, &ThemeManager::themeChanged, this, [this]() {
        for (BarWindow *b : m_bars)
            b->syncGeometryAndVisibility();
    });
    connect(m_config, &ShellConfig::configChanged, this, [this]() {
        for (BarWindow *b : m_bars)
            b->syncGeometryAndVisibility();
    });
    connect(m_config, &ShellConfig::barHiddenChanged, this, [this]() {
        for (BarWindow *b : m_bars)
            b->syncGeometryAndVisibility();
    });
    connect(m_hypr, &HyprlandClient::stateChanged, this, &BarManager::updateAllBars);
    connect(m_hypr, &HyprlandClient::activeWindowChanged, this, &BarManager::updateAllBars);
    connect(m_hypr, &HyprlandClient::keyboardLayoutChanged, this, &BarManager::updateAllBars);
    connect(m_audio, &AudioService::audioChanged, this, &BarManager::updateAllBars);
    connect(m_stats, &SystemStatsService::statsChanged, this, &BarManager::updateAllBars);
    connect(m_stats, &SystemStatsService::indicatorsChanged, this, &BarManager::updateAllBars);
    connect(m_stats, &SystemStatsService::updateStatusChanged, this, &BarManager::updateAllBars);
    connect(m_notif, &NotificationDaemon::dndChanged, this, &BarManager::updateAllBars);
    connect(m_tray, &TrayWatcher::itemsChanged, this, &BarManager::updateAllBars);

    syncScreens();
}

BarManager::~BarManager() {
    qDeleteAll(m_bars);
    m_bars.clear();
}

void BarManager::readWorkspaceBlocks() {
    m_monitorBlocks.clear();
    const QString path = qEnvironmentVariable("XDG_STATE_HOME", QDir::homePath() + QStringLiteral("/.local/state"))
                         + QStringLiteral("/quattro/monitors/workspaces");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        const QStringList fields = line.split(QLatin1Char('\t'));
        if (fields.size() >= 3 && !fields[1].isEmpty()) {
            bool ok = false;
            const int blk = fields[0].toInt(&ok);
            if (ok)
                m_monitorBlocks.insert(fields[1], blk);
        }
    }
}

void BarManager::syncScreens() {
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (int i = m_bars.size() - 1; i >= 0; --i) {
        if (!screens.contains(m_bars[i]->targetScreen())) {
            m_bars[i]->close();
            m_bars[i]->deleteLater();
            m_bars.removeAt(i);
        }
    }
    for (QScreen *s : screens) {
        bool exists = false;
        for (BarWindow *b : m_bars) {
            if (b->targetScreen() == s) { exists = true; break; }
        }
        if (!exists) {
            auto *bar = new BarWindow(s, m_theme, m_config, m_hypr, m_audio, m_stats, m_notif, m_tray, m_menu, &m_monitorBlocks);
            bar->setExtraServices(m_extra);
            bar->setClockFormatIndex(m_clockFormatIdx);
            connect(bar, &BarWindow::panelToggleRequested, this, &BarManager::panelToggleRequested);
            bar->syncGeometryAndVisibility();
            m_bars.push_back(bar);
        }
    }
}

void BarManager::setExtraServices(const BarExtraServices &extra) {
    m_extra = extra;
    if (m_extra.media)
        connect(m_extra.media, &MediaService::mediaChanged, this, &BarManager::updateAllBars);
    if (m_extra.nightlight)
        connect(m_extra.nightlight, &NightlightService::changed, this, &BarManager::updateAllBars);
    if (m_extra.remote)
        connect(m_extra.remote, &RemoteSessionService::changed, this, &BarManager::updateAllBars);
    for (BarWindow *b : m_bars)
        b->setExtraServices(m_extra);
}

void BarManager::updateAllBars() {
    for (BarWindow *b : m_bars)
        b->update();
}

void BarManager::cycleClockFormat() {
    m_clockFormatIdx = (m_clockFormatIdx + 1) % 4;
    for (BarWindow *b : m_bars)
        b->setClockFormatIndex(m_clockFormatIdx);
}

QString BarManager::debugGeometryJson() const {
    QJsonArray arr;
    for (const BarWindow *b : m_bars)
        arr.append(b->debugGeometry());
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

BarWindow::BarWindow(QScreen *screen,
                     ThemeManager *theme,
                     ShellConfig *config,
                     HyprlandClient *hypr,
                     AudioService *audio,
                     SystemStatsService *stats,
                     NotificationDaemon *notif,
                     TrayWatcher *tray,
                     MenuWindow *menu,
                     const QHash<QString, int> *monitorBlocks)
    : QWidget(nullptr)
    , m_screen(screen)
    , m_theme(theme)
    , m_config(config)
    , m_hypr(hypr)
    , m_audio(audio)
    , m_stats(stats)
    , m_notif(notif)
    , m_tray(tray)
    , m_menu(menu)
    , m_monitorBlocks(monitorBlocks)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMouseTracking(true);
    setWindowTitle(QStringLiteral("quattro-bar"));

    const QSize screenSz = screen ? screen->size() : QSize(1920, 1080);
    resize(screenSz.width(), m_theme->barSizeHorizontal());
    winId();
    if (screen)
        windowHandle()->setScreen(screen);

    LayerShellIntegration::configureWindow(windowHandle(), currentLayerSpec(size()));
}

bool BarWindow::isVertical() const {
    const QString pos = m_config->barPosition();
    return pos == QLatin1String("left") || pos == QLatin1String("right");
}

QString BarWindow::monitorName() const {
    return m_screen ? m_screen->name() : QString();
}

LayerSpec BarWindow::currentLayerSpec(const QSize &sz) const {
    const QString pos = m_config->barPosition();
    LayerSpec spec;
    spec.layer = LayerType::Top;
    spec.keyboard = KeyboardFocus::None;
    spec.scope = QStringLiteral("quattro-bar");
    spec.desiredSize = sz;

    if (pos == QLatin1String("bottom")) {
        spec.anchors = static_cast<uint32_t>(AnchorEdge::Bottom)
                       | static_cast<uint32_t>(AnchorEdge::Left)
                       | static_cast<uint32_t>(AnchorEdge::Right);
        spec.exclusiveZone = m_config->barHidden() ? 0 : sz.height();
    } else if (pos == QLatin1String("left")) {
        spec.anchors = static_cast<uint32_t>(AnchorEdge::Left)
                       | static_cast<uint32_t>(AnchorEdge::Top)
                       | static_cast<uint32_t>(AnchorEdge::Bottom);
        spec.exclusiveZone = m_config->barHidden() ? 0 : sz.width();
    } else if (pos == QLatin1String("right")) {
        spec.anchors = static_cast<uint32_t>(AnchorEdge::Right)
                       | static_cast<uint32_t>(AnchorEdge::Top)
                       | static_cast<uint32_t>(AnchorEdge::Bottom);
        spec.exclusiveZone = m_config->barHidden() ? 0 : sz.width();
    } else {
        spec.anchors = static_cast<uint32_t>(AnchorEdge::Top)
                       | static_cast<uint32_t>(AnchorEdge::Left)
                       | static_cast<uint32_t>(AnchorEdge::Right);
        spec.exclusiveZone = m_config->barHidden() ? 0 : sz.height();
    }
    return spec;
}

void BarWindow::syncGeometryAndVisibility() {
    const QSize screenSz = m_screen ? m_screen->size() : QSize(1920, 1080);
    const bool vert = isVertical();
    const QSize targetSz = vert
                               ? QSize(m_theme->barSizeVertical(), screenSz.height())
                               : QSize(screenSz.width(), m_theme->barSizeHorizontal());
    resize(targetSz);
    LayerShellIntegration::updateWindowSpec(windowHandle(), currentLayerSpec(targetSz));

    if (m_config->barHidden())
        hide();
    else if (!isVisible())
        show();
    update();
}

void BarWindow::setClockFormatIndex(int idx) {
    m_clockFormatIdx = idx;
    update();
}

QJsonObject BarWindow::debugGeometry() const {
    QJsonObject o;
    o[QStringLiteral("screen")] = monitorName();
    o[QStringLiteral("position")] = m_config->barPosition();
    o[QStringLiteral("width")] = width();
    o[QStringLiteral("height")] = height();
    o[QStringLiteral("visible")] = isVisible();
    return o;
}

QVector<int> BarWindow::workspaceIdsForMonitor() const {
    constexpr int slotsCount = 10;
    const QString mon = monitorName();
    const int activeId = m_hypr->activeWorkspaceForMonitor(mon);
    const int block = m_monitorBlocks && m_monitorBlocks->contains(mon)
                          ? m_monitorBlocks->value(mon)
                          : (activeId > 0 ? (activeId - 1) / slotsCount : 0);
    const int base = block * slotsCount;

    QVector<int> ids{base + 1, base + 2, base + 3, base + 4, base + 5};
    QVector<int> guests;
    for (const HyprWorkspace &ws : m_hypr->workspaces()) {
        if (ws.id <= 0)
            continue;
        if ((ws.id - 1) / slotsCount == block) {
            if (!ids.contains(ws.id))
                ids.push_back(ws.id);
        } else if (ws.monitorName == mon) {
            guests.push_back(ws.id);
        }
    }
    std::sort(ids.begin(), ids.end());
    std::sort(guests.begin(), guests.end());
    ids.append(guests);
    return ids;
}

QString BarWindow::clockFormatString() const {
    static const QStringList horizFormats{
        QStringLiteral("dddd HH:mm"),
        QStringLiteral("d MMMM yyyy"),
        QStringLiteral("HH:mm:ss"),
        QStringLiteral("ddd d MMM HH:mm")
    };
    static const QStringList vertFormats{
        QStringLiteral("HH\n—\nmm"),
        QStringLiteral("dd\nMMM\nyy"),
        QStringLiteral("HH\nmm\nss"),
        QStringLiteral("HH\nmm")
    };
    const auto &list = isVertical() ? vertFormats : horizFormats;
    return list.at(m_clockFormatIdx % list.size());
}

QString BarWindow::formattedClock() const {
    const QDateTime now = QDateTime::currentDateTime();
    const QString fmt = clockFormatString();
    // Support literal 'W'ww (ISO week) like Clock BarWidget.qml
    QString out = fmt;
    if (out.contains(QStringLiteral("'W'ww"))) {
        const int week = now.date().weekNumber();
        out.replace(QStringLiteral("'W'ww"), QStringLiteral("'W%1'").arg(week, 2, 10, QLatin1Char('0')));
    }
    return QLocale().toString(now, out);
}

BarWindow::WidgetRender BarWindow::buildWidget(const BarWidgetEntry &entry, bool vertical) {
    const QString &id = entry.id;
    const int barH = m_theme->barSizeHorizontal();
    const int barW = m_theme->barSizeVertical();
    const int cross = vertical ? barW : barH;
    const int slot = m_theme->barIconSlot();
    const int statusSlot = m_theme->barStatusSlot();

    QFont textFont(m_theme->fontFamily());
    textFont.setPixelSize(m_theme->fontBody());
    const QFontMetrics textFm(textFont);

    QFont iconFont(m_theme->fontFamily());
    iconFont.setPixelSize(m_theme->barIconFont());

    const QColor fg = m_theme->bar().text;
    const QColor urgentCol = m_theme->bar().active;

    WidgetRender w;
    w.id = id;

    auto makeIconButton = [&](const QString &glyph, int btnSlot, const QColor &col, const QString &tooltip,
                              std::function<void(Qt::MouseButton, QPoint)> onClick,
                              std::function<void(int)> onWheel = {}) {
        w.size = vertical ? QSize(cross, btnSlot) : QSize(btnSlot, cross);
        w.draw = [=](QPainter &p, const QRect &r, QVector<HitAction> &hits) {
            p.setFont(iconFont);
            p.setPen(col);
            p.drawText(r, Qt::AlignCenter, glyph);
            hits.push_back({r, tooltip, onClick, onWheel});
        };
    };

    if (id == QLatin1String("quattro.menu")) {
        makeIconButton(QStringLiteral("󰣇"), slot, fg, QStringLiteral("Menu"),
                       [this](Qt::MouseButton, QPoint) {
                           m_menu->toggleJson(QStringLiteral("{\"menu\":\"root\"}"));
                       });
    } else if (id == QLatin1String("quattro.workspaces")) {
        const QVector<int> ids = workspaceIdsForMonitor();
        const int cellW = vertical ? cross : m_theme->space(20);
        const int cellH = vertical ? m_theme->space(20) : cross;
        const int gap = m_theme->space(1);
        const int total = ids.size() * (vertical ? cellH : cellW) + std::max(0, static_cast<int>(ids.size()) - 1) * gap;
        w.size = vertical ? QSize(cross, total) : QSize(total, cross);
        w.draw = [this, ids, vertical, cellW, cellH, gap, fg, iconFont, textFont](QPainter &p, const QRect &r, QVector<HitAction> &hits) {
            const int activeId = m_hypr->activeWorkspaceForMonitor(monitorName());
            int pos = vertical ? r.top() : r.left();
            for (int wsId : ids) {
                const QRect cell = vertical ? QRect(r.left(), pos, cellW, cellH)
                                            : QRect(pos, r.top(), cellW, cellH);
                const bool shown = (wsId == activeId);
                const bool occ = m_hypr->workspaceHasWindows(wsId);
                const int slotNum = (wsId - 1) % 10 + 1;
                const QString label = shown ? QStringLiteral("󱓻")
                                            : (slotNum == 10 ? QStringLiteral("0") : QString::number(slotNum));
                p.setFont(shown ? iconFont : textFont);
                p.setPen(ThemeManager::withAlpha(fg, (shown || occ) ? 1.0 : 0.5));
                p.drawText(cell, Qt::AlignCenter, label);

                hits.push_back({
                    cell,
                    QStringLiteral("Workspace %1").arg(slotNum),
                    [this, wsId, shown](Qt::MouseButton, QPoint) {
                        if (shown) {
                            QProcess::startDetached(QStringLiteral("quattro-hyprland-workspace-layout-toggle"), {});
                        } else {
                            m_hypr->dispatch(QStringLiteral("workspace %1").arg(wsId));
                        }
                    },
                    [this, ids, activeId](int delta) {
                        const int idx = ids.indexOf(activeId);
                        if (idx < 0) return;
                        const int nextIdx = std::clamp(idx + (delta > 0 ? -1 : 1), 0, static_cast<int>(ids.size()) - 1);
                        if (nextIdx != idx)
                            m_hypr->dispatch(QStringLiteral("workspace %1").arg(ids[nextIdx]));
                    }
                });
                pos += (vertical ? cellH : cellW) + gap;
            }
        };
    } else if (id == QLatin1String("quattro.active-window")) {
        const QString title = m_hypr->activeWindowTitle();
        const int maxW = m_theme->space(220);
        const int tw = title.isEmpty() || vertical ? 0 : std::min(maxW, textFm.horizontalAdvance(title) + m_theme->space(12));
        w.size = vertical ? QSize(0, 0) : QSize(tw, cross);
        w.draw = [=](QPainter &p, const QRect &r, QVector<HitAction> &) {
            if (title.isEmpty() || r.isEmpty()) return;
            p.setFont(textFont);
            p.setPen(ThemeManager::withAlpha(fg, 0.85));
            p.drawText(r, Qt::AlignCenter, textFm.elidedText(title, Qt::ElideRight, r.width() - m_theme->space(8)));
        };
    } else if (id == QLatin1String("quattro.indicators")) {
        struct IndSpec {
            QString glyph;
            bool active;
            bool urgent;
            QString tip;
            std::function<void()> toggle;
        };
        const bool nlOn = m_extra.nightlight ? m_extra.nightlight->enabled() : m_stats->nightLight();
        QVector<IndSpec> all{
            {QStringLiteral("󰟵"), m_stats->passwordlessSudo(), true, QStringLiteral("Passwordless Sudo"),
             [this]() { QProcess::startDetached(QStringLiteral("quattro-sudo-passwordless"), {m_stats->passwordlessSudo() ? QStringLiteral("--disable") : QString()}); m_stats->refreshIndicators(); }},
            {QStringLiteral("󰻂"), m_stats->screenRecording(), true, QStringLiteral("Screen Recording"),
             [this]() { QProcess::startDetached(QStringLiteral("quattro-capture-screenrecording"), {m_stats->screenRecording() ? QStringLiteral("--stop-recording") : QString()}); }},
            {QStringLiteral("󰔎"), nlOn, false, QStringLiteral("Night Light"),
             [this]() {
                 if (m_extra.nightlight) m_extra.nightlight->toggle();
                 else m_stats->toggleNightLight();
             }},
            {QStringLiteral("󰂛"), m_notif->doNotDisturb(), false, QStringLiteral("Do Not Disturb"),
             [this]() { m_notif->toggleDoNotDisturb(); }},
            {QStringLiteral(""), m_stats->stayAwake(), false, QStringLiteral("Stay Awake"),
             [this]() { m_stats->toggleStayAwake(); }},
        };
        if (m_extra.remote && m_extra.remote->active()) {
            all.push_back({QStringLiteral("󰢹"), true, true, QStringLiteral("Remote Desktop Active"), []() {}});
        }
        QVector<IndSpec> shown;
        for (const auto &ind : all) {
            if (ind.active || m_indicatorsHovered)
                shown.push_back(ind);
        }
        const int count = shown.size();
        const int extent = count > 0 ? count * statusSlot : m_theme->space(8);
        w.size = vertical ? QSize(cross, extent) : QSize(extent, cross);
        w.draw = [=](QPainter &p, const QRect &r, QVector<HitAction> &hits) {
            if (shown.isEmpty()) {
                hits.push_back({r, QStringLiteral("Indicators"), [](Qt::MouseButton, QPoint) {}, {}});
                return;
            }
            int pos = vertical ? r.top() : r.left();
            for (const auto &ind : shown) {
                const QRect cell = vertical ? QRect(r.left(), pos, cross, statusSlot)
                                            : QRect(pos, r.top(), statusSlot, cross);
                p.setFont(iconFont);
                QColor c = ind.active ? (ind.urgent ? urgentCol : fg) : ThemeManager::withAlpha(fg, 0.45);
                p.setPen(c);
                p.drawText(cell, Qt::AlignCenter, ind.glyph);
                hits.push_back({cell, ind.tip, [ind](Qt::MouseButton, QPoint) { ind.toggle(); }, {}});
                pos += statusSlot;
            }
        };
    } else if (id == QLatin1String("quattro.clock")) {
        const QString text = formattedClock();
        if (vertical) {
            const QStringList lines = text.split(QLatin1Char('\n'));
            const int h = lines.size() * textFm.height() + m_theme->space(8);
            w.size = QSize(cross, h);
            w.draw = [=](QPainter &p, const QRect &r, QVector<HitAction> &hits) {
                p.setFont(textFont);
                p.setPen(fg);
                p.drawText(r, Qt::AlignCenter, text);
                hits.push_back({
                    r,
                    QDateTime::currentDateTime().toString(QStringLiteral("dddd, d MMMM yyyy")),
                    [this](Qt::MouseButton btn, QPoint globalPt) {
                        if (btn == Qt::RightButton)
                            setClockFormatIndex((m_clockFormatIdx + 1) % 4);
                        else
                            emit panelToggleRequested(QStringLiteral("quattro.clock"), globalPt, m_screen);
                    },
                    {}
                });
            };
        } else {
            const int tw = textFm.horizontalAdvance(text) + m_theme->space(14);
            w.size = QSize(tw, cross);
            w.draw = [=](QPainter &p, const QRect &r, QVector<HitAction> &hits) {
                p.setFont(textFont);
                p.setPen(fg);
                p.drawText(r, Qt::AlignCenter, text);
                hits.push_back({
                    r,
                    QDateTime::currentDateTime().toString(QStringLiteral("dddd, d MMMM yyyy")),
                    [this](Qt::MouseButton btn, QPoint globalPt) {
                        if (btn == Qt::RightButton)
                            setClockFormatIndex((m_clockFormatIdx + 1) % 4);
                        else
                            emit panelToggleRequested(QStringLiteral("quattro.clock"), globalPt, m_screen);
                    },
                    {}
                });
            };
        }
    } else if (id == QLatin1String("quattro.tray")) {
        const auto &items = m_tray->items();
        const int sz = statusSlot;
        const int total = items.size() * sz;
        w.size = vertical ? QSize(cross, total) : QSize(total, cross);
        w.draw = [this, items, vertical, cross, sz, fg, iconFont](QPainter &p, const QRect &r, QVector<HitAction> &hits) {
            const int iconPx = m_theme->barIconSize();
            int pos = vertical ? r.top() : r.left();
            for (const TrayItem &it : items) {
                const QRect cell = vertical ? QRect(r.left(), pos, cross, sz)
                                            : QRect(pos, r.top(), sz, cross);
                if (!it.pixmap.isNull()) {
                    const QRect pxRect(cell.left() + (cell.width() - iconPx) / 2,
                                       cell.top() + (cell.height() - iconPx) / 2,
                                       iconPx, iconPx);
                    p.drawPixmap(pxRect, it.pixmap.scaled(iconPx, iconPx, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                } else {
                    p.setFont(iconFont);
                    p.setPen(fg);
                    p.drawText(cell, Qt::AlignCenter, QStringLiteral("󰀻"));
                }
                const QString key = it.key;
                hits.push_back({
                    cell,
                    it.tooltip.isEmpty() ? it.title : it.tooltip,
                    [this, key](Qt::MouseButton btn, QPoint globalPt) {
                        if (btn == Qt::RightButton) m_tray->contextMenuItem(key, globalPt.x(), globalPt.y());
                        else if (btn == Qt::MiddleButton) m_tray->secondaryActivateItem(key, globalPt.x(), globalPt.y());
                        else m_tray->activateItem(key, globalPt.x(), globalPt.y());
                    },
                    {}
                });
                pos += sz;
            }
        };
    } else if (id == QLatin1String("quattro.keyboard-layout")) {
        QString lang = m_hypr->keyboardLayout();
        QString shortCode = lang.left(2).toLower();
        if (lang.contains(QLatin1String("English"), Qt::CaseInsensitive)) shortCode = QStringLiteral("us");
        if (shortCode.isEmpty()) shortCode = QStringLiteral("us");
        const int kw = vertical ? cross : textFm.horizontalAdvance(shortCode) + m_theme->space(10);
        w.size = vertical ? QSize(cross, statusSlot) : QSize(kw, cross);
        w.draw = [=](QPainter &p, const QRect &r, QVector<HitAction> &hits) {
            p.setFont(textFont);
            p.setPen(fg);
            p.drawText(r, Qt::AlignCenter, shortCode);
            hits.push_back({r, lang.isEmpty() ? QStringLiteral("Keyboard Layout") : lang,
                            [](Qt::MouseButton, QPoint) {
                                QProcess::startDetached(QStringLiteral("hyprctl"), {QStringLiteral("switchxkblayout"), QStringLiteral("all"), QStringLiteral("next")});
                            }, {}});
        };
    } else if (id == QLatin1String("quattro.microphone")) {
        const bool showMic = m_audio->micActive() || m_audio->sourceMuted();
        if (!showMic) {
            w.size = QSize(0, 0);
            w.draw = [](QPainter &, const QRect &, QVector<HitAction> &) {};
        } else {
            const QString glyph = m_audio->sourceMuted() ? QStringLiteral("󰍭") : QStringLiteral("󰍬");
            makeIconButton(glyph, statusSlot, m_audio->sourceMuted() ? urgentCol : fg,
                           QStringLiteral("Microphone"),
                           [this](Qt::MouseButton, QPoint) { m_audio->toggleSourceMute(); });
        }
    } else if (id == QLatin1String("quattro.system-update")) {
        if (!m_stats->updateAvailable()) {
            w.size = QSize(0, 0);
            w.draw = [](QPainter &, const QRect &, QVector<HitAction> &) {};
        } else {
            makeIconButton(QStringLiteral("\uf021"), statusSlot, urgentCol, QStringLiteral("Updates Available"),
                           [](Qt::MouseButton, QPoint) {
                               QProcess::startDetached(QStringLiteral("quattro-launch-floating-terminal-with-presentation"),
                                                       {QStringLiteral("quattro-update")});
                           });
        }
    } else if (id == QLatin1String("quattro.bluetooth")) {
        makeIconButton(QStringLiteral("󰂯"), statusSlot, fg, QStringLiteral("Bluetooth"),
                       [this](Qt::MouseButton, QPoint globalPt) {
                           emit panelToggleRequested(QStringLiteral("quattro.bluetooth"), globalPt, m_screen);
                       });
    } else if (id == QLatin1String("quattro.network")) {
        const QString glyph = m_stats->ethernetConnected()
                                  ? QStringLiteral("󰈀")
                                  : (m_stats->wifiConnected() ? QStringLiteral("󰤨") : QStringLiteral("󰤭"));
        makeIconButton(glyph, statusSlot, m_stats->networkConnected() ? fg : ThemeManager::withAlpha(fg, 0.55),
                       QStringLiteral("Network"),
                       [this](Qt::MouseButton, QPoint globalPt) {
                           emit panelToggleRequested(QStringLiteral("quattro.network"), globalPt, m_screen);
                       });
    } else if (id == QLatin1String("quattro.audio")) {
        const int vol = m_audio->sinkVolume();
        const bool muted = m_audio->sinkMuted();
        QString glyph = QStringLiteral("");
        if (muted || vol <= 0) glyph = QStringLiteral("");
        else if (vol <= 33) glyph = QStringLiteral("");
        else if (vol <= 66) glyph = QStringLiteral("");
        makeIconButton(glyph, statusSlot, muted ? urgentCol : fg,
                       QStringLiteral("Volume: %1%").arg(vol),
                       [this](Qt::MouseButton btn, QPoint globalPt) {
                           if (btn == Qt::MiddleButton)
                               m_audio->toggleSinkMute();
                           else
                               emit panelToggleRequested(QStringLiteral("quattro.audio"), globalPt, m_screen);
                       },
                       [this](int delta) {
                           m_audio->adjustVolume(delta > 0 ? 5 : -5);
                       });
    } else if (id == QLatin1String("quattro.monitor")) {
        const QString txt = vertical
                                ? QStringLiteral("%1%").arg(m_stats->cpuPercent())
                                : QStringLiteral("󰍛 %1%").arg(m_stats->cpuPercent());
        const int mw = vertical ? cross : textFm.horizontalAdvance(txt) + m_theme->space(10);
        w.size = vertical ? QSize(cross, statusSlot) : QSize(mw, cross);
        w.draw = [=](QPainter &p, const QRect &r, QVector<HitAction> &hits) {
            p.setFont(textFont);
            p.setPen(fg);
            p.drawText(r, Qt::AlignCenter, txt);
            hits.push_back({r, QStringLiteral("CPU %1%  RAM %2%").arg(m_stats->cpuPercent()).arg(m_stats->ramPercent()),
                            [this](Qt::MouseButton, QPoint globalPt) {
                                emit panelToggleRequested(QStringLiteral("quattro.monitor"), globalPt, m_screen);
                            }, {}});
        };
    } else if (id == QLatin1String("quattro.power")) {
        if (m_stats->batteryPresent()) {
            const int pct = m_stats->batteryPercent();
            const bool chg = m_stats->batteryCharging();
            QString icon = chg ? QStringLiteral("󰂄")
                               : (pct > 80 ? QStringLiteral("󰁹")
                                           : (pct > 40 ? QStringLiteral("󰁾")
                                                       : (pct > 15 ? QStringLiteral("󰁻") : QStringLiteral("󰂃"))));
            const QString txt = vertical ? icon : QStringLiteral("%1 %2%").arg(icon).arg(pct);
            const int pw = vertical ? cross : textFm.horizontalAdvance(txt) + m_theme->space(10);
            w.size = vertical ? QSize(cross, statusSlot) : QSize(pw, cross);
            w.draw = [=](QPainter &p, const QRect &r, QVector<HitAction> &hits) {
                p.setFont(textFont);
                p.setPen((!chg && pct <= 15) ? urgentCol : fg);
                p.drawText(r, Qt::AlignCenter, txt);
                hits.push_back({r, QStringLiteral("Battery %1%").arg(pct),
                                [this](Qt::MouseButton, QPoint globalPt) {
                                    emit panelToggleRequested(QStringLiteral("quattro.power"), globalPt, m_screen);
                                }, {}});
            };
        } else {
            makeIconButton(QStringLiteral(""), statusSlot, fg, QStringLiteral("Power"),
                           [this](Qt::MouseButton, QPoint globalPt) {
                               emit panelToggleRequested(QStringLiteral("quattro.power"), globalPt, m_screen);
                           });
        }
    } else if (id == QLatin1String("quattro.weather")) {
        makeIconButton(QStringLiteral("󰖕"), statusSlot, ThemeManager::withAlpha(fg, 0.85),
                       QStringLiteral("Weather"),
                       [this](Qt::MouseButton, QPoint globalPt) {
                           emit panelToggleRequested(QStringLiteral("quattro.weather"), globalPt, m_screen);
                       });
    } else if (id == QLatin1String("quattro.elsewhen")) {
        makeIconButton(QStringLiteral("󰇧"), statusSlot, ThemeManager::withAlpha(fg, 0.85),
                       QStringLiteral("World Clock"),
                       [this](Qt::MouseButton, QPoint globalPt) {
                           emit panelToggleRequested(QStringLiteral("quattro.elsewhen"), globalPt, m_screen);
                       });
    } else if (id == QLatin1String("quattro.dropbox")) {
        makeIconButton(QStringLiteral("󰇣"), statusSlot, ThemeManager::withAlpha(fg, 0.85),
                       QStringLiteral("Dropbox"),
                       [this](Qt::MouseButton, QPoint globalPt) {
                           emit panelToggleRequested(QStringLiteral("quattro.dropbox"), globalPt, m_screen);
                       });
    } else if (id == QLatin1String("quattro.tailscale")) {
        makeIconButton(QStringLiteral("󰖂"), statusSlot, ThemeManager::withAlpha(fg, 0.85),
                       QStringLiteral("Tailscale"),
                       [this](Qt::MouseButton, QPoint globalPt) {
                           emit panelToggleRequested(QStringLiteral("quattro.tailscale"), globalPt, m_screen);
                       });
    } else if (id == QLatin1String("quattro.media")) {
        const MprisPlayerInfo *mp = m_extra.media ? m_extra.media->activePlayer() : nullptr;
        const bool playing = mp && mp->isPlaying();
        const QString title = mp && !mp->title.isEmpty() ? mp->title : (mp ? mp->identity : QStringLiteral("Media"));
        if (vertical || !mp) {
            makeIconButton(playing ? QStringLiteral("󰎆") : QStringLiteral("󰝚"), statusSlot,
                           playing ? m_theme->accent() : ThemeManager::withAlpha(fg, 0.6),
                           title,
                           [this](Qt::MouseButton btn, QPoint globalPt) {
                               if (btn == Qt::MiddleButton && m_extra.media)
                                   m_extra.media->runAction(QStringLiteral("play-pause"), false);
                               else
                                   emit panelToggleRequested(QStringLiteral("quattro.media"), globalPt, m_screen);
                           });
        } else {
            const QString txt = QStringLiteral("󰎆 ") + textFm.elidedText(title, Qt::ElideRight, 130);
            const int mw = textFm.horizontalAdvance(txt) + m_theme->space(12);
            w.size = QSize(mw, cross);
            w.draw = [=](QPainter &p, const QRect &r, QVector<HitAction> &hits) {
                p.setFont(textFont);
                p.setPen(playing ? fg : ThemeManager::withAlpha(fg, 0.65));
                p.drawText(r, Qt::AlignCenter, txt);
                hits.push_back({r, title,
                                [this](Qt::MouseButton btn, QPoint globalPt) {
                                    if (btn == Qt::MiddleButton && m_extra.media)
                                        m_extra.media->runAction(QStringLiteral("play-pause"), false);
                                    else
                                        emit panelToggleRequested(QStringLiteral("quattro.media"), globalPt, m_screen);
                                },
                                [this](int delta) {
                                    if (m_extra.media)
                                        m_extra.media->switchSource(delta > 0 ? -1 : 1, false, false);
                                }});
            };
        }
    } else {
        w.size = QSize(0, 0);
        w.draw = [](QPainter &, const QRect &, QVector<HitAction> &) {};
    }

    return w;
}

void BarWindow::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QString bgMode = m_config->barBackground();
    QColor bg = m_theme->bar().background;
    if (bgMode == QLatin1String("none"))
        bg = QColor(0, 0, 0, 0);
    else if (bgMode == QLatin1String("glassy"))
        bg.setAlphaF(std::min<float>(bg.alphaF(), 0.35f));
    p.fillRect(rect(), bg);

    m_hits.clear();

    const bool vert = isVertical();
    const BarLayout layout = m_config->barLayout();
    const int edgePad = m_theme->space(6);
    const int sectionGap = m_theme->space(4);

    auto buildList = [&](const QVector<BarWidgetEntry> &entries) {
        QVector<WidgetRender> out;
        for (const BarWidgetEntry &e : entries) {
            WidgetRender wr = buildWidget(e, vert);
            if (!wr.size.isEmpty())
                out.push_back(std::move(wr));
        }
        return out;
    };

    const QVector<WidgetRender> leftW = buildList(layout.left);
    const QVector<WidgetRender> centerBeginW = buildList(layout.centerBegin);
    const QVector<WidgetRender> centerW = buildList(layout.center);
    const QVector<WidgetRender> centerEndW = buildList(layout.centerEnd);

    QVector<WidgetRender> rightW = buildList(layout.tray);
    rightW.append(buildList(layout.right));

    auto totalExtent = [&](const QVector<WidgetRender> &list) {
        int sum = 0;
        for (int i = 0; i < list.size(); ++i) {
            sum += vert ? list[i].size.height() : list[i].size.width();
            if (i + 1 < list.size()) sum += sectionGap;
        }
        return sum;
    };

    auto paintSeq = [&](const QVector<WidgetRender> &list, int startCoord) {
        int cur = startCoord;
        for (const WidgetRender &wr : list) {
            const QRect r = vert
                                ? QRect(0, cur, width(), wr.size.height())
                                : QRect(cur, 0, wr.size.width(), height());
            if (wr.draw)
                wr.draw(p, r, m_hits);
            cur += (vert ? wr.size.height() : wr.size.width()) + sectionGap;
        }
    };

    // Left / Top
    paintSeq(leftW, edgePad);

    // Right / Bottom
    const int rightExt = totalExtent(rightW);
    const int fullExt = vert ? height() : width();
    paintSeq(rightW, fullExt - edgePad - rightExt);

    // Center block: [centerBegin] + [center] + [centerEnd] centered around centerW
    const int cExt = totalExtent(centerW);
    const int cStart = (fullExt - cExt) / 2;
    paintSeq(centerW, cStart);

    const int cbExt = totalExtent(centerBeginW);
    if (cbExt > 0)
        paintSeq(centerBeginW, cStart - sectionGap - cbExt);

    if (!centerEndW.isEmpty())
        paintSeq(centerEndW, cStart + cExt + sectionGap);
}

void BarWindow::mousePressEvent(QMouseEvent *event) {
    const QPoint pos = event->pos();
    const QPoint globalPt = event->globalPosition().toPoint();
    for (const HitAction &hit : m_hits) {
        if (hit.rect.contains(pos) && hit.onClick) {
            hit.onClick(event->button(), globalPt);
            return;
        }
    }
}

void BarWindow::mouseMoveEvent(QMouseEvent *event) {
    const QPoint pos = event->pos();
    int found = -1;
    for (int i = 0; i < m_hits.size(); ++i) {
        if (m_hits[i].rect.contains(pos)) {
            found = i;
            break;
        }
    }

    const bool overInd = found >= 0 && (m_hits[found].tooltip == QLatin1String("Indicators")
                                        || m_hits[found].tooltip == QLatin1String("Do Not Disturb")
                                        || m_hits[found].tooltip == QLatin1String("Stay Awake")
                                        || m_hits[found].tooltip == QLatin1String("Night Light")
                                        || m_hits[found].tooltip == QLatin1String("Screen Recording")
                                        || m_hits[found].tooltip == QLatin1String("Passwordless Sudo"));
    if (overInd != m_indicatorsHovered) {
        m_indicatorsHovered = overInd;
        update();
    }

    if (found != m_hoveredHitIndex) {
        m_hoveredHitIndex = found;
        if (found >= 0 && !m_hits[found].tooltip.isEmpty())
            QToolTip::showText(event->globalPosition().toPoint(), m_hits[found].tooltip, this);
        else
            QToolTip::hideText();
    }
}

void BarWindow::leaveEvent(QEvent *) {
    m_hoveredHitIndex = -1;
    if (m_indicatorsHovered) {
        m_indicatorsHovered = false;
        update();
    }
    QToolTip::hideText();
}

void BarWindow::wheelEvent(QWheelEvent *event) {
    const QPoint pos = event->position().toPoint();
    const int delta = event->angleDelta().y() != 0 ? event->angleDelta().y() : event->angleDelta().x();
    if (delta == 0)
        return;

    for (const HitAction &hit : m_hits) {
        if (hit.rect.contains(pos) && hit.onWheel) {
            m_wheelAccum += delta;
            if (std::abs(m_wheelAccum) >= 120) {
                const int steps = m_wheelAccum / 120;
                m_wheelAccum %= 120;
                hit.onWheel(steps);
            }
            return;
        }
    }
}

} // namespace quattro
