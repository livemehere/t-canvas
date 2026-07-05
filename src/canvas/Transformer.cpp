#include "Transformer.h"

#include <array>
#include <cmath>

#include <skia/core/SkColor.h>
#include <skia/core/SkPaint.h>
#include <skia/core/SkPath.h>
#include <skia/core/SkRect.h>

Vec2 Transformer::GetRotateHandle(const Shape &shape) const {
    const auto corners = GetShapeCorners(shape);
    const Vec2 top = Midpoint(corners[0], corners[1]);
    const Vec2 outward = Rotate({0.0f, -40.0f}, shape.rotation);
    return top + outward;
}

Vec2 Transformer::GetHandleWorldPosition(const Shape &shape, DragMode mode) const {
    const auto corners = GetShapeCorners(shape);
    switch (mode) {
        case DragMode::ResizeTopLeft:
            return corners[0];
        case DragMode::ResizeTopRight:
            return corners[1];
        case DragMode::ResizeBottomRight:
            return corners[2];
        case DragMode::ResizeBottomLeft:
            return corners[3];
        case DragMode::Rotate:
            return GetRotateHandle(shape);
        default:
            return shape.position;
    }
}

DragMode Transformer::HitTest(Vec2 screen, const Shape &shape, const CanvasView &view) const {
    constexpr float handleRadius = 7.0f;
    const auto corners = GetShapeCorners(shape);
    const std::array<DragMode, 4> cornerModes{
        DragMode::ResizeTopLeft,
        DragMode::ResizeTopRight,
        DragMode::ResizeBottomRight,
        DragMode::ResizeBottomLeft
    };

    for (int i = 0; i < 4; ++i) {
        if (Distance(screen, view.WorldToScreen(corners[i])) <= handleRadius) {
            return cornerModes[i];
        }
    }

    if (Distance(screen, view.WorldToScreen(GetRotateHandle(shape))) <= handleRadius + 2.0f) {
        return DragMode::Rotate;
    }

    if (PointInShape(view.ScreenToWorld(screen), shape)) {
        return DragMode::Move;
    }

    return DragMode::None;
}

void Transformer::BeginDrag(DragMode mode, Vec2 mouseWorld, const Shape &shape) {
    mode_ = mode;
    startMouseWorld_ = mouseWorld;
    startShape_ = shape;
    handleGrabOffset_ = {0.0f, 0.0f};

    if (mode_ == DragMode::Rotate) {
        const float mouseAngle = RadiansToDegrees(std::atan2(
            mouseWorld.y - shape.position.y,
            mouseWorld.x - shape.position.x
        ));
        rotationOffset_ = shape.rotation - mouseAngle;
    } else if (
        mode_ == DragMode::ResizeTopLeft ||
        mode_ == DragMode::ResizeTopRight ||
        mode_ == DragMode::ResizeBottomRight ||
        mode_ == DragMode::ResizeBottomLeft
    ) {
        handleGrabOffset_ = mouseWorld - GetHandleWorldPosition(shape, mode_);
    }
}

void Transformer::UpdateDrag(Vec2 mouseWorld, Shape &shape) {
    switch (mode_) {
        case DragMode::Move: {
            shape = startShape_;
            const Vec2 delta = mouseWorld - startMouseWorld_;
            shape.position = startShape_.position + delta;
            break;
        }
        case DragMode::ResizeTopLeft:
        case DragMode::ResizeTopRight:
        case DragMode::ResizeBottomRight:
        case DragMode::ResizeBottomLeft:
            shape = startShape_;
            ResizeFromCorner(shape, mouseWorld - handleGrabOffset_, mode_);
            break;
        case DragMode::Rotate: {
            shape = startShape_;
            const float mouseAngle = RadiansToDegrees(std::atan2(
                mouseWorld.y - startShape_.position.y,
                mouseWorld.x - startShape_.position.x
            ));
            shape.rotation = mouseAngle + rotationOffset_;
            break;
        }
        case DragMode::None:
            break;
    }
}

void Transformer::EndDrag() {
    mode_ = DragMode::None;
}

DragMode Transformer::ActiveMode() const {
    return mode_;
}

void Transformer::ResizeFromCorner(Shape &shape, Vec2 mouseWorld, DragMode mode) const {
    Vec2 anchorLocal;
    switch (mode) {
        case DragMode::ResizeTopLeft:
            anchorLocal = {shape.size.x * 0.5f, shape.size.y * 0.5f};
            break;
        case DragMode::ResizeTopRight:
            anchorLocal = {-shape.size.x * 0.5f, shape.size.y * 0.5f};
            break;
        case DragMode::ResizeBottomRight:
            anchorLocal = {-shape.size.x * 0.5f, -shape.size.y * 0.5f};
            break;
        case DragMode::ResizeBottomLeft:
            anchorLocal = {shape.size.x * 0.5f, -shape.size.y * 0.5f};
            break;
        default:
            return;
    }

    const Vec2 anchorWorld = LocalToWorld(anchorLocal, shape);
    const Vec2 anchorToMouseLocal = Rotate(mouseWorld - anchorWorld, -shape.rotation);
    const float newWidth = std::max(20.0f, std::abs(anchorToMouseLocal.x));
    const float newHeight = std::max(20.0f, std::abs(anchorToMouseLocal.y));
    const Vec2 anchorToCenterLocal{
        anchorToMouseLocal.x * 0.5f,
        anchorToMouseLocal.y * 0.5f
    };

    shape.size = {newWidth, newHeight};
    shape.position = anchorWorld + Rotate(anchorToCenterLocal, shape.rotation);
}

void Transformer::Draw(SkCanvas *canvas, const Shape &shape, const CanvasView &view) const {
    const auto corners = GetShapeCorners(shape);

    SkPaint outline;
    outline.setAntiAlias(true);
    outline.setColor(SkColorSetARGB(255, 255, 255, 255));
    outline.setStyle(SkPaint::kStroke_Style);
    outline.setStrokeWidth(1.5f);

    SkPath path;
    Vec2 p = view.WorldToScreen(corners[0]);
    path.moveTo(p.x, p.y);
    for (int i = 1; i < 4; ++i) {
        p = view.WorldToScreen(corners[i]);
        path.lineTo(p.x, p.y);
    }
    path.close();
    canvas->drawPath(path, outline);

    SkPaint handle;
    handle.setAntiAlias(true);
    handle.setColor(SK_ColorWHITE);
    handle.setStyle(SkPaint::kFill_Style);

    SkPaint rotateHandle = handle;
    rotateHandle.setColor(SkColorSetARGB(255, 255, 220, 80));

    constexpr float half = 6.0f;
    for (Vec2 corner: corners) {
        const Vec2 screen = view.WorldToScreen(corner);
        canvas->drawRect(SkRect::MakeXYWH(screen.x - half, screen.y - half, half * 2.0f, half * 2.0f), handle);
    }

    const Vec2 top = view.WorldToScreen(Midpoint(corners[0], corners[1]));
    const Vec2 rotate = view.WorldToScreen(GetRotateHandle(shape));
    canvas->drawLine(top.x, top.y, rotate.x, rotate.y, outline);
    canvas->drawCircle(rotate.x, rotate.y, 7.0f, rotateHandle);
}
