#pragma once
#include <raylib.h>

#include "sim/Turret.h"

namespace ls {

// Turret art, in one place because it is drawn twice: on the battlefield at
// world scale, and in the Prepare screen's picker at icon scale. GDD 12.1
// asks for constructed vector shapes — an assembly of polygons with a thick
// outline and a limited palette — rather than the circle, square and triangle
// the game shipped with, which read as placeholders because they were.
//
// `scale` is 1.0 for a turret on a 1280x720 battlefield. `facing` is radians.
void drawTurret(TurretKind kind, Vector2 centre, float scale, float facing,
                bool overcharged, bool overheated);

// The same silhouettes without state, for menus.
void drawTurretIcon(TurretKind kind, Vector2 centre, float scale);

// The base: a fortified emplacement rather than a filled circle. `health` is
// 0..1 and drives both the colour and how hard the core pulses.
void drawBase(Vector2 centre, float radius, float health, float clock);

}  // namespace ls
