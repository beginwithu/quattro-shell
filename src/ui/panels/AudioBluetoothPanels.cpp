#include "AudioBluetoothPanels.h"

#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <algorithm>

namespace quattro {

// ---------------- AudioPanel ----------------

AudioPanel::AudioPanel(AudioService *audio,
                       ThemeManager *theme,
                       ShellConfig *config,
                       QWidget *parent)
    : ShellPopupWindow(theme, config, QStringLiteral("quattro-audio-panel"), false, parent)
    , m_audio(audio)
{
    connect(m_audio, &AudioService::audioChanged, this, [this]() {
        if (isOpened()) update();
    });
}

void AudioPanel::onOpening() {
    m_scrollOffset = 0;
    m_audio->refreshLists();
}

void AudioPanel::paintCardContent(QPainter &p, const QRect &card) {
    const SurfaceColors &sc = theme()->popups();
    const QColor muted = theme()->muted();
    const QColor accent = sc.accent.isValid() ? sc.accent : theme()->accent();
    const int pad = 16;
    const int innerW = card.width() - pad * 2;

    m_sinkRows.clear();
    m_sourceRows.clear();
    m_streamHits.clear();

    const QRect hero(card.left() + pad, card.top() + pad, innerW, 44);
    const QString ico = m_audio->sinkMuted() ? QStringLiteral("󰖁")
                                             : (m_audio->headphoneActive() ? QStringLiteral("󰋋") : QStringLiteral("󰕾"));
    UiPaint::drawHero(p, hero, theme(), ico,
                      QStringLiteral("Audio Mixer"),
                      QStringLiteral("%1% output • %2% mic")
                          .arg(m_audio->sinkVolume())
                          .arg(m_audio->sourceVolume()),
                      sc.text, muted, accent);

    const QRect viewport(card.left() + pad, hero.bottom() + 10, innerW, card.bottom() - pad - (hero.bottom() + 10));
    p.save();
    p.setClipRect(viewport);

    int y = viewport.top() - m_scrollOffset;
    const int left = viewport.left();

    // Output slider row
    UiPaint::drawSectionHeader(p, QRect(left, y, innerW, 20), theme(), QStringLiteral("Output Volume"), muted);
    y += 22;
    m_outMuteRect = QRect(left, y, 30, 26);
    UiPaint::drawPillButton(p, m_outMuteRect, theme(), m_audio->sinkMuted() ? QStringLiteral("󰖁") : QStringLiteral("󰕾"),
                            !m_audio->sinkMuted(), m_outMuteRect.contains(m_hoverPos));
    m_outSliderRect = QRect(m_outMuteRect.right() + 8, y + 1, innerW - 84, 24);
    UiPaint::drawSlider(p, m_outSliderRect, theme(), std::clamp(m_audio->sinkVolume(), 0, 150) / 100.0,
                        m_outSliderRect.contains(m_hoverPos));
    p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontCaption()));
    p.setPen(sc.text);
    p.drawText(QRect(m_outSliderRect.right() + 6, y, 42, 26), Qt::AlignVCenter | Qt::AlignRight,
               QStringLiteral("%1%").arg(m_audio->sinkVolume()));
    y += 34;

    for (const auto &s : m_audio->sinks()) {
        if (s.isMonitor) continue;
        const QRect r(left, y, innerW, 28);
        m_sinkRows.push_back({s.name, r});
        const bool hov = r.contains(m_hoverPos);
        if (s.isDefault) {
            p.setBrush(ThemeManager::withAlpha(accent, 0.18));
            p.setPen(QPen(accent, 1));
            p.drawRoundedRect(r, 7, 7);
        } else if (hov) {
            p.setBrush(ThemeManager::withAlpha(sc.text, 0.08));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(r, 7, 7);
        }
        p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBodySmall(),
                        s.isDefault ? QFont::DemiBold : QFont::Normal));
        p.setPen(s.isDefault ? accent : sc.text);
        const QString lbl = (s.isHeadphones ? QStringLiteral("󰋋 ") : QStringLiteral("󰓃 ")) + s.description;
        p.drawText(r.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft,
                   p.fontMetrics().elidedText(lbl, Qt::ElideRight, r.width() - 16));
        y += 32;
    }

    y += 6;
    UiPaint::drawSectionHeader(p, QRect(left, y, innerW, 20), theme(), QStringLiteral("Microphone Input"), muted);
    y += 22;
    m_inMuteRect = QRect(left, y, 30, 26);
    UiPaint::drawPillButton(p, m_inMuteRect, theme(), m_audio->sourceMuted() ? QStringLiteral("󰍭") : QStringLiteral("󰍬"),
                            !m_audio->sourceMuted(), m_inMuteRect.contains(m_hoverPos));
    m_inSliderRect = QRect(m_inMuteRect.right() + 8, y + 1, innerW - 84, 24);
    UiPaint::drawSlider(p, m_inSliderRect, theme(), std::clamp(m_audio->sourceVolume(), 0, 100) / 100.0,
                        m_inSliderRect.contains(m_hoverPos));
    p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontCaption()));
    p.setPen(sc.text);
    p.drawText(QRect(m_inSliderRect.right() + 6, y, 42, 26), Qt::AlignVCenter | Qt::AlignRight,
               QStringLiteral("%1%").arg(m_audio->sourceVolume()));
    y += 34;

    for (const auto &src : m_audio->sources()) {
        if (src.isMonitor) continue;
        const QRect r(left, y, innerW, 28);
        m_sourceRows.push_back({src.name, r});
        const bool hov = r.contains(m_hoverPos);
        if (src.isDefault) {
            p.setBrush(ThemeManager::withAlpha(accent, 0.18));
            p.setPen(QPen(accent, 1));
            p.drawRoundedRect(r, 7, 7);
        } else if (hov) {
            p.setBrush(ThemeManager::withAlpha(sc.text, 0.08));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(r, 7, 7);
        }
        p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBodySmall(),
                        src.isDefault ? QFont::DemiBold : QFont::Normal));
        p.setPen(src.isDefault ? accent : sc.text);
        p.drawText(r.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft,
                   p.fontMetrics().elidedText(QStringLiteral("󰍬 ") + src.description, Qt::ElideRight, r.width() - 16));
        y += 32;
    }

    const auto streams = m_audio->sinkInputs();
    if (!streams.isEmpty()) {
        y += 6;
        UiPaint::drawSectionHeader(p, QRect(left, y, innerW, 20), theme(), QStringLiteral("App Streams"), muted);
        y += 22;
        for (const auto &st : streams) {
            p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontCaption(), QFont::DemiBold));
            p.setPen(sc.text);
            const QString appTitle = st.appName.isEmpty() ? st.binary : st.appName;
            p.drawText(QRect(left, y, innerW, 18), Qt::AlignVCenter | Qt::AlignLeft,
                       p.fontMetrics().elidedText(appTitle + (st.mediaName.isEmpty() ? QString() : QStringLiteral(" — ") + st.mediaName),
                                                  Qt::ElideRight, innerW));
            y += 20;
            const QRect mRect(left, y, 28, 24);
            const QRect sRect(mRect.right() + 8, y, innerW - 76, 24);
            m_streamHits.push_back({st.index, sRect, mRect});
            UiPaint::drawPillButton(p, mRect, theme(), st.muted ? QStringLiteral("󰖁") : QStringLiteral("󰕾"),
                                    !st.muted, mRect.contains(m_hoverPos));
            UiPaint::drawSlider(p, sRect, theme(), std::clamp(st.volume, 0, 100) / 100.0, sRect.contains(m_hoverPos));
            p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontCaption()));
            p.setPen(muted);
            p.drawText(QRect(sRect.right() + 6, y, 36, 24), Qt::AlignVCenter | Qt::AlignRight,
                       QStringLiteral("%1%").arg(st.volume));
            y += 30;
        }
    }

    p.restore();
    const int contentHeight = (y + m_scrollOffset) - viewport.top();
    m_maxScroll = std::max(0, contentHeight - viewport.height());
    m_scrollOffset = std::clamp(m_scrollOffset, 0, m_maxScroll);
}

