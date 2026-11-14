#include "SpeedTestOverlay.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>
#include <fcntl.h>
#include <unistd.h>
#include <vector>

namespace quattro {

SpeedTestOverlay::SpeedTestOverlay(Mode mode,
                                   ThemeManager *theme,
                                   ShellConfig *config,
                                   QWidget *parent)
    : ShellPopupWindow(theme,
                       config,
                       mode == Mode::Network ? QStringLiteral("quattro-speedtest")
                                             : QStringLiteral("quattro-disk-speedtest"),
                       true,
                       parent)
    , m_mode(mode)
{}

void SpeedTestOverlay::onOpening() {
    if (!m_running && m_primaryMbps == 0.0) {
        m_phase = QStringLiteral("Ready");
        if (m_mode == Mode::Disk) {
            const QStorageInfo st(QDir::homePath());
            m_serverInfo = QStringLiteral("Target: %1 (%2 GB free)")
                               .arg(st.rootPath())
                               .arg(st.bytesAvailable() / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
        } else {
            m_serverInfo = QStringLiteral("Ookla / Cloudflare HTTP fallback");
        }
        startTest();
    }
}

void SpeedTestOverlay::onClosing() {
    stopTest();
}

void SpeedTestOverlay::stopTest() {
    m_running = false;
    if (m_proc) {
        m_proc->kill();
        m_proc->deleteLater();
        m_proc = nullptr;
    }
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_phase = QStringLiteral("Stopped");
    update();
}

void SpeedTestOverlay::startTest() {
    stopTest();
    m_running = true;
    m_primaryMbps = 0.0;
    m_secondaryMbps = 0.0;
    m_pingMs = 0.0;
    m_history.clear();
    m_detail.clear();

    if (m_mode == Mode::Network)
        startNetworkSpeedtest();
    else
        startDiskSpeedtest();
    update();
}

void SpeedTestOverlay::startNetworkSpeedtest() {
    m_phase = QStringLiteral("Measuring network speed...");
    m_proc = new QProcess(this);
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
        QProcess *p = m_proc;
        m_proc = nullptr;
        const QByteArray out = p ? p->readAllStandardOutput() : QByteArray();
        if (p) p->deleteLater();

        if (!m_running)
            return;

        if (exitCode == 0 && !out.trimmed().isEmpty()) {
            const QJsonDocument doc = QJsonDocument::fromJson(out);
            if (doc.isObject()) {
                const QJsonObject obj = doc.object();
                // speedtest-cli uses bits/s; Ookla official CLI uses bytes/s under download.bandwidth
                if (obj.contains(QStringLiteral("download")) && obj.value(QStringLiteral("download")).isObject()) {
                    const double dlBytes = obj.value(QStringLiteral("download")).toObject().value(QStringLiteral("bandwidth")).toDouble();
                    const double ulBytes = obj.value(QStringLiteral("upload")).toObject().value(QStringLiteral("bandwidth")).toDouble();
                    m_primaryMbps = (dlBytes * 8.0) / 1e6;
                    m_secondaryMbps = (ulBytes * 8.0) / 1e6;
                    m_pingMs = obj.value(QStringLiteral("ping")).toObject().value(QStringLiteral("latency")).toDouble();
                    m_serverInfo = obj.value(QStringLiteral("server")).toObject().value(QStringLiteral("name")).toString();
                } else {
                    m_primaryMbps = obj.value(QStringLiteral("download")).toDouble() / 1e6;
                    m_secondaryMbps = obj.value(QStringLiteral("upload")).toDouble() / 1e6;
                    m_pingMs = obj.value(QStringLiteral("ping")).toDouble();
                    m_serverInfo = obj.value(QStringLiteral("server")).toObject().value(QStringLiteral("sponsor")).toString();
                }
                m_running = false;
                m_phase = QStringLiteral("Completed");
                update();
                return;
            }
        }
        runCurlFallbackNetwork();
    });

    m_proc->start(QStringLiteral("speedtest-cli"), {QStringLiteral("--json")});
    if (!m_proc->waitForStarted(300)) {
        m_proc->deleteLater();
        m_proc = nullptr;
        runCurlFallbackNetwork();
    }
}

