#include "MonitorNetworkPanels.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>

namespace quattro {

// ---------------- MonitorPanel ----------------

MonitorPanel::MonitorPanel(HyprlandClient *hypr,
                           BrightnessService *brightness,
                           ThemeManager *theme,
                           ShellConfig *config,
                           QWidget *parent)
    : ShellPopupWindow(theme, config, QStringLiteral("quattro-monitor-panel"), false, parent)
    , m_hypr(hypr)
    , m_brightness(brightness)
{}

void MonitorPanel::onOpening() {
    m_scrollOffset = 0;
    fetchMonitorsJson();

    auto *bp = new QProcess(this);
    connect(bp, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, bp](int code, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(bp->readAllStandardOutput()).trimmed();
        bp->deleteLater();
        if (code == 0 && !out.isEmpty()) {
            const int cur = out.toInt();
            auto *mp = new QProcess(this);
            connect(mp, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, mp, cur](int mc, QProcess::ExitStatus) {
                const QString mOut = QString::fromUtf8(mp->readAllStandardOutput()).trimmed();
                mp->deleteLater();
                const int mx = (mc == 0 && mOut.toInt() > 0) ? mOut.toInt() : 100;
                m_brightnessPct = std::clamp(qRound(cur * 100.0 / mx), 1, 100);
                if (isOpened()) update();
            });
            mp->start(QStringLiteral("brightnessctl"), {QStringLiteral("m")});
        }
    });
    bp->start(QStringLiteral("brightnessctl"), {QStringLiteral("g")});
}

void MonitorPanel::fetchMonitorsJson() {
    auto *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus) {
        const QByteArray raw = proc->readAllStandardOutput();
        proc->deleteLater();
        if (exitCode != 0) return;

        const QJsonArray arr = QJsonDocument::fromJson(raw).array();
        m_monitors.clear();
        for (const QJsonValue &v : arr) {
            const QJsonObject o = v.toObject();
            MonitorDetailedInfo info;
            info.name = o.value(QStringLiteral("name")).toString();
            info.description = o.value(QStringLiteral("description")).toString();
            info.width = o.value(QStringLiteral("width")).toInt(1920);
            info.height = o.value(QStringLiteral("height")).toInt(1080);
            info.refreshRate = o.value(QStringLiteral("refreshRate")).toDouble(60.0);
            info.x = o.value(QStringLiteral("x")).toInt(0);
            info.y = o.value(QStringLiteral("y")).toInt(0);
            info.scale = o.value(QStringLiteral("scale")).toDouble(1.0);
            info.transform = o.value(QStringLiteral("transform")).toInt(0);
            info.focused = o.value(QStringLiteral("focused")).toBool(false);
            const QJsonArray modes = o.value(QStringLiteral("availableModes")).toArray();
            for (const QJsonValue &mv : modes) {
                const QString ms = mv.toString();
                if (!ms.isEmpty() && !info.availableModes.contains(ms)) {
                    info.availableModes.push_back(ms);
                    if (info.availableModes.size() >= 6) break;
                }
            }
            m_monitors.push_back(info);
        }
        m_selectedIdx = std::clamp(m_selectedIdx, 0, std::max(0, static_cast<int>(m_monitors.size()) - 1));
        for (int i = 0; i < m_monitors.size(); ++i) {
            if (m_monitors.at(i).focused) { m_selectedIdx = i; break; }
        }
        if (isOpened()) update();
    });
    proc->start(QStringLiteral("hyprctl"), {QStringLiteral("-j"), QStringLiteral("monitors")});
}

void MonitorPanel::applyMonitorKeyword(const MonitorDetailedInfo &m) {
    const QString mode = QStringLiteral("%1x%2@%3").arg(m.width).arg(m.height).arg(qRound(m.refreshRate));
    const QString arg = QStringLiteral("%1,%2,%3x%4,%5,transform,%6")
                            .arg(m.name)
                            .arg(mode)
                            .arg(m.x)
                            .arg(m.y)
                            .arg(m.scale, 0, 'f', 2)
                            .arg(m.transform);
    m_hypr->dispatch(QStringLiteral("keyword monitor %1").arg(arg));
}

