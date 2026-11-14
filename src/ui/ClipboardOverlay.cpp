#include "ClipboardOverlay.h"

#include <QImageReader>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>

namespace quattro {

ClipboardOverlay::ClipboardOverlay(ClipboardService *service,
                                   ThemeManager *theme,
                                   ShellConfig *config,
                                   QWidget *parent)
    : ShellPopupWindow(theme, config, QStringLiteral("quattro-clipboard"), true, parent)
    , m_service(service)
{
    connect(m_service, &ClipboardService::historyChanged, this, [this]() {
        refilter();
        if (isOpened())
            update();
    });
}

void ClipboardOverlay::onOpening() {
    m_query.clear();
    m_scrollOffset = 0;
    m_selectedPos = 0;
    refilter();
}

void ClipboardOverlay::refilter() {
    m_filteredIndices.clear();
    const auto &all = m_service->entries();
    const QString needle = m_query.trimmed();
    for (int i = 0; i < all.size(); ++i) {
        const auto &e = all.at(i);
        if (needle.isEmpty() ||
            e.text.contains(needle, Qt::CaseInsensitive) ||
            e.mime.contains(needle, Qt::CaseInsensitive) ||
            e.type.contains(needle, Qt::CaseInsensitive)) {
            m_filteredIndices.push_back(i);
        }
    }
    if (m_filteredIndices.isEmpty())
        m_selectedPos = 0;
    else
        m_selectedPos = std::clamp(m_selectedPos, 0, static_cast<int>(m_filteredIndices.size()) - 1);
}

QPixmap ClipboardOverlay::previewPixmapFor(const QString &path) {
    if (path.isEmpty())
        return {};
    auto it = m_thumbCache.find(path);
    if (it != m_thumbCache.end())
        return *it;
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage img = reader.read();
    QPixmap pix;
    if (!img.isNull())
        pix = QPixmap::fromImage(img.scaled(360, 320, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_thumbCache.insert(path, pix);
    return pix;
}

void ClipboardOverlay::paintCardContent(QPainter &p, const QRect &card) {
    const SurfaceColors &sc = theme()->popups();
    const QColor muted = theme()->muted();
    const QColor accent = sc.accent.isValid() ? sc.accent : theme()->accent();

    const int pad = 16;
    const QRect searchRect(card.left() + pad, card.top() + pad, card.width() - pad * 2 - 92, 38);
    UiPaint::drawSearchHeader(p, searchRect, theme(), m_query,
                              QStringLiteral("Search clipboard history..."),
                              sc.text, muted, accent);

    m_clearBtnRect = QRect(searchRect.right() + 8, searchRect.top() + 3, 84, 32);
    UiPaint::drawPillButton(p, m_clearBtnRect, theme(), QStringLiteral("Clear All"),
                            false, m_clearBtnRect.contains(m_hoverPos), true);

    const int bodyTop = searchRect.bottom() + 10;
    const int bodyH = card.bottom() - pad - 24 - bodyTop;
    const int halfW = (card.width() - pad * 3) / 2;

    m_listRect = QRect(card.left() + pad, bodyTop, halfW, bodyH);
    const QRect previewRect(m_listRect.right() + pad, bodyTop, halfW, bodyH);

    // Left list
    p.save();
    p.setClipRect(m_listRect);
    const auto &entries = m_service->entries();
    const int totalH = m_filteredIndices.size() * m_rowHeight;
    m_maxScroll = std::max(0, totalH - m_listRect.height());
    m_scrollOffset = std::clamp(m_scrollOffset, 0, m_maxScroll);

    if (m_filteredIndices.isEmpty()) {
        p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBody()));
        p.setPen(muted);
        p.drawText(m_listRect, Qt::AlignCenter, QStringLiteral("Clipboard history is empty"));
    } else {
        for (int idx = 0; idx < m_filteredIndices.size(); ++idx) {
            const int y = m_listRect.top() + idx * m_rowHeight - m_scrollOffset;
            if (y + m_rowHeight < m_listRect.top() || y > m_listRect.bottom())
                continue;
            const QRect row(m_listRect.left(), y, m_listRect.width(), m_rowHeight - 4);
            const bool selected = (idx == m_selectedPos);
            const bool hovered = row.contains(m_hoverPos);

            if (selected) {
                p.setBrush(ThemeManager::withAlpha(accent, 0.22));
                p.setPen(QPen(accent, 1));
                p.drawRoundedRect(row, 8, 8);
            } else if (hovered) {
                p.setBrush(ThemeManager::withAlpha(sc.text, 0.08));
                p.setPen(Qt::NoPen);
                p.drawRoundedRect(row, 8, 8);
            }

            const ClipboardEntry &e = entries.at(m_filteredIndices.at(idx));
            p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBodySmall(),
                            selected ? QFont::DemiBold : QFont::Normal));
            p.setPen(sc.text);
            QString label = (e.type == QLatin1String("image"))
                                ? QStringLiteral("[Image] %1").arg(e.mime)
                                : e.text.simplified();
            p.drawText(row.adjusted(10, 4, -10, -18), Qt::AlignVCenter | Qt::AlignLeft,
                       p.fontMetrics().elidedText(label, Qt::ElideRight, row.width() - 20));

            p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontCaption()));
            p.setPen(muted);
            p.drawText(row.adjusted(10, 22, -10, -2), Qt::AlignVCenter | Qt::AlignLeft, e.capturedAt);
        }
    }
    p.restore();

    // Right preview pane
    p.save();
    p.setBrush(ThemeManager::withAlpha(sc.text, 0.05));
    p.setPen(QPen(ThemeManager::withAlpha(sc.text, 0.12), 1));
    p.drawRoundedRect(previewRect, 10, 10);

    if (!m_filteredIndices.isEmpty() && m_selectedPos < m_filteredIndices.size()) {
        const ClipboardEntry &e = entries.at(m_filteredIndices.at(m_selectedPos));
        const QRect inner = previewRect.adjusted(12, 12, -12, -12);
        if (e.type == QLatin1String("image")) {
            QPixmap pix = previewPixmapFor(e.path);
            if (!pix.isNull()) {
                QPixmap fitted = pix.scaled(inner.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                const QPoint topLeft(inner.center().x() - fitted.width() / 2,
                                     inner.center().y() - fitted.height() / 2);
                p.drawPixmap(topLeft, fitted);
            } else {
                p.setPen(muted);
                p.drawText(inner, Qt::AlignCenter, QStringLiteral("Image unavailable"));
            }
        } else {
            p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBodySmall()));
            p.setPen(sc.text);
            QTextOption opt;
            opt.setWrapMode(QTextOption::WrapAnywhere);
            p.drawText(inner, e.text.left(2400), opt);
        }
    }
    p.restore();

    // Footer hint
    const QRect footer(card.left() + pad, card.bottom() - pad - 18, card.width() - pad * 2, 18);
    p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontCaption()));
    p.setPen(muted);
    p.drawText(footer, Qt::AlignVCenter | Qt::AlignLeft,
               QStringLiteral("Enter: paste  •  Shift+Enter: copy  •  Alt+Enter: open  •  Del: remove  •  Shift+Del: clear all"));
}

