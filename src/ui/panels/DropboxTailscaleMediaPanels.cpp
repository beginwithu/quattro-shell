#include "DropboxTailscaleMediaPanels.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QUrl>
#include <algorithm>

namespace quattro {

// ---------------- DropboxPanel ----------------

DropboxPanel::DropboxPanel(ThemeManager *theme, ShellConfig *config, QWidget *parent)
    : ShellPopupWindow(theme, config, QStringLiteral("quattro-dropbox-panel"), false, parent)
{
    refreshDropbox();
}

void DropboxPanel::onOpening() {
    m_scrollOffset = 0;
    refreshDropbox();
}

void DropboxPanel::refreshDropbox() {
    // Check ~/.dropbox/info.json + ~/.dropbox/dropbox.pid
    m_dropboxDir = QDir::homePath() + QStringLiteral("/Dropbox");
    QFile infoF(QDir::homePath() + QStringLiteral("/.dropbox/info.json"));
    if (infoF.open(QIODevice::ReadOnly)) {
        const QJsonObject obj = QJsonDocument::fromJson(infoF.readAll()).object();
        const QJsonObject pers = obj.value(QStringLiteral("personal")).toObject();
        if (!pers.value(QStringLiteral("path")).toString().isEmpty())
            m_dropboxDir = pers.value(QStringLiteral("path")).toString();
    }

    QFile pidF(QDir::homePath() + QStringLiteral("/.dropbox/dropbox.pid"));
    m_running = false;
    if (pidF.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString pid = QString::fromUtf8(pidF.readAll()).trimmed();
        if (!pid.isEmpty() && QFileInfo::exists(QStringLiteral("/proc/%1").arg(pid)))
            m_running = true;
    }

    auto *p = new QProcess(this);
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, p](int code, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(p->readAllStandardOutput()).trimmed();
        p->deleteLater();
        if (code == 0 && !out.isEmpty()) {
            m_statusLine = out.split(QLatin1Char('\n')).first().trimmed();
            m_running = !m_statusLine.contains(QStringLiteral("isn't running"), Qt::CaseInsensitive);
        } else {
            m_statusLine = m_running ? QStringLiteral("Running") : QStringLiteral("Dropbox not running");
        }
        if (isOpened()) update();
    });
    p->start(QStringLiteral("dropbox-cli"), {QStringLiteral("status")});

    // Gather up to 15 most recently modified files in Dropbox directory
    m_recentFiles.clear();
    QDir dir(m_dropboxDir);
    if (dir.exists()) {
        const QFileInfoList list = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
        for (int i = 0; i < std::min(15, static_cast<int>(list.size())); ++i)
            m_recentFiles.push_back(list.at(i).fileName());
    }
}

