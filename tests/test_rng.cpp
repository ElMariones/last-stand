#include <doctest/doctest.h>
#include "math/Rng.h"
#include <vector>
#include <cstdint>

using ls::Pcg32;

TEST_CASE("same seed produces the same sequence") {
    Pcg32 a{12345u};
    Pcg32 b{12345u};
    for (int i = 0; i < 1000; ++i) {
        CHECK(a.nextU32() == b.nextU32());
    }
}

TEST_CASE("different seeds diverge") {
    Pcg32 a{1u};
    Pcg32 b{2u};
    int same = 0;
    for (int i = 0; i < 1000; ++i) {
        if (a.nextU32() == b.nextU32()) ++same;
    }
    CHECK(same < 5);
}

TEST_CASE("different streams with the same seed diverge") {
    Pcg32 a{99u, 1u};
    Pcg32 b{99u, 2u};
    int same = 0;
    for (int i = 0; i < 1000; ++i) {
        if (a.nextU32() == b.nextU32()) ++same;
    }
    CHECK(same < 5);
}

TEST_CASE("nextFloat stays in [0,1)") {
    Pcg32 r{7u};
    for (int i = 0; i < 10000; ++i) {
        const float v = r.nextFloat();
        CHECK(v >= 0.0f);
        CHECK(v < 1.0f);
    }
}

TEST_CASE("nextBounded stays in [0,bound) and covers the range") {
    Pcg32 r{7u};
    std::vector<int> hits(6, 0);
    for (int i = 0; i < 60000; ++i) {
        const uint32_t v = r.nextBounded(6u);
        REQUIRE(v < 6u);
        hits[static_cast<size_t>(v)]++;
    }
    for (size_t i = 0; i < 6u; ++i) {
        CHECK(hits[i] > 8000);
    }
}

TEST_CASE("nextBounded(1) always returns 0") {
    Pcg32 r{7u};
    for (int i = 0; i < 100; ++i) CHECK(r.nextBounded(1u) == 0u);
}

TEST_CASE("nextRange stays within bounds") {
    Pcg32 r{7u};
    for (int i = 0; i < 10000; ++i) {
        const float v = r.nextRange(-5.0f, 5.0f);
        CHECK(v >= -5.0f);
        CHECK(v <= 5.0f);
    }
}
