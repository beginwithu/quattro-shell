#pragma once

#include "../CommonUi.h"
#include "../../services/AudioService.h"
#include <QProcess>
#include <QVector>

namespace quattro {

class AudioPanel : public ShellPopupWindow {
    Q_OBJECT
public:
    AudioPanel(AudioService *audio,
               ThemeManager *theme,
               ShellConfig *config,
               QWidget *parent = nullptr);

protected:
    QSize desiredCardSize() const override { return QSize(420, 520); }
    void onOpening() override;
    void paintCardContent(QPainter &p, const QRect &card) override;
    void handleCardMousePress(QMouseEvent *event, const QRect &card) override;
    void handleCardMouseMove(QMouseEvent *event, const QRect &card) override;
    void handleCardMouseRelease(QMouseEvent *event, const QRect &card) override;

private:
    enum class DragTarget { None, SinkVol, SourceVol, StreamVol };
    void applySliderDrag(const QPoint &pos);

    AudioService *m_audio;
    DragTarget m_dragTarget = DragTarget::None;
    uint32_t m_dragStreamIdx = 0;

    QRect m_outSliderRect;
    QRect m_outMuteRect;
    QRect m_inSliderRect;
    QRect m_inMuteRect;
    QRect m_activeSliderRect;
    QVector<QPair<QString, QRect>> m_sinkRows;
    QVector<QPair<QString, QRect>> m_sourceRows;
    struct StreamHit { uint32_t idx; QRect slider; QRect mute; };
    QVector<StreamHit> m_streamHits;
};

struct BluetoothDeviceItem {
    QString mac;
    QString name;
    bool paired = false;
    bool connected = false;
    bool trusted = false;
    int batteryPercent = -1;
};

class BluetoothPanel : public ShellPopupWindow {
    Q_OBJECT
public:
    BluetoothPanel(ThemeManager *theme,
                   ShellConfig *config,
                   QWidget *parent = nullptr);

protected:
    QSize desiredCardSize() const override { return QSize(390, 480); }
    void onOpening() override;
    void paintCardContent(QPainter &p, const QRect &card) override;
    void handleCardMousePress(QMouseEvent *event, const QRect &card) override;

private:
    void refreshState();
    void togglePower();
    void startScan();
    void toggleDevice(const BluetoothDeviceItem &dev);

    bool m_powered = false;
    bool m_scanning = false;
    bool m_busy = false;
    QString m_statusText;
    QVector<BluetoothDeviceItem> m_devices;
    QRect m_powerToggleRect;
    QRect m_scanBtnRect;
    QVector<QPair<int, QRect>> m_devRows;
};

} // namespace quattro
