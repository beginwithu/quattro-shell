#include "LayerShellIntegration.h"

#include <QGuiApplication>
#include <QtWaylandClient/private/qwaylanddisplay_p.h>
#include <QtWaylandClient/private/qwaylandscreen_p.h>
#include <QtWaylandClient/private/qwaylandwindow_p.h>
#include <wayland-client-core.h>

Q_DECLARE_METATYPE(quattro::LayerSpec)

namespace quattro {

namespace {
constexpr const char *kLayerSpecProp = "_quattro_layer_spec";
} // namespace

LayerShellIntegration &LayerShellIntegration::instance() {
    static LayerShellIntegration inst;
    return inst;
}

LayerShellIntegration::LayerShellIntegration()
    : QtWaylandClient::QWaylandShellIntegrationTemplate<LayerShellIntegration>(4)
{
}

bool LayerShellIntegration::ensureInitialized() {
    if (m_initialized)
        return isActive();
    m_initialized = true;
    QWaylandClientExtension::initialize();
    return isActive();
}

QtWaylandClient::QWaylandShellSurface *LayerShellIntegration::createShellSurface(QtWaylandClient::QWaylandWindow *window) {
    if (!ensureInitialized())
        return nullptr;

    LayerSpec spec;
    if (window && window->window()) {
        const QVariant v = window->window()->property(kLayerSpecProp);
        if (v.canConvert<LayerSpec>())
            spec = v.value<LayerSpec>();
    }
    return new LayerShellSurface(this, window, spec);
}

void LayerShellIntegration::configureWindow(QWindow *window, const LayerSpec &spec) {
    if (!window)
        return;
    window->setProperty(kLayerSpecProp, QVariant::fromValue(spec));
    window->setFlag(Qt::FramelessWindowHint, true);
    window->setFlag(Qt::WindowTransparentForInput, spec.clickThrough);
    if (!window->handle())
        window->create();

    if (auto *ww = dynamic_cast<QtWaylandClient::QWaylandWindow *>(window->handle()))
        ww->setShellIntegration(&instance());
}

void LayerShellIntegration::updateWindowSpec(QWindow *window, const LayerSpec &spec) {
    if (!window)
        return;
    window->setProperty(kLayerSpecProp, QVariant::fromValue(spec));
    window->setFlag(Qt::WindowTransparentForInput, spec.clickThrough);
    if (auto *ww = dynamic_cast<QtWaylandClient::QWaylandWindow *>(window->handle())) {
        if (auto *ls = dynamic_cast<LayerShellSurface *>(ww->shellSurface()))
            ls->applySpec(spec, true);
    }
}

LayerShellSurface::LayerShellSurface(QtWayland::zwlr_layer_shell_v1 *shell,
                                     QtWaylandClient::QWaylandWindow *window,
                                     const LayerSpec &spec)
    : QtWaylandClient::QWaylandShellSurface(window)
    , m_spec(spec)
{
    ::wl_output *output = nullptr;
    if (auto *ws = window->waylandScreen())
        output = ws->output();

    init(shell->get_layer_surface(window->wlSurface(),
                                  output,
                                  static_cast<uint32_t>(spec.layer),
                                  spec.scope));
    applySpec(spec, false);
}

LayerShellSurface::~LayerShellSurface() {
    if (isInitialized())
        destroy();
}

void LayerShellSurface::applySpec(const LayerSpec &spec, bool commitNow) {
    m_spec = spec;
    if (!isInitialized())
        return;

    if (version() >= 2)
        set_layer(static_cast<uint32_t>(spec.layer));

    set_anchor(spec.anchors);

    QSize sz = spec.desiredSize;
    if (sz.isEmpty() && window() && window()->window())
        sz = window()->window()->size();

    const bool anchoredH = (spec.anchors & static_cast<uint32_t>(AnchorEdge::Left))
                           && (spec.anchors & static_cast<uint32_t>(AnchorEdge::Right));
    const bool anchoredV = (spec.anchors & static_cast<uint32_t>(AnchorEdge::Top))
                           && (spec.anchors & static_cast<uint32_t>(AnchorEdge::Bottom));

    const uint32_t w = anchoredH ? 0u : static_cast<uint32_t>(std::max(1, sz.width()));
    const uint32_t h = anchoredV ? 0u : static_cast<uint32_t>(std::max(1, sz.height()));
    set_size(w, h);

    set_exclusive_zone(spec.exclusiveZone);
    set_margin(spec.margins.top(), spec.margins.right(), spec.margins.bottom(), spec.margins.left());

    uint32_t kbd = static_cast<uint32_t>(spec.keyboard);
    if (kbd == static_cast<uint32_t>(KeyboardFocus::OnDemand) && version() < 4)
        kbd = static_cast<uint32_t>(KeyboardFocus::Exclusive);
    set_keyboard_interactivity(kbd);

    if (commitNow && window() && window()->wlSurface())
        wl_surface_commit(window()->wlSurface());
}

void LayerShellSurface::setWindowGeometry(const QRect &rect) {
    if (!isInitialized())
        return;
    const bool anchoredH = (m_spec.anchors & static_cast<uint32_t>(AnchorEdge::Left))
                           && (m_spec.anchors & static_cast<uint32_t>(AnchorEdge::Right));
    const bool anchoredV = (m_spec.anchors & static_cast<uint32_t>(AnchorEdge::Top))
                           && (m_spec.anchors & static_cast<uint32_t>(AnchorEdge::Bottom));
    const uint32_t w = anchoredH ? 0u : static_cast<uint32_t>(std::max(1, rect.width()));
    const uint32_t h = anchoredV ? 0u : static_cast<uint32_t>(std::max(1, rect.height()));
    set_size(w, h);
}

void LayerShellSurface::applyConfigure() {
    if (!m_pendingConfigure)
        return;
    m_pendingConfigure = false;
    ack_configure(m_pendingSerial);

    QSize next = m_pendingSize;
    if (window() && window()->window()) {
        const QSize cur = window()->window()->size();
        if (next.width() <= 0) next.setWidth(std::max(1, cur.width()));
        if (next.height() <= 0) next.setHeight(std::max(1, cur.height()));
    }
    if (next.width() > 0 && next.height() > 0)
        resizeFromApplyConfigure(next);
}

void LayerShellSurface::zwlr_layer_surface_v1_configure(uint32_t serial, uint32_t width, uint32_t height) {
    m_pendingSerial = serial;
    m_pendingSize = QSize(static_cast<int>(width), static_cast<int>(height));
    m_pendingConfigure = true;

    const bool firstExpose = !m_configured;
    m_configured = true;

    applyConfigureWhenPossible();
    if (firstExpose && window())
        window()->updateExposure();
}

void LayerShellSurface::zwlr_layer_surface_v1_closed() {
    if (window() && window()->window())
        window()->window()->close();
}

} // namespace quattro
