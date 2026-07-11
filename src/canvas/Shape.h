#pragma once

#include <array>
#include <string>
#include <vector>

#include <skia/core/SkImage.h>

#include "Math2D.h"

struct Color {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

enum class ShapeType {
    Rect,
    Circle,
    Line,
    Arrow,
    Text,
    Image,
    Brush
};

struct Shape {
    ShapeType type = ShapeType::Rect;
    std::string name = "Rectangle";
    Vec2 position{180.0f, 130.0f};
    Vec2 size{200.0f, 100.0f};
    float rotation = 0.0f;
    float cornerRadius = 0.0f;
    float borderWidth = 3.0f;
    float arrowHeadSize = 28.0f;
    bool fillEnabled = false;
    bool borderEnabled = true;
    Color fill{1.0f, 0.22f, 0.20f, 1.0f};
    Color border{1.0f, 0.22f, 0.20f, 1.0f};
    bool visible = true;
    bool locked = false;
    bool blurBackground = false;
    float blurRadius = 5.0f;
    float brushSize = 4.0f;
    std::string text = "Text";
    std::string imagePath;
    float cropLeft = 0.0f;
    float cropTop = 0.0f;
    float cropRight = 1.0f;
    float cropBottom = 1.0f;
    std::vector<Vec2> brushPoints;
    sk_sp<SkImage> image;
};

Vec2 LocalToWorld(Vec2 point, const Shape &shape);
Vec2 WorldToLocal(Vec2 point, const Shape &shape);
std::array<Vec2, 4> GetShapeCorners(const Shape &shape);
Vec2 Midpoint(Vec2 a, Vec2 b);
bool PointInShape(Vec2 world, const Shape &shape);