void MonitorPanel::paintCardContent(QPainter &p, const QRect &card) {
    const SurfaceColors &sc = theme()->popups();
    const QColor muted = theme()->muted();
    const QColor accent = sc.accent.isValid() ? sc.accent : theme()->accent();
    const int pad = 16;
    const int innerW = card.width() - pad * 2;
    int y = card.top() + pad;

    UiPaint::drawHero(p, QRect(card.left() + pad, y, innerW, 44), theme(),
                      QStringLiteral("󰍹"), QStringLiteral("Displays"),
                      m_monitors.isEmpty() ? QStringLiteral("No monitors detected")
                                           : QStringLiteral("%1 display(s) connected").arg(m_monitors.size()),
                      sc.text, muted, accent);
    y += 52;

    // Visual layout map
    m_mapRect = QRect(card.left() + pad, y, innerW, 130);
    p.save();
    p.setBrush(ThemeManager::withAlpha(sc.text, 0.05));
    p.setPen(QPen(ThemeManager::withAlpha(sc.text, 0.14), 1));
    p.drawRoundedRect(m_mapRect, 10, 10);

    if (!m_monitors.isEmpty()) {
        int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;
        for (const auto &m : m_monitors) {
            minX = std::min(minX, m.x);
            minY = std::min(minY, m.y);
            maxX = std::max(maxX, m.x + qRound(m.width / std::max(0.5, m.scale)));
            maxY = std::max(maxY, m.y + qRound(m.height / std::max(0.5, m.scale)));
        }
        const int spanW = std::max(800, maxX - minX);
        const int spanH = std::max(600, maxY - minY);
        m_mapScale = std::min((m_mapRect.width() - 36.0) / spanW, (m_mapRect.height() - 28.0) / spanH);
        const int offX = m_mapRect.center().x() - qRound((minX + spanW * 0.5) * m_mapScale);
        const int offY = m_mapRect.center().y() - qRound((minY + spanH * 0.5) * m_mapScale);

        for (int i = 0; i < m_monitors.size(); ++i) {
            const auto &m = m_monitors.at(i);
            const QRect mr(offX + qRound(m.x * m_mapScale),
                           offY + qRound(m.y * m_mapScale),
                           std::max(36, qRound((m.width / std::max(0.5, m.scale)) * m_mapScale)),
                           std::max(24, qRound((m.height / std::max(0.5, m.scale)) * m_mapScale)));
            const bool sel = (i == m_selectedIdx);
            p.setBrush(sel ? ThemeManager::withAlpha(accent, 0.30) : ThemeManager::withAlpha(sc.text, 0.12));
            p.setPen(QPen(sel ? accent : ThemeManager::withAlpha(sc.text, 0.3), sel ? 2 : 1));
            p.drawRoundedRect(mr, 6, 6);
            p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontCaption(), QFont::DemiBold));
            p.setPen(sc.text);
            p.drawText(mr, Qt::AlignCenter, m.name);
        }
    }
    p.restore();
    y += 140;

    // Monitor tabs
    m_tabRects.clear();
    if (!m_monitors.isEmpty()) {
        const int tw = (innerW - (m_monitors.size() - 1) * 6) / m_monitors.size();
        for (int i = 0; i < m_monitors.size(); ++i) {
            const QRect tr(card.left() + pad + i * (tw + 6), y, tw, 28);
            m_tabRects.push_back({i, tr});
            UiPaint::drawPillButton(p, tr, theme(), m_monitors.at(i).name,
                                    i == m_selectedIdx, tr.contains(m_hoverPos));
        }
        y += 38;

        const auto &cur = m_monitors.at(m_selectedIdx);
        UiPaint::drawSectionHeader(p, QRect(card.left() + pad, y, innerW, 18), theme(), QStringLiteral("Scale"), muted);
        y += 22;
        m_scaleRects.clear();
        const QList<double> scales = {1.0, 1.25, 1.5, 2.0};
        const int sw = (innerW - 18) / 4;
        for (int i = 0; i < scales.size(); ++i) {
            const QRect sr(card.left() + pad + i * (sw + 6), y, sw, 28);
            m_scaleRects.push_back({scales.at(i), sr});
            UiPaint::drawPillButton(p, sr, theme(), QStringLiteral("%1x").arg(scales.at(i), 0, 'g', 3),
                                    std::abs(cur.scale - scales.at(i)) < 0.05, sr.contains(m_hoverPos));
        }
        y += 36;

        UiPaint::drawSectionHeader(p, QRect(card.left() + pad, y, innerW, 18), theme(), QStringLiteral("Resolution / Refresh"), muted);
        y += 22;
        m_modeRects.clear();
        const int cols = 2;
        const int mw = (innerW - 8) / cols;
        const int shownModes = std::min(4, static_cast<int>(cur.availableModes.size()));
        for (int i = 0; i < shownModes; ++i) {
            const QRect mr(card.left() + pad + (i % cols) * (mw + 8), y + (i / cols) * 32, mw, 28);
            m_modeRects.push_back({cur.availableModes.at(i), mr});
            const QString curMode = QStringLiteral("%1x%2@%3").arg(cur.width).arg(cur.height).arg(cur.refreshRate, 0, 'f', 2);
            UiPaint::drawPillButton(p, mr, theme(), cur.availableModes.at(i),
                                    cur.availableModes.at(i).startsWith(QStringLiteral("%1x%2@%3").arg(cur.width).arg(cur.height).arg(qRound(cur.refreshRate))),
                                    mr.contains(m_hoverPos));
            Q_UNUSED(curMode);
        }
        y += ((shownModes + 1) / 2) * 32 + 6;

        m_rotateRect = QRect(card.left() + pad, y, innerW, 28);
        UiPaint::drawPillButton(p, m_rotateRect, theme(),
                                QStringLiteral("Rotate 90° (Transform: %1)").arg(cur.transform),
                                false, m_rotateRect.contains(m_hoverPos));
        y += 38;
    }

    UiPaint::drawSectionHeader(p, QRect(card.left() + pad, y, innerW, 18), theme(), QStringLiteral("Backlight Brightness"), muted);
    y += 22;
    m_brightRect = QRect(card.left() + pad, y, innerW - 44, 24);
    UiPaint::drawSlider(p, m_brightRect, theme(), m_brightnessPct / 100.0, m_brightRect.contains(m_hoverPos) || m_draggingBright);
    p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontCaption()));
    p.setPen(sc.text);
    p.drawText(QRect(m_brightRect.right() + 6, y, 38, 24), Qt::AlignVCenter | Qt::AlignRight,
               QStringLiteral("%1%").arg(m_brightnessPct));
}

