#pragma once
#include <cmath>

namespace ls {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

constexpr Vec2 operator+(Vec2 a, Vec2 b) { return Vec2{a.x + b.x, a.y + b.y}; }
constexpr Vec2 operator-(Vec2 a, Vec2 b) { return Vec2{a.x - b.x, a.y - b.y}; }
constexpr Vec2 operator*(Vec2 a, float s) { return Vec2{a.x * s, a.y * s}; }
constexpr Vec2 operator*(float s, Vec2 a) { return a * s; }
constexpr Vec2 operator-(Vec2 a) { return Vec2{-a.x, -a.y}; }

constexpr Vec2& operator+=(Vec2& a, Vec2 b) { a.x += b.x; a.y += b.y; return a; }
constexpr Vec2& operator-=(Vec2& a, Vec2 b) { a.x -= b.x; a.y -= b.y; return a; }

constexpr float lengthSq(Vec2 a) { return a.x * a.x + a.y * a.y; }
inline float length(Vec2 a) { return std::sqrt(lengthSq(a)); }

constexpr float distanceSq(Vec2 a, Vec2 b) { return lengthSq(b - a); }

// Returns {0,0} for a zero-length input. Callers depend on this being safe.
inline Vec2 normalized(Vec2 a) {
    const float lenSq = lengthSq(a);
    if (lenSq <= 1e-12f) return Vec2{0.0f, 0.0f};
    const float inv = 1.0f / std::sqrt(lenSq);
    return Vec2{a.x * inv, a.y * inv};
}

constexpr Vec2 lerp(Vec2 a, Vec2 b, float t) { return a + (b - a) * t; }

}  // namespace ls
