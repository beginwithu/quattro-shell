#pragma once

#include <QMargins>
#include <QObject>
#include <QSize>
#include <QString>
#include <QWindow>

#include <QtWaylandClient/private/qwaylandshellintegration_p.h>
#include <QtWaylandClient/private/qwaylandshellsurface_p.h>
#include "qwayland-wlr-layer-shell-unstable-v1.h"

namespace quattro {

enum class LayerType : uint32_t {
    Background = 0,
    Bottom = 1,
    Top = 2,
    Overlay = 3,
};

enum class AnchorEdge : uint32_t {
    None = 0,
    Top = 1,
    Bottom = 2,
    Left = 4,
    Right = 8,
};

enum class KeyboardFocus : uint32_t {
    None = 0,
    Exclusive = 1,
    OnDemand = 2,
};

struct LayerSpec {
    LayerType layer = LayerType::Top;
    uint32_t anchors = 0; // bitmask of AnchorEdge
    int exclusiveZone = 0;
    QMargins margins;
    KeyboardFocus keyboard = KeyboardFocus::None;
    QString scope = QStringLiteral("quattro");
    QSize desiredSize; // 0 means compositor-assigned along anchored axis
    bool clickThrough = false;
};

class LayerShellSurface;

class LayerShellIntegration : public QtWaylandClient::QWaylandShellIntegrationTemplate<LayerShellIntegration>,
                              public QtWayland::zwlr_layer_shell_v1 {
public:
    static LayerShellIntegration &instance();

    bool ensureInitialized();
    QtWaylandClient::QWaylandShellSurface *createShellSurface(QtWaylandClient::QWaylandWindow *window) override;

    static void configureWindow(QWindow *window, const LayerSpec &spec);
    static void updateWindowSpec(QWindow *window, const LayerSpec &spec);

private:
    LayerShellIntegration();
    bool m_initialized = false;
};

class LayerShellSurface : public QtWaylandClient::QWaylandShellSurface,
                          public QtWayland::zwlr_layer_surface_v1 {
    Q_OBJECT
public:
    LayerShellSurface(QtWayland::zwlr_layer_shell_v1 *shell,
                      QtWaylandClient::QWaylandWindow *window,
                      const LayerSpec &spec);
    ~LayerShellSurface() override;

    bool isExposed() const override { return m_configured; }
    void applyConfigure() override;
    void setWindowGeometry(const QRect &rect) override;

    void applySpec(const LayerSpec &spec, bool commitNow = true);

protected:
    void zwlr_layer_surface_v1_configure(uint32_t serial, uint32_t width, uint32_t height) override;
    void zwlr_layer_surface_v1_closed() override;

private:
    LayerSpec m_spec;
    bool m_configured = false;
    bool m_pendingConfigure = false;
    uint32_t m_pendingSerial = 0;
    QSize m_pendingSize;
};

} // namespace quattro
