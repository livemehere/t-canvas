#include "CanvasView.h"

#include <cmath>

Vec2 CanvasView::ScreenToWorld(Vec2 point) const {
    return {
        (point.x - pan.x) / zoom,
        (point.y - pan.y) / zoom
    };
}

Vec2 CanvasView::WorldToScreen(Vec2 point) const {
    return {
        point.x * zoom + pan.x,
        point.y * zoom + pan.y
    };
}

void CanvasView::ZoomAt(Vec2 screenPoint, float wheelDelta, float sensitivity) {
    const float oldZoom = zoom;
    const float zoomFactor = std::pow(1.12f, wheelDelta * sensitivity);
    zoom = Clamp(zoom * zoomFactor, 0.1f, 16.0f);

    const Vec2 worldBefore{
        (screenPoint.x - pan.x) / oldZoom,
        (screenPoint.y - pan.y) / oldZoom
    };

    pan = {
        screenPoint.x - worldBefore.x * zoom,
        screenPoint.y - worldBefore.y * zoom
    };
}
