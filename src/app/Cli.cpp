#include "app/Cli.h"

#include <cstdlib>
#include <cstring>

namespace ls {

namespace {

// Returns true and writes the value when argv[i+1] exists; advances i.
bool takeValue(int argc, const char* const* argv, int& i, uint64_t& out) {
    if (i + 1 >= argc) return false;
    out = std::strtoull(argv[i + 1], nullptr, 10);
    ++i;
    return true;
}

bool takeText(int argc, const char* const* argv, int& i, const char*& out) {
    if (i + 1 >= argc) return false;
    out = argv[i + 1];
    ++i;
    return true;
}

}  // namespace

Options parseArgs(int argc, const char* const* argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (std::strcmp(a, "--bench") == 0) {
            o.bench = true;
        } else if (std::strcmp(a, "--no-render") == 0) {
            o.noRender = true;
        } else if (std::strcmp(a, "--help") == 0 || std::strcmp(a, "-h") == 0) {
            o.help = true;
        } else if (std::strcmp(a, "--naive-separation") == 0) {
            o.naiveSeparation = true;
        } else if (std::strcmp(a, "--balance") == 0) {
            uint64_t v = 10u;
            takeValue(argc, argv, i, v);
            o.balanceRuns = static_cast<int>(v);
        } else if (std::strcmp(a, "--shot-level") == 0) {
            uint64_t v = 0;
            if (takeValue(argc, argv, i, v)) o.shotLevel = static_cast<int>(v);
        } else if (std::strcmp(a, "--shot-tutorial") == 0) {
            o.shotTutorial = true;
        } else if (std::strcmp(a, "--matrix") == 0) {
            o.matrix = true;
        } else if (std::strcmp(a, "--balance-level") == 0) {
            uint64_t v = 0u;
            if (takeValue(argc, argv, i, v)) o.balanceLevel = static_cast<int>(v);
        } else if (std::strcmp(a, "--no-lod") == 0) {
            o.noLod = true;
        } else if (std::strcmp(a, "--no-batch") == 0) {
            o.noBatch = true;
        } else if (std::strcmp(a, "--ticks") == 0) {
            takeValue(argc, argv, i, o.ticks);
        } else if (std::strcmp(a, "--seed") == 0) {
            takeValue(argc, argv, i, o.seed);
        } else if (std::strcmp(a, "--shot") == 0) {
            takeValue(argc, argv, i, o.shotTicks);
        } else if (std::strcmp(a, "--shot-screen") == 0) {
            takeText(argc, argv, i, o.shotScreen);
        } else if (std::strcmp(a, "--shot-out") == 0) {
            takeText(argc, argv, i, o.shotPath);
        } else if (std::strcmp(a, "--width") == 0) {
            uint64_t v = 0u;
            if (takeValue(argc, argv, i, v)) o.windowWidth = static_cast<int>(v);
        } else if (std::strcmp(a, "--height") == 0) {
            uint64_t v = 0u;
            if (takeValue(argc, argv, i, v)) o.windowHeight = static_cast<int>(v);
        } else if (std::strcmp(a, "--sweep") == 0) {
            if (takeText(argc, argv, i, o.sweepPath)) o.bench = true;
        } else if (std::strcmp(a, "--stage") == 0) {
            takeText(argc, argv, i, o.stage);
        } else if (std::strcmp(a, "--notes") == 0) {
            takeText(argc, argv, i, o.notes);
        } else if (std::strcmp(a, "--render-bench") == 0) {
            uint64_t v = o.renderBench;
            if (takeValue(argc, argv, i, v)) o.renderBench = static_cast<uint32_t>(v);
        } else if (std::strcmp(a, "--render-frames") == 0) {
            takeValue(argc, argv, i, o.renderFrames);
        } else if (std::strcmp(a, "--spawn") == 0) {
            uint64_t v = o.spawn;
            if (takeValue(argc, argv, i, v)) o.spawn = static_cast<uint32_t>(v);
        }
        // Unknown flags are ignored rather than fatal: a benchmark run
        // should never die because of a stale argument in a script.
    }
    return o;
}

const char* usageText() {
    return
        "LAST STAND\n"
        "\n"
        "  laststand                       run the game\n"
        "  laststand --bench [options]     run the headless benchmark\n"
        "  laststand --sweep FILE          write the entity-count curve to FILE\n"
        "\n"
        "Options:\n"
        "  --bench             run the simulation headlessly and report timings\n"
        "  --no-render         alias for --bench (no window is opened)\n"
        "  --ticks <n>         ticks to simulate      (default 10000)\n"
        "  --seed <n>          simulation seed        (default 1234)\n"
        "  --spawn <n>         enemies to spawn       (default 100)\n"
        "  --sweep <file>      benchmark 100..5000 entities, append rows to file\n"
        "  --stage <name>      stage label for the sweep rows (default stage0)\n"
        "  --notes <text>      notes column for the sweep rows\n"
        "  --naive-separation  use the O(n^2) Stage 0 separation loop\n"
        "  --balance <runs>    auto-play the loop and print the economy curve\n"
        "  --balance-level <n> level index for --balance (default 0)\n"
        "  --matrix            play every sector at several Scrap budgets\n"
        "  --shot-level <n>    sector index for a battle/report capture\n"
        "  --shot-tutorial     keep the tutorial visible in a capture\n"
        "  --render-bench <n>  open a window, draw n enemies, time the frames\n"
        "  --no-lod            draw every enemy at the lowest detail tier\n"
        "  --no-batch          one raylib draw call per enemy (the M4 path)\n"
        "  --render-frames <n> frames to time in --render-bench (default 600)\n"
        "  --shot <n>          render n frames, screenshot, exit\n"
        "  --shot-screen <s>   which screen to capture (title|menu|options|\n"
        "                      levels|prepare|battle|report|tree)\n"
        "  --shot-out <file>   screenshot path (default shot.png)\n"
        "  --width/--height    override the saved window size for this run\n"
        "  -h, --help          this text\n";
}

}  // namespace ls
