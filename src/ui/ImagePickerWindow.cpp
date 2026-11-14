#include "ImagePickerWindow.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QScreen>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace quattro {

namespace {
constexpr int kExpandedW = 768;
constexpr int kExpandedH = 475;
constexpr int kSliceW = 108;
constexpr int kSliceH = 432;
constexpr int kSliceSpacing = -30;
constexpr int kSkewOffset = 28;
}

ImagePickerWindow::ImagePickerWindow(ThemeManager *theme, ShellConfig *config, QWidget *parent)
    : ShellPopupWindow(theme, config, QStringLiteral("quattro-image-selector"), true, parent)
{
    refreshThemesAsync();
}

QSize ImagePickerWindow::desiredCardSize() const {
    const QScreen *scr = qGuiApp->screenAt(QCursor::pos());
    if (!scr) scr = qGuiApp->primaryScreen();
    const int maxW = scr ? scr->size().width() - 80 : 1800;
    const int cardW = std::min(maxW, kExpandedW + 13 * (kSliceW + kSliceSpacing) + 40);
    const int bottomChrome = (m_showLabels ? 44 : 10) + (m_filterable ? 36 : 0) + 20;
    return QSize(cardW, kExpandedH + 30 + bottomChrome);
}

bool ImagePickerWindow::isVideoPath(const QString &path) {
    const QString lower = path.toLower();
    return lower.endsWith(QLatin1String(".mp4")) || lower.endsWith(QLatin1String(".m4v")) ||
           lower.endsWith(QLatin1String(".mov")) || lower.endsWith(QLatin1String(".webm")) ||
           lower.endsWith(QLatin1String(".mkv")) || lower.endsWith(QLatin1String(".avi"));
}

void ImagePickerWindow::parseTsvRows(const QString &tsv, bool themeRows) {
    m_allItems.clear();
    m_pixCache.clear();
    const QString stockDir = config()->quattroPath() + QStringLiteral("/themes");
    const QStringList lines = tsv.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const int tab = line.indexOf(QLatin1Char('\t'));
        const QString imgPath = (tab >= 0 ? line.left(tab) : line).trimmed();
        const QString thumbPath = (tab >= 0 ? line.mid(tab + 1) : imgPath).trimmed();
        if (imgPath.isEmpty())
            continue;
        const QFileInfo fi(imgPath);
        QString label = fi.completeBaseName();
        if (label.isEmpty())
            label = fi.fileName();
        const bool isStock = themeRows && fi.canonicalFilePath().startsWith(QFileInfo(stockDir).canonicalFilePath());
        m_allItems.push_back({imgPath, thumbPath, label, themeRows, isStock});
    }
    applyFilter();
}

void ImagePickerWindow::applyFilter() {
    m_filteredIndices.clear();
    const QString needle = m_filterText.trimmed();
    for (int i = 0; i < m_allItems.size(); ++i) {
        if (needle.isEmpty() || m_allItems.at(i).label.contains(needle, Qt::CaseInsensitive))
            m_filteredIndices.push_back(i);
    }
    if (m_filteredIndices.isEmpty()) {
        m_selectedPos = 0;
        return;
    }
    if (!m_selectedTarget.isEmpty() && needle.isEmpty()) {
        const int targetIdx = indexForPath(m_selectedTarget);
        if (targetIdx >= 0) {
            m_selectedPos = targetIdx;
            return;
        }
    }
    m_selectedPos = std::clamp(m_selectedPos, 0, static_cast<int>(m_filteredIndices.size()) - 1);
}

int ImagePickerWindow::indexForPath(const QString &path) const {
    const QString canonical = QFileInfo(path).canonicalFilePath();
    for (int i = 0; i < m_filteredIndices.size(); ++i) {
        const PickerItem &item = m_allItems.at(m_filteredIndices.at(i));
        if (item.path == path || item.label == path ||
            (!canonical.isEmpty() && QFileInfo(item.path).canonicalFilePath() == canonical)) {
            return i;
        }
    }
    return -1;
}

