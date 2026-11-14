#pragma once

#include <QIcon>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace quattro {

struct DesktopAppEntry {
    QString id;           // desktop id without .desktop (e.g. "firefox", "org.gnome.Nautilus")
    QString filePath;     // full path to .desktop file
    QString name;
    QString genericName;
    QString comment;
    QString execRaw;
    QString execClean;
    QString iconName;
    QString startupWmClass;
    QStringList categories;
    QIcon icon;
    bool noDisplay = false;
    bool terminal = false;
    bool isUserOwned = false;
};

class DesktopEntryStore : public QObject {
    Q_OBJECT
public:
    explicit DesktopEntryStore(const QString &quattroPath, QObject *parent = nullptr);

    void scan();
    void ensureScanned();

    const QVector<DesktopAppEntry> &allEntries() const { return m_all; }
    QVector<DesktopAppEntry> visibleApps() const;

    const DesktopAppEntry *findById(const QString &desktopId) const;
    const DesktopAppEntry *heuristicLookup(const QString &appId, const QString &title = QString()) const;
    const DesktopAppEntry *claimingEntryFor(const QString &appId) const;

    QIcon iconForName(const QString &iconName) const;
    static void launch(const DesktopAppEntry &entry);
    static bool launchById(const DesktopEntryStore &store, const QString &id);
    bool uninstall(const QString &desktopId);

signals:
    void changed();

private:
    static QString normalizeId(QString id);
    static QString cleanExec(QString exec);
    QString m_quattroPath;
    QVector<DesktopAppEntry> m_all;
    bool m_scanned = false;
};

} // namespace quattro
