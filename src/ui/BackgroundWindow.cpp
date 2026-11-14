#include "BackgroundWindow.h"

#include "../wayland/LayerShellIntegration.h"
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QMediaPlayer>
#include <QPainter>
#include <QPainterPath>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoSink>
#include <algorithm>
#include <cmath>

namespace quattro {

bool BackgroundManager::isVideoPath(const QString &path) {
    const QString l = path.toLower();
    return l.endsWith(QLatin1String(".mp4")) || l.endsWith(QLatin1String(".m4v")) ||
           l.endsWith(QLatin1String(".mov")) || l.endsWith(QLatin1String(".webm")) ||
           l.endsWith(QLatin1String(".mkv")) || l.endsWith(QLatin1String(".avi"));
}

QImage BackgroundManager::loadScaledCover(const QString &path, const QSize &pixelSize) {
    if (path.isEmpty() || !QFileInfo::exists(path) || isVideoPath(path))
        return {};

    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize orig = reader.size();
    if (orig.isValid() && !pixelSize.isEmpty()) {
        const double sx = static_cast<double>(pixelSize.width()) / orig.width();
        const double sy = static_cast<double>(pixelSize.height()) / orig.height();
        const double scale = std::max(sx, sy);
        if (scale < 1.0) {
            const QSize scaled(std::max(1, static_cast<int>(std::ceil(orig.width() * scale))),
                               std::max(1, static_cast<int>(std::ceil(orig.height() * scale))));
            reader.setScaledSize(scaled);
        }
    }
    return reader.read();
}

BackgroundManager::BackgroundManager(ThemeManager *theme, QObject *parent)
    : QObject(parent)
    , m_theme(theme)
    , m_linkPath(qEnvironmentVariable("XDG_STATE_HOME", QDir::homePath() + QStringLiteral("/.local/state"))
                 + QStringLiteral("/quattro/current/background"))
{
    const QString parentDir = QFileInfo(m_linkPath).absolutePath();
    if (QFileInfo::exists(parentDir))
        m_watcher.addPath(parentDir);

    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this]() {
        checkSymlink();
    });
    connect(qGuiApp, &QGuiApplication::screenAdded, this, [this](QScreen *) { syncScreens(); });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this](QScreen *) { syncScreens(); });

    // Light 3s fallback check in case symlink was swapped atomically outside watcher
    m_pollTimer.setInterval(3000);
    connect(&m_pollTimer, &QTimer::timeout, this, &BackgroundManager::checkSymlink);
    m_pollTimer.start();

    syncScreens();
    refreshFromSymlink();
}

BackgroundManager::~BackgroundManager() {
    qDeleteAll(m_surfaces);
    m_surfaces.clear();
}

void BackgroundManager::syncScreens() {
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (int i = m_surfaces.size() - 1; i >= 0; --i) {
        if (!screens.contains(m_surfaces[i]->targetScreen())) {
            m_surfaces[i]->close();
            m_surfaces[i]->deleteLater();
            m_surfaces.removeAt(i);
        }
    }
    for (QScreen *s : screens) {
        bool exists = false;
        for (BackgroundSurface *surf : m_surfaces) {
            if (surf->targetScreen() == s) { exists = true; break; }
        }
        if (!exists) {
            auto *surf = new BackgroundSurface(s, m_theme);
            if (!m_currentPath.isEmpty())
                surf->transitionTo(m_currentPath, true);
            surf->show();
            m_surfaces.push_back(surf);
        }
    }
}

void BackgroundManager::refreshFromSymlink() {
    checkSymlink();
}

void BackgroundManager::checkSymlink() {
    const QFileInfo fi(m_linkPath);
    if (!fi.exists())
        return;
    const QString target = fi.isSymLink() ? fi.symLinkTarget() : fi.absoluteFilePath();
    if (!target.isEmpty() && target != m_currentPath)
        setBackground(target, m_currentPath.isEmpty());
}

void BackgroundManager::setBackground(const QString &path, bool instant) {
    const QString clean = path.trimmed();
    if (clean.isEmpty() || !QFileInfo::exists(clean))
        return;
    m_currentPath = clean;
    for (BackgroundSurface *surf : m_surfaces)
        surf->transitionTo(clean, instant);
}

QString BackgroundManager::prepareThemeIntro(const QString &path) {
    const QString clean = path.trimmed();
    if (!clean.isEmpty() && QFileInfo::exists(clean))
        m_preparedIntroPath = clean;
    else
        m_preparedIntroPath = m_currentPath;
    return QStringLiteral("ok");
}

QString BackgroundManager::playThemeIntro(const QString &path, int durationMs) {
    QString target = path.trimmed();
    if (target.isEmpty()) target = m_preparedIntroPath;
    if (target.isEmpty()) target = m_currentPath;
    m_preparedIntroPath.clear();
    if (target.isEmpty() || !QFileInfo::exists(target))
        return QStringLiteral("missing");
    m_currentPath = target;
    for (BackgroundSurface *surf : m_surfaces)
        surf->transitionTo(target, false, true, durationMs);
    return QStringLiteral("ok");
}

