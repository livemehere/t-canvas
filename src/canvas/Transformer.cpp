#include "Transformer.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <skia/core/SkColor.h>
#include <skia/core/SkPaint.h>
#include <skia/core/SkPath.h>
#include <skia/core/SkRect.h>

namespace {
bool IsLineShape(const Shape &shape) {
    return shape.type == ShapeType::Line || shape.type == ShapeType::Arrow;
}

Vec2 LineStart(const Shape &shape) {
    return LocalToWorld({-shape.size.x * 0.5f, 0.0f}, shape);
}

Vec2 LineEnd(const Shape &shape) {
    return LocalToWorld({shape.size.x * 0.5f, 0.0f}, shape);
}

Vec2 ConstrainLineEndpoint(Vec2 anchor, Vec2 endpoint) {
    const Vec2 delta = endpoint - anchor;
    if (std::abs(delta.x) >= std::abs(delta.y)) {
        return {endpoint.x, anchor.y};
    }
    return {anchor.x, endpoint.y};
}

float DistanceToSegment(Vec2 point, Vec2 a, Vec2 b) {
    const Vec2 ab = b - a;
    const Vec2 ap = point - a;
    const float lengthSq = ab.x * ab.x + ab.y * ab.y;
    if (lengthSq <= 0.0001f) {
        return Distance(point, a);
    }
    const float t = std::max(0.0f, std::min(1.0f, (ap.x * ab.x + ap.y * ab.y) / lengthSq));
    return Distance(point, a + ab * t);
}
} // namespace

Vec2 Transformer::GetRotateHandle(const Shape &shape) const {
    const auto corners = GetShapeCorners(shape);
    const Vec2 top = Midpoint(corners[0], corners[1]);
    const Vec2 outward = Rotate({0.0f, -40.0f}, shape.rotation);
    return top + outward;
}

Vec2 Transformer::GetHandleWorldPosition(const Shape &shape, DragMode mode) const {
    if (IsLineShape(shape)) {
        if (mode == DragMode::LineStart) {
            return LineStart(shape);
        }
        if (mode == DragMode::LineEnd) {
            return LineEnd(shape);
        }
    }

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
        case DragMode::CropTopLeft:
            return corners[0];
        case DragMode::CropTopRight:
            return corners[1];
        case DragMode::CropBottomRight:
            return corners[2];
        case DragMode::CropBottomLeft:
            return corners[3];
        case DragMode::Rotate:
            return GetRotateHandle(shape);
        default:
            return shape.position;
    }
}

