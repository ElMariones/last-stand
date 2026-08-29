#include <doctest/doctest.h>
#include "app/Cli.h"

#include <initializer_list>
#include <vector>

using ls::Options;

namespace {
Options parse(std::initializer_list<const char*> args) {
    std::vector<const char*> v{"laststand"};
    for (const char* a : args) v.push_back(a);
    return ls::parseArgs(static_cast<int>(v.size()), v.data());
}
}  // namespace

TEST_CASE("defaults") {
    const Options o = parse({});
    CHECK_FALSE(o.bench);
    CHECK_FALSE(o.noRender);
    CHECK_FALSE(o.help);
    CHECK(o.ticks == 10000u);
    CHECK(o.seed == 1234u);
    CHECK(o.spawn == 100u);
    CHECK(o.shotTicks == 0u);
}

TEST_CASE("--bench sets the flag") { CHECK(parse({"--bench"}).bench); }
TEST_CASE("--no-render sets the flag") { CHECK(parse({"--no-render"}).noRender); }
TEST_CASE("--ticks takes a value") { CHECK(parse({"--ticks", "5000"}).ticks == 5000u); }
TEST_CASE("--seed takes a value") { CHECK(parse({"--seed", "99"}).seed == 99u); }
TEST_CASE("--spawn takes a value") { CHECK(parse({"--spawn", "5000"}).spawn == 5000u); }

TEST_CASE("--shot takes a tick count") {
    CHECK(parse({"--shot", "120"}).shotTicks == 120u);
}

TEST_CASE("flags combine") {
    const Options o = parse({"--bench", "--no-render", "--ticks", "200", "--spawn", "42"});
    CHECK(o.bench);
    CHECK(o.noRender);
    CHECK(o.ticks == 200u);
    CHECK(o.spawn == 42u);
}

TEST_CASE("--help sets the flag") {
    CHECK(parse({"--help"}).help);
    CHECK(parse({"-h"}).help);
}

TEST_CASE("a value flag with no value keeps the default rather than crashing") {
    const Options o = parse({"--ticks"});
    CHECK(o.ticks == 10000u);
}

TEST_CASE("an unknown flag is ignored and does not disturb the rest") {
    const Options o = parse({"--wat", "--spawn", "7"});
    CHECK(o.spawn == 7u);
}

TEST_CASE("usageText is non-empty") {
    CHECK(ls::usageText() != nullptr);
    CHECK(ls::usageText()[0] != '\0');
}