void MonitorPanel::handleCardMousePress(QMouseEvent *event, const QRect & /*card*/) {
    const QPoint pos = event->pos();
    if (m_brightRect.contains(pos)) {
        m_draggingBright = true;
        const double f = std::clamp((pos.x() - m_brightRect.left()) / double(std::max(1, m_brightRect.width())), 0.02, 1.0);
        m_brightnessPct = qRound(f * 100.0);
        QProcess::startDetached(QStringLiteral("brightnessctl"), {QStringLiteral("set"), QStringLiteral("%1%").arg(m_brightnessPct)});
        update();
        return;
    }
    if (m_mapRect.contains(pos) && !m_monitors.isEmpty()) {
        m_draggingLayout = true;
        m_dragStartMouse = pos;
        m_dragOrigPos = QPoint(m_monitors.at(m_selectedIdx).x, m_monitors.at(m_selectedIdx).y);
        return;
    }
    for (const auto &tr : m_tabRects) {
        if (tr.second.contains(pos)) { m_selectedIdx = tr.first; update(); return; }
    }
    if (m_selectedIdx < m_monitors.size()) {
        for (const auto &sr : m_scaleRects) {
            if (sr.second.contains(pos)) {
                m_monitors[m_selectedIdx].scale = sr.first;
                applyMonitorKeyword(m_monitors.at(m_selectedIdx));
                update();
                return;
            }
        }
        for (const auto &mr : m_modeRects) {
            if (mr.second.contains(pos)) {
                const QString mStr = mr.first;
                const int atIdx = mStr.indexOf(QLatin1Char('@'));
                const QString res = atIdx > 0 ? mStr.left(atIdx) : mStr;
                const QStringList wh = res.split(QLatin1Char('x'));
                if (wh.size() == 2) {
                    m_monitors[m_selectedIdx].width = wh[0].toInt();
                    m_monitors[m_selectedIdx].height = wh[1].toInt();
                }
                if (atIdx > 0) {
                    QString hz = mStr.mid(atIdx + 1);
                    hz.remove(QStringLiteral("Hz"));
                    m_monitors[m_selectedIdx].refreshRate = hz.toDouble();
                }
                applyMonitorKeyword(m_monitors.at(m_selectedIdx));
                update();
                return;
            }
        }
        if (m_rotateRect.contains(pos)) {
            m_monitors[m_selectedIdx].transform = (m_monitors[m_selectedIdx].transform + 1) % 4;
            applyMonitorKeyword(m_monitors.at(m_selectedIdx));
            update();
            return;
        }
    }
}

