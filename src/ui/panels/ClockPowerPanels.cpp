#include "ClockPowerPanels.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QPainter>
#include <QProcess>

namespace quattro {

// ---------------- ClockPanel ----------------

ClockPanel::ClockPanel(ThemeManager *theme, ShellConfig *config, QWidget *parent)
    : ShellPopupWindow(theme, config, QStringLiteral("quattro-clock-panel"), false, parent)
    , m_displayMonth(QDate::currentDate().year(), QDate::currentDate().month(), 1)
{
    m_tick.setInterval(1000);
    connect(&m_tick, &QTimer::timeout, this, [this]() {
        if (isOpened()) update();
    });
}

void ClockPanel::onOpening() {
    const QDate now = QDate::currentDate();
    m_displayMonth = QDate(now.year(), now.month(), 1);
    m_tick.start();

    // Optional memento mori birthdate (~/.local/state/quattro/settings/clock.json)
    m_birthDate = QDate();
    QFile f(QDir::homePath() + QStringLiteral("/.local/state/quattro/settings/clock.json"));
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        const QString bd = o.value(QStringLiteral("birthdate")).toString();
        if (!bd.isEmpty())
            m_birthDate = QDate::fromString(bd, Qt::ISODate);
        const int ls = o.value(QStringLiteral("lifespan")).toInt(80);
        if (ls > 0) m_lifespanYears = ls;
    }
}

void ClockPanel::onClosing() {
    m_tick.stop();
}

void ClockPanel::paintCardContent(QPainter &p, const QRect &card) {
    const SurfaceColors &sc = theme()->popups();
    const QColor muted = theme()->muted();
    const QColor accent = sc.accent.isValid() ? sc.accent : theme()->accent();
    const int pad = 16;
    const int innerW = card.width() - pad * 2;

    const QDateTime dt = QDateTime::currentDateTime();
    UiPaint::drawHero(p, QRect(card.left() + pad, card.top() + pad, innerW, 50),
                      theme(), QStringLiteral("󰥔"),
                      dt.toString(QStringLiteral("HH:mm:ss")),
                      dt.toString(QStringLiteral("dddd, MMMM d yyyy")),
                      sc.text, muted, accent);

    // Month header + navigation
    const int navY = card.top() + pad + 60;
    m_prevMonthRect = QRect(card.left() + pad, navY, 32, 26);
    m_nextMonthRect = QRect(card.right() - pad - 32, navY, 32, 26);
    m_todayRect = QRect(m_prevMonthRect.right() + 8, navY, m_nextMonthRect.left() - m_prevMonthRect.right() - 16, 26);

    UiPaint::drawPillButton(p, m_prevMonthRect, theme(), QStringLiteral("‹"), false, m_prevMonthRect.contains(m_hoverPos));
    UiPaint::drawPillButton(p, m_todayRect, theme(), m_displayMonth.toString(QStringLiteral("MMMM yyyy")),
                            false, m_todayRect.contains(m_hoverPos));
    UiPaint::drawPillButton(p, m_nextMonthRect, theme(), QStringLiteral("›"), false, m_nextMonthRect.contains(m_hoverPos));

    // Weekday + ISO week header
    const int gridTop = navY + 34;
    const int colW = innerW / 8; // 1 ISO week col + 7 days
    const QStringList hdr = {QStringLiteral("Wk"), QStringLiteral("Mo"), QStringLiteral("Tu"),
                             QStringLiteral("We"), QStringLiteral("Th"), QStringLiteral("Fr"),
                             QStringLiteral("Sa"), QStringLiteral("Su")};
    p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontCaption(), QFont::DemiBold));
    p.setPen(muted);
    for (int c = 0; c < 8; ++c)
        p.drawText(QRect(card.left() + pad + c * colW, gridTop, colW, 22), Qt::AlignCenter, hdr.at(c));

    // Calendar cells (6 weeks)
    const int firstDow = m_displayMonth.dayOfWeek(); // 1=Mon..7=Sun
    QDate cursor = m_displayMonth.addDays(-(firstDow - 1));
    const QDate today = QDate::currentDate();
    const int rowH = 30;
    for (int r = 0; r < 6; ++r) {
        const int y = gridTop + 24 + r * rowH;
        const int isoWk = cursor.weekNumber();
        p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontCaption()));
        p.setPen(muted);
        p.drawText(QRect(card.left() + pad, y, colW, rowH), Qt::AlignCenter, QString::number(isoWk));

        for (int c = 1; c < 8; ++c) {
            const QRect cell(card.left() + pad + c * colW + 2, y + 2, colW - 4, rowH - 4);
            const bool isCurrentMonth = (cursor.month() == m_displayMonth.month());
            const bool isToday = (cursor == today);
            if (isToday) {
                p.setBrush(accent);
                p.setPen(Qt::NoPen);
                p.drawRoundedRect(cell, 7, 7);
                p.setPen(theme()->background());
                p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBodySmall(), QFont::Bold));
            } else {
                p.setPen(isCurrentMonth ? sc.text : ThemeManager::withAlpha(muted, 0.45));
                p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBodySmall()));
            }
            p.drawText(cell, Qt::AlignCenter, QString::number(cursor.day()));
            cursor = cursor.addDays(1);
        }
    }

    // Year progress / Memento Mori footer bar
    const int footerY = card.bottom() - pad - 44;
    double frac = today.dayOfYear() / double(today.daysInYear());
    QString lbl = QStringLiteral("Year %1: %2% complete (%3/%4)")
                      .arg(today.year())
                      .arg(qRound(frac * 100.0))
                      .arg(today.dayOfYear())
                      .arg(today.daysInYear());
    if (m_birthDate.isValid()) {
        const qint64 weeksLived = m_birthDate.daysTo(today) / 7;
        const qint64 totalWeeks = qint64(m_lifespanYears) * 52;
        frac = totalWeeks > 0 ? std::clamp(weeksLived / double(totalWeeks), 0.0, 1.0) : 0.0;
        lbl = QStringLiteral("Memento Mori: %1 / %2 weeks (%3%)")
                  .arg(weeksLived).arg(totalWeeks).arg(qRound(frac * 100.0));
    }
    p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontCaption()));
    p.setPen(muted);
    p.drawText(QRect(card.left() + pad, footerY, innerW, 20), Qt::AlignVCenter | Qt::AlignLeft, lbl);
    UiPaint::drawProgressBar(p, QRect(card.left() + pad, footerY + 24, innerW, 8), 4, frac,
                             ThemeManager::withAlpha(sc.text, 0.14), accent);
}

