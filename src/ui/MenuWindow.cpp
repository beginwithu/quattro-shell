#include "MenuWindow.h"

#include "../wayland/LayerShellIntegration.h"
#include "BorderPainter.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QProcess>
#include <QRegularExpression>
#include <QScreen>
#include <QSet>
#include <QWheelEvent>
#include <algorithm>

namespace quattro {

namespace {

QByteArray readFileBytes(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

void writeSignalFile(const QString &path, const QString &text) {
    if (path.isEmpty())
        return;
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(text.toUtf8());
}

QString cleanExecField(QString exec) {
    static const QRegularExpression fieldCodes(QStringLiteral("\\s+%[fFuUdDnNickvm]"));
    exec.remove(fieldCodes);
    return exec.trimmed();
}

} // namespace

MenuWindow::MenuWindow(ThemeManager *theme, ShellConfig *config, QWidget *parent)
    : QWidget(parent)
    , m_theme(theme)
    , m_config(config)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setWindowTitle(QStringLiteral("quattro-menu"));

    const QSize sz = qGuiApp->primaryScreen() ? qGuiApp->primaryScreen()->size() : QSize(1920, 1080);
    resize(sz);
    winId();

    LayerSpec spec;
    spec.layer = LayerType::Overlay;
    spec.anchors = static_cast<uint32_t>(AnchorEdge::Top)
                   | static_cast<uint32_t>(AnchorEdge::Bottom)
                   | static_cast<uint32_t>(AnchorEdge::Left)
                   | static_cast<uint32_t>(AnchorEdge::Right);
    spec.exclusiveZone = -1;
    spec.keyboard = KeyboardFocus::Exclusive;
    spec.scope = QStringLiteral("quattro-menu");
    spec.desiredSize = sz;
    LayerShellIntegration::configureWindow(windowHandle(), spec);

    connect(m_theme, &ThemeManager::themeChanged, this, [this]() {
        if (m_opened) update();
    });

