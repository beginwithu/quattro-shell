#include "AudioService.h"

#include <QMetaObject>
#include <QProcess>
#include <pulse/pulseaudio.h>
#include <algorithm>
#include <cmath>

namespace quattro {

AudioService::AudioService(QObject *parent)
    : QObject(parent)
{
    startPulse();
}

AudioService::~AudioService() {
    stopPulse();
}

void AudioService::startPulse() {
    m_mainloop = pa_threaded_mainloop_new();
    if (!m_mainloop)
        return;

    pa_mainloop_api *api = pa_threaded_mainloop_get_api(m_mainloop);
    m_ctx = pa_context_new(api, "quattro-cpp");
    if (!m_ctx) {
        pa_threaded_mainloop_free(m_mainloop);
        m_mainloop = nullptr;
        return;
    }

    pa_context_set_state_callback(m_ctx, [](pa_context *c, void *userdata) {
        auto *self = static_cast<AudioService *>(userdata);
        const pa_context_state_t st = pa_context_get_state(c);
        if (st == PA_CONTEXT_READY) {
            pa_context_set_subscribe_callback(c, [](pa_context *, pa_subscription_event_type_t t, uint32_t, void *ud) {
                auto *s = static_cast<AudioService *>(ud);
                const auto fac = static_cast<pa_subscription_event_type_t>(t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK);
                if (fac == PA_SUBSCRIPTION_EVENT_SERVER) {
                    QMetaObject::invokeMethod(s, [s]() {
                        s->queryServerInfo();
                        s->queryAllLists();
                    }, Qt::QueuedConnection);
                } else if (fac == PA_SUBSCRIPTION_EVENT_SINK || fac == PA_SUBSCRIPTION_EVENT_SINK_INPUT) {
                    QMetaObject::invokeMethod(s, [s]() {
                        if (!s->m_defaultSink.isEmpty()) s->querySink(s->m_defaultSink);
                        s->queryAllLists();
                    }, Qt::QueuedConnection);
                } else if (fac == PA_SUBSCRIPTION_EVENT_SOURCE || fac == PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT) {
                    QMetaObject::invokeMethod(s, [s]() {
                        if (!s->m_defaultSource.isEmpty()) s->querySource(s->m_defaultSource);
                        s->querySourceOutputs();
                        s->queryAllLists();
                    }, Qt::QueuedConnection);
                }
            }, self);

            pa_operation *op = pa_context_subscribe(
                c,
                static_cast<pa_subscription_mask_t>(PA_SUBSCRIPTION_MASK_SERVER |
                                                    PA_SUBSCRIPTION_MASK_SINK |
                                                    PA_SUBSCRIPTION_MASK_SINK_INPUT |
                                                    PA_SUBSCRIPTION_MASK_SOURCE |
                                                    PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT),
                nullptr, nullptr);
            if (op) pa_operation_unref(op);

            QMetaObject::invokeMethod(self, [self]() {
                self->queryServerInfo();
                self->querySourceOutputs();
                self->queryAllLists();
            }, Qt::QueuedConnection);
        }
    }, this);

    pa_context_connect(m_ctx, nullptr, PA_CONTEXT_NOFAIL, nullptr);
    pa_threaded_mainloop_start(m_mainloop);
}

void AudioService::stopPulse() {
    if (m_mainloop) {
        pa_threaded_mainloop_lock(m_mainloop);
        if (m_ctx) {
            pa_context_disconnect(m_ctx);
            pa_context_unref(m_ctx);
            m_ctx = nullptr;
        }
        pa_threaded_mainloop_unlock(m_mainloop);
        pa_threaded_mainloop_stop(m_mainloop);
        pa_threaded_mainloop_free(m_mainloop);
        m_mainloop = nullptr;
    }
}

void AudioService::refreshLists() {
    queryServerInfo();
    queryAllLists();
}

void AudioService::queryServerInfo() {
    if (!m_mainloop || !m_ctx) return;
    pa_threaded_mainloop_lock(m_mainloop);
    if (pa_context_get_state(m_ctx) == PA_CONTEXT_READY) {
        pa_operation *op = pa_context_get_server_info(m_ctx, [](pa_context *, const pa_server_info *info, void *ud) {
            if (!info) return;
            auto *self = static_cast<AudioService *>(ud);
            const QString sink = QString::fromUtf8(info->default_sink_name ? info->default_sink_name : "");
            const QString src = QString::fromUtf8(info->default_source_name ? info->default_source_name : "");
            QMetaObject::invokeMethod(self, [self, sink, src]() {
                self->m_defaultSink = sink;
                self->m_defaultSource = src;
                if (!sink.isEmpty()) self->querySink(sink);
                if (!src.isEmpty()) self->querySource(src);
                self->queryAllLists();
            }, Qt::QueuedConnection);
        }, this);
        if (op) pa_operation_unref(op);
    }
    pa_threaded_mainloop_unlock(m_mainloop);
}

void AudioService::querySink(const QString &name) {
    if (!m_mainloop || !m_ctx || name.isEmpty()) return;
    pa_threaded_mainloop_lock(m_mainloop);
    if (pa_context_get_state(m_ctx) == PA_CONTEXT_READY) {
        const QByteArray utf = name.toUtf8();
        pa_operation *op = pa_context_get_sink_info_by_name(m_ctx, utf.constData(),
            [](pa_context *, const pa_sink_info *info, int eol, void *ud) {
                if (eol || !info) return;
                auto *self = static_cast<AudioService *>(ud);
                const pa_volume_t avg = pa_cvolume_avg(&info->volume);
                const int pct = static_cast<int>(std::lround(static_cast<double>(avg) * 100.0 / PA_VOLUME_NORM));
                const bool muted = info->mute != 0;
                const uint32_t ch = info->volume.channels;
                const QString portName = info->active_port && info->active_port->name
                                             ? QString::fromUtf8(info->active_port->name).toLower()
                                             : QString();
                const QString desc = info->description ? QString::fromUtf8(info->description).toLower() : QString();
                const bool hp = portName.contains(QLatin1String("headphone")) || desc.contains(QLatin1String("headphone"));
                QMetaObject::invokeMethod(self, [self, pct, muted, ch, hp]() {
                    const bool changed = (pct != self->m_sinkVolume || muted != self->m_sinkMuted || hp != self->m_headphoneActive);
                    self->m_sinkVolume = std::clamp(pct, 0, 150);
                    self->m_sinkMuted = muted;
                    self->m_sinkChannels = ch;
                    self->m_headphoneActive = hp;
                    if (changed) {
                        emit self->audioChanged();
                        if (self->m_initialSyncDone)
                            emit self->volumeOsdRequested(self->m_sinkVolume, self->m_sinkMuted);
                    }
                    self->m_initialSyncDone = true;
                }, Qt::QueuedConnection);
            }, this);
        if (op) pa_operation_unref(op);
    }
    pa_threaded_mainloop_unlock(m_mainloop);
}

void AudioService::querySource(const QString &name) {
    if (!m_mainloop || !m_ctx || name.isEmpty()) return;
    pa_threaded_mainloop_lock(m_mainloop);
    if (pa_context_get_state(m_ctx) == PA_CONTEXT_READY) {
        const QByteArray utf = name.toUtf8();
        pa_operation *op = pa_context_get_source_info_by_name(m_ctx, utf.constData(),
            [](pa_context *, const pa_source_info *info, int eol, void *ud) {
                if (eol || !info) return;
                auto *self = static_cast<AudioService *>(ud);
                const pa_volume_t avg = pa_cvolume_avg(&info->volume);
                const int pct = static_cast<int>(std::lround(static_cast<double>(avg) * 100.0 / PA_VOLUME_NORM));
                const bool muted = info->mute != 0;
                const uint32_t ch = info->volume.channels;
                QMetaObject::invokeMethod(self, [self, pct, muted, ch]() {
                    if (pct != self->m_sourceVolume || muted != self->m_sourceMuted) {
                        self->m_sourceVolume = std::clamp(pct, 0, 150);
                        self->m_sourceChannels = std::max<uint32_t>(1, ch);
                        self->m_sourceMuted = muted;
                        emit self->audioChanged();
                    }
                }, Qt::QueuedConnection);
            }, this);
        if (op) pa_operation_unref(op);
    }
    pa_threaded_mainloop_unlock(m_mainloop);
}

void AudioService::querySourceOutputs() {
    if (!m_mainloop || !m_ctx) return;
    pa_threaded_mainloop_lock(m_mainloop);
    if (pa_context_get_state(m_ctx) == PA_CONTEXT_READY) {
        auto *found = new bool(false);
        auto *pair = new std::pair<AudioService *, bool *>(this, found);
        pa_operation *op = pa_context_get_source_output_info_list(m_ctx,
            [](pa_context *, const pa_source_output_info *info, int eol, void *ud) {
                auto *p = static_cast<std::pair<AudioService *, bool *> *>(ud);
                if (eol > 0) {
                    const bool active = *p->second;
                    AudioService *self = p->first;
                    delete p->second;
                    delete p;
                    QMetaObject::invokeMethod(self, [self, active]() {
                        if (active != self->m_micActive) {
                            self->m_micActive = active;
                            emit self->audioChanged();
                        }
                    }, Qt::QueuedConnection);
                    return;
                }
                if (info) {
                    const char *mediaRole = pa_proplist_gets(info->proplist, PA_PROP_MEDIA_ROLE);
                    const char *appId = pa_proplist_gets(info->proplist, PA_PROP_APPLICATION_ID);
                    const bool isProbe = (mediaRole && qstrcmp(mediaRole, "abstract") == 0)
                                         || (appId && qstrcmp(appId, "org.PulseAudio.pavucontrol") == 0);
                    if (!isProbe && !info->corked)
                        *p->second = true;
                }
            }, pair);
        if (op) pa_operation_unref(op);
        else { delete found; delete pair; }
    }
    pa_threaded_mainloop_unlock(m_mainloop);
}

void AudioService::queryAllLists() {
    if (!m_mainloop || !m_ctx) return;
    pa_threaded_mainloop_lock(m_mainloop);
    if (pa_context_get_state(m_ctx) == PA_CONTEXT_READY) {
        // 1. Sinks
        auto *sinksAcc = new QVector<AudioDeviceInfo>();
        auto *sinksPair = new std::pair<AudioService *, QVector<AudioDeviceInfo> *>(this, sinksAcc);
        pa_operation *op1 = pa_context_get_sink_info_list(m_ctx,
            [](pa_context *, const pa_sink_info *info, int eol, void *ud) {
                auto *p = static_cast<std::pair<AudioService *, QVector<AudioDeviceInfo> *> *>(ud);
                if (eol > 0) {
                    QVector<AudioDeviceInfo> list = std::move(*p->second);
                    AudioService *self = p->first;
                    delete p->second;
                    delete p;
                    QMetaObject::invokeMethod(self, [self, list]() mutable {
                        for (auto &d : list) d.isDefault = (d.name == self->m_defaultSink);
                        self->m_sinks = std::move(list);
                        emit self->audioChanged();
                    }, Qt::QueuedConnection);
                    return;
                }
                if (info) {
                    AudioDeviceInfo d;
                    d.index = info->index;
                    d.name = info->name ? QString::fromUtf8(info->name) : QString();
                    d.description = info->description ? QString::fromUtf8(info->description) : d.name;
                    d.volume = static_cast<int>(std::lround(static_cast<double>(pa_cvolume_avg(&info->volume)) * 100.0 / PA_VOLUME_NORM));
                    d.channels = info->volume.channels;
                    d.muted = info->mute != 0;
                    const QString pn = (info->active_port && info->active_port->name) ? QString::fromUtf8(info->active_port->name).toLower() : QString();
                    d.isHeadphones = pn.contains(QLatin1String("headphone")) || d.description.toLower().contains(QLatin1String("headphone"));
                    p->second->push_back(d);
                }
            }, sinksPair);
        if (op1) pa_operation_unref(op1); else { delete sinksAcc; delete sinksPair; }

        // 2. Sources
        auto *srcAcc = new QVector<AudioDeviceInfo>();
        auto *srcPair = new std::pair<AudioService *, QVector<AudioDeviceInfo> *>(this, srcAcc);
        pa_operation *op2 = pa_context_get_source_info_list(m_ctx,
            [](pa_context *, const pa_source_info *info, int eol, void *ud) {
                auto *p = static_cast<std::pair<AudioService *, QVector<AudioDeviceInfo> *> *>(ud);
                if (eol > 0) {
                    QVector<AudioDeviceInfo> list = std::move(*p->second);
                    AudioService *self = p->first;
                    delete p->second;
                    delete p;
                    QMetaObject::invokeMethod(self, [self, list]() mutable {
                        for (auto &d : list) d.isDefault = (d.name == self->m_defaultSource);
                        self->m_sources = std::move(list);
                        emit self->audioChanged();
                    }, Qt::QueuedConnection);
                    return;
                }
                if (info) {
                    if (info->monitor_of_sink != PA_INVALID_INDEX)
                        return;
                    AudioDeviceInfo d;
                    d.index = info->index;
                    d.name = info->name ? QString::fromUtf8(info->name) : QString();
                    d.description = info->description ? QString::fromUtf8(info->description) : d.name;
                    d.volume = static_cast<int>(std::lround(static_cast<double>(pa_cvolume_avg(&info->volume)) * 100.0 / PA_VOLUME_NORM));
                    d.channels = info->volume.channels;
                    d.muted = info->mute != 0;
                    p->second->push_back(d);
                }
            }, srcPair);
        if (op2) pa_operation_unref(op2); else { delete srcAcc; delete srcPair; }

        // 3. Sink inputs (playback streams)
        auto *inAcc = new QVector<AudioStreamInfo>();
        auto *inPair = new std::pair<AudioService *, QVector<AudioStreamInfo> *>(this, inAcc);
        pa_operation *op3 = pa_context_get_sink_input_info_list(m_ctx,
            [](pa_context *, const pa_sink_input_info *info, int eol, void *ud) {
                auto *p = static_cast<std::pair<AudioService *, QVector<AudioStreamInfo> *> *>(ud);
                if (eol > 0) {
                    QVector<AudioStreamInfo> list = std::move(*p->second);
                    AudioService *self = p->first;
                    delete p->second;
                    delete p;
                    QMetaObject::invokeMethod(self, [self, list]() mutable {
                        self->m_sinkInputs = std::move(list);
                        emit self->audioChanged();
                    }, Qt::QueuedConnection);
                    return;
                }
                if (info) {
                    AudioStreamInfo s;
                    s.index = info->index;
                    const char *app = pa_proplist_gets(info->proplist, PA_PROP_APPLICATION_NAME);
                    const char *bin = pa_proplist_gets(info->proplist, PA_PROP_APPLICATION_PROCESS_BINARY);
                    const char *med = pa_proplist_gets(info->proplist, PA_PROP_MEDIA_NAME);
                    s.appName = app ? QString::fromUtf8(app) : (info->name ? QString::fromUtf8(info->name) : QString());
                    s.binary = bin ? QString::fromUtf8(bin) : QString();
                    s.mediaName = med ? QString::fromUtf8(med) : QString();
                    s.volume = static_cast<int>(std::lround(static_cast<double>(pa_cvolume_avg(&info->volume)) * 100.0 / PA_VOLUME_NORM));
                    s.channels = info->volume.channels;
                    s.muted = info->mute != 0;
                    p->second->push_back(s);
                }
            }, inPair);
        if (op3) pa_operation_unref(op3); else { delete inAcc; delete inPair; }
    }
    pa_threaded_mainloop_unlock(m_mainloop);
}

QStringList AudioService::playbackStreamLabels() const {
    QStringList out;
    for (const auto &s : m_sinkInputs) {
        if (!s.appName.isEmpty()) out.push_back(s.appName);
        if (!s.binary.isEmpty()) out.push_back(s.binary);
    }
    return out;
}

void AudioService::setSinkVolume(int percent) {
    if (!m_mainloop || !m_ctx || m_defaultSink.isEmpty()) return;
    const int clamped = std::clamp(percent, 0, 100);
    pa_cvolume cv;
    pa_cvolume_set(&cv, std::max<uint8_t>(1, static_cast<uint8_t>(m_sinkChannels)),
                   static_cast<pa_volume_t>(std::lround(clamped * static_cast<double>(PA_VOLUME_NORM) / 100.0)));
    pa_threaded_mainloop_lock(m_mainloop);
    if (pa_context_get_state(m_ctx) == PA_CONTEXT_READY) {
        const QByteArray utf = m_defaultSink.toUtf8();
        pa_operation *op = pa_context_set_sink_volume_by_name(m_ctx, utf.constData(), &cv, nullptr, nullptr);
        if (op) pa_operation_unref(op);
    }
    pa_threaded_mainloop_unlock(m_mainloop);
}

void AudioService::adjustVolume(int deltaPercent) {
    setSinkVolume(m_sinkVolume + deltaPercent);
}

void AudioService::setSinkMute(bool muted) {
    if (!m_mainloop || !m_ctx || m_defaultSink.isEmpty()) return;
    pa_threaded_mainloop_lock(m_mainloop);
    if (pa_context_get_state(m_ctx) == PA_CONTEXT_READY) {
        const QByteArray utf = m_defaultSink.toUtf8();
        pa_operation *op = pa_context_set_sink_mute_by_name(m_ctx, utf.constData(), muted ? 1 : 0, nullptr, nullptr);
        if (op) pa_operation_unref(op);
    }
    pa_threaded_mainloop_unlock(m_mainloop);
}

void AudioService::setSourceVolume(int percent) {
    if (!m_mainloop || !m_ctx || m_defaultSource.isEmpty()) return;
    const int clamped = std::clamp(percent, 0, 100);
    pa_cvolume cv;
    pa_cvolume_set(&cv, std::max<uint8_t>(1, static_cast<uint8_t>(m_sourceChannels)),
                   static_cast<pa_volume_t>(std::lround(clamped * static_cast<double>(PA_VOLUME_NORM) / 100.0)));
    pa_threaded_mainloop_lock(m_mainloop);
    if (pa_context_get_state(m_ctx) == PA_CONTEXT_READY) {
        const QByteArray utf = m_defaultSource.toUtf8();
        pa_operation *op = pa_context_set_source_volume_by_name(m_ctx, utf.constData(), &cv, nullptr, nullptr);
        if (op) pa_operation_unref(op);
    }
    pa_threaded_mainloop_unlock(m_mainloop);
}

void AudioService::setSourceMute(bool muted) {
    if (!m_mainloop || !m_ctx || m_defaultSource.isEmpty()) return;
    pa_threaded_mainloop_lock(m_mainloop);
    if (pa_context_get_state(m_ctx) == PA_CONTEXT_READY) {
        const QByteArray utf = m_defaultSource.toUtf8();
        pa_operation *op = pa_context_set_source_mute_by_name(m_ctx, utf.constData(), muted ? 1 : 0, nullptr, nullptr);
        if (op) pa_operation_unref(op);
    }
    pa_threaded_mainloop_unlock(m_mainloop);
}

void AudioService::toggleSinkMute() {
    setSinkMute(!m_sinkMuted);
}

void AudioService::toggleSourceMute() {
    setSourceMute(!m_sourceMuted);
}

void AudioService::toggleAllMuted() {
    const bool anyAudible = !m_sinkMuted || (!m_sources.isEmpty() && !m_sourceMuted);
    setSinkMute(anyAudible);
    if (!m_sources.isEmpty())
        setSourceMute(anyAudible);
}

void AudioService::setDefaultSink(const QString &name) {
    if (name.isEmpty()) return;
    QProcess::startDetached(QStringLiteral("quattro-audio-output-set-default"), {name});
    pa_threaded_mainloop_lock(m_mainloop);
    if (pa_context_get_state(m_ctx) == PA_CONTEXT_READY) {
        const QByteArray utf = name.toUtf8();
        pa_operation *op = pa_context_set_default_sink(m_ctx, utf.constData(), nullptr, nullptr);
        if (op) pa_operation_unref(op);
    }
    pa_threaded_mainloop_unlock(m_mainloop);
}

void AudioService::setDefaultSource(const QString &name) {
    if (name.isEmpty()) return;
    QProcess::startDetached(QStringLiteral("quattro-audio-input-set-default"), {name});
    pa_threaded_mainloop_lock(m_mainloop);
    if (pa_context_get_state(m_ctx) == PA_CONTEXT_READY) {
        const QByteArray utf = name.toUtf8();
        pa_operation *op = pa_context_set_default_source(m_ctx, utf.constData(), nullptr, nullptr);
        if (op) pa_operation_unref(op);
    }
    pa_threaded_mainloop_unlock(m_mainloop);
}

void AudioService::setSinkInputVolume(uint32_t index, int percent) {
    if (!m_mainloop || !m_ctx) return;
    const int clamped = std::clamp(percent, 0, 150);
    uint32_t ch = 2;
    for (const auto &s : m_sinkInputs) {
        if (s.index == index) { ch = std::max<uint32_t>(1, s.channels); break; }
    }
    pa_cvolume cv;
    pa_cvolume_set(&cv, static_cast<uint8_t>(ch),
                   static_cast<pa_volume_t>(std::lround(clamped * static_cast<double>(PA_VOLUME_NORM) / 100.0)));
    pa_threaded_mainloop_lock(m_mainloop);
    if (pa_context_get_state(m_ctx) == PA_CONTEXT_READY) {
        pa_operation *op = pa_context_set_sink_input_volume(m_ctx, index, &cv, nullptr, nullptr);
        if (op) pa_operation_unref(op);
    }
    pa_threaded_mainloop_unlock(m_mainloop);
}

void AudioService::setSinkInputMute(uint32_t index, bool muted) {
    if (!m_mainloop || !m_ctx) return;
    pa_threaded_mainloop_lock(m_mainloop);
    if (pa_context_get_state(m_ctx) == PA_CONTEXT_READY) {
        pa_operation *op = pa_context_set_sink_input_mute(m_ctx, index, muted ? 1 : 0, nullptr, nullptr);
        if (op) pa_operation_unref(op);
    }
    pa_threaded_mainloop_unlock(m_mainloop);
}

} // namespace quattro
