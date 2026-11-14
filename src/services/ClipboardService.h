#pragma once

#include <QJsonArray>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <QVector>

namespace quattro {

struct ClipboardEntry {
    QString type; // "text" | "image"
    QString text;
    QString mime;
    QString path;
    QString capturedAt;
    QString key;
};

class ClipboardService : public QObject {
    Q_OBJECT
public:
    static constexpr int kMaxEntries = 500;

    explicit ClipboardService(QObject *parent = nullptr);
    ~ClipboardService() override;

    void startWatchers();
    const QVector<ClipboardEntry> &entries() const { return m_entries; }

    void addOrBumpEntry(const ClipboardEntry &entry);
    void removeEntryAt(int index);
    void clearHistory();

    void activateEntry(int index, bool copyOnly, bool openExternal);
    static bool isDuplicateKey(const QVector<ClipboardEntry> &list, const QString &key);

signals:
    void historyChanged();

private:
    void loadHistory();
    void saveHistory();
    void captureSnapshot(const QString &hintMime = QString());
    void spawnWatcher(const QString &typeArg);

    QString m_historyPath;
    QString m_imagesDir;
    QVector<ClipboardEntry> m_entries;
    QProcess *m_textWatcher = nullptr;
    QProcess *m_imageWatcher = nullptr;
    QTimer m_restartTimer;
};

} // namespace quattro