void ClockPanel::handleCardMousePress(QMouseEvent *event, const QRect & /*card*/) {
    if (m_prevMonthRect.contains(event->pos())) {
        m_displayMonth = m_displayMonth.addMonths(-1);
        update();
    } else if (m_nextMonthRect.contains(event->pos())) {
        m_displayMonth = m_displayMonth.addMonths(1);
        update();
    } else if (m_todayRect.contains(event->pos())) {
        const QDate now = QDate::currentDate();
        m_displayMonth = QDate(now.year(), now.month(), 1);
        update();
    }
}

// ---------------- PowerPanel ----------------

PowerPanel::PowerPanel(BatteryService *battery,
                       SystemStatsService *stats,
                       ThemeManager *theme,
                       ShellConfig *config,
                       QWidget *parent)
    : ShellPopupWindow(theme, config, QStringLiteral("quattro-power-panel"), false, parent)
    , m_battery(battery)
    , m_stats(stats)
{
    connect(m_battery, &BatteryService::profileChanged, this, [this]() {
        if (isOpened()) update();
    });
    connect(m_stats, &SystemStatsService::statsChanged, this, [this]() {
        if (isOpened()) update();
    });
}

void PowerPanel::onOpening() {
    m_battery->refreshPowerProfile();
    probeDetailedBattery();
}

void PowerPanel::probeDetailedBattery() {
    auto *p = new QProcess(this);
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, p](int exitCode, QProcess::ExitStatus) {
        const QByteArray out = p->readAllStandardOutput();
        p->deleteLater();
        if (exitCode == 0) {
            const QJsonObject obj = QJsonDocument::fromJson(out).object();
            if (obj.contains(QStringLiteral("power_w"))) m_powerWatts = obj.value(QStringLiteral("power_w")).toDouble(-1.0);
            if (obj.contains(QStringLiteral("health"))) m_healthPercent = obj.value(QStringLiteral("health")).toDouble(-1.0);
            if (obj.contains(QStringLiteral("cycles"))) m_cycleCount = obj.value(QStringLiteral("cycles")).toInt(-1);
            m_timeRemaining = obj.value(QStringLiteral("remaining")).toString();
            if (isOpened()) update();
        }
    });
    p->start(QStringLiteral("quattro-battery-status"), {QStringLiteral("--json")});
}