bool ClipboardOverlay::handleKey(QKeyEvent *event) {
    const int k = event->key();
    const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
    const bool alt = event->modifiers().testFlag(Qt::AltModifier);

    if (k == Qt::Key_Up) {
        if (m_selectedPos > 0) {
            --m_selectedPos;
            const int y = m_selectedPos * m_rowHeight;
            if (y < m_scrollOffset) m_scrollOffset = y;
            update();
        }
        return true;
    }
    if (k == Qt::Key_Down) {
        if (m_selectedPos + 1 < m_filteredIndices.size()) {
            ++m_selectedPos;
            const int bottom = (m_selectedPos + 1) * m_rowHeight;
            if (bottom > m_scrollOffset + m_listRect.height())
                m_scrollOffset = std::max(0, bottom - m_listRect.height());
            update();
        }
        return true;
    }
    if (k == Qt::Key_Delete) {
        if (shift) {
            askConfirm(QStringLiteral("Clear clipboard history?"),
                       QStringLiteral("All saved text and image entries will be deleted."),
                       [this]() {
                m_service->clearHistory();
                refilter();
                update();
            });
            return true;
        }
        if (!m_filteredIndices.isEmpty()) {
            m_service->removeEntryAt(m_filteredIndices.at(m_selectedPos));
            refilter();
            update();
        }
        return true;
    }
    if (k == Qt::Key_Return || k == Qt::Key_Enter) {
        if (!m_filteredIndices.isEmpty()) {
            m_service->activateEntry(m_filteredIndices.at(m_selectedPos), shift, alt);
            closePopup();
        }
        return true;
    }
    if (k == Qt::Key_Backspace) {
        if (!m_query.isEmpty()) {
            m_query.chop(1);
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

void ClipboardOverlay::handleCardMousePress(QMouseEvent *event, const QRect & /*card*/) {
    if (m_clearBtnRect.contains(event->pos())) {
        askConfirm(QStringLiteral("Clear clipboard history?"),
                   QStringLiteral("All saved text and image entries will be deleted."),
                   [this]() {
            m_service->clearHistory();
            refilter();
            update();
        });
        return;
    }
    if (m_listRect.contains(event->pos())) {
        const int idx = (event->pos().y() - m_listRect.top() + m_scrollOffset) / m_rowHeight;
        if (idx >= 0 && idx < m_filteredIndices.size()) {
            if (m_selectedPos == idx || event->button() == Qt::LeftButton) {
                m_selectedPos = idx;
                m_service->activateEntry(m_filteredIndices.at(idx), false, false);
                closePopup();
                return;
            }
            m_selectedPos = idx;
            update();
        }
    }
}

} // namespace quattro
