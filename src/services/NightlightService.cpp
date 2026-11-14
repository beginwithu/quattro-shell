#include "NightlightService.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>

namespace quattro {

NightlightService::NightlightService(QObject *parent)
    : QObject(parent)
{
    refresh();
}

void NightlightService::refresh() {
    auto *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0) {
            const QString out = QString::fromUtf8(proc->readAllStandardOutput());
            static const QRegularExpression numRe(QStringLiteral("[0-9]+"));
            const auto m = numRe.match(out);
            if (m.hasMatch()) {
                m_temperature = m.captured(0).toInt();
                m_stateLoaded = true;
                emit changed();
            }
        }
        proc->deleteLater();
    });
    proc->start(QStringLiteral("hyprctl"), {QStringLiteral("hyprsunset"), QStringLiteral("temperature")});
}

void NightlightService::applyTemperature(int temp) {
    m_temperature = temp;
    m_stateLoaded = true;
    emit changed();

    if (m_applying) {
        m_pendingTemp = temp;
        return;
    }
    m_applying = true;

    const QString identitySuffix = (temp >= kDayTemp)
                                       ? QStringLiteral(" && hyprctl hyprsunset identity")
                                       : QString();
    const QString script = QStringLiteral(
        "pgrep -x hyprsunset >/dev/null || { setsid uwsm-app -- hyprsunset >/dev/null 2>&1 & sleep 1; }; "
        "hyprctl hyprsunset temperature %1%2").arg(temp).arg(identitySuffix);

    auto *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int, QProcess::ExitStatus) {
        proc->deleteLater();
        m_applying = false;
        if (m_pendingTemp >= 0) {
            const int next = m_pendingTemp;
            m_pendingTemp = -1;
            applyTemperature(next);
        } else {
            refresh();
        }
    });
    proc->start(QStringLiteral("bash"), {QStringLiteral("-c"), script});
}

QString NightlightService::enable() {
    applyTemperature(kNightTemp);
    return QStringLiteral("enabled");
}

QString NightlightService::disable() {
    applyTemperature(kDayTemp);
    return QStringLiteral("disabled");
}

QString NightlightService::toggle() {
    return enabled() ? disable() : enable();
}

QString NightlightService::statusJson() const {
    QJsonObject o{
        {QStringLiteral("enabled"), enabled()},
        {QStringLiteral("temperature"), m_stateLoaded ? QJsonValue(m_temperature) : QJsonValue::Null}
    };
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

} // namespace quattro
