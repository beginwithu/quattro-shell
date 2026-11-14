#include "ReminderOverlay.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>

namespace quattro {

ReminderOverlay::ReminderOverlay(ThemeManager *theme, ShellConfig *config, QWidget *parent)
    : ShellPopupWindow(theme, config, QStringLiteral("quattro-reminders"), true, parent)
{}

void ReminderOverlay::onOpening() {
    m_step = 0;
    m_timeInput = QStringLiteral("15m");
    m_messageInput.clear();
    m_errorText.clear();
}

int ReminderOverlay::parseMinutes(const QString &raw) {
    const QString s = raw.trimmed().toLower();
    if (s.isEmpty())
        return 0;
    bool ok = false;
    int plain = s.toInt(&ok);
    if (ok && plain > 0)
        return plain;

    static const QRegularExpression re(QStringLiteral("(?:(\\d+)h)?\\s*(?:(\\d+)m)?"));
    const QRegularExpressionMatch m = re.match(s);
    if (m.hasMatch()) {
        const int h = m.captured(1).toInt();
        const int mins = m.captured(2).toInt();
        const int total = h * 60 + mins;
        if (total > 0)
            return total;
    }
    return 0;
}

void ReminderOverlay::submit() {
    const int mins = parseMinutes(m_timeInput);
    if (mins <= 0) {
        m_errorText = QStringLiteral("Enter a valid duration (e.g. 10, 15m, 1h30m)");
        m_step = 0;
        update();
        return;
    }
    const QString msg = m_messageInput.trimmed().isEmpty()
                            ? QStringLiteral("Reminder")
                            : m_messageInput.trimmed();

    closePopup();

    if (!QProcess::startDetached(QStringLiteral("quattro-reminder"),
                                 {QString::number(mins), msg})) {
        QTimer::singleShot(mins * 60000, this, [msg]() {
            QProcess::startDetached(QStringLiteral("quattro-notification-send"),
                                    {QStringLiteral("Reminder"), msg});
        });
    }
}

void ReminderOverlay::paintCardContent(QPainter &p, const QRect &card) {
    const SurfaceColors &sc = theme()->popups();
    const QColor muted = theme()->muted();
    const QColor accent = sc.accent.isValid() ? sc.accent : theme()->accent();

    const int pad = 20;
    const int w = card.width() - pad * 2;
    int y = card.top() + pad;

    UiPaint::drawHero(p, QRect(card.left() + pad, y, w, 48), theme(),
                      QStringLiteral("󰀠"),
                      m_step == 0 ? QStringLiteral("Set Reminder Timer")
                                  : QStringLiteral("Reminder Message"),
                      m_step == 0 ? QStringLiteral("Step 1 of 2 • Choose duration")
                                  : QStringLiteral("Step 2 of 2 • In %1 min").arg(parseMinutes(m_timeInput)),
                      sc.text, muted, accent);
    y += 60;

    // Input field
    const QRect inputRect(card.left() + pad, y, w, 40);
    p.setBrush(ThemeManager::withAlpha(sc.text, 0.08));
    p.setPen(QPen(accent, 1.5));
    p.drawRoundedRect(inputRect, 8, 8);
    p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBody()));
    const QString &cur = (m_step == 0) ? m_timeInput : m_messageInput;
    const QString ph = (m_step == 0) ? QStringLiteral("Minutes or duration (15m, 1h)")
                                     : QStringLiteral("What should I remind you about?");
    p.setPen(cur.isEmpty() ? muted : sc.text);
    p.drawText(inputRect.adjusted(12, 0, -12, 0), Qt::AlignVCenter | Qt::AlignLeft, cur.isEmpty() ? ph : cur);
    y += 50;

    m_presetButtons.clear();
    if (m_step == 0) {
        const QStringList presets = {QStringLiteral("5m"), QStringLiteral("10m"), QStringLiteral("15m"),
                                     QStringLiteral("30m"), QStringLiteral("1h"), QStringLiteral("2h")};
        const int bw = (w - (presets.size() - 1) * 6) / presets.size();
        for (int i = 0; i < presets.size(); ++i) {
            const QRect r(card.left() + pad + i * (bw + 6), y, bw, 30);
            m_presetButtons.push_back({presets.at(i), r});
            UiPaint::drawPillButton(p, r, theme(), presets.at(i),
                                    m_timeInput == presets.at(i), r.contains(m_hoverPos));
        }
        y += 38;
    }

    if (!m_errorText.isEmpty()) {
        p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontCaption()));
        p.setPen(theme()->urgent());
        p.drawText(QRect(card.left() + pad, y, w, 18), Qt::AlignLeft | Qt::AlignVCenter, m_errorText);
    }

    const int btnW = 104;
    const int btnY = card.bottom() - pad - 32;
    m_nextBtnRect = QRect(card.right() - pad - btnW, btnY, btnW, 32);
    UiPaint::drawPillButton(p, m_nextBtnRect, theme(),
                            m_step == 0 ? QStringLiteral("Next") : QStringLiteral("Schedule"),
                            true, m_nextBtnRect.contains(m_hoverPos));

    m_backBtnRect = QRect(m_nextBtnRect.left() - 8 - btnW, btnY, btnW, 32);
    if (m_step == 1)
        UiPaint::drawPillButton(p, m_backBtnRect, theme(), QStringLiteral("Back"),
                                false, m_backBtnRect.contains(m_hoverPos));
}

bool ReminderOverlay::handleKey(QKeyEvent *event) {
    const int k = event->key();
    if (k == Qt::Key_Return || k == Qt::Key_Enter) {
        if (m_step == 0) {
            if (parseMinutes(m_timeInput) > 0) {
                m_errorText.clear();
                m_step = 1;
                update();
            } else {
                m_errorText = QStringLiteral("Enter a valid duration (e.g. 15m)");
                update();
            }
        } else {
            submit();
        }
        return true;
    }
    if (k == Qt::Key_Backspace) {
        QString &target = (m_step == 0) ? m_timeInput : m_messageInput;
        if (!target.isEmpty()) {
            target.chop(1);
            update();
        } else if (m_step == 1) {
            m_step = 0;
            update();
        }
        return true;
    }
    const QString t = event->text();
    if (!t.isEmpty() && t.at(0).isPrint()) {
        QString &target = (m_step == 0) ? m_timeInput : m_messageInput;
        target += t;
        m_errorText.clear();
        update();
        return true;
    }
    return false;
}

void ReminderOverlay::handleCardMousePress(QMouseEvent *event, const QRect & /*card*/) {
    if (m_nextBtnRect.contains(event->pos())) {
        if (m_step == 0) {
            if (parseMinutes(m_timeInput) > 0) {
                m_errorText.clear();
                m_step = 1;
                update();
            }
        } else {
            submit();
        }
        return;
    }
    if (m_step == 1 && m_backBtnRect.contains(event->pos())) {
        m_step = 0;
        update();
        return;
    }
    if (m_step == 0) {
        for (const auto &pair : m_presetButtons) {
            if (pair.second.contains(event->pos())) {
                m_timeInput = pair.first;
                m_errorText.clear();
                m_step = 1;
                update();
                return;
            }
        }
    }
}

} // namespace quattro