    loadMenuFiles();
}

void MenuWindow::loadMenuFiles() {
    m_items.clear();
    m_itemOrder.clear();

    auto ingestFile = [&](const QString &path) {
        const QByteArray raw = readFileBytes(path);
        if (raw.isEmpty())
            return;
        const QByteArray stripped = ShellConfig::stripJsonc(raw);
        const QJsonDocument doc = QJsonDocument::fromJson(stripped);
        if (!doc.isObject())
            return;
        const QJsonObject root = doc.object();
        for (auto it = root.begin(); it != root.end(); ++it) {
            if (!it.value().isObject())
                continue;
            const QString id = it.key();
            const QJsonObject obj = it.value().toObject();

            MenuItem item = m_items.value(id);
            item.id = id;
            const int lastDot = id.lastIndexOf(QLatin1Char('.'));
            item.parentId = lastDot >= 0 ? id.left(lastDot) : QStringLiteral("root");
            if (obj.contains(QStringLiteral("icon"))) item.icon = obj.value(QStringLiteral("icon")).toString();
            if (obj.contains(QStringLiteral("label"))) item.label = obj.value(QStringLiteral("label")).toString();
            if (obj.contains(QStringLiteral("action"))) item.action = obj.value(QStringLiteral("action")).toString();
            if (obj.contains(QStringLiteral("provider"))) item.provider = obj.value(QStringLiteral("provider")).toString();
            if (obj.contains(QStringLiteral("when"))) item.whenCmd = obj.value(QStringLiteral("when")).toString();
            if (obj.contains(QStringLiteral("title"))) item.title = obj.value(QStringLiteral("title")).toString();
            if (obj.contains(QStringLiteral("aliases"))) {
                item.aliases.clear();
                for (const QJsonValue &av : obj.value(QStringLiteral("aliases")).toArray())
                    item.aliases.push_back(av.toString());
            }
            if (!m_items.contains(id))
                m_itemOrder.push_back(id);
            m_items.insert(id, item);
        }
    };

    ingestFile(m_config->quattroPath() + QStringLiteral("/default/quattro/quattro-menu.jsonc"));
    const QString userExt = qEnvironmentVariable("XDG_CONFIG_HOME", QDir::homePath() + QStringLiteral("/.config"))
                            + QStringLiteral("/quattro/extensions/quattro-menu.jsonc");
    ingestFile(userExt);

    // Mark parent items that have children
    QSet<QString> parents;
    for (auto it = m_items.cbegin(); it != m_items.cend(); ++it)
        parents.insert(it.value().parentId);
    for (auto it = m_items.begin(); it != m_items.end(); ++it)
        it.value().hasChildren = parents.contains(it.key()) || !it.value().provider.isEmpty();
}

void MenuWindow::loadDesktopApps() {
    if (m_appsLoaded)
        return;
    m_appsLoaded = true;
    m_apps.clear();

    const QStringList dirs{
        m_config->quattroPath() + QStringLiteral("/applications"),
        QDir::homePath() + QStringLiteral("/.local/share/applications"),
        QStringLiteral("/usr/local/share/applications"),
        QStringLiteral("/usr/share/applications")
    };

    QSet<QString> seenNames;
    for (const QString &dir : dirs) {
        if (!QFileInfo::exists(dir))
            continue;
        QDirIterator it(dir, {QStringLiteral("*.desktop")}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString filePath = it.next();
            const QString relId = QFileInfo(filePath).fileName();
            if (seenNames.contains(relId))
                continue;

            QFile f(filePath);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;

            bool inDesktopEntry = false;
            bool noDisplay = false;
            bool hidden = false;
            bool terminal = false;
            QString name, genericName, exec, iconName, type;

            while (!f.atEnd()) {
                const QString line = QString::fromUtf8(f.readLine()).trimmed();
                if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
                    continue;
                if (line.startsWith(QLatin1Char('['))) {
                    inDesktopEntry = (line == QLatin1String("[Desktop Entry]"));
                    continue;
                }
                if (!inDesktopEntry)
                    continue;
                const int eq = line.indexOf(QLatin1Char('='));
                if (eq <= 0)
                    continue;
                const QString k = line.left(eq).trimmed();
                const QString v = line.mid(eq + 1).trimmed();
                if (k == QLatin1String("Type")) type = v;
                else if (k == QLatin1String("Name") && name.isEmpty()) name = v;
                else if (k == QLatin1String("GenericName") && genericName.isEmpty()) genericName = v;
                else if (k == QLatin1String("Exec")) exec = v;
                else if (k == QLatin1String("Icon")) iconName = v;
                else if (k == QLatin1String("NoDisplay")) noDisplay = (v == QLatin1String("true"));
                else if (k == QLatin1String("Hidden")) hidden = (v == QLatin1String("true"));
                else if (k == QLatin1String("Terminal")) terminal = (v == QLatin1String("true"));
            }

            if (type != QLatin1String("Application") || noDisplay || hidden || name.isEmpty() || exec.isEmpty())
                continue;

            seenNames.insert(relId);
            MenuItem app;
            app.id = QStringLiteral("app:") + relId;
            app.label = name;
            app.subtitle = genericName;
            app.icon = QStringLiteral("󰀻");
            app.desktopExec = cleanExecField(exec);
            app.terminal = terminal;
            if (!iconName.isEmpty()) {
                if (QFileInfo::exists(iconName))
                    app.appIcon = QIcon(iconName);
                else {
                    const QString customIcon = m_config->quattroPath() + QStringLiteral("/applications/icons/") + iconName + QStringLiteral(".png");
                    if (QFileInfo::exists(customIcon))
                        app.appIcon = QIcon(customIcon);
                    else
                        app.appIcon = QIcon::fromTheme(iconName);
                }
            }
            m_apps.push_back(app);
        }
    }

    std::sort(m_apps.begin(), m_apps.end(), [](const MenuItem &a, const MenuItem &b) {
        return a.label.compare(b.label, Qt::CaseInsensitive) < 0;
    });
}

void MenuWindow::refreshMenuData() {
    m_appsLoaded = false;
    loadMenuFiles();
    if (m_opened)
        rebuildVisibleItems();
}

void MenuWindow::openJson(const QString &payloadJson) {
    const QJsonDocument doc = QJsonDocument::fromJson(payloadJson.toUtf8());
    const QJsonObject p = doc.object();

    if (m_items.isEmpty())
        loadMenuFiles();

    const QString mode = p.value(QStringLiteral("mode")).toString(QStringLiteral("menu"));
    if (mode == QLatin1String("select") || mode == QLatin1String("input")) {
        openDmenu(p);
        return;
    }

    QString route = p.value(QStringLiteral("initialMenu")).toString();
    if (route.isEmpty())
        route = p.value(QStringLiteral("menu")).toString(QStringLiteral("root"));
    openRoute(route);
}

void MenuWindow::toggleJson(const QString &payloadJson) {
    if (m_opened)
        closeMenu();
    else
        openJson(payloadJson);
}

void MenuWindow::openRoute(const QString &route) {
    m_mode = QStringLiteral("menu");
    m_navStack.clear();
    m_filterText.clear();
    m_selectedIndex = 0;
    m_scrollOffset = 0;

    QString resolved = route.trimmed();
    if (resolved.isEmpty())
        resolved = QStringLiteral("root");

    // Match aliases if exact route isn't a submenu
    if (resolved != QLatin1String("root") && !m_items.contains(resolved)) {
        for (auto it = m_items.cbegin(); it != m_items.cend(); ++it) {
            if (it.value().aliases.contains(resolved, Qt::CaseInsensitive)) {
                resolved = it.key();
                break;
            }
        }
    }

    m_activeRoute = resolved;
    rebuildVisibleItems();

    if (QScreen *s = qGuiApp->primaryScreen())
        resize(s->size());
    m_opened = true;
    show();
    activateWindow();
    setFocus();
    update();
}

void MenuWindow::openDmenu(const QJsonObject &p) {
    m_mode = p.value(QStringLiteral("mode")).toString(QStringLiteral("select"));
    m_dmenuPrompt = p.value(QStringLiteral("prompt")).toString();
    m_selectionFile = p.value(QStringLiteral("selectionFile")).toString();
    m_doneFile = p.value(QStringLiteral("doneFile")).toString();
    m_dmenuOptions.clear();
    for (const QJsonValue &v : p.value(QStringLiteral("options")).toArray())
        m_dmenuOptions.push_back(v.toString());
    m_filterText.clear();
    m_selectedIndex = 0;
    m_scrollOffset = 0;
    rebuildVisibleItems();

    if (QScreen *s = qGuiApp->primaryScreen())
        resize(s->size());
    m_opened = true;
    show();
    activateWindow();
    setFocus();
    update();
}

void MenuWindow::closeMenu() {
    if (!m_opened)
        return;
    m_opened = false;
    if (!m_doneFile.isEmpty()) {
        writeSignalFile(m_doneFile, QStringLiteral("cancel\n"));
        m_doneFile.clear();
    }
    hide();
}

void MenuWindow::rebuildVisibleItems() {
    m_visibleItems.clear();
    const QString needle = m_filterText.trimmed();

    if (m_mode == QLatin1String("select") || m_mode == QLatin1String("input")) {
        for (const QString &opt : m_dmenuOptions) {
            if (!needle.isEmpty() && !opt.contains(needle, Qt::CaseInsensitive))
                continue;
            MenuItem it;
            it.id = opt;
            it.label = opt;
            m_visibleItems.push_back(it);
        }
        if (m_mode == QLatin1String("input") && m_visibleItems.isEmpty() && !needle.isEmpty()) {
            MenuItem it;
            it.id = needle;
            it.label = needle;
            m_visibleItems.push_back(it);
        }
    } else {
        const MenuItem routeItem = m_items.value(m_activeRoute);
        if (routeItem.provider == QLatin1String("apps")) {
            loadDesktopApps();
            for (const MenuItem &app : m_apps) {
                if (!needle.isEmpty()
                    && !app.label.contains(needle, Qt::CaseInsensitive)
                    && !app.subtitle.contains(needle, Qt::CaseInsensitive)) {
                    continue;
                }
                m_visibleItems.push_back(app);
            }
        } else {
            for (const QString &id : m_itemOrder) {
                const MenuItem &it = m_items.value(id);
                if (needle.isEmpty()) {
                    if (it.parentId != m_activeRoute)
                        continue;
                } else {
                    if (m_activeRoute != QLatin1String("root") && !id.startsWith(m_activeRoute + QLatin1Char('.')))
                        continue;
                    bool match = it.label.contains(needle, Qt::CaseInsensitive)
                                 || it.id.contains(needle, Qt::CaseInsensitive);
                    if (!match) {
                        for (const QString &a : it.aliases) {
                            if (a.contains(needle, Qt::CaseInsensitive)) { match = true; break; }
                        }
                    }
                    if (!match)
                        continue;
                }
                m_visibleItems.push_back(it);
            }
        }
    }

    if (m_selectedIndex >= m_visibleItems.size())
        m_selectedIndex = std::max(0, static_cast<int>(m_visibleItems.size()) - 1);
    if (m_selectedIndex < 0)
        m_selectedIndex = 0;
}

void MenuWindow::activateRow(int index) {
    if (index < 0 || index >= m_visibleItems.size())
        return;

    const MenuItem item = m_visibleItems[index];

    if (m_mode == QLatin1String("select") || m_mode == QLatin1String("input")) {
        writeSignalFile(m_selectionFile, item.label + QLatin1Char('\n'));
        writeSignalFile(m_doneFile, QStringLiteral("ok\n"));
        m_doneFile.clear();
        m_opened = false;
        hide();
        return;
    }

    if (!item.desktopExec.isEmpty()) {
        closeMenu();
        if (item.terminal) {
            QProcess::startDetached(QStringLiteral("quattro-launch-floating-terminal-with-presentation"), {item.desktopExec});
        } else {
            QProcess::startDetached(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), item.desktopExec});
        }
        return;
    }

    if (item.hasChildren && item.action.isEmpty()) {
        m_navStack.push_back(m_activeRoute);
        m_activeRoute = item.id;
        m_filterText.clear();
        m_selectedIndex = 0;
        m_scrollOffset = 0;
        rebuildVisibleItems();
        update();
        return;
    }

    if (!item.action.isEmpty()) {
        const QString cmd = item.action;
        closeMenu();
        QProcess::startDetached(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), cmd});
    } else if (item.hasChildren) {
        m_navStack.push_back(m_activeRoute);
        m_activeRoute = item.id;
        m_filterText.clear();
        m_selectedIndex = 0;
        m_scrollOffset = 0;
        rebuildVisibleItems();
        update();
    }
}

