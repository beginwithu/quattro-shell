#include "ClipboardService.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSaveFile>

namespace quattro {

ClipboardService::ClipboardService(QObject *parent)
    : QObject(parent)
{
    const QString stateHome = qEnvironmentVariable("XDG_STATE_HOME", QDir::homePath() + QStringLiteral("/.local/state"));
    m_historyPath = stateHome + QStringLiteral("/quattro/clipboard-history.json");
    m_imagesDir = stateHome + QStringLiteral("/quattro/clipboard-images");
    QDir().mkpath(m_imagesDir);
    loadHistory();

    m_restartTimer.setSingleShot(true);
    m_restartTimer.setInterval(1000);
    connect(&m_restartTimer, &QTimer::timeout, this, &ClipboardService::startWatchers);
}

ClipboardService::~ClipboardService() {
    if (m_textWatcher) { m_textWatcher->kill(); m_textWatcher->waitForFinished(200); }
    if (m_imageWatcher) { m_imageWatcher->kill(); m_imageWatcher->waitForFinished(200); }
}

void ClipboardService::loadHistory() {
    m_entries.clear();
    QFile f(m_historyPath);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray())
        return;
    for (const QJsonValue &v : doc.array()) {
        const QJsonObject o = v.toObject();
        ClipboardEntry e;
        e.type = o.value(QStringLiteral("type")).toString();
        e.text = o.value(QStringLiteral("text")).toString();
        e.mime = o.value(QStringLiteral("mime")).toString();
        e.path = o.value(QStringLiteral("path")).toString();
        e.capturedAt = o.value(QStringLiteral("capturedAt")).toString();
        e.key = (e.type == QLatin1String("image"))
                    ? (QStringLiteral("image:") + e.path)
                    : (QStringLiteral("text:") + e.text);
        if (!e.type.isEmpty())
            m_entries.push_back(e);
    }
}

void ClipboardService::saveHistory() {
    QDir().mkpath(QFileInfo(m_historyPath).absolutePath());
    QJsonArray arr;
    for (const auto &e : m_entries) {
        QJsonObject o;
        o[QStringLiteral("type")] = e.type;
        if (e.type == QLatin1String("image")) {
            o[QStringLiteral("mime")] = e.mime;
            o[QStringLiteral("path")] = e.path;
            o[QStringLiteral("capturedAt")] = e.capturedAt;
        } else {
            o[QStringLiteral("text")] = e.text;
        }
        arr.append(o);
    }
    QSaveFile f(m_historyPath);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
        f.commit();
    }
}

bool ClipboardService::isDuplicateKey(const QVector<ClipboardEntry> &list, const QString &key) {
    for (const auto &e : list) {
        if (e.key == key)
            return true;
    }
    return false;
}

void ClipboardService::addOrBumpEntry(const ClipboardEntry &entry) {
    ClipboardEntry item = entry;
    if (item.key.isEmpty()) {
        item.key = (item.type == QLatin1String("image"))
                       ? (QStringLiteral("image:") + item.path)
                       : (QStringLiteral("text:") + item.text);
    }
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].key == item.key) {
            m_entries.removeAt(i);
            break;
        }
    }
    m_entries.prepend(item);
    while (m_entries.size() > kMaxEntries)
        m_entries.removeLast();
    saveHistory();
    emit historyChanged();
}

void ClipboardService::removeEntryAt(int index) {
    if (index < 0 || index >= m_entries.size())
        return;
    m_entries.removeAt(index);
    saveHistory();
    emit historyChanged();
}

void ClipboardService::clearHistory() {
    m_entries.clear();
    saveHistory();
    emit historyChanged();
}

void ClipboardService::activateEntry(int index, bool copyOnly, bool openExternal) {
    if (index < 0 || index >= m_entries.size())
        return;
    const ClipboardEntry &e = m_entries[index];

    if (openExternal) {
        QProcess::startDetached(QStringLiteral("quattro-clipboard-open"),
                                {QStringLiteral("--history-index"), QString::number(index)});
        return;
    }

    if (e.type == QLatin1String("image")) {
        QStringList args{e.mime.isEmpty() ? QStringLiteral("image/png") : e.mime, e.path};
        if (copyOnly) args.prepend(QStringLiteral("--copy-only"));
        QProcess::startDetached(QStringLiteral("quattro-clipboard-paste-file"), args);
    } else {
        QStringList args{QStringLiteral("--history-index"), QString::number(index)};
        if (copyOnly) args.prepend(QStringLiteral("--copy-only"));
        else args.prepend(QStringLiteral("--shift-insert"));
        QProcess::startDetached(QStringLiteral("quattro-clipboard-paste-text"), args);
    }
}