void PowerPanel::paintCardContent(QPainter &p, const QRect &card) {
    const SurfaceColors &sc = theme()->popups();
    const QColor muted = theme()->muted();
    const QColor accent = sc.accent.isValid() ? sc.accent : theme()->accent();
    const int pad = 16;
    const int innerW = card.width() - pad * 2;
    int y = card.top() + pad;

    const int pct = m_stats->batteryPercent();
    const bool charging = m_stats->batteryCharging();
    QString sub = charging ? QStringLiteral("Charging") : QStringLiteral("Discharging");
    if (!m_timeRemaining.isEmpty())
        sub += QStringLiteral(" • %1").arg(m_timeRemaining);
    if (!m_stats->batteryPresent())
        sub = QStringLiteral("AC Power");

    UiPaint::drawHero(p, QRect(card.left() + pad, y, innerW, 48), theme(),
                      charging ? QStringLiteral("󰂄") : QStringLiteral("󰁹"),
                      m_stats->batteryPresent() ? QStringLiteral("Battery %1%").arg(pct) : QStringLiteral("Power Management"),
                      sub, sc.text, muted, accent);
    y += 58;

    if (m_stats->batteryPresent()) {
        UiPaint::drawProgressBar(p, QRect(card.left() + pad, y, innerW, 10), 5, pct / 100.0,
                                 ThemeManager::withAlpha(sc.text, 0.14),
                                 pct <= 15 && !charging ? theme()->urgent() : accent);
        y += 18;
        QStringList meta;
        if (m_powerWatts >= 0.0) meta << QStringLiteral("Draw: %1 W").arg(m_powerWatts, 0, 'f', 1);
        if (m_healthPercent >= 0.0) meta << QStringLiteral("Health: %1%").arg(qRound(m_healthPercent));
        if (m_cycleCount >= 0) meta << QStringLiteral("Cycles: %1").arg(m_cycleCount);
        if (!meta.isEmpty()) {
            p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontCaption()));
            p.setPen(muted);
            p.drawText(QRect(card.left() + pad, y, innerW, 18), Qt::AlignLeft | Qt::AlignVCenter, meta.join(QStringLiteral("  •  ")));
            y += 24;
        }
    }

    UiPaint::drawSectionHeader(p, QRect(card.left() + pad, y, innerW, 20), theme(), QStringLiteral("Power Profile"), muted);
    y += 26;

    m_profileRects.clear();
    const QString active = m_battery->activePowerProfile();
    const QList<QPair<QString, QString>> profiles = {
        {QStringLiteral("power-saver"), QStringLiteral("Power Saver")},
        {QStringLiteral("balanced"), QStringLiteral("Balanced")},
        {QStringLiteral("performance"), QStringLiteral("Performance")}
    };
    const int pw = (innerW - 12) / 3;
    for (int i = 0; i < profiles.size(); ++i) {
        const QRect r(card.left() + pad + i * (pw + 6), y, pw, 32);
        m_profileRects.push_back({profiles.at(i).first, r});
        UiPaint::drawPillButton(p, r, theme(), profiles.at(i).second,
                                active == profiles.at(i).first, r.contains(m_hoverPos));
    }
    y += 46;

    UiPaint::drawSectionHeader(p, QRect(card.left() + pad, y, innerW, 20), theme(), QStringLiteral("Session Actions"), muted);
    y += 26;

    m_actionRects.clear();
    const QList<QPair<QString, QString>> actions = {
        {QStringLiteral("lock"), QStringLiteral("Lock")},
        {QStringLiteral("suspend"), QStringLiteral("Suspend")},
        {QStringLiteral("reboot"), QStringLiteral("Reboot")},
        {QStringLiteral("poweroff"), QStringLiteral("Shut Down")}
    };
    const int aw = (innerW - 8) / 2;
    for (int i = 0; i < actions.size(); ++i) {
        const int r = i / 2;
        const int c = i % 2;
        const QRect box(card.left() + pad + c * (aw + 8), y + r * 40, aw, 32);
        m_actionRects.push_back({actions.at(i).first, box});
        const bool danger = (actions.at(i).first == QLatin1String("reboot") || actions.at(i).first == QLatin1String("poweroff"));
        UiPaint::drawPillButton(p, box, theme(), actions.at(i).second, false, box.contains(m_hoverPos), danger);
    }
}

void PowerPanel::handleCardMousePress(QMouseEvent *event, const QRect & /*card*/) {
    for (const auto &pr : m_profileRects) {
        if (pr.second.contains(event->pos())) {
            m_battery->setPowerProfile(pr.first);
            update();
            return;
        }
    }
    for (const auto &ar : m_actionRects) {
        if (ar.second.contains(event->pos())) {
            const QString act = ar.first;
            if (act == QLatin1String("lock")) {
                closePopup();
                QProcess::startDetached(QStringLiteral("quattro-lock-screen"), {});
            } else if (act == QLatin1String("suspend")) {
                closePopup();
                QProcess::startDetached(QStringLiteral("systemctl"), {QStringLiteral("suspend")});
            } else {
                askConfirm(act == QLatin1String("reboot") ? QStringLiteral("Reboot system?")
                                                         : QStringLiteral("Shut down system?"),
                           QStringLiteral("All running applications will be closed."),
                           [this, act]() {
                    closePopup();
                    QProcess::startDetached(QStringLiteral("systemctl"), {act});
                });
            }
            return;
        }
    }
}

} // namespace quattro