void AudioPanel::applySliderDrag(const QPoint &pos) {
    if (m_activeSliderRect.width() <= 0)
        return;
    const double f = std::clamp((pos.x() - m_activeSliderRect.left()) / double(m_activeSliderRect.width()), 0.0, 1.0);
    const int pct = qRound(f * 100.0);
    if (m_dragTarget == DragTarget::SinkVol)
        m_audio->setSinkVolume(pct);
    else if (m_dragTarget == DragTarget::SourceVol)
        m_audio->setSourceVolume(pct);
    else if (m_dragTarget == DragTarget::StreamVol)
        m_audio->setSinkInputVolume(m_dragStreamIdx, pct);
}

void AudioPanel::handleCardMousePress(QMouseEvent *event, const QRect & /*card*/) {
    const QPoint pos = event->pos();
    if (m_outMuteRect.contains(pos)) { m_audio->toggleSinkMute(); return; }
    if (m_inMuteRect.contains(pos)) { m_audio->toggleSourceMute(); return; }
    if (m_outSliderRect.contains(pos)) {
        m_dragTarget = DragTarget::SinkVol;
        m_activeSliderRect = m_outSliderRect;
        applySliderDrag(pos);
        return;
    }
    if (m_inSliderRect.contains(pos)) {
        m_dragTarget = DragTarget::SourceVol;
        m_activeSliderRect = m_inSliderRect;
        applySliderDrag(pos);
        return;
    }
    for (const auto &s : m_sinkRows) {
        if (s.second.contains(pos)) { m_audio->setDefaultSink(s.first); return; }
    }
    for (const auto &src : m_sourceRows) {
        if (src.second.contains(pos)) { m_audio->setDefaultSource(src.first); return; }
    }
    for (const auto &sh : m_streamHits) {
        if (sh.mute.contains(pos)) {
            for (const auto &st : m_audio->sinkInputs()) {
                if (st.index == sh.idx) { m_audio->setSinkInputMute(sh.idx, !st.muted); break; }
            }
            return;
        }
        if (sh.slider.contains(pos)) {
            m_dragTarget = DragTarget::StreamVol;
            m_dragStreamIdx = sh.idx;
            m_activeSliderRect = sh.slider;
            applySliderDrag(pos);
            return;
        }
    }
}

