#pragma once
#include <raylib.h>

namespace ls::theme {

// GDD 12.1's palette in one place, so the game stops being a scatter of
// ad-hoc Color{} literals. Cold player, warm world, sickly enemies: the
// player can read the battlefield by hue alone, which is what keeps ten
// thousand entities from becoming mud.

// --- ground ---------------------------------------------------------------
constexpr Color kVoid       {  9,  8,  10, 255};   // behind everything
constexpr Color kGround     { 27, 24,  23, 255};   // walkable floor
constexpr Color kGroundEdge { 38, 33,  31, 255};   // the grid that gives it scale
constexpr Color kGroundLine { 46, 40,  36, 255};   // play-area boundary
constexpr Color kWall       { 46, 38,  34, 255};
constexpr Color kWallLight  { 92, 62,  40, 255};   // warm rimlight, sun-side
constexpr Color kWallShadow { 18, 15,  14, 255};

// --- the player: cold ------------------------------------------------------
constexpr Color kCold       {150, 220, 255, 255};
constexpr Color kColdDim    { 74, 116, 140, 255};
constexpr Color kColdDeep   { 34,  56,  72, 255};
constexpr Color kTracer     {214, 240, 255, 255};

// --- the enemy: sickly -----------------------------------------------------
constexpr Color kGrunt      {168, 200, 120, 255};
constexpr Color kRunner     {130, 210, 150, 255};
constexpr Color kTank       {200, 150,  90, 255};
constexpr Color kOutline    { 16,  20,  16, 255};

// --- fire ------------------------------------------------------------------
constexpr Color kFireCore   {255, 250, 224, 255};
constexpr Color kFireMid    {255, 186,  64, 255};
constexpr Color kFireDeep   {214,  86,  28, 255};

// --- signal ----------------------------------------------------------------
constexpr Color kScrap      {255, 214, 110, 255};
constexpr Color kDanger     {255,  90,  70, 255};
constexpr Color kGood       {150, 255, 170, 255};

// --- type ------------------------------------------------------------------
constexpr Color kInk        {226, 236, 246, 255};
constexpr Color kInkDim     {146, 158, 172, 255};
constexpr Color kInkFaint   { 84,  92, 104, 255};

// A type scale rather than arbitrary sizes: display for the one thing a
// screen is about, then heading, body, small, micro.
constexpr int kDisplay = 64;
constexpr int kTitle   = 40;
constexpr int kHeading = 26;
constexpr int kBody    = 19;
constexpr int kSmall   = 16;
constexpr int kMicro   = 13;

// Spacing is a 8px rhythm; every gap in the UI is a multiple of it.
constexpr float kUnit = 8.0f;
constexpr float kGutter = kUnit * 3.0f;

inline Color withAlpha(Color c, float a) {
    const float k = (a < 0.0f) ? 0.0f : (a > 1.0f ? 1.0f : a);
    return Color{c.r, c.g, c.b,
                 static_cast<unsigned char>(static_cast<float>(c.a) * k)};
}

inline Color mix(Color a, Color b, float t) {
    const float k = (t < 0.0f) ? 0.0f : (t > 1.0f ? 1.0f : t);
    return Color{
        static_cast<unsigned char>(static_cast<float>(a.r) +
                                   (static_cast<float>(b.r) - static_cast<float>(a.r)) * k),
        static_cast<unsigned char>(static_cast<float>(a.g) +
                                   (static_cast<float>(b.g) - static_cast<float>(a.g)) * k),
        static_cast<unsigned char>(static_cast<float>(a.b) +
                                   (static_cast<float>(b.b) - static_cast<float>(a.b)) * k),
        static_cast<unsigned char>(static_cast<float>(a.a) +
                                   (static_cast<float>(b.a) - static_cast<float>(a.a)) * k)};
}

}  // namespace ls::theme
