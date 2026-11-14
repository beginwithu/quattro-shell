#include "DesktopEntryStore.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

namespace quattro {

DesktopEntryStore::DesktopEntryStore(const QString &quattroPath, QObject *parent)
    : QObject(parent)
    , m_quattroPath(quattroPath)
{
}

QString DesktopEntryStore::normalizeId(QString id) {
    id = id.trimmed();
    if (id.endsWith(QLatin1String(".desktop"), Qt::CaseInsensitive))
        id.chop(8);
    return id;
}

QString DesktopEntryStore::cleanExec(QString exec) {
    static const QRegularExpression fieldCodes(QStringLiteral("\\s+%[fFuUdDnNickvm]"));
    exec.remove(fieldCodes);
    return exec.trimmed();
}

void DesktopEntryStore::ensureScanned() {
    if (!m_scanned)
        scan();
}

void DesktopEntryStore::scan() {
    m_scanned = true;
    m_all.clear();

    const QString homeApps = QDir::homePath() + QStringLiteral("/.local/share/applications");
    const QStringList dirs{
        m_quattroPath + QStringLiteral("/applications"),
        homeApps,
        QStringLiteral("/usr/local/share/applications"),
        QStringLiteral("/usr/share/applications")
    };

    QSet<QString> seen;
    for (const QString &dir : dirs) {
        if (!QFileInfo::exists(dir))
            continue;
        QDirIterator it(dir, {QStringLiteral("*.desktop")}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString filePath = it.next();
            const QString relPath = QDir(dir).relativeFilePath(filePath);
            QString flatId = relPath;
            flatId.replace(QLatin1Char('/'), QLatin1Char('-'));
            const QString id = normalizeId(flatId);
            const QString baseId = normalizeId(QFileInfo(filePath).fileName());
            if (seen.contains(id) || seen.contains(baseId))
                continue;

            QFile f(filePath);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;

            bool inDesktopEntry = false;
            bool hidden = false;
            DesktopAppEntry entry;
            entry.id = baseId;
            entry.filePath = filePath;
            entry.isUserOwned = filePath.startsWith(homeApps);
            QString type;

            while (!f.atEnd()) {
                const QString line = QString::fromUtf8(f.readLine()).trimmed();
                if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
                    continue;
                if (line.startsWith(QLatin1Char('['))) {
                    inDesktopEntry = (line == QLatin1String("[Desktop Entry]"));
                    continue;
                }
                if (!inDesktopEntry)
                    continue;
                const int eq = line.indexOf(QLatin1Char('='));
                if (eq <= 0)
                    continue;
                const QString k = line.left(eq).trimmed();
                const QString v = line.mid(eq + 1).trimmed();
                if (k == QLatin1String("Type")) type = v;
                else if (k == QLatin1String("Name") && entry.name.isEmpty()) entry.name = v;
                else if (k == QLatin1String("GenericName") && entry.genericName.isEmpty()) entry.genericName = v;
                else if (k == QLatin1String("Comment") && entry.comment.isEmpty()) entry.comment = v;
                else if (k == QLatin1String("Exec")) entry.execRaw = v;
                else if (k == QLatin1String("Icon")) entry.iconName = v;
                else if (k == QLatin1String("StartupWMClass")) entry.startupWmClass = v;
                else if (k == QLatin1String("Categories")) entry.categories = v.split(QLatin1Char(';'), Qt::SkipEmptyParts);
                else if (k == QLatin1String("NoDisplay")) entry.noDisplay = (v == QLatin1String("true"));
                else if (k == QLatin1String("Hidden")) hidden = (v == QLatin1String("true"));
                else if (k == QLatin1String("Terminal")) entry.terminal = (v == QLatin1String("true"));
            }

            if (type != QLatin1String("Application") || hidden || entry.name.isEmpty() || entry.execRaw.isEmpty())
                continue;

            seen.insert(id);
            seen.insert(baseId);
            entry.execClean = cleanExec(entry.execRaw);
            entry.icon = iconForName(entry.iconName);
            m_all.push_back(entry);
        }
    }

    std::sort(m_all.begin(), m_all.end(), [](const DesktopAppEntry &a, const DesktopAppEntry &b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    emit changed();
}

QIcon DesktopEntryStore::iconForName(const QString &iconName) const {
    if (iconName.isEmpty())
        return {};
    if (QFileInfo::exists(iconName))
        return QIcon(iconName);
    const QString customPng = m_quattroPath + QStringLiteral("/applications/icons/") + iconName + QStringLiteral(".png");
    if (QFileInfo::exists(customPng))
        return QIcon(customPng);
    const QString customSvg = m_quattroPath + QStringLiteral("/applications/icons/") + iconName + QStringLiteral(".svg");
    if (QFileInfo::exists(customSvg))
        return QIcon(customSvg);
    const QString homeIcon = QDir::homePath() + QStringLiteral("/.local/share/icons/hicolor/256x256/apps/") + iconName + QStringLiteral(".png");
    if (QFileInfo::exists(homeIcon))
        return QIcon(homeIcon);
    return QIcon::fromTheme(iconName);
}

QVector<DesktopAppEntry> DesktopEntryStore::visibleApps() const {
    QVector<DesktopAppEntry> out;
    for (const DesktopAppEntry &e : m_all) {
        if (!e.noDisplay)
            out.push_back(e);
    }
    return out;
}

const DesktopAppEntry *DesktopEntryStore::findById(const QString &desktopId) const {
    const QString norm = normalizeId(desktopId);
    for (const DesktopAppEntry &e : m_all) {
        if (e.id.compare(norm, Qt::CaseInsensitive) == 0)
            return &e;
    }
    return nullptr;
}

const DesktopAppEntry *DesktopEntryStore::heuristicLookup(const QString &appId, const QString &title) const {
    Q_UNUSED(title);
    const QString needle = normalizeId(appId);
    if (needle.isEmpty())
        return nullptr;

    // 1. Exact ID or StartupWMClass match
    for (const DesktopAppEntry &e : m_all) {
        if (e.id.compare(needle, Qt::CaseInsensitive) == 0)
            return &e;
        if (!e.startupWmClass.isEmpty() && e.startupWmClass.compare(needle, Qt::CaseInsensitive) == 0)
            return &e;
    }

    // 2. Claimed via --app-id=<needle> or org.quattro.<cmd>
    if (const auto *c = claimingEntryFor(appId))
        return c;

    // 3. Last dot segment of reverse-DNS appId
    const QString lower = needle.toLower();
    const int dot = lower.lastIndexOf(QLatin1Char('.'));
    const QString shortId = dot >= 0 ? lower.mid(dot + 1) : lower;
    for (const DesktopAppEntry &e : m_all) {
        const QString elow = e.id.toLower();
        if (elow == shortId || elow.endsWith(QLatin1Char('.') + shortId))
            return &e;
    }

    // 4. Match exec basename
    for (const DesktopAppEntry &e : m_all) {
        const QString bin = QFileInfo(e.execClean.section(QLatin1Char(' '), 0, 0)).fileName().toLower();
        if (!bin.isEmpty() && (bin == lower || bin == shortId))
            return &e;
    }
    return nullptr;
}

const DesktopAppEntry *DesktopEntryStore::claimingEntryFor(const QString &appId) const {
    if (appId.isEmpty())
        return nullptr;
    const QString flag = QStringLiteral("--app-id=") + appId;
    for (const DesktopAppEntry &e : m_all) {
        if (e.execRaw.contains(flag, Qt::CaseInsensitive))
            return &e;
    }
    if (appId.startsWith(QLatin1String("org.quattro."), Qt::CaseInsensitive)) {
        const QString cmd = appId.mid(12).toLower();
        for (const DesktopAppEntry &e : m_all) {
            if (!e.terminal)
                continue;
            const QString bin = QFileInfo(e.execClean.section(QLatin1Char(' '), 0, 0)).fileName().toLower();
            if (bin == cmd)
                return &e;
        }
    }
    return nullptr;
}

void DesktopEntryStore::launch(const DesktopAppEntry &entry) {
    if (entry.execClean.isEmpty())
        return;
    if (!entry.id.isEmpty() && QFileInfo::exists(QStringLiteral("/usr/bin/gtk-launch"))) {
        if (QProcess::startDetached(QStringLiteral("uwsm-app"), {QStringLiteral("--"), entry.id + QStringLiteral(".desktop")}))
            return;
    }
    const QString cmd = entry.terminal
                            ? QStringLiteral("quattro-launch-tui ") + entry.execClean
                            : entry.execClean;
    QProcess::startDetached(QStringLiteral("sh"), {QStringLiteral("-c"), QStringLiteral("uwsm-app -- ") + cmd});
}

bool DesktopEntryStore::launchById(const DesktopEntryStore &store, const QString &id) {
    const DesktopAppEntry *e = store.findById(id);
    if (!e)
        return false;
    launch(*e);
    return true;
}

bool DesktopEntryStore::uninstall(const QString &desktopId) {
    const DesktopAppEntry *e = findById(desktopId);
    if (!e)
        return false;
    const bool ok = QProcess::startDetached(QStringLiteral("quattro-pkg-drop"), {e->id});
    if (e->isUserOwned)
        QFile::remove(e->filePath);
    scan();
    return ok;
}

} // namespace quattro
