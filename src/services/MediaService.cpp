#include "MediaService.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QTimer>
#include <algorithm>

namespace quattro {

namespace {
const QString kMprisPrefix = QStringLiteral("org.mpris.MediaPlayer2.");
const QString kMprisPath = QStringLiteral("/org/mpris/MediaPlayer2");
const QString kMprisPlayerIface = QStringLiteral("org.mpris.MediaPlayer2.Player");
const QString kMprisRootIface = QStringLiteral("org.mpris.MediaPlayer2");

QString normalizedLabel(const QString &s) {
    QString out;
    out.reserve(s.size());
    for (const QChar c : s.toLower()) {
        if (c.isLetterOrNumber())
            out.append(c);
    }
    return out;
}
} // namespace

MediaService::MediaService(AudioService *audio, QObject *parent)
    : QObject(parent)
    , m_audio(audio)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected()) {
        bus.connect(QStringLiteral("org.freedesktop.DBus"),
                    QStringLiteral("/org/freedesktop/DBus"),
                    QStringLiteral("org.freedesktop.DBus"),
                    QStringLiteral("NameOwnerChanged"),
                    this, SLOT(onNameOwnerChanged(QString,QString,QString)));
        bus.connect(QString(), kMprisPath, QStringLiteral("org.freedesktop.DBus.Properties"),
                    QStringLiteral("PropertiesChanged"),
                    this, SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));
    }

    if (m_audio)
        connect(m_audio, &AudioService::audioChanged, this, &MediaService::recomputeActivePlayer);

    m_trackOsdTimer.setInterval(120);
    connect(&m_trackOsdTimer, &QTimer::timeout, this, [this]() {
        ++m_trackOsdAttempts;
        const auto *ap = activePlayer();
        const QString curSig = ap ? ap->trackSignature() : QString();
        if ((ap && curSig != m_pendingOsdSig && !ap->title.isEmpty()) || m_trackOsdAttempts >= 10) {
            m_trackOsdTimer.stop();
            if (ap && ap->hasTrack()) {
                const QString icon = m_pendingOsdAction == QLatin1String("previous")
                                         ? QStringLiteral("media-previous")
                                         : QStringLiteral("media-next");
                const QString msg = ap->artist.isEmpty() ? ap->title : (ap->title + QStringLiteral(" · ") + ap->artist);
                emit osdRequested(icon, msg);
            }
        }
    });

    refreshPlayers();
}

void MediaService::refreshPlayers() {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface())
        return;

    const QDBusReply<QStringList> names = bus.interface()->registeredServiceNames();
    if (!names.isValid())
        return;

    m_players.clear();
    for (const QString &svc : names.value()) {
        if (svc.startsWith(kMprisPrefix))
            fetchPlayer(svc);
    }
    recomputeActivePlayer();
    emit mediaChanged();
}

void MediaService::onNameOwnerChanged(const QString &name, const QString &, const QString &newOwner) {
    if (!name.startsWith(kMprisPrefix))
        return;
    if (newOwner.isEmpty()) {
        for (int i = m_players.size() - 1; i >= 0; --i) {
            if (m_players[i].dbusName == name)
                m_players.removeAt(i);
        }
        if (m_preferredKey == name)
            m_preferredKey.clear();
        recomputeActivePlayer();
        emit mediaChanged();
    } else {
        fetchPlayer(name);
        recomputeActivePlayer();
        emit mediaChanged();
    }
}

void MediaService::onPropertiesChanged(const QString &iface, const QVariantMap &, const QStringList &) {
    if (iface != kMprisPlayerIface && iface != kMprisRootIface)
        return;
    // Refresh active players cleanly
    QTimer::singleShot(15, this, &MediaService::refreshPlayers);
}

