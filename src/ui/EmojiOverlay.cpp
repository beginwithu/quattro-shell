#include "EmojiOverlay.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QProcess>
#include <algorithm>

namespace quattro {

EmojiOverlay::EmojiOverlay(ThemeManager *theme, ShellConfig *config, QWidget *parent)
    : ShellPopupWindow(theme, config, QStringLiteral("quattro-emojis"), true, parent)
{}

void EmojiOverlay::ensureLoaded() {
    if (m_loaded)
        return;
    m_loaded = true;
    QFile f(QStringLiteral(":/assets/emojis.json"));
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray())
        return;
    const QJsonArray arr = doc.array();
    m_all.reserve(arr.size());
    for (const QJsonValue &v : arr) {
        const QJsonObject obj = v.toObject();
        const QString e = obj.value(QStringLiteral("e")).toString();
        if (!e.isEmpty())
            m_all.push_back({e, obj.value(QStringLiteral("k")).toString()});
    }
}

void EmojiOverlay::onOpening() {
    ensureLoaded();
    m_query.clear();
    m_scrollOffset = 0;
    m_selected = 0;
    refilter();
}

void EmojiOverlay::refilter() {
    m_filtered.clear();
    const QString needle = m_query.trimmed();
    for (int i = 0; i < m_all.size(); ++i) {
        if (needle.isEmpty() ||
            m_all.at(i).keywords.contains(needle, Qt::CaseInsensitive) ||
            m_all.at(i).emoji.contains(needle)) {
            m_filtered.push_back(i);
            if (needle.isEmpty() && m_filtered.size() >= 300)
                break; // Fast initial view; search queries inspect all entries
        }
    }
    if (m_filtered.isEmpty())
        m_selected = 0;
    else
        m_selected = std::clamp(m_selected, 0, static_cast<int>(m_filtered.size()) - 1);
}

void EmojiOverlay::insertEmoji(const QString &emoji) {
    closePopup();
    if (!QProcess::startDetached(QStringLiteral("quattro-menu-emoji-insert"), {emoji})) {
        auto *p = new QProcess(this);
        connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), p, &QObject::deleteLater);
        p->start(QStringLiteral("wl-copy"), {emoji});
    }
}

void EmojiOverlay::paintCardContent(QPainter &p, const QRect &card) {
    const SurfaceColors &sc = theme()->popups();
    const QColor muted = theme()->muted();
    const QColor accent = sc.accent.isValid() ? sc.accent : theme()->accent();

    const int pad = 16;
    const QRect searchRect(card.left() + pad, card.top() + pad, card.width() - pad * 2, 38);
    UiPaint::drawSearchHeader(p, searchRect, theme(), m_query,
                              QStringLiteral("Search emojis..."),
                              sc.text, muted, accent);

    m_gridRect = QRect(card.left() + pad, searchRect.bottom() + 10,
                       card.width() - pad * 2, card.bottom() - pad - 26 - (searchRect.bottom() + 10));
    m_cols = std::max(1, m_gridRect.width() / m_cellSize);

    const int rows = (m_filtered.size() + m_cols - 1) / m_cols;
    m_maxScroll = std::max(0, rows * m_cellSize - m_gridRect.height());
    m_scrollOffset = std::clamp(m_scrollOffset, 0, m_maxScroll);

    p.save();
    p.setClipRect(m_gridRect);
    if (m_filtered.isEmpty()) {
        p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBody()));
        p.setPen(muted);
        p.drawText(m_gridRect, Qt::AlignCenter, QStringLiteral("No matching emojis"));
    } else {
        QFont emojiFont(QStringLiteral("Noto Color Emoji"), 20);
        p.setFont(emojiFont);
        for (int idx = 0; idx < m_filtered.size(); ++idx) {
            const int r = idx / m_cols;
            const int c = idx % m_cols;
            const int y = m_gridRect.top() + r * m_cellSize - m_scrollOffset;
            if (y + m_cellSize < m_gridRect.top() || y > m_gridRect.bottom())
                continue;
            const QRect cell(m_gridRect.left() + c * m_cellSize, y, m_cellSize - 4, m_cellSize - 4);
            const bool sel = (idx == m_selected);
            const bool hov = cell.contains(m_hoverPos);
            if (sel) {
                p.setBrush(ThemeManager::withAlpha(accent, 0.25));
                p.setPen(QPen(accent, 1.5));
                p.drawRoundedRect(cell, 8, 8);
            } else if (hov) {
                p.setBrush(ThemeManager::withAlpha(sc.text, 0.09));
                p.setPen(Qt::NoPen);
                p.drawRoundedRect(cell, 8, 8);
            }
            p.setPen(sc.text);
            p.drawText(cell, Qt::AlignCenter, m_all.at(m_filtered.at(idx)).emoji);
        }
    }
    p.restore();

    const QRect statusRect(card.left() + pad, card.bottom() - pad - 20, card.width() - pad * 2, 20);
    p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontCaption()));
    p.setPen(muted);
    QString caption = QStringLiteral("Type to search • Enter to insert");
    if (!m_filtered.isEmpty() && m_selected < m_filtered.size())
        caption = m_all.at(m_filtered.at(m_selected)).keywords;
    p.drawText(statusRect, Qt::AlignVCenter | Qt::AlignLeft,
               p.fontMetrics().elidedText(caption, Qt::ElideRight, statusRect.width()));
}

bool EmojiOverlay::handleKey(QKeyEvent *event) {
    const int k = event->key();
    auto ensureVisible = [this]() {
        const int r = m_selected / std::max(1, m_cols);
        const int top = r * m_cellSize;
        if (top < m_scrollOffset) m_scrollOffset = top;
        if (top + m_cellSize > m_scrollOffset + m_gridRect.height())
            m_scrollOffset = std::max(0, top + m_cellSize - m_gridRect.height());
    };

    if (k == Qt::Key_Right && m_selected + 1 < m_filtered.size()) {
        ++m_selected; ensureVisible(); update(); return true;
    }
    if (k == Qt::Key_Left && m_selected > 0) {
        --m_selected; ensureVisible(); update(); return true;
    }
    if (k == Qt::Key_Down && m_selected + m_cols < m_filtered.size()) {
        m_selected += m_cols; ensureVisible(); update(); return true;
    }
    if (k == Qt::Key_Up && m_selected - m_cols >= 0) {
        m_selected -= m_cols; ensureVisible(); update(); return true;
    }
    if (k == Qt::Key_Return || k == Qt::Key_Enter) {
        if (!m_filtered.isEmpty())
            insertEmoji(m_all.at(m_filtered.at(m_selected)).emoji);
        return true;
    }
    if (k == Qt::Key_Backspace) {
        if (!m_query.isEmpty()) {
            m_query.chop(1);
            m_scrollOffset = 0;
            refilter();
            update();
        }
        return true;
    }
    const QString t = event->text();
    if (!t.isEmpty() && t.at(0).isPrint()) {
        m_query += t;
        m_scrollOffset = 0;
        refilter();
        update();
        return true;
    }
    return false;
}

void EmojiOverlay::handleCardMousePress(QMouseEvent *event, const QRect & /*card*/) {
    if (!m_gridRect.contains(event->pos()))
        return;
    const int c = (event->pos().x() - m_gridRect.left()) / m_cellSize;
    const int r = (event->pos().y() - m_gridRect.top() + m_scrollOffset) / m_cellSize;
    const int idx = r * m_cols + c;
    if (c >= 0 && c < m_cols && idx >= 0 && idx < m_filtered.size())
        insertEmoji(m_all.at(m_filtered.at(idx)).emoji);
}

} // namespace quattro
