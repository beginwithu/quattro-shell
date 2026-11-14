#include "WifiQrOverlay.h"

#include <QMouseEvent>
#include <QPainter>
#include <QProcess>
#include <qrencode.h>

namespace quattro {

WifiQrOverlay::WifiQrOverlay(ThemeManager *theme, ShellConfig *config, QWidget *parent)
    : ShellPopupWindow(theme, config, QStringLiteral("quattro-wifiqr"), true, parent)
{}

void WifiQrOverlay::onOpening() {
    m_ssid.clear();
    m_password.clear();
    m_qrImage = QImage();
    m_showPassword = false;
    m_statusText = QStringLiteral("Reading active Wi-Fi credentials...");
    fetchActiveWifi();
}

void WifiQrOverlay::fetchActiveWifi() {
    auto *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        if (exitCode == 0 && !out.isEmpty()) {
            const QStringList lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            for (const QString &line : lines) {
                if (line.startsWith(QStringLiteral("yes:"))) {
                    const QString ssid = line.mid(4);
                    m_ssid = ssid;
                    auto *p2 = new QProcess(this);
                    connect(p2, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                            this, [this, p2](int, QProcess::ExitStatus) {
                        m_password = QString::fromUtf8(p2->readAllStandardOutput()).trimmed();
                        p2->deleteLater();
                        m_statusText.clear();
                        rebuildQr();
                        update();
                    });
                    p2->start(QStringLiteral("quattro-network-password"), {ssid});
                    return;
                }
            }
        }
        m_statusText = QStringLiteral("No active Wi-Fi connection");
        update();
    });
    proc->start(QStringLiteral("nmcli"), {QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("ACTIVE,SSID"), QStringLiteral("dev"), QStringLiteral("wifi")});
}

void WifiQrOverlay::rebuildQr() {
    if (m_ssid.isEmpty())
        return;

    auto esc = [](QString s) {
        s.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
        s.replace(QLatin1Char(';'), QStringLiteral("\\;"));
        s.replace(QLatin1Char(','), QStringLiteral("\\,"));
        s.replace(QLatin1Char(':'), QStringLiteral("\\:"));
        return s;
    };

    const QString sec = m_password.isEmpty() ? QStringLiteral("nopass") : m_security;
    const QString payload = m_password.isEmpty()
                                ? QStringLiteral("WIFI:T:nopass;S:%1;;").arg(esc(m_ssid))
                                : QStringLiteral("WIFI:T:%1;S:%2;P:%3;;").arg(sec, esc(m_ssid), esc(m_password));

    const QByteArray utf8 = payload.toUtf8();
    QRcode *qr = QRcode_encodeString(utf8.constData(), 0, QR_ECLEVEL_M, QR_MODE_8, 1);
    if (!qr)
        return;

    const int n = qr->width;
    const int margin = 2;
    const int scale = 8;
    const int imgSize = (n + margin * 2) * scale;
    QImage img(imgSize, imgSize, QImage::Format_RGB32);
    img.fill(Qt::white);

    QPainter ip(&img);
    ip.setPen(Qt::NoPen);
    ip.setBrush(Qt::black);
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            if (qr->data[y * n + x] & 1)
                ip.drawRect((x + margin) * scale, (y + margin) * scale, scale, scale);
        }
    }
    QRcode_free(qr);
    m_qrImage = img;
}

void WifiQrOverlay::paintCardContent(QPainter &p, const QRect &card) {
    const SurfaceColors &sc = theme()->popups();
    const QColor muted = theme()->muted();
    const QColor accent = sc.accent.isValid() ? sc.accent : theme()->accent();
    const int pad = 20;

    UiPaint::drawHero(p, QRect(card.left() + pad, card.top() + pad, card.width() - pad * 2, 48),
                      theme(), QStringLiteral("󰐲"),
                      m_ssid.isEmpty() ? QStringLiteral("Wi-Fi Share") : m_ssid,
                      m_password.isEmpty() ? m_statusText : QStringLiteral("Scan to join network"),
                      sc.text, muted, accent);

    const QRect qrBox(card.center().x() - 120, card.top() + 86, 240, 240);
    if (!m_qrImage.isNull()) {
        p.save();
        p.setBrush(Qt::white);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(qrBox, 12, 12);
        p.drawImage(qrBox.adjusted(8, 8, -8, -8), m_qrImage);
        p.restore();
    } else {
        p.setPen(muted);
        p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBody()));
        p.drawText(qrBox, Qt::AlignCenter, m_statusText);
    }

    const int footerY = card.bottom() - pad - 76;
    if (!m_password.isEmpty()) {
        p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBodySmall()));
        p.setPen(sc.text);
        const QString passShown = m_showPassword ? m_password : QString(m_password.size(), QChar(0x2022));
        p.drawText(QRect(card.left() + pad, footerY, card.width() - pad * 2, 24),
                   Qt::AlignCenter, QStringLiteral("Password: %1").arg(passShown));

        const int bw = 148;
        m_togglePassRect = QRect(card.left() + pad, footerY + 34, bw, 32);
        m_copyPassRect = QRect(card.right() - pad - bw, footerY + 34, bw, 32);
        UiPaint::drawPillButton(p, m_togglePassRect, theme(),
                                m_showPassword ? QStringLiteral("Hide Password") : QStringLiteral("Show Password"),
                                false, m_togglePassRect.contains(m_hoverPos));
        UiPaint::drawPillButton(p, m_copyPassRect, theme(), QStringLiteral("Copy Password"),
                                true, m_copyPassRect.contains(m_hoverPos));
    } else {
        m_togglePassRect = QRect();
        m_copyPassRect = QRect();
    }
}

void WifiQrOverlay::handleCardMousePress(QMouseEvent *event, const QRect & /*card*/) {
    if (m_togglePassRect.contains(event->pos())) {
        m_showPassword = !m_showPassword;
        update();
    } else if (m_copyPassRect.contains(event->pos()) && !m_password.isEmpty()) {
        auto *p = new QProcess(this);
        connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), p, &QObject::deleteLater);
        p->start(QStringLiteral("wl-copy"), {m_password});
    }
}

} // namespace quattro
