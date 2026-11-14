#include "BrightnessService.h"

#include <QFile>
#include <QProcess>
#include <algorithm>
#include <cmath>

namespace quattro {

BrightnessService::BrightnessService(HyprlandClient *hypr, QObject *parent)
    : QObject(parent)
    , m_hypr(hypr)
{
    refreshDevice();
}

void BrightnessService::refreshDevice() {
    auto *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0)
            m_device = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
    });
    proc->start(QStringLiteral("quattro-hw-display"), {});
}

int BrightnessService::brightnessKeyTarget(const QString &action, int currentPercent) {
    if (action == QLatin1String("raise"))
        return std::min(currentPercent < 5 ? currentPercent + 1 : currentPercent + 5, 100);
    if (action == QLatin1String("lower"))
        return std::max(currentPercent <= 5 ? currentPercent - 1 : currentPercent - 5, 1);
    return currentPercent;
}

int BrightnessService::readCurrentPercent() const {
    if (m_device.isEmpty())
        return -1;
    const QString base = QStringLiteral("/sys/class/backlight/") + m_device;
    QFile fb(base + QStringLiteral("/brightness"));
    QFile fm(base + QStringLiteral("/max_brightness"));
    if (!fb.open(QIODevice::ReadOnly) || !fm.open(QIODevice::ReadOnly))
        return -1;
    const double cur = fb.readAll().trimmed().toDouble();
    const double mx = fm.readAll().trimmed().toDouble();
    if (mx <= 0.0)
        return -1;
    return std::clamp(static_cast<int>(std::lround(100.0 * cur / mx)), 0, 100);
}

bool BrightnessService::handleKey(const QString &action) {
    if (action != QLatin1String("raise") && action != QLatin1String("lower"))
        return false;

    const QString mon = m_hypr ? m_hypr->focusedMonitorName() : QString();
    static const QRegularExpression internalRe(QStringLiteral("^(eDP|LVDS|DSI)-"));
    if (m_device.isEmpty() || !internalRe.match(mon).hasMatch()) {
        const QString step = (action == QLatin1String("raise")) ? QStringLiteral("+5%") : QStringLiteral("5%-");
        QProcess::startDetached(QStringLiteral("quattro-brightness-display"), {step});
        return true;
    }

    if (m_setting)
        return true;

    const int cur = readCurrentPercent();
    if (cur < 0)
        return false;

    const int target = brightnessKeyTarget(action, cur);
    m_setting = true;
    auto *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int, QProcess::ExitStatus) {
        proc->deleteLater();
        m_setting = false;
        const int actual = readCurrentPercent();
        if (actual >= 0)
            emit osdRequested(QStringLiteral("brightness"), QString(), actual, QStringLiteral("%1%").arg(actual));
        refreshDevice();
    });
    proc->start(QStringLiteral("brightnessctl"),
                {QStringLiteral("-q"), QStringLiteral("-d"), m_device, QStringLiteral("set"),
                 QStringLiteral("%1%").arg(target)});
    return true;
}

} // namespace quattro