void DropboxPanel::paintCardContent(QPainter &p, const QRect &card) {
    const SurfaceColors &sc = theme()->popups();
    const QColor muted = theme()->muted();
    const QColor accent = sc.accent.isValid() ? sc.accent : theme()->accent();
    const int pad = 16;
    const int innerW = card.width() - pad * 2;

    const QRect hero(card.left() + pad, card.top() + pad, innerW, 46);
    UiPaint::drawHero(p, hero, theme(), QStringLiteral("󰇣"),
                      QStringLiteral("Dropbox"), m_statusLine, sc.text, muted, accent);

    const int bw = (innerW - 8) / 2;
    m_toggleDaemonRect = QRect(card.left() + pad, hero.bottom() + 10, bw, 30);
    m_openFolderRect = QRect(m_toggleDaemonRect.right() + 8, hero.bottom() + 10, bw, 30);
    UiPaint::drawPillButton(p, m_toggleDaemonRect, theme(),
                            m_running ? QStringLiteral("Stop Daemon") : QStringLiteral("Start Dropbox"),
                            m_running, m_toggleDaemonRect.contains(m_hoverPos));
    UiPaint::drawPillButton(p, m_openFolderRect, theme(),
                            QStringLiteral("Open Folder"), false, m_openFolderRect.contains(m_hoverPos));

    const int hdrY = m_toggleDaemonRect.bottom() + 14;
    UiPaint::drawSectionHeader(p, QRect(card.left() + pad, hdrY, innerW, 20), theme(),
                               QStringLiteral("Recent Items"), muted);

    const QRect listR(card.left() + pad, hdrY + 24, innerW, card.bottom() - pad - (hdrY + 24));
    p.save();
    p.setClipRect(listR);
    if (m_recentFiles.isEmpty()) {
        p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBodySmall()));
        p.setPen(muted);
        p.drawText(listR, Qt::AlignCenter, QStringLiteral("No files found in %1").arg(m_dropboxDir));
    } else {
        for (int i = 0; i < m_recentFiles.size(); ++i) {
            const int y = listR.top() + i * 28 - m_scrollOffset;
            if (y + 28 < listR.top() || y > listR.bottom()) continue;
            p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBodySmall()));
            p.setPen(sc.text);
            p.drawText(QRect(listR.left() + 4, y, listR.width() - 8, 26), Qt::AlignVCenter | Qt::AlignLeft,
                       p.fontMetrics().elidedText(QStringLiteral("󰈙  ") + m_recentFiles.at(i), Qt::ElideRight, listR.width() - 8));
        }
    }
    p.restore();
}

void DropboxPanel::handleCardMousePress(QMouseEvent *event, const QRect & /*card*/) {
    if (m_toggleDaemonRect.contains(event->pos())) {
        QProcess::startDetached(QStringLiteral("dropbox-cli"), {m_running ? QStringLiteral("stop") : QStringLiteral("start")});
        refreshDropbox();
        update();
    } else if (m_openFolderRect.contains(event->pos())) {
        closePopup();
        QProcess::startDetached(QStringLiteral("xdg-open"), {m_dropboxDir});
    }
}

// ---------------- TailscalePanel ----------------

TailscalePanel::TailscalePanel(ThemeManager *theme, ShellConfig *config, QWidget *parent)
    : ShellPopupWindow(theme, config, QStringLiteral("quattro-tailscale-panel"), false, parent)
{
    refreshTailscale();
}

void TailscalePanel::onOpening() {
    m_scrollOffset = 0;
    refreshTailscale();
}

void TailscalePanel::refreshTailscale() {
    auto *p = new QProcess(this);
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, p](int code, QProcess::ExitStatus) {
        const QByteArray raw = p->readAllStandardOutput();
        p->deleteLater();
        if (code != 0) return;

        const QJsonObject obj = QJsonDocument::fromJson(raw).object();
        m_backendState = obj.value(QStringLiteral("BackendState")).toString();
        m_up = (m_backendState == QLatin1String("Running"));

        const QJsonObject self = obj.value(QStringLiteral("Self")).toObject();
        m_selfName = self.value(QStringLiteral("HostName")).toString();
        const QJsonArray selfIps = self.value(QStringLiteral("TailscaleIPs")).toArray();
        m_selfIp = selfIps.isEmpty() ? QString() : selfIps.first().toString();

        m_peers.clear();
        m_activeExitNode.clear();
        const QJsonObject peers = obj.value(QStringLiteral("Peer")).toObject();
        for (auto it = peers.begin(); it != peers.end(); ++it) {
            const QJsonObject po = it.value().toObject();
            TailscalePeerItem item;
            item.hostName = po.value(QStringLiteral("HostName")).toString();
            const QJsonArray ips = po.value(QStringLiteral("TailscaleIPs")).toArray();
            item.ip = ips.isEmpty() ? QString() : ips.first().toString();
            item.os = po.value(QStringLiteral("OS")).toString();
            item.online = po.value(QStringLiteral("Online")).toBool(false);
            item.exitNode = po.value(QStringLiteral("ExitNode")).toBool(false);
            item.exitNodeOption = po.value(QStringLiteral("ExitNodeOption")).toBool(false);
            if (item.exitNode) m_activeExitNode = item.hostName;
            m_peers.push_back(item);
        }
        std::sort(m_peers.begin(), m_peers.end(), [](const auto &a, const auto &b) {
            if (a.online != b.online) return a.online > b.online;
            return a.hostName.compare(b.hostName, Qt::CaseInsensitive) < 0;
        });
        emit stateChanged();
        if (isOpened()) update();
    });
    p->start(QStringLiteral("tailscale"), {QStringLiteral("status"), QStringLiteral("--json")});
}

