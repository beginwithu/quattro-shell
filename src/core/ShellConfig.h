#pragma once

#include <QFileSystemWatcher>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>

namespace quattro {

struct BarWidgetEntry {
    QString id;
    QJsonObject raw;
};

struct BarLayout {
    QVector<BarWidgetEntry> left;
    QVector<BarWidgetEntry> centerBegin;
    QVector<BarWidgetEntry> center;
    QVector<BarWidgetEntry> centerEnd;
    QVector<BarWidgetEntry> tray;
    QVector<BarWidgetEntry> right;
};

class ShellConfig : public QObject {
    Q_OBJECT
public:
    explicit ShellConfig(QObject *parent = nullptr);

    void loadFromDisk();
    bool reloadUserConfig();
    bool applyJsonText(const QByteArray &defaultsRaw, const QByteArray &userRaw);

    QString quattroPath() const { return m_quattroPath; }
    QString shellPath() const { return m_quattroPath + QStringLiteral("/shell"); }
    QString defaultsPath() const { return m_quattroPath + QStringLiteral("/config/quattro/shell.json"); }
    QString userConfigPath() const { return m_userConfigPath; }

    const QJsonObject &rootObject() const { return m_effective; }
    QString barPosition() const;
    QString barBackground() const;
    bool barHidden() const { return m_barHidden; }
    BarLayout barLayout() const;

    int idleScreensaverSec() const;
    int idleLockSec() const;

    // Dock config
    QString dockPosition() const;
    bool dockAutohide() const;
    QStringList dockPinned() const;
    bool setDockPosition(const QString &pos);
    bool setDockAutohide(bool enabled);
    bool setDockPinned(const QStringList &pinned);

    // Inline bar & panel settings
    QJsonObject barWidgetEntry(const QString &id) const;
    QString barWidgetIdAt(const QString &section, int index) const;
    bool updateEntryInline(const QString &id, const QJsonObject &patch);

    QString cycleBarBackground();
    bool setBarWidgetSetting(const QString &id, const QString &key, const QJsonValue &value);
    QString putBarWidget(const QString &id, const QString &placementJson);
    QString moveBarWidget(const QString &id, const QString &placementJson);
    QString setBarWidget(const QString &id, const QString &key, const QString &valueJson, const QString &selectorJson);
    QString setPluginEnabled(const QString &id, bool enabled, const QString &placementJson = QString());
    QString listPluginsJson() const;
    void syncBarHidden();

    static QByteArray stripJsonc(const QByteArray &input);

signals:
    void configChanged();
    void barHiddenChanged(bool hidden);

private:
    bool persistUserConfig(const QJsonObject &nextObj);
    static QJsonObject builtinFallback();
    static QVector<BarWidgetEntry> parseSection(const QJsonArray &arr);

    QString m_home;
    QString m_quattroPath;
    QString m_userConfigPath;
    QString m_barOffFlagPath;

    QJsonObject m_defaults;
    QJsonObject m_effective;
    bool m_userUnreadable = false;
    bool m_barHidden = false;

    QFileSystemWatcher m_watcher;
};

} // namespace quattro