QString ImagePickerWindow::preloadRows(const QString &imageRowsB64,
                                       const QString &selectedImage,
                                       bool showLabels,
                                       bool filterable) {
    if (isOpened() || m_requestActive)
        return QStringLiteral("ok");
    const QString tsv = QString::fromUtf8(QByteArray::fromBase64(imageRowsB64.toUtf8()));
    m_lastRowsTsv = tsv;
    m_selectedTarget = selectedImage;
    m_showLabels = showLabels;
    m_filterable = filterable;
    m_themeMode = false;
    parseTsvRows(tsv, false);
    return QStringLiteral("ok");
}

QString ImagePickerWindow::openSelector(const QString &dirs,
                                        const QString &imageRowsB64,
                                        const QString &selectedImage,
                                        const QString &selectionFile,
                                        const QString &doneFile,
                                        bool showLabels,
                                        bool filterable) {
    if (m_requestActive)
        releasePendingRequest(QString());

    ++m_requestSerial;
    m_requestActive = true;
    m_themeMode = false;
    m_selectionFile = selectionFile;
    m_doneFile = doneFile;
    m_selectedTarget = selectedImage;
    m_showLabels = showLabels;
    m_filterable = filterable;
    m_filterText.clear();

    const QString tsv = !imageRowsB64.isEmpty()
                            ? QString::fromUtf8(QByteArray::fromBase64(imageRowsB64.toUtf8()))
                            : QString();

    if (!tsv.isEmpty()) {
        m_lastRowsTsv = tsv;
        parseTsvRows(tsv, false);
        openAt();
    } else if (!dirs.isEmpty()) {
        startDirectoryScan(dirs);
        openAt();
    } else if (!m_lastRowsTsv.isEmpty()) {
        parseTsvRows(m_lastRowsTsv, false);
        openAt();
    } else {
        const QString fallbackDir = QDir::homePath() + QStringLiteral("/.local/state/quattro/current/theme/backgrounds");
        startDirectoryScan(fallbackDir);
        openAt();
    }

    return QStringLiteral("ok");
}

void ImagePickerWindow::openPayload(const QString &payloadJson) {
    const QJsonDocument doc = QJsonDocument::fromJson(payloadJson.toUtf8());
    const QJsonObject obj = doc.object();
    if (obj.value(QStringLiteral("source")).toString() == QLatin1String("themes")) {
        openThemes();
        return;
    }
    openSelector(obj.value(QStringLiteral("imageDirs")).toString(),
                 QString::fromUtf8(obj.value(QStringLiteral("imageRows")).toString().toUtf8().toBase64()),
                 obj.value(QStringLiteral("selectedImage")).toString(),
                 obj.value(QStringLiteral("selectionFile")).toString(),
                 obj.value(QStringLiteral("doneFile")).toString(),
                 obj.value(QStringLiteral("showLabels")).toBool(false),
                 obj.value(QStringLiteral("filterable")).toBool(false));
}

void ImagePickerWindow::openThemes() {
    if (m_requestActive)
        releasePendingRequest(QString());

    m_themeMode = true;
    m_requestActive = true;
    m_showLabels = true;
    m_filterable = true;
    m_filterText.clear();
    m_selectionFile.clear();
    m_doneFile.clear();

    QFile f(QDir::homePath() + QStringLiteral("/.local/state/quattro/current/theme.name"));
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
        m_selectedTarget = QString::fromUtf8(f.readAll()).trimmed();

    refreshThemesAsync();
    openAt();
}

void ImagePickerWindow::startDirectoryScan(const QString &dirs) {
    const int serial = m_requestSerial;
    const QString script = config()->quattroPath() + QStringLiteral("/shell/plugins/image-picker/list.sh");
    auto *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, serial](int, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(proc->readAllStandardOutput());
        proc->deleteLater();
        if (serial != m_requestSerial)
            return;
        m_lastRowsTsv = out;
        parseTsvRows(out, false);
        update();
    });
    proc->start(script, {dirs});
}

