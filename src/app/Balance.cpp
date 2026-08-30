#include "app/Balance.h"

#include <algorithm>
#include <cstdio>

#include "app/Session.h"

namespace ls {

namespace {

constexpr float kDt = 1.0f / 60.0f;
constexpr int   kMaxTicks = 40000;

void playOut(Session& session) {
    for (int i = 0; i < kMaxTicks && session.phase() == Phase::Battle; ++i) {
        session.updateBattle(kDt);
    }
}

// What a reasonable player does: follow the report's advice when it is
// affordable, otherwise buy the cheapest upgrade on the board. Keeps buying
// until nothing else is affordable, because that is also what players do.
int spendEverything(Session& session) {
    int bought = 0;
    for (int guard = 0; guard < 64; ++guard) {
        bool purchased = false;

        // More guns is a real option now, so the model player weighs it: buy
        // one whenever it is the cheapest thing on the board and the arsenal
        // is still small. Without this the harness would be measuring an
        // economy the player does not have.
        {
            const TurretKind kind = session.selectedKind();
            const uint32_t total = session.owned(TurretKind::MachineGun) +
                                   session.owned(TurretKind::Cannon) +
                                   session.owned(TurretKind::Flamethrower);
            if (total < 10u && session.canAffordTurret(kind)) {
                uint32_t cheapestNode = 0xFFFFFFFFu;
                for (size_t n = 0; n < kNodeCount; ++n) {
                    const auto node = static_cast<NodeId>(n);
                    if (!isRepeatable(node) && session.tree().has(node)) continue;
                    cheapestNode = std::min(cheapestNode, session.tree().cost(node));
                }
                if (session.turretPrice(kind) <= cheapestNode &&
                    session.buyTurret(kind)) {
                    ++bought;
                    continue;
                }
            }
        }

        const FailureAnalysis& fa = session.failure();
        for (int i = 0; i < fa.suggestionCount && !purchased; ++i) {
            const NodeId node = fa.suggestions[static_cast<size_t>(i)];
            if (!session.tree().canAfford(node, session.scrap())) continue;
            const uint32_t before = session.scrap();
            session.buy(node);
            if (session.scrap() != before) {
                purchased = true;
                ++bought;
            }
        }
        if (purchased) continue;

        // Cheapest affordable node that is not an already-owned one-shot.
        NodeId best = NodeId::Damage;
        uint32_t bestCost = 0xFFFFFFFFu;
        for (size_t n = 0; n < kNodeCount; ++n) {
            const auto node = static_cast<NodeId>(n);
            if (!isRepeatable(node) && session.tree().has(node)) continue;
            const uint32_t cost = session.tree().cost(node);
            if (cost < bestCost && cost <= session.scrap()) {
                bestCost = cost;
                best = node;
            }
        }
        if (bestCost == 0xFFFFFFFFu) break;

        const uint32_t before = session.scrap();
        session.buy(best);
        if (session.scrap() == before) break;
        ++bought;
    }
    return bought;
}

}  // namespace

BalanceReport runBalance(int runs, int levelIndex) {
    // No save path: the harness always starts from a fresh player.
    Session session{nullptr};
    session.goMenu();
    session.selectLevel(levelIndex);

    BalanceReport report;
    report.runs.reserve(static_cast<size_t>(runs));

    for (int r = 0; r < runs; ++r) {
        if (session.phase() != Phase::Battle) {
            if (session.phase() == Phase::Prepare) {
                session.autoDeploy();
                session.startBattle();
            } else {
                session.retry();
            }
        }
        playOut(session);

        BalanceRun run;
        run.index = r + 1;
        run.level = session.levelIndex();
        run.victory = session.result().victory;
        run.kills = session.result().kills;
        run.totalEnemies = session.result().totalEnemies;
        run.seconds = session.telemetry().elapsedSeconds();
        run.payout = session.payout().scrap;
        run.multiplier = session.payout().multiplier;

        session.openTree();
        run.bought = spendEverything(session);
        // Anything bought has to actually reach the field before the next run.
        session.backToPrepare();
        session.autoDeploy();
        run.scrapAfter = session.scrap();

        report.peakKills = std::max(report.peakKills, run.kills);
        if (report.runsToFirstPurchase < 0 && run.bought > 0) {
            report.runsToFirstPurchase = run.index;
        }
        if (report.runsToFirstClear < 0 && run.victory) {
            report.runsToFirstClear = run.index;
        }
        report.runs.push_back(run);

        // A cleared level sends the player onward, which is the flow the
        // economy actually has to support: arrive at Level 2 with the tree
        // Level 1 paid for, not from scratch.
        if (run.victory && session.levelIndex() < kLevelCount - 1) {
            session.selectLevel(session.levelIndex() + 1);
        }
    }
    return report;
}

void printBalance(const BalanceReport& report) {
    std::printf(
        "run  sector  result   kills        time   payout  x     bought  scrap\n");
    for (const BalanceRun& r : report.runs) {
        std::printf("%3d  %6d  %-7s  %5u/%-5u %5.0fs  %7u  %.2f  %5d  %6u\n",
                    r.index, r.level + 1, r.victory ? "CLEAR" : "loss", r.kills,
                    r.totalEnemies, static_cast<double>(r.seconds), r.payout,
                    static_cast<double>(r.multiplier), r.bought, r.scrapAfter);
    }
    std::printf("\nfirst purchase after run  %d\n", report.runsToFirstPurchase);
    std::printf("first clear after run     %d\n", report.runsToFirstClear);
    std::printf("peak kills                %u\n", report.peakKills);
}

}  // namespace ls