void MonitorPanel::handleCardMouseMove(QMouseEvent *event, const QRect &card) {
    ShellPopupWindow::handleCardMouseMove(event, card);
    if (m_draggingBright) {
        const double f = std::clamp((event->pos().x() - m_brightRect.left()) / double(std::max(1, m_brightRect.width())), 0.02, 1.0);
        m_brightnessPct = qRound(f * 100.0);
        QProcess::startDetached(QStringLiteral("brightnessctl"), {QStringLiteral("set"), QStringLiteral("%1%").arg(m_brightnessPct)});
        update();
    } else if (m_draggingLayout && m_selectedIdx < m_monitors.size() && m_mapScale > 0.001) {
        const QPoint delta = event->pos() - m_dragStartMouse;
        m_monitors[m_selectedIdx].x = m_dragOrigPos.x() + qRound(delta.x() / m_mapScale);
        m_monitors[m_selectedIdx].y = m_dragOrigPos.y() + qRound(delta.y() / m_mapScale);
        update();
    }
}

void MonitorPanel::handleCardMouseRelease(QMouseEvent *, const QRect &) {
    if (m_draggingLayout && m_selectedIdx < m_monitors.size())
        applyMonitorKeyword(m_monitors.at(m_selectedIdx));
    m_draggingLayout = false;
    m_draggingBright = false;
}

// ---------------- NetworkPanel ----------------

NetworkPanel::NetworkPanel(ThemeManager *theme, ShellConfig *config, QWidget *parent)
    : ShellPopupWindow(theme, config, QStringLiteral("quattro-network-panel"), false, parent)
{}

void NetworkPanel::onOpening() {
    m_scrollOffset = 0;
    m_promptSsid.clear();
    m_passwordInput.clear();
    refreshNetwork();
}