void ImagePickerWindow::refreshThemesAsync() {
    const int serial = m_requestSerial;
    auto *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, serial](int, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(proc->readAllStandardOutput());
        proc->deleteLater();
        if (m_themeMode || !isOpened()) {
            parseTsvRows(out, true);
            if (isOpened() && serial == m_requestSerial)
                update();
        }
    });
    proc->start(QStringLiteral("quattro-theme-switcher"), {QStringLiteral("--print-rows")});
}

void ImagePickerWindow::cancel() {
    if (!isOpened() && !m_requestActive)
        return;
    releasePendingRequest(QString());
    closePopup();
}

void ImagePickerWindow::onClosing() {
    if (m_requestActive)
        releasePendingRequest(QString());
}

void ImagePickerWindow::releasePendingRequest(const QString &selectedResult) {
    m_requestActive = false;
    if (!m_selectionFile.isEmpty()) {
        QFile sf(m_selectionFile);
        if (sf.open(QIODevice::WriteOnly | QIODevice::Truncate))
            sf.write(selectedResult.toUtf8());
    }
    if (!m_doneFile.isEmpty()) {
        QFile df(m_doneFile);
        if (df.open(QIODevice::WriteOnly | QIODevice::Truncate))
            df.write("1");
    }
    m_selectionFile.clear();
    m_doneFile.clear();
}

void ImagePickerWindow::applySelected() {
    if (m_filteredIndices.isEmpty() || m_selectedPos >= m_filteredIndices.size()) {
        cancel();
        return;
    }
    const PickerItem &item = m_allItems.at(m_filteredIndices.at(m_selectedPos));
    if (m_themeMode) {
        const QString themeName = item.label;
        m_requestActive = false;
        closePopup();
        QProcess::startDetached(QStringLiteral("quattro-theme-set"), {themeName});
        return;
    }
    releasePendingRequest(item.path);
    closePopup();
}

QPixmap ImagePickerWindow::pixmapFor(const PickerItem &item, const QSize &targetSize) {
    const QString src = item.previewPath.isEmpty() ? item.path : item.previewPath;
    const QString key = src + QLatin1Char('@') + QString::number(targetSize.width());
    auto it = m_pixCache.find(key);
    if (it != m_pixCache.end())
        return *it;

    QString loadPath = src;
    if (isVideoPath(src)) {
        const QString cacheDir = qEnvironmentVariable("XDG_CACHE_HOME", QDir::homePath() + QStringLiteral("/.cache"))
                                 + QStringLiteral("/quattro/image-picker-thumbs");
        QDir().mkpath(cacheDir);
        const QString h = QString::fromLatin1(QCryptographicHash::hash(src.toUtf8(), QCryptographicHash::Md5).toHex());
        const QString thumb = cacheDir + QLatin1Char('/') + h + QStringLiteral(".jpg");
        if (QFileInfo::exists(thumb)) {
            loadPath = thumb;
        } else if (!m_thumbPending.value(thumb, false)) {
            m_thumbPending.insert(thumb, true);
            auto *p = new QProcess(this);
            connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, p, thumb](int, QProcess::ExitStatus) {
                p->deleteLater();
                m_thumbPending.remove(thumb);
                if (isOpened()) update();
            });
            p->start(QStringLiteral("ffmpegthumbnailer"),
                     {QStringLiteral("-i"), src, QStringLiteral("-o"), thumb, QStringLiteral("-s"), QStringLiteral("1024")});
            return {};
        } else {
            return {};
        }
    }

    QImageReader reader(loadPath);
    reader.setAutoTransform(true);
    const QImage img = reader.read();
    if (img.isNull())
        return {};
    const QImage scaled = img.scaled(targetSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const QPixmap pix = QPixmap::fromImage(scaled);
    m_pixCache.insert(key, pix);
    return pix;
}

void ImagePickerWindow::paintEvent(QPaintEvent *event) {
    // Draw dimmed full-screen backdrop without enclosing box frame (parallelogram carousel floats cleanly!)
    if (!isOpened())
        return;
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), scrimColor());

    const QRect card = computeCardRect();
    paintCardContent(p, card);
}

