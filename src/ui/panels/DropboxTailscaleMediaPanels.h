#pragma once

#include "../CommonUi.h"
#include "../../services/MediaService.h"
#include <QNetworkAccessManager>
#include <QPixmap>
#include <QProcess>
#include <QVector>

namespace quattro {

class DropboxPanel : public ShellPopupWindow {
    Q_OBJECT
public:
    DropboxPanel(ThemeManager *theme, ShellConfig *config, QWidget *parent = nullptr);

    QString summaryText() const { return m_statusLine; }

protected:
    QSize desiredCardSize() const override { return QSize(390, 440); }
    void onOpening() override;
    void paintCardContent(QPainter &p, const QRect &card) override;
    void handleCardMousePress(QMouseEvent *event, const QRect &card) override;

private:
    void refreshDropbox();

    bool m_running = false;
    QString m_statusLine = QStringLiteral("Idle");
    QString m_accountEmail;
    QString m_dropboxDir;
    QStringList m_recentFiles;

    QRect m_toggleDaemonRect;
    QRect m_openFolderRect;
};

struct TailscalePeerItem {
    QString hostName;
    QString ip;
    QString os;
    bool online = false;
    bool exitNode = false;
    bool exitNodeOption = false;
};

class TailscalePanel : public ShellPopupWindow {
    Q_OBJECT
public:
    TailscalePanel(ThemeManager *theme, ShellConfig *config, QWidget *parent = nullptr);

    bool isUp() const { return m_up; }
    QString selfIp() const { return m_selfIp; }
    void refreshTailscale();

signals:
    void stateChanged();

protected:
    QSize desiredCardSize() const override { return QSize(410, 480); }
    void onOpening() override;
    void paintCardContent(QPainter &p, const QRect &card) override;
    void handleCardMousePress(QMouseEvent *event, const QRect &card) override;

private:
    bool m_up = false;
    bool m_busy = false;
    QString m_backendState;
    QString m_selfName;
    QString m_selfIp;
    QString m_activeExitNode;
    QVector<TailscalePeerItem> m_peers;

    QRect m_upToggleRect;
    QRect m_copySelfRect;
    QVector<QPair<int, QRect>> m_peerRows;
};

class MediaPanel : public ShellPopupWindow {
    Q_OBJECT
public:
    MediaPanel(MediaService *media,
               ThemeManager *theme,
               ShellConfig *config,
               QWidget *parent = nullptr);

protected:
    QSize desiredCardSize() const override { return QSize(400, 380); }
    void onOpening() override;
    void paintCardContent(QPainter &p, const QRect &card) override;
    void handleCardMousePress(QMouseEvent *event, const QRect &card) override;

private:
    void fetchArtIfNeeded(const QString &url);

    MediaService *m_media;
    QNetworkAccessManager m_nam;
    QString m_loadedArtUrl;
    QPixmap m_artPixmap;

    QRect m_prevRect;
    QRect m_playPauseRect;
    QRect m_nextRect;
    QVector<QPair<QString, QRect>> m_sourceRows;
};

} // namespace quattro