void NetworkPanel::refreshNetwork() {
    auto *pr = new QProcess(this);
    connect(pr, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, pr](int, QProcess::ExitStatus) {
        const QString rOut = QString::fromUtf8(pr->readAllStandardOutput()).trimmed().toLower();
        pr->deleteLater();
        m_wifiEnabled = (rOut == QLatin1String("enabled"));

        auto *pl = new QProcess(this);
        connect(pl, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, pl](int, QProcess::ExitStatus) {
            const QString out = QString::fromUtf8(pl->readAllStandardOutput());
            pl->deleteLater();
            m_networks.clear();
            m_activeSsid.clear();
            QStringList seen;
            for (const QString &line : out.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
                const QStringList parts = line.split(QLatin1Char(':'));
                if (parts.size() < 4) continue;
                const bool inUse = (parts[0] == QLatin1String("*"));
                const int sig = parts[parts.size() - 1].toInt();
                const QString sec = parts[parts.size() - 2];
                const QString ssid = parts.mid(1, parts.size() - 3).join(QLatin1Char(':')).trimmed();
                if (ssid.isEmpty() || seen.contains(ssid)) continue;
                seen.push_back(ssid);
                if (inUse) m_activeSsid = ssid;
                m_networks.push_back({inUse, ssid, sec, sig});
            }
            if (isOpened()) update();
        });
        pl->start(QStringLiteral("nmcli"), {QStringLiteral("-t"), QStringLiteral("-f"),
                                            QStringLiteral("IN-USE,SSID,SECURITY,SIGNAL"), QStringLiteral("dev"), QStringLiteral("wifi"), QStringLiteral("list")});
    });
    pr->start(QStringLiteral("nmcli"), {QStringLiteral("radio"), QStringLiteral("wifi")});

    auto *pip = new QProcess(this);
    connect(pip, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, pip](int, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(pip->readAllStandardOutput());
        pip->deleteLater();
        m_ipAddress.clear();
        m_dnsServer.clear();
        for (const QString &l : out.split(QLatin1Char('\n'))) {
            if (l.startsWith(QStringLiteral("IP4.ADDRESS")) && m_ipAddress.isEmpty())
                m_ipAddress = l.section(QLatin1Char(':'), 1).trimmed().section(QLatin1Char('/'), 0, 0);
            else if (l.startsWith(QStringLiteral("IP4.DNS")) && m_dnsServer.isEmpty())
                m_dnsServer = l.section(QLatin1Char(':'), 1).trimmed();
        }
        if (isOpened()) update();
    });
    pip->start(QStringLiteral("nmcli"), {QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("IP4.ADDRESS,IP4.DNS"), QStringLiteral("dev"), QStringLiteral("show")});
}

void NetworkPanel::toggleWifiRadio() {
    auto *p = new QProcess(this);
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, p](int, QProcess::ExitStatus) {
        p->deleteLater();
        refreshNetwork();
    });
    p->start(QStringLiteral("nmcli"), {QStringLiteral("radio"), QStringLiteral("wifi"), m_wifiEnabled ? QStringLiteral("off") : QStringLiteral("on")});
}

void NetworkPanel::connectToSsid(const QString &ssid, const QString &password) {
    m_statusText = QStringLiteral("Connecting to %1...").arg(ssid);
    update();
    auto *p = new QProcess(this);
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, p](int code, QProcess::ExitStatus) {
        const QString err = QString::fromUtf8(p->readAllStandardError()).trimmed();
        p->deleteLater();
        m_statusText = (code == 0) ? QString() : (err.isEmpty() ? QStringLiteral("Connection failed") : err);
        m_promptSsid.clear();
        m_passwordInput.clear();
        refreshNetwork();
    });
    QStringList args = {QStringLiteral("dev"), QStringLiteral("wifi"), QStringLiteral("connect"), ssid};
    if (!password.isEmpty())
        args << QStringLiteral("password") << password;
    p->start(QStringLiteral("nmcli"), args);
}