void AudioPanel::handleCardMouseMove(QMouseEvent *event, const QRect &card) {
    ShellPopupWindow::handleCardMouseMove(event, card);
    if (m_dragTarget != DragTarget::None)
        applySliderDrag(event->pos());
}

void AudioPanel::handleCardMouseRelease(QMouseEvent *event, const QRect &card) {
    Q_UNUSED(event); Q_UNUSED(card);
    m_dragTarget = DragTarget::None;
}

// ---------------- BluetoothPanel ----------------

BluetoothPanel::BluetoothPanel(ThemeManager *theme, ShellConfig *config, QWidget *parent)
    : ShellPopupWindow(theme, config, QStringLiteral("quattro-bluetooth-panel"), false, parent)
{}

void BluetoothPanel::onOpening() {
    m_scrollOffset = 0;
    refreshState();
}

void BluetoothPanel::refreshState() {
    auto *p1 = new QProcess(this);
    connect(p1, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, p1](int, QProcess::ExitStatus) {
        const QString showOut = QString::fromUtf8(p1->readAllStandardOutput());
        p1->deleteLater();
        m_powered = showOut.contains(QStringLiteral("Powered: yes"));
        m_scanning = showOut.contains(QStringLiteral("Discovering: yes"));

        auto *p2 = new QProcess(this);
        connect(p2, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, p2](int, QProcess::ExitStatus) {
            const QString devOut = QString::fromUtf8(p2->readAllStandardOutput());
            p2->deleteLater();
            QVector<BluetoothDeviceItem> parsed;
            const QStringList lines = devOut.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            for (const QString &l : lines) {
                if (!l.startsWith(QStringLiteral("Device "))) continue;
                const QString rest = l.mid(7).trimmed();
                const int sp = rest.indexOf(QLatin1Char(' '));
                if (sp <= 0) continue;
                BluetoothDeviceItem item;
                item.mac = rest.left(sp);
                item.name = rest.mid(sp + 1).trimmed();
                parsed.push_back(item);
            }
            m_devices = parsed;
            // Query connection status per device
            for (int i = 0; i < m_devices.size(); ++i) {
                const QString mac = m_devices.at(i).mac;
                auto *pi = new QProcess(this);
                connect(pi, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                        this, [this, pi, mac](int, QProcess::ExitStatus) {
                    const QString info = QString::fromUtf8(pi->readAllStandardOutput());
                    pi->deleteLater();
                    for (auto &d : m_devices) {
                        if (d.mac == mac) {
                            d.paired = info.contains(QStringLiteral("Paired: yes"));
                            d.connected = info.contains(QStringLiteral("Connected: yes"));
                            d.trusted = info.contains(QStringLiteral("Trusted: yes"));
                            static const QRegularExpression batRe(QStringLiteral("Battery Percentage:.*\\((\\d+)\\)"));
                            const auto m = batRe.match(info);
                            if (m.hasMatch()) d.batteryPercent = m.captured(1).toInt();
                        }
                    }
                    std::stable_sort(m_devices.begin(), m_devices.end(), [](const auto &a, const auto &b) {
                        if (a.connected != b.connected) return a.connected > b.connected;
                        if (a.paired != b.paired) return a.paired > b.paired;
                        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
                    });
                    if (isOpened()) update();
                });
                pi->start(QStringLiteral("bluetoothctl"), {QStringLiteral("info"), mac});
            }
            if (isOpened()) update();
        });
        p2->start(QStringLiteral("bluetoothctl"), {QStringLiteral("devices")});
    });
    p1->start(QStringLiteral("bluetoothctl"), {QStringLiteral("show")});
}

