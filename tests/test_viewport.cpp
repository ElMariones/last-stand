#include <doctest/doctest.h>

#include "render/Viewport.h"

using ls::Viewport;

TEST_CASE("a window the same shape as the world fills it exactly") {
    const Viewport v = ls::fitViewport(1280.0f, 720.0f, 1280.0f, 720.0f);
    CHECK(v.zoom == doctest::Approx(1.0f));
    CHECK(v.origin.x == doctest::Approx(0.0f));
    CHECK(v.origin.y == doctest::Approx(0.0f));
}

TEST_CASE("a bigger window of the same shape scales, it does not crop") {
    const Viewport v = ls::fitViewport(1280.0f, 720.0f, 2560.0f, 1440.0f);
    CHECK(v.zoom == doctest::Approx(2.0f));
    const ls::Vec2 corner = v.worldToScreen(ls::Vec2{1280.0f, 720.0f});
    CHECK(corner.x == doctest::Approx(2560.0f));
    CHECK(corner.y == doctest::Approx(1440.0f));
}

TEST_CASE("a wider window letterboxes on the sides, centred") {
    const Viewport v = ls::fitViewport(1280.0f, 720.0f, 1920.0f, 720.0f);
    CHECK(v.zoom == doctest::Approx(1.0f));       // height is the constraint
    CHECK(v.origin.x == doctest::Approx(320.0f)); // (1920 - 1280) / 2
    CHECK(v.origin.y == doctest::Approx(0.0f));
}

TEST_CASE("a taller window letterboxes above and below, centred") {
    const Viewport v = ls::fitViewport(1280.0f, 720.0f, 1280.0f, 1000.0f);
    CHECK(v.zoom == doctest::Approx(1.0f));
    CHECK(v.origin.y == doctest::Approx(140.0f));
}

TEST_CASE("the scale is uniform, so a range circle is never an ellipse") {
    const Viewport v = ls::fitViewport(1280.0f, 720.0f, 1920.0f, 800.0f);
    const ls::Vec2 a = v.worldToScreen(ls::Vec2{0.0f, 0.0f});
    const ls::Vec2 bx = v.worldToScreen(ls::Vec2{100.0f, 0.0f});
    const ls::Vec2 by = v.worldToScreen(ls::Vec2{0.0f, 100.0f});
    CHECK((bx.x - a.x) == doctest::Approx(by.y - a.y));
}

TEST_CASE("screen and world round-trip at any window size") {
    const float sizes[][2] = {{1280.0f, 720.0f}, {1920.0f, 1080.0f},
                              {1024.0f, 900.0f}, {3440.0f, 1440.0f}};
    for (const auto& s : sizes) {
        const Viewport v = ls::fitViewport(1280.0f, 720.0f, s[0], s[1]);
        for (float wx = 0.0f; wx <= 1280.0f; wx += 320.0f) {
            for (float wy = 0.0f; wy <= 720.0f; wy += 180.0f) {
                const ls::Vec2 back =
                    v.screenToWorld(v.worldToScreen(ls::Vec2{wx, wy}));
                CHECK(back.x == doctest::Approx(wx).epsilon(0.001));
                CHECK(back.y == doctest::Approx(wy).epsilon(0.001));
            }
        }
    }
}

TEST_CASE("a degenerate window falls back to identity, not to NaN") {
    // A zero-sized window happens for a frame while minimising. The contract
    // is that the transform stays finite and invertible, not that it produces
    // any particular answer.
    const Viewport v = ls::fitViewport(1280.0f, 720.0f, 0.0f, 0.0f);
    CHECK(v.zoom > 0.0f);

    const ls::Vec2 back = v.screenToWorld(v.worldToScreen(ls::Vec2{40.0f, 90.0f}));
    CHECK(back.x == doctest::Approx(40.0f));
    CHECK(back.y == doctest::Approx(90.0f));
}

TEST_CASE("interface scale follows the window and stays in sane bounds") {
    CHECK(ls::uiScaleForWindow(1280.0f, 720.0f) == doctest::Approx(1.0f));
    CHECK(ls::uiScaleForWindow(2560.0f, 1440.0f) == doctest::Approx(2.0f));
    CHECK(ls::uiScaleForWindow(640.0f, 360.0f) == doctest::Approx(0.75f));
    CHECK(ls::uiScaleForWindow(7680.0f, 4320.0f) == doctest::Approx(2.0f));
}