void NetworkPanel::paintCardContent(QPainter &p, const QRect &card) {
    const SurfaceColors &sc = theme()->popups();
    const QColor muted = theme()->muted();
    const QColor accent = sc.accent.isValid() ? sc.accent : theme()->accent();
    const int pad = 16;
    const int innerW = card.width() - pad * 2;

    const QString sub = m_activeSsid.isEmpty()
                            ? (m_wifiEnabled ? QStringLiteral("Disconnected") : QStringLiteral("Wi-Fi Off"))
                            : QStringLiteral("%1%2").arg(m_activeSsid, m_ipAddress.isEmpty() ? QString() : QStringLiteral(" • ") + m_ipAddress);
    const QRect hero(card.left() + pad, card.top() + pad, innerW - 50, 44);
    UiPaint::drawHero(p, hero, theme(),
                      m_wifiEnabled ? QStringLiteral("󰤨") : QStringLiteral("󰤭"),
                      QStringLiteral("Network"), sub, sc.text, muted, accent);
    m_wifiToggleRect = QRect(card.right() - pad - 42, card.top() + pad + 11, 42, 22);
    UiPaint::drawToggleSwitch(p, m_wifiToggleRect, theme(), m_wifiEnabled, m_wifiToggleRect.contains(m_hoverPos));

    const int btnW = (innerW - 12) / 3;
    const int actY = hero.bottom() + 10;
    m_rescanRect = QRect(card.left() + pad, actY, btnW, 28);
    m_qrRect = QRect(m_rescanRect.right() + 6, actY, btnW, 28);
    m_speedRect = QRect(m_qrRect.right() + 6, actY, btnW, 28);
    UiPaint::drawPillButton(p, m_rescanRect, theme(), m_scanning ? QStringLiteral("Scanning...") : QStringLiteral("Rescan"),
                            m_scanning, m_rescanRect.contains(m_hoverPos));
    UiPaint::drawPillButton(p, m_qrRect, theme(), QStringLiteral("Wi-Fi QR"), false, m_qrRect.contains(m_hoverPos));
    UiPaint::drawPillButton(p, m_speedRect, theme(), QStringLiteral("Speed Test"), false, m_speedRect.contains(m_hoverPos));

    int listTop = m_rescanRect.bottom() + 10;
    if (!m_promptSsid.isEmpty()) {
        const QRect pBox(card.left() + pad, listTop, innerW, 96);
        p.setBrush(ThemeManager::withAlpha(sc.text, 0.06));
        p.setPen(QPen(accent, 1));
        p.drawRoundedRect(pBox, 8, 8);
        p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBodySmall(), QFont::DemiBold));
        p.setPen(sc.text);
        p.drawText(pBox.adjusted(10, 6, -10, -68), Qt::AlignVCenter | Qt::AlignLeft,
                   QStringLiteral("Password for %1").arg(m_promptSsid));
        p.setBrush(ThemeManager::withAlpha(sc.text, 0.1));
        p.setPen(Qt::NoPen);
        const QRect inp(pBox.left() + 10, pBox.top() + 32, pBox.width() - 20, 28);
        p.drawRoundedRect(inp, 6, 6);
        p.setPen(sc.text);
        p.drawText(inp.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft,
                   m_passwordInput.isEmpty() ? QStringLiteral("Type password...") : QString(m_passwordInput.size(), QChar(0x2022)));
        m_passwordCancelRect = QRect(pBox.left() + 10, pBox.bottom() - 30, 86, 24);
        m_passwordConnectRect = QRect(pBox.right() - 96, pBox.bottom() - 30, 86, 24);
        UiPaint::drawPillButton(p, m_passwordCancelRect, theme(), QStringLiteral("Cancel"), false, m_passwordCancelRect.contains(m_hoverPos));
        UiPaint::drawPillButton(p, m_passwordConnectRect, theme(), QStringLiteral("Join"), true, m_passwordConnectRect.contains(m_hoverPos));
        listTop += 104;
    }

    m_listRect = QRect(card.left() + pad, listTop, innerW, card.bottom() - pad - (m_statusText.isEmpty() ? 0 : 20) - listTop);
    m_netRows.clear();
    p.save();
    p.setClipRect(m_listRect);
    const int rowH = 42;
    m_maxScroll = std::max(0, static_cast<int>(m_networks.size()) * rowH - m_listRect.height());
    m_scrollOffset = std::clamp(m_scrollOffset, 0, m_maxScroll);

    for (int i = 0; i < m_networks.size(); ++i) {
        const int y = m_listRect.top() + i * rowH - m_scrollOffset;
        if (y + rowH < m_listRect.top() || y > m_listRect.bottom()) continue;
        const QRect r(m_listRect.left(), y, m_listRect.width(), rowH - 4);
        m_netRows.push_back({i, r});
        const auto &n = m_networks.at(i);
        if (n.inUse) {
            p.setBrush(ThemeManager::withAlpha(accent, 0.18));
            p.setPen(QPen(accent, 1));
            p.drawRoundedRect(r, 8, 8);
        } else if (r.contains(m_hoverPos)) {
            p.setBrush(ThemeManager::withAlpha(sc.text, 0.08));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(r, 8, 8);
        }
        p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBody(), n.inUse ? QFont::DemiBold : QFont::Normal));
        p.setPen(n.inUse ? accent : sc.text);
        p.drawText(r.adjusted(10, 4, -60, -16), Qt::AlignVCenter | Qt::AlignLeft,
                   p.fontMetrics().elidedText(n.ssid, Qt::ElideRight, r.width() - 70));
        p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontCaption()));
        p.setPen(muted);
        p.drawText(r.adjusted(10, 20, -60, -2), Qt::AlignVCenter | Qt::AlignLeft,
                   n.security.isEmpty() ? QStringLiteral("Open") : n.security);
        p.drawText(r.adjusted(0, 0, -10, 0), Qt::AlignVCenter | Qt::AlignRight, QStringLiteral("%1%").arg(n.signal));
    }
    p.restore();

    if (!m_statusText.isEmpty()) {
        p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontCaption()));
        p.setPen(muted);
        p.drawText(QRect(card.left() + pad, card.bottom() - pad - 18, innerW, 18),
                   Qt::AlignLeft | Qt::AlignVCenter, m_statusText);
    }
}