BackgroundSurface::BackgroundSurface(QScreen *screen, ThemeManager *theme)
    : QWidget(nullptr)
    , m_screen(screen)
    , m_theme(theme)
{
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setWindowTitle(QStringLiteral("quattro-background"));

    const QSize sz = screen ? screen->size() : QSize(1920, 1080);
    resize(sz);
    winId();
    if (screen)
        windowHandle()->setScreen(screen);

    LayerSpec spec;
    spec.layer = LayerType::Background;
    spec.anchors = static_cast<uint32_t>(AnchorEdge::Top)
                   | static_cast<uint32_t>(AnchorEdge::Bottom)
                   | static_cast<uint32_t>(AnchorEdge::Left)
                   | static_cast<uint32_t>(AnchorEdge::Right);
    spec.exclusiveZone = -1;
    spec.keyboard = KeyboardFocus::None;
    spec.scope = QStringLiteral("quattro-background");
    spec.desiredSize = sz;
    LayerShellIntegration::configureWindow(windowHandle(), spec);

    m_anim.setStartValue(0.0);
    m_anim.setEndValue(1.0);
    m_anim.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_progress = v.toDouble();
        update();
    });
    connect(&m_anim, &QVariantAnimation::finished, this, [this]() {
        m_previous = QImage();
        m_progress = 1.0;
        update();
    });
    connect(m_theme, &ThemeManager::themeChanged, this, [this]() { update(); });
}

void BackgroundSurface::stopVideo() {
    if (m_player) {
        m_player->stop();
        m_player->setSource(QUrl());
    }
}

void BackgroundSurface::transitionTo(const QString &path, bool instant, bool slantedWipe, int overrideMs) {
    m_path = path;
    m_slantedWipe = slantedWipe;
    m_anim.stop();

    if (BackgroundManager::isVideoPath(path)) {
        if (!m_player) {
            m_videoSink = new QVideoSink(this);
            m_player = new QMediaPlayer(this);
            m_player->setLoops(QMediaPlayer::Infinite);
            m_player->setVideoSink(m_videoSink);
            connect(m_videoSink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
                QImage img = frame.toImage();
                if (!img.isNull()) {
                    m_current = img;
                    update();
                }
            });
        }
        m_previous = m_current;
        m_player->setSource(QUrl::fromLocalFile(path));
        m_player->play();
        const int dur = instant ? 0 : (overrideMs >= 0 ? overrideMs : m_theme->duration(320));
        if (dur > 0 && !m_previous.isNull()) {
            m_progress = 0.0;
            m_anim.setDuration(dur);
            m_anim.start();
        } else {
            m_progress = 1.0;
        }
        update();
        return;
    }

    stopVideo();
    const qreal dpr = m_screen ? m_screen->devicePixelRatio() : 1.0;
    const QSize pxSize = size() * dpr;
    QImage next = BackgroundManager::loadScaledCover(path, pxSize);
    if (next.isNull())
        return;

    const int dur = instant ? 0 : (overrideMs >= 0 ? overrideMs : m_theme->duration(280));
    if (dur <= 0 || m_current.isNull()) {
        m_previous = QImage();
        m_current = next;
        m_progress = 1.0;
        update();
        return;
    }

    m_previous = m_current;
    m_current = next;
    m_progress = 0.0;
    m_anim.setDuration(dur);
    m_anim.start();
}

void BackgroundSurface::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (!m_path.isEmpty() && !BackgroundManager::isVideoPath(m_path))
        transitionTo(m_path, true);
}

void BackgroundSurface::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), m_theme->background());

    auto drawCover = [&](const QImage &img, double alpha) {
        if (img.isNull() || alpha <= 0.0)
            return;
        p.save();
        p.setOpacity(alpha);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const QSizeF targetSz = size();
        const QSizeF imgSz = img.size();
        const double scale = std::max(targetSz.width() / imgSz.width(),
                                      targetSz.height() / imgSz.height());
        const double drawW = imgSz.width() * scale;
        const double drawH = imgSz.height() * scale;
        const QRectF dst((targetSz.width() - drawW) * 0.5,
                         (targetSz.height() - drawH) * 0.5,
                         drawW, drawH);
        p.drawImage(dst, img);
        p.restore();
    };

    if (!m_previous.isNull() && m_progress < 1.0) {
        drawCover(m_previous, 1.0);
        if (m_slantedWipe) {
            p.save();
            const qreal slant = 160.0;
            const qreal xEdge = (width() + slant) * m_progress;
            QPolygonF wipe;
            wipe << QPointF(0, 0)
                 << QPointF(xEdge, 0)
                 << QPointF(xEdge - slant, height())
                 << QPointF(0, height());
            QPainterPath clip;
            clip.addPolygon(wipe);
            p.setClipPath(clip);
            drawCover(m_current, 1.0);
            p.restore();
        } else {
            drawCover(m_current, m_progress);
        }
    } else {
        drawCover(m_current, 1.0);
    }
}

} // namespace quattro
