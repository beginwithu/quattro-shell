#pragma once

#include "../core/HyprlandClient.h"
#include <QObject>
#include <QRegularExpression>
#include <QString>

namespace quattro {

class BrightnessService : public QObject {
    Q_OBJECT
public:
    explicit BrightnessService(HyprlandClient *hypr, QObject *parent = nullptr);

    void refreshDevice();
    bool handleKey(const QString &action); // "raise" | "lower"
    static int brightnessKeyTarget(const QString &action, int currentPercent);

signals:
    void osdRequested(const QString &icon, const QString &message, int value, const QString &progressText);

private:
    int readCurrentPercent() const;

    HyprlandClient *m_hypr;
    QString m_device;
    bool m_setting = false;
};

} // namespace quattro