bool NetworkPanel::handleKey(QKeyEvent *event) {
    if (m_promptSsid.isEmpty())
        return false;
    const int k = event->key();
    if (k == Qt::Key_Return || k == Qt::Key_Enter) {
        connectToSsid(m_promptSsid, m_passwordInput);
        return true;
    }
    if (k == Qt::Key_Escape) {
        m_promptSsid.clear();
        m_passwordInput.clear();
        update();
        return true;
    }
    if (k == Qt::Key_Backspace) {
        if (!m_passwordInput.isEmpty()) { m_passwordInput.chop(1); update(); }
        return true;
    }
    const QString t = event->text();
    if (!t.isEmpty() && t.at(0).isPrint()) {
        m_passwordInput += t;
        update();
        return true;
    }
    return false;
}

void NetworkPanel::handleCardMousePress(QMouseEvent *event, const QRect & /*card*/) {
    const QPoint pos = event->pos();
    if (m_wifiToggleRect.contains(pos)) { toggleWifiRadio(); return; }
    if (m_rescanRect.contains(pos)) {
        m_scanning = true;
        update();
        auto *p = new QProcess(this);
        connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, p](int, QProcess::ExitStatus) {
            p->deleteLater();
            m_scanning = false;
            refreshNetwork();
        });
        p->start(QStringLiteral("nmcli"), {QStringLiteral("dev"), QStringLiteral("wifi"), QStringLiteral("rescan")});
        return;
    }
    if (m_qrRect.contains(pos)) { closePopup(); emit openWifiQrRequested(); return; }
    if (m_speedRect.contains(pos)) { closePopup(); emit openSpeedTestRequested(); return; }
    if (!m_promptSsid.isEmpty()) {
        if (m_passwordCancelRect.contains(pos)) { m_promptSsid.clear(); m_passwordInput.clear(); update(); return; }
        if (m_passwordConnectRect.contains(pos)) { connectToSsid(m_promptSsid, m_passwordInput); return; }
    }
    for (const auto &nr : m_netRows) {
        if (nr.second.contains(pos) && nr.first < m_networks.size()) {
            const auto &net = m_networks.at(nr.first);
            if (!net.inUse) {
                if (net.security.isEmpty() || net.security == QLatin1String("--"))
                    connectToSsid(net.ssid);
                else {
                    // Try saved connection first; if it needs password user can re-click or type
                    m_promptSsid = net.ssid;
                    m_passwordInput.clear();
                    update();
                }
            }
            return;
        }
    }
}

} // namespace quattro