void MediaService::fetchPlayer(const QString &service) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusInterface rootIface(service, kMprisPath, kMprisRootIface, bus);
    QDBusInterface playerIface(service, kMprisPath, kMprisPlayerIface, bus);
    if (!playerIface.isValid())
        return;

    MprisPlayerInfo info;
    info.dbusName = service;
    info.identity = rootIface.isValid() ? rootIface.property("Identity").toString() : QString();
    info.desktopEntry = rootIface.isValid() ? rootIface.property("DesktopEntry").toString() : QString();
    info.key = !service.isEmpty() ? service : (!info.desktopEntry.isEmpty() ? info.desktopEntry : info.identity);
    info.isProxy = service.contains(QLatin1String("playerctld"), Qt::CaseInsensitive)
                   || info.desktopEntry.compare(QLatin1String("playerctld"), Qt::CaseInsensitive) == 0;

    info.playbackStatus = playerIface.property("PlaybackStatus").toString();
    info.canPlay = playerIface.property("CanPlay").toBool();
    info.canPause = playerIface.property("CanPause").toBool();
    info.canGoNext = playerIface.property("CanGoNext").toBool();
    info.canGoPrevious = playerIface.property("CanGoPrevious").toBool();

    const QVariant mdVar = playerIface.property("Metadata");
    QVariantMap md;
    if (mdVar.metaType().id() == QMetaType::QVariantMap)
        md = mdVar.toMap();
    else if (mdVar.canConvert<QDBusArgument>()) {
        const QDBusArgument arg = mdVar.value<QDBusArgument>();
        arg >> md;
    }

    info.title = md.value(QStringLiteral("xesam:title")).toString();
    const QStringList artists = md.value(QStringLiteral("xesam:artist")).toStringList();
    info.artist = artists.join(QStringLiteral(", "));
    info.album = md.value(QStringLiteral("xesam:album")).toString();
    info.artUrl = md.value(QStringLiteral("mpris:artUrl")).toString();

    if (info.isPlaying()) {
        if (!m_playSerials.contains(info.key))
            m_playSerials.insert(info.key, m_nextSerial++);
        info.playSerial = m_playSerials.value(info.key);
    } else {
        m_playSerials.remove(info.key);
    }

    for (int i = 0; i < m_players.size(); ++i) {
        if (m_players[i].dbusName == service) {
            m_players[i] = info;
            return;
        }
    }
    m_players.push_back(info);
}

bool MediaService::playerMatchesAudioStream(const MprisPlayerInfo &p) const {
    if (!m_audio)
        return false;
    const QString idNorm = normalizedLabel(!p.desktopEntry.isEmpty() ? p.desktopEntry : p.identity);
    if (idNorm.isEmpty())
        return false;
    for (const QString &lbl : m_audio->playbackStreamLabels()) {
        const QString sNorm = normalizedLabel(lbl);
        if (!sNorm.isEmpty() && (sNorm.contains(idNorm) || idNorm.contains(sNorm)))
            return true;
    }
    return false;
}

int MediaService::rankIdlePlayer(const MprisPlayerInfo &p, bool hasPlaybackStream) {
    int bucket = 0;
    if (hasPlaybackStream) bucket = 40;
    else if (p.hasTrack() || !p.album.isEmpty() || !p.artUrl.isEmpty()) bucket = 30;
    else if (p.canPlay || p.canPause || p.canGoNext || p.canGoPrevious) bucket = 20;
    else if (p.hasMetadata()) bucket = 10;
    if (!p.isProxy) bucket += 4;
    if (p.hasTrack()) bucket += 2;
    return bucket;
}

void MediaService::recomputeActivePlayer() {
    if (m_players.isEmpty()) {
        m_activeIndex = -1;
        return;
    }

    // 1. Preferred playing player
    if (!m_preferredKey.isEmpty()) {
        for (int i = 0; i < m_players.size(); ++i) {
            if (m_players[i].key == m_preferredKey && m_players[i].hasMetadata() && m_players[i].isPlaying()) {
                m_activeIndex = i;
                return;
            }
        }
    }

    // 2. Oldest playing player matching an audio stream, then any playing player
    auto findOldestPlaying = [&](bool requireStream) -> int {
        int best = -1;
        for (int i = 0; i < m_players.size(); ++i) {
            const auto &p = m_players[i];
            if (!p.isPlaying()) continue;
            if (requireStream && !playerMatchesAudioStream(p)) continue;
            if (best < 0) { best = i; continue; }
            if (!p.isProxy && m_players[best].isProxy) { best = i; continue; }
            if (p.isProxy == m_players[best].isProxy && p.playSerial < m_players[best].playSerial) best = i;
        }
        return best;
    };

    int idx = findOldestPlaying(true);
    if (idx < 0) idx = findOldestPlaying(false);
    if (idx >= 0) {
        m_activeIndex = idx;
        return;
    }

    // 3. Idle preferred
    if (!m_preferredKey.isEmpty()) {
        for (int i = 0; i < m_players.size(); ++i) {
            if (m_players[i].key == m_preferredKey && m_players[i].hasMetadata()) {
                m_activeIndex = i;
                return;
            }
        }
    }

    // 4. Ranked idle
    int bestIdx = 0;
    int bestRank = -1;
    for (int i = 0; i < m_players.size(); ++i) {
        const int r = rankIdlePlayer(m_players[i], playerMatchesAudioStream(m_players[i]));
        if (r > bestRank) {
            bestRank = r;
            bestIdx = i;
        }
    }
    m_activeIndex = bestIdx;
}