void ClipboardService::startWatchers() {
    if (qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
        return;

    captureSnapshot();

    auto spawnFor = [&](QProcess *&slot, const QString &mimeType) {
        if (slot && slot->state() != QProcess::NotRunning)
            return;
        if (slot)
            slot->deleteLater();
        slot = new QProcess(this);
        connect(slot, &QProcess::readyReadStandardOutput, this, [this, slot, mimeType]() {
            const QByteArray out = slot->readAllStandardOutput();
            if (!out.isEmpty())
                captureSnapshot(mimeType);
        });
        connect(slot, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this](int, QProcess::ExitStatus) {
            if (!m_restartTimer.isActive())
                m_restartTimer.start();
        });
        // `wl-paste --type <mime> --watch echo 1` pings stdout whenever clipboard changes
        slot->start(QStringLiteral("wl-paste"),
                    {QStringLiteral("--type"), mimeType, QStringLiteral("--watch"), QStringLiteral("echo"), QStringLiteral("1")});
    };

    spawnFor(m_textWatcher, QStringLiteral("text"));
    spawnFor(m_imageWatcher, QStringLiteral("image/png"));
}

void ClipboardService::captureSnapshot(const QString &hintMime) {
    if (qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
        return;

    auto *typesProc = new QProcess(this);
    connect(typesProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, typesProc, hintMime](int code, QProcess::ExitStatus) {
        const QString typesStr = QString::fromUtf8(typesProc->readAllStandardOutput());
        typesProc->deleteLater();
        if (code != 0)
            return;
        const QStringList types = typesStr.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        if (types.contains(QStringLiteral("x-kde-passwordManagerHint")))
            return;

        QString imgMime;
        for (const QString &t : types) {
            if (t == QLatin1String("image/png") || t == QLatin1String("image/jpeg") || t == QLatin1String("image/webp")) {
                imgMime = t;
                break;
            }
        }
        const QString targetMime = !imgMime.isEmpty() ? imgMime : (!hintMime.isEmpty() ? hintMime : QStringLiteral("text"));

        auto *readProc = new QProcess(this);
        connect(readProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, readProc, targetMime, imgMime](int rcode, QProcess::ExitStatus) {
            const QByteArray data = readProc->readAllStandardOutput();
            readProc->deleteLater();
            if (rcode != 0 || data.isEmpty())
                return;

            if (!imgMime.isEmpty()) {
                const QString ext = imgMime.section(QLatin1Char('/'), 1, 1).replace(QStringLiteral("jpeg"), QStringLiteral("jpg"));
                const QString hash = QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
                const QString outPath = m_imagesDir + QLatin1Char('/') + hash + QLatin1Char('.') + ext;
                if (!QFileInfo::exists(outPath)) {
                    QSaveFile imgFile(outPath);
                    if (imgFile.open(QIODevice::WriteOnly)) {
                        imgFile.write(data);
                        imgFile.commit();
                    }
                }
                ClipboardEntry e;
                e.type = QStringLiteral("image");
                e.mime = imgMime;
                e.path = outPath;
                e.capturedAt = QLocale().toString(QDateTime::currentDateTime(), QStringLiteral("dddd HH:mm"));
                addOrBumpEntry(e);
            } else {
                QString txt;
                // Decode UTF-16 BOM if present, otherwise UTF-8
                if (data.size() >= 2 && ((static_cast<unsigned char>(data[0]) == 0xFF && static_cast<unsigned char>(data[1]) == 0xFE)
                                          || (static_cast<unsigned char>(data[0]) == 0xFE && static_cast<unsigned char>(data[1]) == 0xFF))) {
                    txt = QString::fromUtf16(reinterpret_cast<const char16_t *>(data.constData() + 2), (data.size() - 2) / 2);
                } else {
                    txt = QString::fromUtf8(data);
                }
                if (!txt.trimmed().isEmpty()) {
                    ClipboardEntry e;
                    e.type = QStringLiteral("text");
                    e.text = txt;
                    addOrBumpEntry(e);
                }
            }
        });
        readProc->start(QStringLiteral("wl-paste"),
                        {QStringLiteral("--no-newline"), QStringLiteral("--type"), targetMime});
    });
    typesProc->start(QStringLiteral("wl-paste"), {QStringLiteral("--list-types")});
}

} // namespace quattro