void TailscalePanel::paintCardContent(QPainter &p, const QRect &card) {
    const SurfaceColors &sc = theme()->popups();
    const QColor muted = theme()->muted();
    const QColor accent = sc.accent.isValid() ? sc.accent : theme()->accent();
    const int pad = 16;
    const int innerW = card.width() - pad * 2;

    const QRect hero(card.left() + pad, card.top() + pad, innerW - 50, 46);
    const QString sub = m_up ? (m_selfIp.isEmpty() ? m_backendState : m_selfName + QStringLiteral(" • ") + m_selfIp)
                             : (m_backendState.isEmpty() ? QStringLiteral("Stopped") : m_backendState);
    UiPaint::drawHero(p, hero, theme(), QStringLiteral("󰖂"), QStringLiteral("Tailscale"), sub, sc.text, muted, accent);
    m_upToggleRect = QRect(card.right() - pad - 42, card.top() + pad + 11, 42, 22);
    UiPaint::drawToggleSwitch(p, m_upToggleRect, theme(), m_up, m_upToggleRect.contains(m_hoverPos), m_busy);

    m_copySelfRect = QRect(card.left() + pad, hero.bottom() + 8, innerW, 28);
    UiPaint::drawPillButton(p, m_copySelfRect, theme(),
                            m_selfIp.isEmpty() ? QStringLiteral("Copy IP unavailable") : QStringLiteral("Copy Self IP: %1").arg(m_selfIp),
                            false, m_copySelfRect.contains(m_hoverPos));

    const int listTop = m_copySelfRect.bottom() + 12;
    const QRect listR(card.left() + pad, listTop, innerW, card.bottom() - pad - listTop);
    m_peerRows.clear();
    p.save();
    p.setClipRect(listR);

    const int rowH = 40;
    m_maxScroll = std::max(0, static_cast<int>(m_peers.size()) * rowH - listR.height());
    m_scrollOffset = std::clamp(m_scrollOffset, 0, m_maxScroll);

    for (int i = 0; i < m_peers.size(); ++i) {
        const int y = listR.top() + i * rowH - m_scrollOffset;
        if (y + rowH < listR.top() || y > listR.bottom()) continue;
        const QRect r(listR.left(), y, listR.width(), rowH - 4);
        m_peerRows.push_back({i, r});
        const auto &peer = m_peers.at(i);
        if (peer.exitNode) {
            p.setBrush(ThemeManager::withAlpha(accent, 0.18));
            p.setPen(QPen(accent, 1));
            p.drawRoundedRect(r, 8, 8);
        } else if (r.contains(m_hoverPos)) {
            p.setBrush(ThemeManager::withAlpha(sc.text, 0.08));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(r, 8, 8);
        }
        p.setBrush(peer.online ? QColor(QStringLiteral("#9ece6a")) : ThemeManager::withAlpha(muted, 0.5));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPoint(r.left() + 12, r.center().y()), 4, 4);

        p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBodySmall(), QFont::DemiBold));
        p.setPen(sc.text);
        p.drawText(r.adjusted(24, 3, -110, -16), Qt::AlignVCenter | Qt::AlignLeft, peer.hostName);
        p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontCaption()));
        p.setPen(muted);
        p.drawText(r.adjusted(24, 19, -110, -2), Qt::AlignVCenter | Qt::AlignLeft, peer.ip);
        p.drawText(r.adjusted(0, 0, -10, 0), Qt::AlignVCenter | Qt::AlignRight,
                   peer.exitNode ? QStringLiteral("Exit Node") : peer.os);
    }
    p.restore();
}

