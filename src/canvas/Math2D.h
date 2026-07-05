#pragma once

#include <cmath>

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

inline Vec2 operator+(Vec2 a, Vec2 b) {
    return {a.x + b.x, a.y + b.y};
}

inline Vec2 operator-(Vec2 a, Vec2 b) {
    return {a.x - b.x, a.y - b.y};
}

inline Vec2 operator*(Vec2 a, float value) {
    return {a.x * value, a.y * value};
}

inline float Clamp(float value, float min, float max) {
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

inline float Distance(Vec2 a, Vec2 b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

inline float DegreesToRadians(float degrees) {
    return degrees * 3.1415926535f / 180.0f;
}

inline float RadiansToDegrees(float radians) {
    return radians * 180.0f / 3.1415926535f;
}

inline Vec2 Rotate(Vec2 point, float degrees) {
    const float radians = DegreesToRadians(degrees);
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return {
        point.x * c - point.y * s,
        point.x * s + point.y * c
    };
}