QRect MenuWindow::cardRect() const {
    const int cardW = std::min(width() - m_theme->space(40), m_theme->space(420));
    const int headerH = m_theme->space(46);
    const int rowH = m_theme->space(38);
    const int maxRows = 10;
    const int visibleCount = std::clamp(static_cast<int>(m_visibleItems.size()), 1, maxRows);
    const int pad = m_theme->space(10);
    const int cardH = std::min(height() - m_theme->space(80), headerH + visibleCount * rowH + pad * 2);
    return QRect((width() - cardW) / 2, (height() - cardH) / 2, cardW, cardH);
}

void MenuWindow::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    setFocus(Qt::ActiveWindowFocusReason);
}

void MenuWindow::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
    case Qt::Key_Escape:
        closeMenu();
        return;
    case Qt::Key_Up:
        if (!m_visibleItems.isEmpty()) {
            m_selectedIndex = (m_selectedIndex - 1 + m_visibleItems.size()) % m_visibleItems.size();
            if (m_selectedIndex < m_scrollOffset) m_scrollOffset = m_selectedIndex;
            if (m_selectedIndex >= m_scrollOffset + 10) m_scrollOffset = m_selectedIndex - 9;
            update();
        }
        return;
    case Qt::Key_Down:
        if (!m_visibleItems.isEmpty()) {
            m_selectedIndex = (m_selectedIndex + 1) % m_visibleItems.size();
            if (m_selectedIndex < m_scrollOffset) m_scrollOffset = m_selectedIndex;
            if (m_selectedIndex >= m_scrollOffset + 10) m_scrollOffset = m_selectedIndex - 9;
            update();
        }
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_mode == QLatin1String("input") && m_visibleItems.isEmpty() && !m_filterText.isEmpty()) {
            writeSignalFile(m_selectionFile, m_filterText + QLatin1Char('\n'));
            writeSignalFile(m_doneFile, QStringLiteral("ok\n"));
            m_doneFile.clear();
            m_opened = false;
            hide();
            return;
        }
        activateRow(m_selectedIndex);
        return;
    case Qt::Key_Backspace:
        if (!m_filterText.isEmpty()) {
            m_filterText.chop(1);
            m_selectedIndex = 0;
            m_scrollOffset = 0;
            rebuildVisibleItems();
            update();
        } else if (!m_navStack.isEmpty()) {
            m_activeRoute = m_navStack.takeLast();
            m_selectedIndex = 0;
            m_scrollOffset = 0;
            rebuildVisibleItems();
            update();
        }
        return;
    default:
        break;
    }

    const QString txt = event->text();
    if (!txt.isEmpty() && txt[0].isPrint()) {
        m_filterText.append(txt);
        m_selectedIndex = 0;
        m_scrollOffset = 0;
        rebuildVisibleItems();
        update();
    }
}

