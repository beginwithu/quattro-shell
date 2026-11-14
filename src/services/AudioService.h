#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

struct pa_threaded_mainloop;
struct pa_context;

namespace quattro {

struct AudioDeviceInfo {
    uint32_t index = 0;
    QString name;
    QString description;
    int volume = 0;
    uint32_t channels = 2;
    bool muted = false;
    bool isDefault = false;
    bool isHeadphones = false;
    bool isMonitor = false;
};

struct AudioStreamInfo {
    uint32_t index = 0;
    QString appName;
    QString mediaName;
    QString binary;
    int volume = 100;
    uint32_t channels = 2;
    bool muted = false;
};

class AudioService : public QObject {
    Q_OBJECT
public:
    explicit AudioService(QObject *parent = nullptr);
    ~AudioService() override;

    int sinkVolume() const { return m_sinkVolume; }
    bool sinkMuted() const { return m_sinkMuted; }
    int sourceVolume() const { return m_sourceVolume; }
    bool sourceMuted() const { return m_sourceMuted; }
    bool micActive() const { return m_micActive; }
    bool headphoneActive() const { return m_headphoneActive; }
    QString defaultSinkName() const { return m_defaultSink; }
    QString defaultSourceName() const { return m_defaultSource; }

    QVector<AudioDeviceInfo> sinks() const { return m_sinks; }
    QVector<AudioDeviceInfo> sources() const { return m_sources; }
    QVector<AudioStreamInfo> sinkInputs() const { return m_sinkInputs; }
    QStringList playbackStreamLabels() const;

    void refreshLists();
    void adjustVolume(int deltaPercent);
    void setSinkVolume(int percent);
    void setSinkMute(bool muted);
    void setSourceVolume(int percent);
    void setSourceMute(bool muted);
    void toggleSinkMute();
    void toggleSourceMute();
    void toggleAllMuted();
    void setDefaultSink(const QString &name);
    void setDefaultSource(const QString &name);
    void setSinkInputVolume(uint32_t index, int percent);
    void setSinkInputMute(uint32_t index, bool muted);

signals:
    void audioChanged();
    void volumeOsdRequested(int percent, bool muted);

private:
    void startPulse();
    void stopPulse();
    void queryServerInfo();
    void querySink(const QString &name);
    void querySource(const QString &name);
    void querySourceOutputs();
    void queryAllLists();

    pa_threaded_mainloop *m_mainloop = nullptr;
    pa_context *m_ctx = nullptr;

    QString m_defaultSink;
    QString m_defaultSource;
    int m_sinkVolume = 0;
    uint32_t m_sinkChannels = 2;
    bool m_sinkMuted = false;
    int m_sourceVolume = 0;
    uint32_t m_sourceChannels = 1;
    bool m_sourceMuted = false;
    bool m_micActive = false;
    bool m_headphoneActive = false;
    bool m_initialSyncDone = false;

    QVector<AudioDeviceInfo> m_sinks;
    QVector<AudioDeviceInfo> m_sources;
    QVector<AudioStreamInfo> m_sinkInputs;
};

} // namespace quattro