void TailscalePanel::handleCardMousePress(QMouseEvent *event, const QRect & /*card*/) {
    if (m_upToggleRect.contains(event->pos())) {
        m_busy = true;
        update();
        auto *p = new QProcess(this);
        connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, p](int, QProcess::ExitStatus) {
            p->deleteLater();
            m_busy = false;
            refreshTailscale();
        });
        p->start(QStringLiteral("tailscale"), {m_up ? QStringLiteral("down") : QStringLiteral("up")});
        return;
    }
    if (m_copySelfRect.contains(event->pos()) && !m_selfIp.isEmpty()) {
        QProcess::startDetached(QStringLiteral("wl-copy"), {m_selfIp});
        return;
    }
    for (const auto &pr : m_peerRows) {
        if (pr.second.contains(event->pos()) && pr.first < m_peers.size()) {
            const auto &peer = m_peers.at(pr.first);
            if (!peer.ip.isEmpty())
                QProcess::startDetached(QStringLiteral("wl-copy"), {peer.ip});
            return;
        }
    }
}

// ---------------- MediaPanel ----------------

MediaPanel::MediaPanel(MediaService *media,
                       ThemeManager *theme,
                       ShellConfig *config,
                       QWidget *parent)
    : ShellPopupWindow(theme, config, QStringLiteral("quattro-media-panel"), false, parent)
    , m_media(media)
{
    connect(m_media, &MediaService::mediaChanged, this, [this]() {
        if (isOpened()) update();
    });
}

void MediaPanel::onOpening() {
    m_scrollOffset = 0;
    m_media->refreshPlayers();
}

void MediaPanel::fetchArtIfNeeded(const QString &url) {
    if (url.isEmpty() || url == m_loadedArtUrl)
        return;
    m_loadedArtUrl = url;
    m_artPixmap = QPixmap();
    if (url.startsWith(QStringLiteral("file://"))) {
        m_artPixmap.load(QUrl(url).toLocalFile());
        return;
    }
    if (url.startsWith(QLatin1Char('/'))) {
        m_artPixmap.load(url);
        return;
    }
    if (url.startsWith(QStringLiteral("http"))) {
        QNetworkReply *reply = m_nam.get(QNetworkRequest(QUrl(url)));
        connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
            const QByteArray bytes = reply->readAll();
            reply->deleteLater();
            if (url == m_loadedArtUrl && !bytes.isEmpty()) {
                m_artPixmap.loadFromData(bytes);
                if (isOpened()) update();
            }
        });
    }
}

