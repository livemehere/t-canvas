#include "Shape.h"

#include <algorithm>

namespace {
float DistanceToSegment(Vec2 point, Vec2 a, Vec2 b) {
    const Vec2 ab = b - a;
    const Vec2 ap = point - a;
    const float lengthSq = ab.x * ab.x + ab.y * ab.y;
    if (lengthSq <= 0.0001f) {
        return Distance(point, a);
    }
    const float t = Clamp((ap.x * ab.x + ap.y * ab.y) / lengthSq, 0.0f, 1.0f);
    return Distance(point, a + ab * t);
}
} // namespace

Vec2 LocalToWorld(Vec2 point, const Shape &shape) {
    return shape.position + Rotate(point, shape.rotation);
}

Vec2 WorldToLocal(Vec2 point, const Shape &shape) {
    return Rotate(point - shape.position, -shape.rotation);
}

std::array<Vec2, 4> GetShapeCorners(const Shape &shape) {
    const float hw = shape.size.x * 0.5f;
    const float hh = shape.size.y * 0.5f;
    return {
        LocalToWorld({-hw, -hh}, shape),
        LocalToWorld({hw, -hh}, shape),
        LocalToWorld({hw, hh}, shape),
        LocalToWorld({-hw, hh}, shape)
    };
}

Vec2 Midpoint(Vec2 a, Vec2 b) {
    return {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
}

bool PointInShape(Vec2 world, const Shape &shape) {
    if (shape.type == ShapeType::Brush) {
        if (shape.brushPoints.empty()) {
            return false;
        }
        const Vec2 local = WorldToLocal(world, shape);
        const float threshold = std::max(6.0f, shape.brushSize * 0.5f);
        if (shape.brushPoints.size() == 1) {
            return Distance(local, shape.brushPoints.front()) <= threshold;
        }
        for (size_t i = 1; i < shape.brushPoints.size(); ++i) {
            if (DistanceToSegment(local, shape.brushPoints[i - 1], shape.brushPoints[i]) <= threshold) {
                return true;
            }
        }
        return false;
    }

    if (shape.type == ShapeType::Line || shape.type == ShapeType::Arrow) {
        const Vec2 start = LocalToWorld({-shape.size.x * 0.5f, 0.0f}, shape);
        const Vec2 end = LocalToWorld({shape.size.x * 0.5f, 0.0f}, shape);
        return DistanceToSegment(world, start, end) <= std::max(6.0f, shape.borderWidth * 2.0f);
    }

    const Vec2 local = WorldToLocal(world, shape);
    const float hw = shape.size.x * 0.5f;
    const float hh = shape.size.y * 0.5f;
    return local.x >= -hw && local.x <= hw && local.y >= -hh && local.y <= hh;
}