void SpeedTestOverlay::runCurlFallbackNetwork() {
    m_phase = QStringLiteral("Downloading 25 MB Cloudflare probe...");
    m_dlBytes = 0;
    m_dlTimer.start();

    QNetworkRequest req(QUrl(QStringLiteral("https://speed.cloudflare.com/__down?bytes=25000000")));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_nam.get(req);

    connect(m_reply, &QNetworkReply::readyRead, this, [this]() {
        if (!m_reply || !m_running)
            return;
        if (m_pingMs <= 0.0 && m_dlTimer.elapsed() > 0)
            m_pingMs = m_dlTimer.elapsed();
        const QByteArray chunk = m_reply->readAll();
        m_dlBytes += chunk.size();
        const double sec = std::max(0.05, m_dlTimer.elapsed() / 1000.0);
        m_primaryMbps = (m_dlBytes * 8.0) / (sec * 1e6);
        m_history.push_back(m_primaryMbps);
        if (m_history.size() > 48)
            m_history.pop_front();
        update();
    });

    connect(m_reply, &QNetworkReply::finished, this, [this]() {
        if (!m_reply)
            return;
        const bool ok = (m_reply->error() == QNetworkReply::NoError);
        const QString errStr = m_reply->errorString();
        m_reply->deleteLater();
        m_reply = nullptr;
        if (!m_running)
            return;
        if (!ok) {
            m_running = false;
            m_phase = QStringLiteral("Probe failed");
            m_detail = errStr;
            update();
            return;
        }
        // Now measure 5 MB upload
        m_phase = QStringLiteral("Uploading 5 MB Cloudflare probe...");
        QByteArray payload(5 * 1024 * 1024, 'q');
        m_dlTimer.restart();
        QNetworkRequest upReq(QUrl(QStringLiteral("https://speed.cloudflare.com/__up")));
        upReq.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));
        m_reply = m_nam.post(upReq, payload);
        connect(m_reply, &QNetworkReply::uploadProgress, this, [this](qint64 sent, qint64 /*total*/) {
            const double sec = std::max(0.05, m_dlTimer.elapsed() / 1000.0);
            m_secondaryMbps = (sent * 8.0) / (sec * 1e6);
            update();
        });
        connect(m_reply, &QNetworkReply::finished, this, [this]() {
            if (m_reply) {
                m_reply->deleteLater();
                m_reply = nullptr;
            }
            m_running = false;
            m_phase = QStringLiteral("Completed");
            update();
        });
    });
}