void BluetoothPanel::togglePower() {
    m_busy = true;
    auto *p = new QProcess(this);
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, p](int, QProcess::ExitStatus) {
        p->deleteLater();
        m_busy = false;
        refreshState();
    });
    p->start(QStringLiteral("bluetoothctl"), {QStringLiteral("power"), m_powered ? QStringLiteral("off") : QStringLiteral("on")});
}

void BluetoothPanel::startScan() {
    if (m_scanning) return;
    m_scanning = true;
    m_statusText = QStringLiteral("Scanning for nearby devices...");
    update();
    auto *p = new QProcess(this);
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, p](int, QProcess::ExitStatus) {
        p->deleteLater();
        m_scanning = false;
        m_statusText.clear();
        refreshState();
    });
    p->start(QStringLiteral("bluetoothctl"), {QStringLiteral("--timeout"), QStringLiteral("8"), QStringLiteral("scan"), QStringLiteral("on")});
}

void BluetoothPanel::toggleDevice(const BluetoothDeviceItem &dev) {
    m_statusText = (dev.connected ? QStringLiteral("Disconnecting ") : QStringLiteral("Connecting ")) + dev.name + QStringLiteral("...");
    update();
    auto *p = new QProcess(this);
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, p](int, QProcess::ExitStatus) {
        p->deleteLater();
        m_statusText.clear();
        refreshState();
    });
    p->start(QStringLiteral("bluetoothctl"), {dev.connected ? QStringLiteral("disconnect") : QStringLiteral("connect"), dev.mac});
}

