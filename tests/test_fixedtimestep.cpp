#include <doctest/doctest.h>
#include "core/FixedTimestep.h"

using ls::FixedTimestep;

TEST_CASE("exactly one tick per 1/60s frame") {
    FixedTimestep ts{60.0, 0.25};
    for (int i = 0; i < 100; ++i) {
        CHECK(ts.advance(1.0 / 60.0) == 1);
    }
}

TEST_CASE("a half-tick frame produces no tick, two produce one") {
    FixedTimestep ts{60.0, 0.25};
    CHECK(ts.advance(1.0 / 120.0) == 0);
    CHECK(ts.advance(1.0 / 120.0) == 1);
}

TEST_CASE("a slow frame produces multiple ticks") {
    FixedTimestep ts{60.0, 0.25};
    CHECK(ts.advance(0.05) == 3);
}

TEST_CASE("a very slow frame is clamped, preventing the spiral of death") {
    FixedTimestep ts{60.0, 0.25};
    CHECK(ts.advance(10.0) == 15);
}

TEST_CASE("alpha stays in [0,1)") {
    FixedTimestep ts{60.0, 0.25};
    for (int i = 0; i < 500; ++i) {
        ts.advance(0.00713);
        CHECK(ts.alpha() >= 0.0);
        CHECK(ts.alpha() < 1.0);
    }
}

TEST_CASE("alpha is zero immediately after an exact tick boundary") {
    FixedTimestep ts{60.0, 0.25};
    ts.advance(1.0 / 60.0);
    CHECK(ts.alpha() == doctest::Approx(0.0).epsilon(1e-9));
}

TEST_CASE("totalTicks accumulates") {
    FixedTimestep ts{60.0, 0.25};
    ts.advance(0.05);
    ts.advance(0.05);
    CHECK(ts.totalTicks() == 6u);
}

TEST_CASE("tickSeconds reflects the configured rate") {
    FixedTimestep ts{60.0, 0.25};
    CHECK(ts.tickSeconds() == doctest::Approx(1.0 / 60.0));
}

TEST_CASE("no drift over many frames") {
    FixedTimestep ts{60.0, 0.25};
    int ticks = 0;
    for (int i = 0; i < 6000; ++i) ticks += ts.advance(1.0 / 60.0);
    CHECK(ticks == 6000);
}