const MprisPlayerInfo *MediaService::activePlayer() const {
    if (m_activeIndex < 0 || m_activeIndex >= m_players.size())
        return nullptr;
    return &m_players[m_activeIndex];
}

QVector<MprisPlayerInfo> MediaService::sourcePlayers() const {
    QVector<MprisPlayerInfo> out;
    for (const auto &p : m_players) {
        if (p.hasMetadata() && (p.isPlaying() || p.canPlay))
            out.push_back(p);
    }
    std::sort(out.begin(), out.end(), [](const MprisPlayerInfo &a, const MprisPlayerInfo &b) {
        if (a.isPlaying() != b.isPlaying()) return a.isPlaying();
        if (a.isProxy != b.isProxy) return !a.isProxy;
        return a.identity.compare(b.identity, Qt::CaseInsensitive) < 0;
    });
    return out;
}

int MediaService::findBestPlayerForAction(const QString &action, const QString &targetKey) const {
    if (!targetKey.isEmpty()) {
        for (int i = 0; i < m_players.size(); ++i) {
            if (m_players[i].key == targetKey || m_players[i].dbusName == targetKey)
                return i;
        }
    }
    if (action == QLatin1String("pause") || action == QLatin1String("playPause")) {
        for (int i = 0; i < m_players.size(); ++i) {
            if (m_players[i].isPlaying() && !m_players[i].isProxy)
                return i;
        }
    }
    if (m_activeIndex >= 0 && m_activeIndex < m_players.size())
        return m_activeIndex;
    return m_players.isEmpty() ? -1 : 0;
}

void MediaService::callPlayerMethod(const QString &dbusName, const QString &method) {
    QDBusMessage msg = QDBusMessage::createMethodCall(dbusName, kMprisPath, kMprisPlayerIface, method);
    QDBusConnection::sessionBus().asyncCall(msg);
}

bool MediaService::runAction(const QString &action, bool showFeedback, const QString &targetKey) {
    const int idx = findBestPlayerForAction(action, targetKey);
    if (idx < 0 || idx >= m_players.size())
        return false;

    const MprisPlayerInfo target = m_players[idx];
    m_preferredKey = target.key;

    QString dbusMethod;
    QString osdIcon = QStringLiteral("media");
    if (action == QLatin1String("next")) { dbusMethod = QStringLiteral("Next"); osdIcon = QStringLiteral("media-next"); }
    else if (action == QLatin1String("previous")) { dbusMethod = QStringLiteral("Previous"); osdIcon = QStringLiteral("media-previous"); }
    else if (action == QLatin1String("play")) { dbusMethod = QStringLiteral("Play"); osdIcon = QStringLiteral("media-play"); }
    else if (action == QLatin1String("pause")) { dbusMethod = QStringLiteral("Pause"); osdIcon = QStringLiteral("media-pause"); }
    else if (action == QLatin1String("playPause")) {
        dbusMethod = QStringLiteral("PlayPause");
        osdIcon = target.isPlaying() ? QStringLiteral("media-pause") : QStringLiteral("media-play");
    } else {
        return false;
    }

    callPlayerMethod(target.dbusName, dbusMethod);

    if (showFeedback) {
        if (action == QLatin1String("next") || action == QLatin1String("previous")) {
            m_pendingOsdSig = target.trackSignature();
            m_pendingOsdAction = action;
            m_trackOsdAttempts = 0;
            m_trackOsdTimer.start();
        } else if (target.hasTrack()) {
            const QString msg = target.artist.isEmpty() ? target.title : (target.title + QStringLiteral(" · ") + target.artist);
            emit osdRequested(osdIcon, msg);
        }
    }
    QTimer::singleShot(80, this, &MediaService::refreshPlayers);
    return true;
}