void BluetoothPanel::paintCardContent(QPainter &p, const QRect &card) {
    const SurfaceColors &sc = theme()->popups();
    const QColor muted = theme()->muted();
    const QColor accent = sc.accent.isValid() ? sc.accent : theme()->accent();
    const int pad = 16;
    const int innerW = card.width() - pad * 2;

    const QRect hero(card.left() + pad, card.top() + pad, innerW - 50, 44);
    UiPaint::drawHero(p, hero, theme(),
                      m_powered ? QStringLiteral("󰂯") : QStringLiteral("󰂲"),
                      QStringLiteral("Bluetooth"),
                      m_powered ? QStringLiteral("Powered On") : QStringLiteral("Powered Off"),
                      sc.text, muted, accent);
    m_powerToggleRect = QRect(card.right() - pad - 42, card.top() + pad + 11, 42, 22);
    UiPaint::drawToggleSwitch(p, m_powerToggleRect, theme(), m_powered, m_powerToggleRect.contains(m_hoverPos), m_busy);

    m_scanBtnRect = QRect(card.left() + pad, hero.bottom() + 10, innerW, 30);
    UiPaint::drawPillButton(p, m_scanBtnRect, theme(),
                            m_scanning ? QStringLiteral("Scanning...") : QStringLiteral("Scan Devices"),
                            m_scanning, m_scanBtnRect.contains(m_hoverPos));

    const QRect listR(card.left() + pad, m_scanBtnRect.bottom() + 10, innerW, card.bottom() - pad - (m_scanBtnRect.bottom() + 10));
    m_devRows.clear();
    p.save();
    p.setClipRect(listR);

    if (!m_powered) {
        p.setPen(muted);
        p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBody()));
        p.drawText(listR, Qt::AlignCenter, QStringLiteral("Bluetooth is turned off"));
    } else if (m_devices.isEmpty()) {
        p.setPen(muted);
        p.drawText(listR, Qt::AlignCenter, QStringLiteral("No paired or nearby devices"));
    } else {
        const int rowH = 44;
        m_maxScroll = std::max(0, static_cast<int>(m_devices.size()) * rowH - listR.height());
        m_scrollOffset = std::clamp(m_scrollOffset, 0, m_maxScroll);

        for (int i = 0; i < m_devices.size(); ++i) {
            const int y = listR.top() + i * rowH - m_scrollOffset;
            if (y + rowH < listR.top() || y > listR.bottom()) continue;
            const QRect r(listR.left(), y, listR.width(), rowH - 4);
            m_devRows.push_back({i, r});
            const auto &d = m_devices.at(i);
            if (d.connected) {
                p.setBrush(ThemeManager::withAlpha(accent, 0.18));
                p.setPen(QPen(accent, 1));
                p.drawRoundedRect(r, 8, 8);
            } else if (r.contains(m_hoverPos)) {
                p.setBrush(ThemeManager::withAlpha(sc.text, 0.08));
                p.setPen(Qt::NoPen);
                p.drawRoundedRect(r, 8, 8);
            }
            p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBody(), d.connected ? QFont::DemiBold : QFont::Normal));
            p.setPen(d.connected ? accent : sc.text);
            p.drawText(r.adjusted(10, 4, -70, -18), Qt::AlignVCenter | Qt::AlignLeft,
                       p.fontMetrics().elidedText(d.name, Qt::ElideRight, r.width() - 80));

            p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontCaption()));
            p.setPen(muted);
            QString sub = d.connected ? QStringLiteral("Connected") : (d.paired ? QStringLiteral("Paired") : d.mac);
            if (d.batteryPercent >= 0)
                sub += QStringLiteral(" • %1%").arg(d.batteryPercent);
            p.drawText(r.adjusted(10, 22, -70, -2), Qt::AlignVCenter | Qt::AlignLeft, sub);

            p.setPen(d.connected ? accent : muted);
            p.drawText(r.adjusted(0, 0, -10, 0), Qt::AlignVCenter | Qt::AlignRight,
                       d.connected ? QStringLiteral("Disconnect") : QStringLiteral("Connect"));
        }
    }
    p.restore();
}

void BluetoothPanel::handleCardMousePress(QMouseEvent *event, const QRect & /*card*/) {
    if (m_powerToggleRect.contains(event->pos())) { togglePower(); return; }
    if (m_scanBtnRect.contains(event->pos()) && m_powered) { startScan(); return; }
    for (const auto &pair : m_devRows) {
        if (pair.second.contains(event->pos()) && pair.first < m_devices.size()) {
            toggleDevice(m_devices.at(pair.first));
            return;
        }
    }
}

} // namespace quattro