void ImagePickerWindow::paintCardContent(QPainter &p, const QRect &card) {
    const SurfaceColors &ipc = theme()->imagePicker();
    const QColor fg = ipc.text.isValid() ? ipc.text : theme()->foreground();
    const QColor selBorder = ipc.selectedBorder.isValid() ? ipc.selectedBorder : theme()->accent();
    const QColor unselBorder = ipc.unselectedBorder.isValid() ? ipc.unselectedBorder : ThemeManager::withAlpha(fg, 0.35);

    m_sliceHitRects.clear();

    if (m_filteredIndices.isEmpty()) {
        p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontTitle()));
        p.setPen(fg);
        p.drawText(card, Qt::AlignCenter,
                   m_filterText.isEmpty() ? QStringLiteral("Loading previews...")
                                          : QStringLiteral("No match for \"%1\"").arg(m_filterText));
        return;
    }

    const int carouselTop = card.top() + 20;
    const int cx = card.center().x();
    const int step = kSliceW + kSliceSpacing; // 78px
    const int maxRadius = std::max(1, std::min(7, (card.width() - kExpandedW) / (2 * step) + 1));

    // Paint outer slices toward center so selected overlaps neighbor edges cleanly
    QVector<int> drawOrder;
    for (int d = maxRadius; d >= 1; --d) {
        if (m_selectedPos - d >= 0) drawOrder.push_back(m_selectedPos - d);
        if (m_selectedPos + d < m_filteredIndices.size()) drawOrder.push_back(m_selectedPos + d);
    }
    drawOrder.push_back(m_selectedPos);

    for (int pos : drawOrder) {
        const bool isCenter = (pos == m_selectedPos);
        const int w = isCenter ? kExpandedW : kSliceW;
        const int h = isCenter ? kExpandedH : kSliceH;
        const int delta = pos - m_selectedPos;
        int xLeft = 0;
        if (isCenter) {
            xLeft = cx - kExpandedW / 2;
        } else if (delta < 0) {
            xLeft = (cx - kExpandedW / 2) + delta * step - (kSliceW - step);
        } else {
            xLeft = (cx + kExpandedW / 2) + (delta - 1) * step;
        }
        const int yTop = carouselTop + (kExpandedH - h) / 2;
        const QRect box(xLeft, yTop, w, h);

        // Parallelogram polygon: skewed top-right -> bottom-left by kSkewOffset
        QPolygonF poly;
        poly << QPointF(box.left() + kSkewOffset, box.top())
             << QPointF(box.right(), box.top())
             << QPointF(box.right() - kSkewOffset, box.bottom())
             << QPointF(box.left(), box.bottom());
        QPainterPath clipPath;
        clipPath.addPolygon(poly);

        p.save();
        p.setClipPath(clipPath);
        const PickerItem &item = m_allItems.at(m_filteredIndices.at(pos));
        QPixmap pix = pixmapFor(item, QSize(isCenter ? kExpandedW : kExpandedW / 2, kExpandedH));
        if (!pix.isNull()) {
            const int px = box.center().x() - pix.width() / 2;
            const int py = box.center().y() - pix.height() / 2;
            p.drawPixmap(px, py, pix);
        } else {
            p.fillRect(box, ThemeManager::withAlpha(theme()->background(), 0.85));
        }

        if (!isCenter) {
            p.fillRect(box, ThemeManager::withAlpha(theme()->background(), 0.35));
        }
        p.restore();

        // Stroke parallelogram border
        p.save();
        p.setPen(QPen(isCenter ? selBorder : unselBorder, isCenter ? 3.0 : 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawPolygon(poly);
        p.restore();

        m_sliceHitRects.push_back({pos, box});
    }

    int labelY = carouselTop + kExpandedH + 14;
    if (m_showLabels && m_selectedPos < m_filteredIndices.size()) {
        const PickerItem &cur = m_allItems.at(m_filteredIndices.at(m_selectedPos));
        p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontDisplay(), QFont::DemiBold));
        const QRect lblRect(cx - kExpandedW / 2, labelY, kExpandedW, 34);
        // Drop outline against background
        p.setPen(ThemeManager::withAlpha(theme()->background(), 0.7));
        p.drawText(lblRect.translated(1, 1), Qt::AlignCenter, cur.label);
        p.setPen(fg);
        p.drawText(lblRect, Qt::AlignCenter, cur.label);
        labelY += 36;
    }

    if (m_filterable && !m_filterText.isEmpty()) {
        p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontTitle()));
        p.setPen(ThemeManager::withAlpha(fg, 0.85));
        p.drawText(QRect(cx - kExpandedW / 2, labelY, kExpandedW, 28),
                   Qt::AlignCenter, QStringLiteral("Filter: %1").arg(m_filterText));
    }
}

