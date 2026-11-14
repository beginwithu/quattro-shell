#pragma once

#include "AudioService.h"
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>

namespace quattro {

struct MprisPlayerInfo {
    QString dbusName;
    QString key;
    QString identity;
    QString desktopEntry;
    QString playbackStatus; // Playing | Paused | Stopped
    QString title;
    QString artist;
    QString album;
    QString artUrl;
    bool canPlay = false;
    bool canPause = false;
    bool canGoNext = false;
    bool canGoPrevious = false;
    bool isProxy = false;
    qint64 playSerial = 0;

    bool isPlaying() const { return playbackStatus == QLatin1String("Playing"); }
    bool hasMetadata() const {
        return !title.isEmpty() || !artist.isEmpty() || !identity.isEmpty() || !desktopEntry.isEmpty();
    }
    bool hasTrack() const { return !title.isEmpty() || !artist.isEmpty(); }
    QString trackSignature() const {
        return title + QLatin1Char('\x1f') + artist + QLatin1Char('\x1f') + album + QLatin1Char('\x1f') + artUrl;
    }
};

class MediaService : public QObject {
    Q_OBJECT
public:
    explicit MediaService(AudioService *audio, QObject *parent = nullptr);

    void refreshPlayers();

    bool hasPlayer() const { return m_activeIndex >= 0 && m_activeIndex < m_players.size(); }
    bool hasMedia() const { return hasPlayer() && m_players[m_activeIndex].hasTrack(); }
    const MprisPlayerInfo *activePlayer() const;
    QVector<MprisPlayerInfo> sourcePlayers() const;

    bool runAction(const QString &action, bool showFeedback = true, const QString &targetKey = QString());
    bool switchSource(int delta, bool transferPlayback = false, bool showFeedback = true);
    void selectPlayer(const QString &key);
    bool handleVolumeKey(const QString &action);
    QString statusJson() const;

    // Pure selection helper exposed for tests
    static int rankIdlePlayer(const MprisPlayerInfo &p, bool hasPlaybackStream);

public slots:
    void onNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);
    void onPropertiesChanged(const QString &iface, const QVariantMap &changed, const QStringList &invalidated);

signals:
    void mediaChanged();
    void osdRequested(const QString &icon, const QString &message, int value = -1, const QString &progressText = QString());

private:
    void fetchPlayer(const QString &service);
    void recomputeActivePlayer();
    bool playerMatchesAudioStream(const MprisPlayerInfo &p) const;
    int findBestPlayerForAction(const QString &action, const QString &targetKey) const;
    void callPlayerMethod(const QString &dbusName, const QString &method);

    AudioService *m_audio;
    QVector<MprisPlayerInfo> m_players;
    QHash<QString, qint64> m_playSerials;
    qint64 m_nextSerial = 1;
    QString m_preferredKey;
    int m_activeIndex = -1;

    QTimer m_trackOsdTimer;
    QString m_pendingOsdSig;
    QString m_pendingOsdAction;
    int m_trackOsdAttempts = 0;

    qint64 m_lastMuteToggleMs = 0;
};

} // namespace quattro
