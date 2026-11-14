#include "SystemStatsService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace quattro {

namespace {
QString readFirstLine(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(f.readLine()).trimmed();
}
} // namespace

SystemStatsService::SystemStatsService(QObject *parent)
    : QObject(parent)
    , m_stateHome(qEnvironmentVariable("XDG_STATE_HOME", QDir::homePath() + QStringLiteral("/.local/state")))
{
    m_stayAwake = QFileInfo::exists(m_stateHome + QStringLiteral("/quattro/toggles/idle-off"));
    m_nightLight = QFileInfo::exists(m_stateHome + QStringLiteral("/quattro/toggles/nightlight-on"));

    m_fastTimer.setInterval(2000);
    connect(&m_fastTimer, &QTimer::timeout, this, &SystemStatsService::sampleProc);
    m_fastTimer.start();

    m_slowTimer.setInterval(10000);
    connect(&m_slowTimer, &QTimer::timeout, this, [this]() {
        sampleBattery();
        sampleNetwork();
    });
    m_slowTimer.start();

    m_updateTimer.setInterval(21600000);
    connect(&m_updateTimer, &QTimer::timeout, this, &SystemStatsService::checkUpdates);
    m_updateTimer.start();

    sampleProc();
    sampleBattery();
    sampleNetwork();
    refreshIndicators();
    checkUpdates();
}

void SystemStatsService::sampleProc() {
    bool changed = false;

    // CPU from /proc/stat
    {
        QFile f(QStringLiteral("/proc/stat"));
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QByteArray line = f.readLine();
            const QList<QByteArray> parts = line.simplified().split(' ');
            if (parts.size() >= 5 && parts[0] == "cpu") {
                quint64 total = 0;
                quint64 idle = 0;
                for (int i = 1; i < parts.size(); ++i) {
                    const quint64 v = parts[i].toULongLong();
                    total += v;
                    if (i == 4 || i == 5) // idle + iowait
                        idle += v;
                }
                if (m_prevTotal > 0 && total > m_prevTotal) {
                    const quint64 dt = total - m_prevTotal;
                    const quint64 di = idle >= m_prevIdle ? (idle - m_prevIdle) : 0;
                    const int pct = std::clamp(static_cast<int>(std::lround((dt - di) * 100.0 / dt)), 0, 100);
                    if (pct != m_cpuPercent) {
                        m_cpuPercent = pct;
                        changed = true;
                    }
                }
                m_prevTotal = total;
                m_prevIdle = idle;
            }
        }
    }

    // RAM from /proc/meminfo
    {
        QFile f(QStringLiteral("/proc/meminfo"));
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            quint64 memTotal = 0;
            quint64 memAvail = 0;
            while (!f.atEnd()) {
                const QByteArray line = f.readLine();
                if (line.startsWith("MemTotal:")) {
                    const QList<QByteArray> p = line.simplified().split(' ');
                    if (p.size() >= 2) memTotal = p[1].toULongLong();
                } else if (line.startsWith("MemAvailable:")) {
                    const QList<QByteArray> p = line.simplified().split(' ');
                    if (p.size() >= 2) memAvail = p[1].toULongLong();
                }
                if (memTotal && memAvail) break;
            }
            if (memTotal > 0) {
                const int pct = std::clamp(static_cast<int>(std::lround((memTotal - memAvail) * 100.0 / memTotal)), 0, 100);
                if (pct != m_ramPercent) {
                    m_ramPercent = pct;
                    changed = true;
                }
            }
        }
    }

    if (changed)
        emit statsChanged();
}

void SystemStatsService::sampleBattery() {
    QDir psDir(QStringLiteral("/sys/class/power_supply"));
    const QStringList entries = psDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    bool present = false;
    int pct = 100;
    bool charging = false;

    for (const QString &name : entries) {
        const QString base = psDir.filePath(name);
        if (readFirstLine(base + QStringLiteral("/type")) != QLatin1String("Battery"))
            continue;
        if (readFirstLine(base + QStringLiteral("/scope")) == QLatin1String("Device"))
            continue;
        present = true;
        bool ok = false;
        const int cap = readFirstLine(base + QStringLiteral("/capacity")).toInt(&ok);
        if (ok) pct = std::clamp(cap, 0, 100);
        const QString st = readFirstLine(base + QStringLiteral("/status"));
        charging = (st == QLatin1String("Charging") || st == QLatin1String("Full"));
        break;
    }

    if (present != m_batteryPresent || pct != m_batteryPercent || charging != m_batteryCharging) {
        m_batteryPresent = present;
        m_batteryPercent = pct;
        m_batteryCharging = charging;
        emit statsChanged();
    }
}