bool ImagePickerWindow::handleKey(QKeyEvent *event) {
    const int k = event->key();
    if (k == Qt::Key_Left || k == Qt::Key_H) {
        if (m_selectedPos > 0) {
            --m_selectedPos;
            update();
        }
        return true;
    }
    if (k == Qt::Key_Right || k == Qt::Key_L) {
        if (m_selectedPos + 1 < m_filteredIndices.size()) {
            ++m_selectedPos;
            update();
        }
        return true;
    }
    if (k == Qt::Key_Home && !m_filteredIndices.isEmpty()) {
        m_selectedPos = 0; update(); return true;
    }
    if (k == Qt::Key_End && !m_filteredIndices.isEmpty()) {
        m_selectedPos = m_filteredIndices.size() - 1; update(); return true;
    }
    if (k == Qt::Key_Return || k == Qt::Key_Enter) {
        applySelected();
        return true;
    }
    if (k == Qt::Key_Delete && m_themeMode && !m_filteredIndices.isEmpty()) {
        const PickerItem &cur = m_allItems.at(m_filteredIndices.at(m_selectedPos));
        if (!cur.isStockTheme) {
            const QString targetTheme = cur.label;
            askConfirm(QStringLiteral("Delete theme \"%1\"?").arg(targetTheme),
                       QStringLiteral("This removes your custom theme folder from ~/.config/quattro/themes."),
                       [this, targetTheme]() {
                auto *p = new QProcess(this);
                connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                        this, [this, p](int, QProcess::ExitStatus) {
                    p->deleteLater();
                    refreshThemesAsync();
                });
                p->start(QStringLiteral("quattro-theme-remove"), {targetTheme});
            });
        }
        return true;
    }
    if (m_filterable) {
        if (k == Qt::Key_Backspace) {
            if (!m_filterText.isEmpty()) {
                m_filterText.chop(1);
                applyFilter();
                update();
            }
            return true;
        }
        const QString t = event->text();
        if (!t.isEmpty() && t.at(0).isPrint()) {
            m_filterText += t;
            applyFilter();
            update();
            return true;
        }
    }
    return false;
}

void ImagePickerWindow::handleCardMousePress(QMouseEvent *event, const QRect & /*card*/) {
    // Check in reverse paint order (topmost slice = center slice first)
    for (int i = m_sliceHitRects.size() - 1; i >= 0; --i) {
        const auto &pair = m_sliceHitRects.at(i);
        if (pair.second.contains(event->pos())) {
            if (pair.first == m_selectedPos)
                applySelected();
            else {
                m_selectedPos = pair.first;
                update();
            }
            return;
        }
    }
    cancel();
}

void ImagePickerWindow::handleCardWheel(QWheelEvent *event, const QRect & /*card*/) {
    const int delta = event->angleDelta().y() != 0 ? event->angleDelta().y() : event->angleDelta().x();
    if (delta > 0 && m_selectedPos > 0) {
        --m_selectedPos;
        update();
    } else if (delta < 0 && m_selectedPos + 1 < m_filteredIndices.size()) {
        ++m_selectedPos;
        update();
    }
    event->accept();
}

} // namespace quattro