bool MediaService::switchSource(int delta, bool transferPlayback, bool showFeedback) {
    const QVector<MprisPlayerInfo> list = sourcePlayers();
    if (list.isEmpty())
        return false;

    int cur = -1;
    const auto *ap = activePlayer();
    for (int i = 0; i < list.size(); ++i) {
        if ((ap && list[i].key == ap->key) || (!m_preferredKey.isEmpty() && list[i].key == m_preferredKey)) {
            cur = i;
            break;
        }
    }
    const int n = list.size();
    const int nextIdx = ((cur < 0 ? 0 : cur) + (delta >= 0 ? 1 : -1) + n) % n;
    const MprisPlayerInfo &next = list[nextIdx];
    const bool wasPlaying = ap && ap->isPlaying();
    const QString oldDbus = ap ? ap->dbusName : QString();

    m_preferredKey = next.key;
    if (transferPlayback && wasPlaying && oldDbus != next.dbusName) {
        callPlayerMethod(oldDbus, QStringLiteral("Pause"));
        callPlayerMethod(next.dbusName, QStringLiteral("Play"));
    }

    recomputeActivePlayer();
    emit mediaChanged();

    if (showFeedback) {
        const QString msg = !next.title.isEmpty()
                                ? (next.identity + QStringLiteral(" — ") + next.title)
                                : next.identity;
        emit osdRequested(QStringLiteral("media-source"), msg);
    }
    return true;
}

void MediaService::selectPlayer(const QString &key) {
    m_preferredKey = key;
    recomputeActivePlayer();
    emit mediaChanged();
}

bool MediaService::handleVolumeKey(const QString &action) {
    if (!m_audio)
        return false;
    const QString sink = m_audio->defaultSinkName();
    if (!sink.startsWith(QLatin1String("alsa_output.")))
        return false;

    if (action == QLatin1String("mute-toggle")) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - m_lastMuteToggleMs < 250)
            return true;
        m_lastMuteToggleMs = now;
        const bool nextMuted = !m_audio->sinkMuted();
        m_audio->setSinkMute(nextMuted);
        const int pct = m_audio->sinkVolume();
        emit osdRequested((nextMuted || pct == 0) ? QStringLiteral("volume-muted") : QStringLiteral("volume-high"),
                          QString(), pct, QStringLiteral("%1%").arg(pct));
        return true;
    }

    const int step = (action == QLatin1String("raise")) ? 5 : (action == QLatin1String("lower") ? -5 : 0);
    if (step == 0)
        return false;
    const int nextPct = std::clamp(m_audio->sinkVolume() + step, 0, 100);
    if (m_audio->sinkMuted())
        m_audio->setSinkMute(false);
    m_audio->setSinkVolume(nextPct);
    emit osdRequested(nextPct == 0 ? QStringLiteral("volume-muted") : QStringLiteral("volume-high"),
                      QString(), nextPct, QStringLiteral("%1%").arg(nextPct));
    return true;
}

QString MediaService::statusJson() const {
    const auto *ap = activePlayer();
    QJsonObject o{
        {QStringLiteral("hasPlayer"), ap != nullptr},
        {QStringLiteral("hasMedia"), hasMedia()},
        {QStringLiteral("playing"), ap ? ap->isPlaying() : false},
        {QStringLiteral("identity"), ap ? ap->identity : QString()},
        {QStringLiteral("desktopEntry"), ap ? ap->desktopEntry : QString()},
        {QStringLiteral("title"), ap ? ap->title : QString()},
        {QStringLiteral("artist"), ap ? ap->artist : QString()},
        {QStringLiteral("album"), ap ? ap->album : QString()},
        {QStringLiteral("artUrl"), ap ? ap->artUrl : QString()},
        {QStringLiteral("canGoNext"), ap ? ap->canGoNext : false},
        {QStringLiteral("canGoPrevious"), ap ? ap->canGoPrevious : false},
        {QStringLiteral("canTogglePlaying"), ap ? (ap->canPlay || ap->canPause) : false}
    };
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

} // namespace quattro