void SystemStatsService::sampleNetwork() {
    QDir netDir(QStringLiteral("/sys/class/net"));
    const QStringList entries = netDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    bool wifi = false;
    bool eth = false;

    for (const QString &iface : entries) {
        if (iface == QLatin1String("lo"))
            continue;
        const QString base = netDir.filePath(iface);
        if (readFirstLine(base + QStringLiteral("/operstate")) != QLatin1String("up"))
            continue;
        if (QFileInfo::exists(base + QStringLiteral("/wireless")))
            wifi = true;
        else if (iface.startsWith(QLatin1String("en")) || iface.startsWith(QLatin1String("eth")))
            eth = true;
    }

    const bool conn = wifi || eth;
    if (conn != m_networkConnected || wifi != m_wifiConnected || eth != m_ethernetConnected) {
        m_networkConnected = conn;
        m_wifiConnected = wifi;
        m_ethernetConnected = eth;
        emit statsChanged();
    }
}

void SystemStatsService::setStayAwake(bool awake) {
    if (awake == m_stayAwake)
        return;
    m_stayAwake = awake;
    const QString path = m_stateHome + QStringLiteral("/quattro/toggles/idle-off");
    QDir().mkpath(QFileInfo(path).absolutePath());
    if (awake) {
        QFile f(path);
        if (f.open(QIODevice::WriteOnly))
            f.write("1\n");
    } else {
        QFile::remove(path);
    }
    emit indicatorsChanged();
}

void SystemStatsService::setNightLight(bool enabled) {
    if (enabled == m_nightLight)
        return;
    m_nightLight = enabled;
    QProcess::startDetached(QStringLiteral("quattro-toggle-nightlight"), {});
    emit indicatorsChanged();
}

void SystemStatsService::refreshIndicators() {
    if (!m_checkingSudo) {
        m_checkingSudo = true;
        auto *p = new QProcess(this);
        connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, p](int code, QProcess::ExitStatus st) {
            m_checkingSudo = false;
            const bool next = (code == 0 && st == QProcess::NormalExit);
            if (next != m_passwordlessSudo) {
                m_passwordlessSudo = next;
                emit indicatorsChanged();
            }
            p->deleteLater();
        });
        connect(p, &QProcess::errorOccurred, this, [this, p](QProcess::ProcessError) {
            m_checkingSudo = false;
            p->deleteLater();
        });
        p->start(QStringLiteral("quattro-sudo-passwordless"), {QStringLiteral("--active")});
    }

    if (!m_checkingRec) {
        m_checkingRec = true;
        auto *p = new QProcess(this);
        connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, p](int code, QProcess::ExitStatus st) {
            m_checkingRec = false;
            const bool next = (code == 0 && st == QProcess::NormalExit);
            if (next != m_screenRecording) {
                m_screenRecording = next;
                emit indicatorsChanged();
            }
            p->deleteLater();
        });
        connect(p, &QProcess::errorOccurred, this, [this, p](QProcess::ProcessError) {
            m_checkingRec = false;
            p->deleteLater();
        });
        p->start(QStringLiteral("quattro-capture-screenrecording"), {QStringLiteral("--status")});
    }
}

void SystemStatsService::checkUpdates() {
    if (m_checkingUpdates)
        return;
    m_checkingUpdates = true;
    auto *p = new QProcess(this);
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, p](int code, QProcess::ExitStatus st) {
        m_checkingUpdates = false;
        const bool next = (code == 0 && st == QProcess::NormalExit);
        if (next != m_updateAvailable) {
            m_updateAvailable = next;
            emit updateStatusChanged();
        }
        p->deleteLater();
    });
    connect(p, &QProcess::errorOccurred, this, [this, p](QProcess::ProcessError) {
        m_checkingUpdates = false;
        p->deleteLater();
    });
    p->start(QStringLiteral("quattro-update-available"), {});
}

void SystemStatsService::clearUpdates() {
    if (m_updateAvailable) {
        m_updateAvailable = false;
        emit updateStatusChanged();
    }
}

} // namespace quattro
