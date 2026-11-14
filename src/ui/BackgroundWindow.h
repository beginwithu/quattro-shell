#pragma once

#include "../core/ThemeManager.h"
#include <QFileSystemWatcher>
#include <QImage>
#include <QObject>
#include <QScreen>
#include <QTimer>
#include <QVariantAnimation>
#include <QVector>
#include <QWidget>

class QMediaPlayer;
class QVideoSink;

namespace quattro {

class BackgroundSurface;

class BackgroundManager : public QObject {
    Q_OBJECT
public:
    explicit BackgroundManager(ThemeManager *theme, QObject *parent = nullptr);
    ~BackgroundManager() override;

    void setBackground(const QString &path, bool instant = false);
    QString currentBackground() const { return m_currentPath; }
    void refreshFromSymlink();

    QString prepareThemeIntro(const QString &path);
    QString playThemeIntro(const QString &path = QString(), int durationMs = 650);

    static bool isVideoPath(const QString &path);
    static QImage loadScaledCover(const QString &path, const QSize &pixelSize);

private:
    void syncScreens();
    void checkSymlink();

    ThemeManager *m_theme;
    QString m_linkPath;
    QString m_currentPath;
    QString m_preparedIntroPath;
    QVector<BackgroundSurface *> m_surfaces;
    QFileSystemWatcher m_watcher;
    QTimer m_pollTimer;
};

class BackgroundSurface : public QWidget {
    Q_OBJECT
public:
    BackgroundSurface(QScreen *screen, ThemeManager *theme);

    QScreen *targetScreen() const { return m_screen; }
    void transitionTo(const QString &path, bool instant, bool slantedWipe = false, int overrideMs = -1);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void stopVideo();

    QScreen *m_screen;
    ThemeManager *m_theme;
    QString m_path;
    QImage m_current;
    QImage m_previous;
    double m_progress = 1.0;
    bool m_slantedWipe = false;
    QVariantAnimation m_anim;
    QMediaPlayer *m_player = nullptr;
    QVideoSink *m_videoSink = nullptr;
};

} // namespace quattro