void MenuWindow::mousePressEvent(QMouseEvent *event) {
    const QRect card = cardRect();
    if (!card.contains(event->pos())) {
        closeMenu();
        return;
    }
    for (int i = 0; i < m_rowRects.size(); ++i) {
        if (m_rowRects[i].contains(event->pos())) {
            activateRow(m_scrollOffset + i);
            return;
        }
    }
}

void MenuWindow::mouseMoveEvent(QMouseEvent *event) {
    for (int i = 0; i < m_rowRects.size(); ++i) {
        if (m_rowRects[i].contains(event->pos())) {
            const int idx = m_scrollOffset + i;
            if (idx != m_selectedIndex) {
                m_selectedIndex = idx;
                update();
            }
            return;
        }
    }
}

void MenuWindow::wheelEvent(QWheelEvent *event) {
    const int delta = event->angleDelta().y();
    if (delta == 0 || m_visibleItems.size() <= 10)
        return;
    m_scrollOffset = std::clamp(m_scrollOffset + (delta < 0 ? 1 : -1),
                                0,
                                std::max(0, static_cast<int>(m_visibleItems.size()) - 10));
    update();
}

void MenuWindow::paintEvent(QPaintEvent *) {
    if (!m_opened)
        return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto &pal = (m_activeRoute == QLatin1String("apps") && m_items.value(m_activeRoute).provider == QLatin1String("apps"))
                          ? m_theme->launcher()
                          : m_theme->menu();

    p.fillRect(rect(), pal.scrim);

    const QRect card = cardRect();
    const BorderSpec border = m_theme->surfaceBorderSpec(
        QStringLiteral("menu"), QStringLiteral("border"),
        pal.border, std::max(1, m_theme->space(2)));

    BorderPainter::paintCard(p, card, m_theme->cornerRadius(), pal.background, border);

    const int pad = m_theme->space(10);
    const QRect inner = card.adjusted(border.widths.left + pad, border.widths.top + pad,
                                      -(border.widths.right + pad), -(border.widths.bottom + pad));

    // Search / Prompt bar
    const int headerH = m_theme->space(38);
    const QRect searchRect(inner.left(), inner.top(), inner.width(), headerH);
    QFont searchFont(m_theme->menuFontFamily());
    searchFont.setPixelSize(m_theme->fontSubtitle());
    p.setFont(searchFont);

    const QString routeTitle = !m_dmenuPrompt.isEmpty()
                                   ? m_dmenuPrompt
                                   : (m_activeRoute == QLatin1String("root")
                                          ? QStringLiteral("Search...")
                                          : m_items.value(m_activeRoute).label);
    const QString searchDisplay = m_filterText.isEmpty() ? routeTitle : m_filterText;
    p.setPen(m_filterText.isEmpty() ? ThemeManager::withAlpha(pal.text, 0.55) : pal.text);
    p.drawText(searchRect.adjusted(m_theme->space(8), 0, -m_theme->space(8), 0),
               Qt::AlignLeft | Qt::AlignVCenter,
               searchDisplay);

    // Subtle divider under search
    p.fillRect(QRect(inner.left(), searchRect.bottom() + 1, inner.width(), 1),
               ThemeManager::withAlpha(pal.text, 0.14));

    m_rowRects.clear();
    const int rowH = m_theme->space(38);
    int y = searchRect.bottom() + m_theme->space(6);

    QFont rowFont(m_theme->menuFontFamily());
    rowFont.setPixelSize(m_theme->fontBody());
    const QFontMetrics rowFm(rowFont);

    const int maxRows = 10;
    const int end = std::min<int>(m_visibleItems.size(), m_scrollOffset + maxRows);

    if (m_visibleItems.isEmpty()) {
        p.setFont(rowFont);
        p.setPen(ThemeManager::withAlpha(pal.text, 0.5));
        p.drawText(QRect(inner.left(), y, inner.width(), rowH), Qt::AlignCenter, QStringLiteral("No matches"));
        return;
    }

    for (int i = m_scrollOffset; i < end; ++i) {
        const MenuItem &it = m_visibleItems[i];
        const QRect r(inner.left(), y, inner.width(), rowH);
        m_rowRects.push_back(r);

        const bool selected = (i == m_selectedIndex);
        if (selected) {
            BorderSpec selBorder;
            selBorder.color = pal.selectedBorder;
            selBorder.widths = pal.selectedBorder.alpha() > 0 ? BorderWidths{1, 1, 1, 1} : BorderWidths{};
            BorderPainter::paintCard(p, r, std::min(6, m_theme->cornerRadius()), pal.selectedBackground, selBorder);
        }

        const int iconBox = m_theme->space(26);
        const QRect iconRect(r.left() + m_theme->space(8), r.top() + (rowH - iconBox) / 2, iconBox, iconBox);
        if (!it.appIcon.isNull()) {
            it.appIcon.paint(&p, iconRect);
        } else if (!it.icon.isEmpty()) {
            p.setFont(rowFont);
            p.setPen(selected ? pal.selectedText : pal.text);
            p.drawText(iconRect, Qt::AlignCenter, it.icon);
        }

        const int textX = iconRect.right() + m_theme->space(8);
        const int arrowW = (it.hasChildren && it.action.isEmpty()) ? m_theme->space(20) : 0;
        const QRect labelRect(textX, r.top(), r.right() - textX - arrowW - m_theme->space(6), rowH);
        p.setFont(rowFont);
        p.setPen(selected ? pal.selectedText : pal.text);
        p.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter,
                   rowFm.elidedText(it.label, Qt::ElideRight, labelRect.width()));

        if (arrowW > 0) {
            p.setPen(ThemeManager::withAlpha(selected ? pal.selectedText : pal.text, 0.65));
            p.drawText(QRect(r.right() - arrowW - m_theme->space(4), r.top(), arrowW, rowH),
                       Qt::AlignCenter, QStringLiteral("›"));
        }

        y += rowH;
    }
}

} // namespace quattro
