#pragma once

#include "Math2D.h"

struct CanvasView {
    Vec2 pan{0.0f, 0.0f};
    Vec2 lastMouse{0.0f, 0.0f};
    float zoom = 1.0f;
    bool isPanning = false;

    Vec2 ScreenToWorld(Vec2 point) const;
    Vec2 WorldToScreen(Vec2 point) const;
    void ZoomAt(Vec2 screenPoint, float wheelDelta, float sensitivity = 1.0f);
};
