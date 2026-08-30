#include "render/Icons.h"

#include <cmath>

#include "render/Theme.h"

namespace ls {

namespace {

constexpr float kPi = 3.14159265358979323846f;

Vector2 at(Vector2 c, float forward, float side, float scale, float cosF,
           float sinF) {
    // Body-local coordinates: +forward is out of the barrel, +side is right.
    return Vector2{c.x + (forward * cosF - side * sinF) * scale,
                   c.y + (forward * sinF + side * cosF) * scale};
}

void poly(Vector2 c, float scale, float cosF, float sinF,
          const float (*pts)[2], int count, Color fill) {
    // Triangle fan from the first vertex. Every shape here is convex, which
    // is what makes that legal and cheap.
    //
    // The winding is reversed on purpose: raylib culls clockwise triangles,
    // and these point lists are written clockwise because that is how you
    // read a shape off a sketch in screen coordinates. Getting this wrong
    // does not warn, it just silently draws nothing.
    for (int i = 1; i + 1 < count; ++i) {
        DrawTriangle(at(c, pts[0][0], pts[0][1], scale, cosF, sinF),
                     at(c, pts[i + 1][0], pts[i + 1][1], scale, cosF, sinF),
                     at(c, pts[i][0], pts[i][1], scale, cosF, sinF),
                     fill);
    }
}

// A regular n-gon plate with a darker rim: the chassis every turret sits on.
void plate(Vector2 c, float radius, int sides, Color fill, Color rim,
           float spin) {
    for (int i = 0; i < sides; ++i) {
        const float a0 = spin + kPi * 2.0f * static_cast<float>(i) /
                                    static_cast<float>(sides);
        const float a1 = spin + kPi * 2.0f * static_cast<float>(i + 1) /
                                    static_cast<float>(sides);
        DrawTriangle(c,
                     Vector2{c.x + std::cos(a1) * radius,
                             c.y + std::sin(a1) * radius},
                     Vector2{c.x + std::cos(a0) * radius,
                             c.y + std::sin(a0) * radius},
                     fill);
    }
    DrawPolyLinesEx(c, sides, radius, spin * 180.0f / kPi, 1.5f, rim);
}

void drawMachineGun(Vector2 c, float scale, float cosF, float sinF) {
    plate(c, 9.0f * scale, 8, theme::kColdDeep, theme::kCold, 0.39f);

    // Barrel and muzzle brake.
    const float barrel[][2] = {{4.0f, -1.6f}, {15.0f, -1.6f},
                               {15.0f, 1.6f}, {4.0f, 1.6f}};
    poly(c, scale, cosF, sinF, barrel, 4, theme::kCold);
    const float brake[][2] = {{13.0f, -3.0f}, {17.0f, -3.0f},
                              {17.0f, 3.0f}, {13.0f, 3.0f}};
    poly(c, scale, cosF, sinF, brake, 4, theme::kColdDim);

    // Ammo drum on the left flank, and a housing behind the breech.
    const float drum[][2] = {{-2.0f, -8.0f}, {3.0f, -8.0f},
                             {3.0f, -4.0f}, {-2.0f, -4.0f}};
    poly(c, scale, cosF, sinF, drum, 4, theme::kColdDim);
    DrawCircleV(c, 4.2f * scale, theme::kCold);
    DrawCircleV(c, 2.2f * scale, theme::kInk);
}

void drawCannon(Vector2 c, float scale, float cosF, float sinF) {
    plate(c, 11.0f * scale, 4, theme::kColdDeep, theme::kColdDim, kPi * 0.25f);

    // Short, fat barrel with a heavy muzzle.
    const float barrel[][2] = {{2.0f, -3.6f}, {14.0f, -3.0f},
                               {14.0f, 3.0f}, {2.0f, 3.6f}};
    poly(c, scale, cosF, sinF, barrel, 4, theme::kColdDim);
    const float muzzle[][2] = {{13.0f, -5.0f}, {18.0f, -4.2f},
                               {18.0f, 4.2f}, {13.0f, 5.0f}};
    poly(c, scale, cosF, sinF, muzzle, 4, theme::kCold);

    // Recoil housing, and bolts at the corners of the plate.
    const float housing[][2] = {{-9.0f, -5.5f}, {2.0f, -5.5f},
                                {2.0f, 5.5f}, {-9.0f, 5.5f}};
    poly(c, scale, cosF, sinF, housing, 4, theme::kCold);
    for (int i = 0; i < 4; ++i) {
        const float a = kPi * 0.25f + kPi * 0.5f * static_cast<float>(i);
        DrawCircleV(Vector2{c.x + std::cos(a) * 8.0f * scale,
                            c.y + std::sin(a) * 8.0f * scale},
                    1.6f * scale, theme::kColdDeep);
    }
}

void drawFlamethrower(Vector2 c, float scale, float cosF, float sinF) {
    plate(c, 9.5f * scale, 6, theme::kColdDeep, theme::kFireDeep, 0.0f);

    // Twin fuel cylinders across the back.
    const float tankL[][2] = {{-10.0f, -6.5f}, {-2.0f, -6.5f},
                              {-2.0f, -2.5f}, {-10.0f, -2.5f}};
    const float tankR[][2] = {{-10.0f, 2.5f}, {-2.0f, 2.5f},
                              {-2.0f, 6.5f}, {-10.0f, 6.5f}};
    poly(c, scale, cosF, sinF, tankL, 4, theme::kColdDim);
    poly(c, scale, cosF, sinF, tankR, 4, theme::kColdDim);

    // Tapering nozzle, and the pilot flame at its tip.
    const float nozzle[][2] = {{2.0f, -3.4f}, {13.0f, -1.8f},
                               {13.0f, 1.8f}, {2.0f, 3.4f}};
    poly(c, scale, cosF, sinF, nozzle, 4, theme::kFireDeep);
    const float flame[][2] = {{13.0f, -2.4f}, {19.0f, 0.0f}, {13.0f, 2.4f}};
    poly(c, scale, cosF, sinF, flame, 3, theme::kFireMid);
    DrawCircleV(at(c, 14.0f, 0.0f, scale, cosF, sinF), 1.5f * scale,
                theme::kFireCore);
}

}  // namespace

void drawTurret(TurretKind kind, Vector2 centre, float scale, float facing,
                bool overcharged, bool overheated) {
    const float cosF = std::cos(facing);
    const float sinF = std::sin(facing);

    switch (kind) {
        case TurretKind::MachineGun:   drawMachineGun(centre, scale, cosF, sinF); break;
        case TurretKind::Cannon:       drawCannon(centre, scale, cosF, sinF); break;
        case TurretKind::Flamethrower: drawFlamethrower(centre, scale, cosF, sinF); break;
    }

    if (overcharged) {
        DrawCircleLinesV(centre, 14.0f * scale, theme::kScrap);
        DrawCircleLinesV(centre, 16.0f * scale,
                         theme::withAlpha(theme::kScrap, 0.4f));
    } else if (overheated) {
        DrawCircleLinesV(centre, 14.0f * scale, theme::kDanger);
    }
}

void drawTurretIcon(TurretKind kind, Vector2 centre, float scale) {
    drawTurret(kind, centre, scale, 0.0f, false, false);
}

void drawBase(Vector2 centre, float radius, float health, float clock) {
    const Color tone = (health > 0.35f) ? theme::kCold : theme::kDanger;
    // Faster the closer it is to falling: the base's condition is readable
    // from the corner of an eye, without looking at the bar.
    const float pulse =
        0.5f + 0.5f * std::sin(clock * (3.0f + (1.0f - health) * 9.0f));

    // Outer hexagonal wall with corner bastions.
    DrawPolyLinesEx(centre, 6, radius * 1.35f, 0.0f, 2.0f,
                    theme::withAlpha(tone, 0.55f));
    for (int i = 0; i < 6; ++i) {
        const float a = kPi * 2.0f * static_cast<float>(i) / 6.0f;
        DrawCircleV(Vector2{centre.x + std::cos(a) * radius * 1.35f,
                            centre.y + std::sin(a) * radius * 1.35f},
                    radius * 0.16f, theme::withAlpha(tone, 0.8f));
    }

    // Inner keep, and a core that carries the pulse.
    DrawPoly(centre, 6, radius * 0.95f, 30.0f,
             theme::mix(theme::kColdDeep, tone, 0.35f));
    DrawPolyLinesEx(centre, 6, radius * 0.95f, 30.0f, 1.5f, tone);
    DrawCircleV(centre, radius * (0.35f + pulse * 0.12f),
                theme::mix(theme::kColdDeep, tone, 0.5f + pulse * 0.5f));

    // Health as a ring segment, so the base itself reports its own state.
    if (health > 0.0f) {
        DrawRing(centre, radius * 1.05f, radius * 1.18f, -90.0f,
                 -90.0f + 360.0f * health, 32, theme::withAlpha(tone, 0.9f));
    }
}

}  // namespace ls