void SpeedTestOverlay::startDiskSpeedtest() {
    m_phase = QStringLiteral("Benchmarking sequential write/read (256 MB)...");
    const QString path = QDir::homePath() + QStringLiteral("/.cache/quattro-disk-bench.tmp");
    QDir().mkpath(QDir::homePath() + QStringLiteral("/.cache"));

    auto *watcher = new QObject(this);
    (void)QtConcurrent::run([this, watcher, path]() {
        constexpr size_t kChunk = 4 * 1024 * 1024;
        constexpr size_t kTotal = 256 * 1024 * 1024;
        std::vector<char> buf(kChunk, 0x5A);

        QElapsedTimer wt;
        wt.start();
        double writeMBs = 0.0;
        double readMBs = 0.0;

        int wfd = ::open(path.toLocal8Bit().constData(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (wfd >= 0) {
            for (size_t done = 0; done < kTotal; done += kChunk) {
                ssize_t n = ::write(wfd, buf.data(), buf.size());
                if (n <= 0) break;
            }
            ::fsync(wfd);
            ::close(wfd);
            const double wSec = std::max(0.001, wt.elapsed() / 1000.0);
            writeMBs = (kTotal / (1024.0 * 1024.0)) / wSec;
        }

        QElapsedTimer rt;
        rt.start();
        int rfd = ::open(path.toLocal8Bit().constData(), O_RDONLY);
        if (rfd >= 0) {
            posix_fadvise(rfd, 0, 0, POSIX_FADV_DONTNEED);
            for (;;) {
                ssize_t n = ::read(rfd, buf.data(), buf.size());
                if (n <= 0) break;
            }
            ::close(rfd);
            const double rSec = std::max(0.001, rt.elapsed() / 1000.0);
            readMBs = (kTotal / (1024.0 * 1024.0)) / rSec;
        }
        ::unlink(path.toLocal8Bit().constData());

        QMetaObject::invokeMethod(this, [this, watcher, writeMBs, readMBs]() {
            watcher->deleteLater();
            m_primaryMbps = readMBs;
            m_secondaryMbps = writeMBs;
            m_running = false;
            m_phase = QStringLiteral("Completed");
            update();
        }, Qt::QueuedConnection);
    });
}

void SpeedTestOverlay::paintCardContent(QPainter &p, const QRect &card) {
    const SurfaceColors &sc = theme()->popups();
    const QColor muted = theme()->muted();
    const QColor accent = sc.accent.isValid() ? sc.accent : theme()->accent();
    const int pad = 20;

    UiPaint::drawHero(p, QRect(card.left() + pad, card.top() + pad, card.width() - pad * 2, 46),
                      theme(),
                      m_mode == Mode::Network ? QStringLiteral("󰓅") : QStringLiteral("󰋊"),
                      m_mode == Mode::Network ? QStringLiteral("Network Speed Test")
                                              : QStringLiteral("Disk I/O Speed Test"),
                      m_phase, sc.text, muted, accent);

    const int gaugeW = (card.width() - pad * 3) / 2;
    const QRect g1(card.left() + pad, card.top() + 82, gaugeW, 178);
    const QRect g2(g1.right() + pad, card.top() + 82, gaugeW, 178);

    if (m_mode == Mode::Network) {
        UiPaint::drawDialGauge(p, g1, theme(), QStringLiteral("Download"),
                               m_primaryMbps, 500.0, QStringLiteral("Mbps"), accent, sc.text, muted);
        UiPaint::drawDialGauge(p, g2, theme(), QStringLiteral("Upload"),
                               m_secondaryMbps, 500.0, QStringLiteral("Mbps"),
                               QColor(QStringLiteral("#7dcfff")), sc.text, muted);
    } else {
        UiPaint::drawDialGauge(p, g1, theme(), QStringLiteral("Seq Read"),
                               m_primaryMbps, 3000.0, QStringLiteral("MB/s"), accent, sc.text, muted);
        UiPaint::drawDialGauge(p, g2, theme(), QStringLiteral("Seq Write"),
                               m_secondaryMbps, 3000.0, QStringLiteral("MB/s"),
                               QColor(QStringLiteral("#9ece6a")), sc.text, muted);
    }

    // Sparkline or server meta
    const QRect metaRect(card.left() + pad, g1.bottom() + 14, card.width() - pad * 2, 42);
    p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBodySmall()));
    p.setPen(muted);
    QString sub = m_serverInfo;
    if (m_mode == Mode::Network && m_pingMs > 0.0)
        sub = QStringLiteral("Latency: %1 ms  •  %2").arg(m_pingMs, 0, 'f', 1).arg(m_serverInfo);
    if (!m_detail.isEmpty())
        sub = m_detail;
    p.drawText(metaRect, Qt::AlignCenter, p.fontMetrics().elidedText(sub, Qt::ElideRight, metaRect.width()));

    const int btnY = card.bottom() - pad - 34;
    m_modeToggleRect = QRect(card.left() + pad, btnY, 150, 34);
    m_actionBtnRect = QRect(card.right() - pad - 150, btnY, 150, 34);

    UiPaint::drawPillButton(p, m_modeToggleRect, theme(),
                            m_mode == Mode::Network ? QStringLiteral("Switch to Disk")
                                                    : QStringLiteral("Switch to Network"),
                            false, m_modeToggleRect.contains(m_hoverPos));
    UiPaint::drawPillButton(p, m_actionBtnRect, theme(),
                            m_running ? QStringLiteral("Stop") : QStringLiteral("Run Again"),
                            true, m_actionBtnRect.contains(m_hoverPos), m_running);
}

void SpeedTestOverlay::handleCardMousePress(QMouseEvent *event, const QRect & /*card*/) {
    if (m_actionBtnRect.contains(event->pos())) {
        if (m_running)
            stopTest();
        else
            startTest();
        return;
    }
    if (m_modeToggleRect.contains(event->pos())) {
        stopTest();
        m_mode = (m_mode == Mode::Network) ? Mode::Disk : Mode::Network;
        startTest();
    }
}

} // namespace quattro