DragMode Transformer::HitTest(Vec2 screen, const Shape &shape, const CanvasView &view, bool cropMode) const {
    constexpr float handleRadius = 7.0f;

    if (IsLineShape(shape)) {
        const Vec2 start = view.WorldToScreen(LineStart(shape));
        const Vec2 end = view.WorldToScreen(LineEnd(shape));
        if (Distance(screen, start) <= handleRadius) {
            return DragMode::LineStart;
        }
        if (Distance(screen, end) <= handleRadius) {
            return DragMode::LineEnd;
        }
        if (DistanceToSegment(screen, start, end) <= 6.0f) {
            return DragMode::Move;
        }
        return DragMode::None;
    }

    const auto corners = GetShapeCorners(shape);
    const auto hitCorners = corners;
    const std::array<DragMode, 4> cornerModes = cropMode && shape.type == ShapeType::Image
        ? std::array<DragMode, 4>{
            DragMode::CropTopLeft,
            DragMode::CropTopRight,
            DragMode::CropBottomRight,
            DragMode::CropBottomLeft
        }
        : std::array<DragMode, 4>{
            DragMode::ResizeTopLeft,
            DragMode::ResizeTopRight,
            DragMode::ResizeBottomRight,
            DragMode::ResizeBottomLeft
        };

    for (int i = 0; i < 4; ++i) {
        if (Distance(screen, view.WorldToScreen(hitCorners[i])) <= handleRadius) {
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
        mode_ == DragMode::ResizeBottomLeft ||
        mode_ == DragMode::LineStart ||
        mode_ == DragMode::LineEnd ||
        mode_ == DragMode::CropTopLeft ||
        mode_ == DragMode::CropTopRight ||
        mode_ == DragMode::CropBottomRight ||
        mode_ == DragMode::CropBottomLeft
    ) {
        handleGrabOffset_ = mouseWorld - GetHandleWorldPosition(shape, mode_);
    }
}

void Transformer::UpdateDrag(
    Vec2 mouseWorld,
    Shape &shape,
    bool keepAspectRatio,
    bool resizeFromCenter,
    bool cropMode
) {
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
            ResizeFromCorner(shape, mouseWorld - handleGrabOffset_, mode_, keepAspectRatio, resizeFromCenter);
            break;
        case DragMode::LineStart:
        case DragMode::LineEnd:
            shape = startShape_;
            ResizeLineEndpoint(
                shape,
                mouseWorld - handleGrabOffset_,
                mode_,
                keepAspectRatio
            );
            break;
        case DragMode::CropTopLeft:
        case DragMode::CropTopRight:
        case DragMode::CropBottomRight:
        case DragMode::CropBottomLeft:
            shape = startShape_;
            if (cropMode && shape.type == ShapeType::Image) {
                CropFromCorner(shape, mouseWorld - handleGrabOffset_, mode_);
            }
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

void Transformer::ResizeFromCorner(
    Shape &shape,
    Vec2 mouseWorld,
    DragMode mode,
    bool keepAspectRatio,
    bool resizeFromCenter
) const {
    Vec2 anchorLocal;
    Vec2 draggedLocal;
    switch (mode) {
        case DragMode::ResizeTopLeft:
            anchorLocal = {shape.size.x * 0.5f, shape.size.y * 0.5f};
            draggedLocal = {-shape.size.x * 0.5f, -shape.size.y * 0.5f};
            break;
        case DragMode::ResizeTopRight:
            anchorLocal = {-shape.size.x * 0.5f, shape.size.y * 0.5f};
            draggedLocal = {shape.size.x * 0.5f, -shape.size.y * 0.5f};
            break;
        case DragMode::ResizeBottomRight:
            anchorLocal = {-shape.size.x * 0.5f, -shape.size.y * 0.5f};
            draggedLocal = {shape.size.x * 0.5f, shape.size.y * 0.5f};
            break;
        case DragMode::ResizeBottomLeft:
            anchorLocal = {shape.size.x * 0.5f, -shape.size.y * 0.5f};
            draggedLocal = {-shape.size.x * 0.5f, shape.size.y * 0.5f};
            break;
        default:
            return;
    }

    if (resizeFromCenter) {
        Vec2 centerToMouseLocal = Rotate(mouseWorld - startShape_.position, -shape.rotation);
        float newWidth = std::max(20.0f, std::abs(centerToMouseLocal.x) * 2.0f);
        float newHeight = std::max(20.0f, std::abs(centerToMouseLocal.y) * 2.0f);

        if (keepAspectRatio && startShape_.size.y > 0.0001f) {
            const float aspect = startShape_.size.x / startShape_.size.y;
            if (newWidth / std::max(newHeight, 0.0001f) > aspect) {
                newHeight = std::max(20.0f, newWidth / aspect);
            } else {
                newWidth = std::max(20.0f, newHeight * aspect);
            }
        }

        shape.size = {newWidth, newHeight};
        shape.position = startShape_.position;
        return;
    }

    const Vec2 anchorWorld = LocalToWorld(anchorLocal, shape);
    Vec2 anchorToMouseLocal = Rotate(mouseWorld - anchorWorld, -shape.rotation);
    float newWidth = std::max(20.0f, std::abs(anchorToMouseLocal.x));
    float newHeight = std::max(20.0f, std::abs(anchorToMouseLocal.y));

    if (keepAspectRatio && startShape_.size.y > 0.0001f) {
        const float aspect = startShape_.size.x / startShape_.size.y;
        if (newWidth / std::max(newHeight, 0.0001f) > aspect) {
            newHeight = std::max(20.0f, newWidth / aspect);
        } else {
            newWidth = std::max(20.0f, newHeight * aspect);
        }
        anchorToMouseLocal.x = std::copysign(newWidth, anchorToMouseLocal.x);
        anchorToMouseLocal.y = std::copysign(newHeight, anchorToMouseLocal.y);
    }

    const Vec2 anchorToCenterLocal{
        anchorToMouseLocal.x * 0.5f,
        anchorToMouseLocal.y * 0.5f
    };

    shape.size = {newWidth, newHeight};
    shape.position = anchorWorld + Rotate(anchorToCenterLocal, shape.rotation);
}

void Transformer::ResizeLineEndpoint(Shape &shape, Vec2 mouseWorld, DragMode mode, bool constrainAngle) const {
    const Vec2 anchor = mode == DragMode::LineStart ? LineEnd(startShape_) : LineStart(startShape_);
    const Vec2 dragged = constrainAngle ? ConstrainLineEndpoint(anchor, mouseWorld) : mouseWorld;
    const Vec2 delta = dragged - anchor;
    const float length = std::max(2.0f, std::sqrt(delta.x * delta.x + delta.y * delta.y));

    shape.position = Midpoint(anchor, dragged);
    shape.size.x = length;
    shape.size.y = 1.0f;
    shape.rotation = RadiansToDegrees(std::atan2(delta.y, delta.x));
    if (mode == DragMode::LineStart) {
        shape.rotation += 180.0f;
    }
}

void Transformer::CropFromCorner(Shape &shape, Vec2 mouseWorld, DragMode mode) const {
    const Vec2 local = WorldToLocal(mouseWorld, startShape_);
    const float halfWidth = std::max(1.0f, startShape_.size.x * 0.5f);
    const float halfHeight = std::max(1.0f, startShape_.size.y * 0.5f);
    const float u = Clamp((local.x + halfWidth) / (halfWidth * 2.0f), 0.0f, 1.0f);
    const float v = Clamp((local.y + halfHeight) / (halfHeight * 2.0f), 0.0f, 1.0f);

    float displayLeft = 0.0f;
    float displayTop = 0.0f;
    float displayRight = 1.0f;
    float displayBottom = 1.0f;
    switch (mode) {
        case DragMode::CropTopLeft:
            displayLeft = u;
            displayTop = v;
            break;
        case DragMode::CropTopRight:
            displayRight = u;
            displayTop = v;
            break;
        case DragMode::CropBottomRight:
            displayRight = u;
            displayBottom = v;
            break;
        case DragMode::CropBottomLeft:
            displayLeft = u;
            displayBottom = v;
            break;
        default:
            break;
    }

    displayLeft = std::min(displayLeft, displayRight - 0.01f);
    displayTop = std::min(displayTop, displayBottom - 0.01f);
    displayRight = std::max(displayRight, displayLeft + 0.01f);
    displayBottom = std::max(displayBottom, displayTop + 0.01f);

    const float sourceWidth = startShape_.cropRight - startShape_.cropLeft;
    const float sourceHeight = startShape_.cropBottom - startShape_.cropTop;
    shape.cropLeft = startShape_.cropLeft + sourceWidth * displayLeft;
    shape.cropTop = startShape_.cropTop + sourceHeight * displayTop;
    shape.cropRight = startShape_.cropLeft + sourceWidth * displayRight;
    shape.cropBottom = startShape_.cropTop + sourceHeight * displayBottom;
    shape.size = {
        std::max(1.0f, startShape_.size.x * (displayRight - displayLeft)),
        std::max(1.0f, startShape_.size.y * (displayBottom - displayTop))
    };
    const Vec2 localCenter{
        (displayLeft + displayRight - 1.0f) * startShape_.size.x * 0.5f,
        (displayTop + displayBottom - 1.0f) * startShape_.size.y * 0.5f
    };
    shape.position = LocalToWorld(localCenter, startShape_);
}

void Transformer::Draw(SkCanvas *canvas, const Shape &shape, const CanvasView &view, bool cropMode) const {
    if (IsLineShape(shape)) {
        const Vec2 start = view.WorldToScreen(LineStart(shape));
        const Vec2 end = view.WorldToScreen(LineEnd(shape));

        SkPaint outline;
        outline.setAntiAlias(true);
        outline.setColor(SkColorSetARGB(155, 170, 185, 205));
        outline.setStyle(SkPaint::kStroke_Style);
        outline.setStrokeWidth(1.0f);
        canvas->drawLine(start.x, start.y, end.x, end.y, outline);

        SkPaint handle;
        handle.setAntiAlias(true);
        handle.setColor(SkColorSetARGB(230, 224, 232, 242));
        handle.setStyle(SkPaint::kFill_Style);

        constexpr float half = 4.0f;
        canvas->drawRect(SkRect::MakeXYWH(start.x - half, start.y - half, half * 2.0f, half * 2.0f), handle);
        canvas->drawRect(SkRect::MakeXYWH(end.x - half, end.y - half, half * 2.0f, half * 2.0f), handle);
        return;
    }

    const auto corners = GetShapeCorners(shape);
    if (cropMode && shape.type == ShapeType::Image) {
        const auto cropCorners = corners;

        SkPaint cropOutline;
        cropOutline.setAntiAlias(true);
        cropOutline.setColor(SkColorSetARGB(230, 255, 255, 255));
        cropOutline.setStyle(SkPaint::kStroke_Style);
        cropOutline.setStrokeWidth(1.5f);
        SkPath cropPath;
        Vec2 cropPoint = view.WorldToScreen(cropCorners[0]);
        cropPath.moveTo(cropPoint.x, cropPoint.y);
        for (int i = 1; i < 4; ++i) {
            cropPoint = view.WorldToScreen(cropCorners[i]);
            cropPath.lineTo(cropPoint.x, cropPoint.y);
        }
        cropPath.close();
        canvas->drawPath(cropPath, cropOutline);

        SkPaint cropHandle;
        cropHandle.setAntiAlias(true);
        cropHandle.setColor(SkColorSetARGB(245, 255, 255, 255));
        cropHandle.setStyle(SkPaint::kFill_Style);
        constexpr float cropHalf = 5.0f;
        for (Vec2 corner: cropCorners) {
            const Vec2 screen = view.WorldToScreen(corner);
            canvas->drawRect(
                SkRect::MakeXYWH(screen.x - cropHalf, screen.y - cropHalf, cropHalf * 2.0f, cropHalf * 2.0f),
                cropHandle
            );
        }
        return;
    }

    SkPaint outline;
    outline.setAntiAlias(true);
    outline.setColor(SkColorSetARGB(155, 170, 185, 205));
    outline.setStyle(SkPaint::kStroke_Style);
    outline.setStrokeWidth(1.0f);

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
    handle.setColor(SkColorSetARGB(230, 224, 232, 242));
    handle.setStyle(SkPaint::kFill_Style);

    SkPaint rotateHandle = handle;
    rotateHandle.setColor(SkColorSetARGB(230, 245, 205, 95));

    constexpr float half = 4.0f;
    for (Vec2 corner: corners) {
        const Vec2 screen = view.WorldToScreen(corner);
        canvas->drawRect(SkRect::MakeXYWH(screen.x - half, screen.y - half, half * 2.0f, half * 2.0f), handle);
    }

    const Vec2 top = view.WorldToScreen(Midpoint(corners[0], corners[1]));
    const Vec2 rotate = view.WorldToScreen(GetRotateHandle(shape));
    canvas->drawLine(top.x, top.y, rotate.x, rotate.y, outline);
    canvas->drawCircle(rotate.x, rotate.y, 5.0f, rotateHandle);
}