void MediaPanel::paintCardContent(QPainter &p, const QRect &card) {
    const SurfaceColors &sc = theme()->popups();
    const QColor muted = theme()->muted();
    const QColor accent = sc.accent.isValid() ? sc.accent : theme()->accent();
    const int pad = 16;
    const int innerW = card.width() - pad * 2;
    int y = card.top() + pad;

    const MprisPlayerInfo *cur = m_media->activePlayer();
    if (!cur) {
        UiPaint::drawHero(p, QRect(card.left() + pad, y, innerW, 46), theme(),
                          QStringLiteral("󰎆"), QStringLiteral("No Media Playing"),
                          QStringLiteral("Start playback in any MPRIS player"), sc.text, muted, accent);
        return;
    }

    fetchArtIfNeeded(cur->artUrl);

    // Art + track metadata
    const QRect artR(card.left() + pad, y, 76, 76);
    p.save();
    p.setBrush(ThemeManager::withAlpha(accent, 0.16));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(artR, 10, 10);
    if (!m_artPixmap.isNull()) {
        p.drawPixmap(artR, m_artPixmap.scaled(artR.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    } else {
        p.setFont(QFont(theme()->resolvedFontFamily(), 24));
        p.setPen(accent);
        p.drawText(artR, Qt::AlignCenter, QStringLiteral("󰎆"));
    }
    p.restore();

    const int tx = artR.right() + 12;
    const int tw = innerW - 88;
    p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontTitle(), QFont::DemiBold));
    p.setPen(sc.text);
    p.drawText(QRect(tx, y + 4, tw, 24), Qt::AlignVCenter | Qt::AlignLeft,
               p.fontMetrics().elidedText(cur->title.isEmpty() ? cur->identity : cur->title, Qt::ElideRight, tw));
    p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBodySmall()));
    p.setPen(muted);
    p.drawText(QRect(tx, y + 30, tw, 20), Qt::AlignVCenter | Qt::AlignLeft,
               p.fontMetrics().elidedText(cur->artist.isEmpty() ? cur->identity : cur->artist, Qt::ElideRight, tw));
    p.drawText(QRect(tx, y + 52, tw, 18), Qt::AlignVCenter | Qt::AlignLeft,
               p.fontMetrics().elidedText(cur->album, Qt::ElideRight, tw));
    y += 88;

    // Playback transport
    const int cw = (innerW - 16) / 3;
    m_prevRect = QRect(card.left() + pad, y, cw, 34);
    m_playPauseRect = QRect(m_prevRect.right() + 8, y, cw, 34);
    m_nextRect = QRect(m_playPauseRect.right() + 8, y, cw, 34);
    UiPaint::drawPillButton(p, m_prevRect, theme(), QStringLiteral("󰒮  Prev"), false, m_prevRect.contains(m_hoverPos));
    UiPaint::drawPillButton(p, m_playPauseRect, theme(), cur->isPlaying() ? QStringLiteral("󰏤  Pause") : QStringLiteral("󰐊  Play"),
                            true, m_playPauseRect.contains(m_hoverPos));
    UiPaint::drawPillButton(p, m_nextRect, theme(), QStringLiteral("Next  󰒭"), false, m_nextRect.contains(m_hoverPos));
    y += 48;

    // Sources list
    UiPaint::drawSectionHeader(p, QRect(card.left() + pad, y, innerW, 20), theme(), QStringLiteral("Media Sources"), muted);
    y += 24;
    m_sourceRows.clear();
    const auto sources = m_media->sourcePlayers();
    for (const auto &sp : sources) {
        if (y + 32 > card.bottom() - pad) break;
        const QRect sr(card.left() + pad, y, innerW, 30);
        m_sourceRows.push_back({sp.key, sr});
        const bool active = (sp.key == cur->key);
        if (active) {
            p.setBrush(ThemeManager::withAlpha(accent, 0.18));
            p.setPen(QPen(accent, 1));
            p.drawRoundedRect(sr, 8, 8);
        } else if (sr.contains(m_hoverPos)) {
            p.setBrush(ThemeManager::withAlpha(sc.text, 0.08));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(sr, 8, 8);
        }
        p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBodySmall(), active ? QFont::DemiBold : QFont::Normal));
        p.setPen(active ? accent : sc.text);
        const QString lbl = QStringLiteral("%1 — %2").arg(sp.identity, sp.title.isEmpty() ? sp.playbackStatus : sp.title);
        p.drawText(sr.adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft,
                   p.fontMetrics().elidedText(lbl, Qt::ElideRight, sr.width() - 20));
        y += 34;
    }
}

void MediaPanel::handleCardMousePress(QMouseEvent *event, const QRect & /*card*/) {
    if (m_prevRect.contains(event->pos())) { m_media->runAction(QStringLiteral("prev"), false); return; }
    if (m_playPauseRect.contains(event->pos())) { m_media->runAction(QStringLiteral("play-pause"), false); return; }
    if (m_nextRect.contains(event->pos())) { m_media->runAction(QStringLiteral("next"), false); return; }
    for (const auto &sr : m_sourceRows) {
        if (sr.second.contains(event->pos())) {
            m_media->selectPlayer(sr.first);
            update();
            return;
        }
    }
}

} // namespace quattro
