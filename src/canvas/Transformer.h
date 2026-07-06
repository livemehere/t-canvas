#pragma once

#include <skia/core/SkCanvas.h>

#include "CanvasView.h"
#include "Shape.h"

enum class DragMode {
    None,
    Move,
    ResizeTopLeft,
    ResizeTopRight,
    ResizeBottomRight,
    ResizeBottomLeft,
    Rotate,
    LineStart,
    LineEnd
};

class Transformer {
public:
    DragMode HitTest(Vec2 screen, const Shape &shape, const CanvasView &view) const;
    void BeginDrag(DragMode mode, Vec2 mouseWorld, const Shape &shape);
    void UpdateDrag(Vec2 mouseWorld, Shape &shape);
    void EndDrag();
    void Draw(SkCanvas *canvas, const Shape &shape, const CanvasView &view) const;

    DragMode ActiveMode() const;

private:
    DragMode mode_ = DragMode::None;
    Vec2 startMouseWorld_;
    Vec2 handleGrabOffset_;
    Shape startShape_;
    float rotationOffset_ = 0.0f;

    Vec2 GetRotateHandle(const Shape &shape) const;
    Vec2 GetHandleWorldPosition(const Shape &shape, DragMode mode) const;
    void ResizeFromCorner(Shape &shape, Vec2 mouseWorld, DragMode mode) const;
    void ResizeLineEndpoint(Shape &shape, Vec2 mouseWorld, DragMode mode) const;
};
