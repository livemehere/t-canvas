#include "Shape.h"

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
    const Vec2 local = WorldToLocal(world, shape);
    const float hw = shape.size.x * 0.5f;
    const float hh = shape.size.y * 0.5f;
    return local.x >= -hw && local.x <= hw && local.y >= -hh && local.y <= hh;
}
