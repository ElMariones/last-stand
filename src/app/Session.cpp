#include "app/Session.h"

#include <algorithm>
#include <cmath>

#include "math/Rect.h"

namespace ls {

namespace {

constexpr float kAirstrikeDamage = 500.0f;
constexpr float kAirstrikeBand = 40.0f;   // half-height of the strike line
constexpr float kAirstrikeCd = 25.0f;
constexpr float kOverchargeCd = 15.0f;
constexpr float kOverchargeTtl = 4.0f;
constexpr uint64_t kLevelSeedBase = 0x5EEDu;
constexpr int kMaxHistogramRows = 256;   // any real map is far under this

// Builds a turret of the given kind from its base stats, then folds in every
// relevant tree effect (scalars and the behaviour toggles). This is what lets
// two different builds feel different: Overclock vs Explosive Shells change a
// turret's fields, not just a damage number.
Turret makeTurret(TurretKind kind, Vec2 pos, const Effects& e) {
    Turret t;
    t.kind = kind;
    t.position = pos;
    t.armorPierce = e.armorPiercing ? 1.5f : 1.0f;

    switch (kind) {
        case TurretKind::MachineGun:
            t.range = 160.0f;
            t.damage = 5.0f;
            t.fireInterval = 0.125f;
            t.mode = TargetingMode::First;
            if (e.mgOverclock) { t.fireInterval *= 0.5f; t.damage *= 0.7f; }
            t.ricochet = e.mgRicochet;
            t.bulletStorm = e.mgBulletStorm;
            break;
        case TurretKind::Cannon:
            t.range = 260.0f;
            t.damage = 90.0f;
            t.fireInterval = 2.0f;
            t.splashRadius = 45.0f;
            t.knockback = 40.0f;
            t.mode = TargetingMode::Densest;
            if (e.cannonExplosive) t.splashRadius *= 1.5f;
            if (e.cannonKnockback) t.knockback *= 2.5f;
            t.clusterShot = e.cannonCluster;
            break;
        case TurretKind::Flamethrower:
            t.range = 120.0f;
            t.damage = 0.0f;              // no direct damage; burn only
            t.fireInterval = 0.4f;
            t.burnPerHit = 6.0f;
            t.burnDuration = 3.0f;
            t.coneHalfAngle = 35.0f;
            t.mode = TargetingMode::Densest;
            if (e.flameLingering) t.burnDuration *= 2.0f;
            if (e.flameFirestorm) t.burnPerHit *= 2.0f;
            t.ignite = e.flameIgnite;
            break;
    }

    // Global stat scalars.
    t.damage *= e.damageMult;
    t.range *= e.rangeMult;
    t.fireInterval /= e.fireRateMult;
    t.splashRadius *= e.splashMult;
    t.burnPerHit *= e.burnMult;
    return t;
}

Level makeLevelByIndex(int idx) {
    switch (idx) {
        case 1: return makeLevel2();
        case 2: return makeLevel3();
        default: return makeLevel1();
    }
}

}  // namespace

Session::Session(const char* savePath)
    : savePath_(savePath ? savePath : "") {
    if (!savePath_.empty() && ls::load(saveData_, savePath_.c_str())) {
        scrap_ = saveData_.scrap;
        tree_.loadLevels(saveData_.nodeLevels);
        bestKills_   = saveData_.bestKills;
        clearCounts_ = saveData_.clearCounts;
    } else {
        scrap_ = 0u;
    }
    effects_ = tree_.bonuses();
    defaultLoadout();
    resetWorld();
}

void Session::rebuildEffects() {
    effects_ = tree_.bonuses();
    rebuildLoadout();
}

bool Session::kindUnlocked(TurretKind kind) const {
    switch (kind) {
        case TurretKind::Cannon:       return effects_.unlockCannon;
        case TurretKind::Flamethrower: return effects_.unlockFlamethrower;
        case TurretKind::MachineGun:   break;
    }
    return true;
}

// Re-derives every placed turret's stats from the CURRENT tree effects,
// keeping its hardpoint, kind and targeting choice. Without this, buying an
// upgrade and pressing RETRY changed nothing: the loadout still held the
// turrets built from the effects in force when they were placed, and
// resetWorld copies the loadout verbatim.
void Session::rebuildLoadout() {
    for (Turret& t : loadout_) {
        const TargetingMode mode = t.mode;
        const TurretKind kind =
            kindUnlocked(t.kind) ? t.kind : TurretKind::MachineGun;
        t = makeTurret(kind, t.position, effects_);
        // A respec can revoke DENSEST; fall back to the kind's default then.
        if (mode != TargetingMode::Densest || effects_.densest) t.mode = mode;
    }
    if (!kindUnlocked(selectedKind_)) selectedKind_ = TurretKind::MachineGun;
    if (world_ != nullptr) syncWorldTurrets();
}

void Session::defaultLoadout() {
    loadout_.clear();
    const Effects e = effects_;
    for (const Vec2& hp : level_.map.hardpoints) {
        loadout_.push_back(makeTurret(TurretKind::MachineGun, hp, e));
    }
}

void Session::syncWorldTurrets() {
    auto& turrets = world_->turrets();
    turrets.clear();
    for (const Turret& t : loadout_) {
        Turret c = t;
        c.cooldown = 0.0f;
        c.shotsFired = 0u;
        c.kills = 0u;
        c.overchargeTtl = 0.0f;
        c.overheatTtl = 0.0f;
        turrets.push_back(c);
    }
}

void Session::selectLevel(int idx) {
    idx = std::clamp(idx, 0, 2);
    levelIndex_ = idx;
    level_ = makeLevelByIndex(idx);
    defaultLoadout();
    resetWorld();
    phase_ = Phase::Prepare;
    hasResult_ = false;
    airstrikeCd_ = 0.0f;
    overchargeCd_ = 0.0f;
}

void Session::resetWorld() {
    // GDD 4.1: a level is a FIXED invasion — the same enemies, in the same
    // order, every time you play it. The seed is therefore a function of the
    // level alone; it must not drift with clear counts or the player cannot
    // learn a level.
    world_ = std::make_unique<World>(
        level_.map, /*seed*/ kLevelSeedBase + static_cast<uint64_t>(levelIndex_));
    world_->setLevelTotal(level_.totalEnemies);

    ls::Base& base = world_->base();
    base.maxHealth += effects_.baseBonusHp;
    base.health = base.maxHealth;
    base.regenPerSecond = effects_.baseRegen;

    syncWorldTurrets();
    director_ = SpawnDirector{};
}

void Session::cycleKind() {
    const TurretKind order[3] = {
        TurretKind::MachineGun, TurretKind::Cannon, TurretKind::Flamethrower};
    int cur = 0;
    for (int i = 0; i < 3; ++i) {
        if (selectedKind_ == order[i]) { cur = i; break; }
    }
    for (int step = 1; step <= 3; ++step) {
        const TurretKind candidate = order[(cur + step) % 3];
        if (kindUnlocked(candidate)) { selectedKind_ = candidate; return; }
    }
    selectedKind_ = TurretKind::MachineGun;
}

void Session::cycleTargeting() {
    if (phase_ != Phase::Prepare) return;
    // Cycles the LOADOUT, not the live world turrets: the loadout is what
    // survives a retry, and syncWorldTurrets pushes the change into the world.
    for (Turret& t : loadout_) {
        switch (t.mode) {
            case TargetingMode::First:     t.mode = TargetingMode::Closest; break;
            case TargetingMode::Closest:   t.mode = TargetingMode::Strongest; break;
            case TargetingMode::Strongest:
                t.mode = effects_.densest ? TargetingMode::Densest
                                          : TargetingMode::First;
                break;
            case TargetingMode::Densest:   t.mode = TargetingMode::First; break;
        }
    }
    syncWorldTurrets();
}

void Session::setTurretAt(Vec2 worldPos, float halfW, float halfH) {
    if (phase_ != Phase::Prepare) return;

    const size_t baseCount = level_.map.hardpoints.size();
    const size_t limit = baseCount + (effects_.extraHardpoint ? 1u : 0u);

    for (size_t i = 0; i < limit; ++i) {
        const Vec2 hp = (i < baseCount) ? level_.map.hardpoints[i]
                                        : level_.map.baseCenter();
        if (!contains(fromCenter(hp, halfW, halfH), worldPos)) continue;

        // Replace any loadout turret already at this hardpoint, else append.
        for (Turret& t : loadout_) {
            if (distanceSq(t.position, hp) < 1.0f) {
                t = makeTurret(selectedKind_, hp, effects_);
                syncWorldTurrets();
                return;
            }
        }
        loadout_.push_back(makeTurret(selectedKind_, hp, effects_));
        syncWorldTurrets();
        return;
    }
}

void Session::startBattle() {
    if (phase_ != Phase::Prepare) return;
    director_ = SpawnDirector{};
    hasResult_ = false;
    phase_ = Phase::Battle;
}

void Session::cycleTimeScale() {
    timeScale_ = (timeScale_ == 1) ? 2 : (timeScale_ == 2) ? 4 : 1;
}

void Session::fireAirstrike() {
    if (!airstrikeReady() || phase_ != Phase::Battle) return;

    EnemyPool& enemies = world_->enemies();
    const uint32_t n = enemies.count();
    airstrikeCd_ = kAirstrikeCd;
    if (n == 0u) return;

    // Strike the horizontal band containing the most enemies.
    // A fixed stack histogram, not a vector: this runs mid-battle, and the
    // zero-allocation-during-a-battle invariant (GDD 14.3) is asserted by
    // tests/test_noalloc.cpp.
    const int rows = std::min(world_->map().grid.rows(), kMaxHistogramRows);
    int histogram[kMaxHistogramRows] = {};
    for (uint32_t i = 0; i < n; ++i) {
        int cx = 0, cy = 0;
        if (world_->map().grid.worldToCell(enemies.position[i], cx, cy) &&
            cy < rows) {
            ++histogram[static_cast<size_t>(cy)];
        }
    }
    int bestRow = 0;
    int bestCount = -1;
    for (int r = 0; r < rows; ++r) {
        if (histogram[static_cast<size_t>(r)] > bestCount) {
            bestCount = histogram[static_cast<size_t>(r)];
            bestRow = r;
        }
    }

    const float rowY = world_->map().grid.cellCenter(0, bestRow).y;
    for (uint32_t i = n; i-- > 0u;) {
        if (std::fabs(enemies.position[i].y - rowY) <= kAirstrikeBand) {
            applyDamage(enemies, i, kAirstrikeDamage);
        }
    }
    world_->addTracer(Vec2{0.0f, rowY},
                      Vec2{world_->map().grid.worldWidth(), rowY}, 0.15f);
}

void Session::overchargeAt(Vec2 worldPos) {
    if (!overchargeReady() || phase_ != Phase::Battle) return;

    auto& turrets = world_->turrets();
    overchargeCd_ = kOverchargeCd;
    if (turrets.empty()) return;

    size_t best = 0u;
    float bestD = 1e30f;
    for (size_t i = 0; i < turrets.size(); ++i) {
        const float d = distanceSq(turrets[i].position, worldPos);
        if (d < bestD) { bestD = d; best = i; }
    }
    turrets[best].overchargeTtl = kOverchargeTtl;
}

void Session::updateBattle(float dt) {
    if (phase_ != Phase::Battle) return;
    if (airstrikeCd_ > 0.0f) airstrikeCd_ -= dt;
    if (overchargeCd_ > 0.0f) overchargeCd_ -= dt;

    director_.update(*world_, level_, dt);
    world_->tick(dt);
    if (world_->isOver()) finishBattle();
}

void Session::finishBattle() {
    const uint32_t totalKills = world_->totalKills();
    const size_t li = static_cast<size_t>(levelIndex_);
    result_ = BattleResult{world_->isVictory(), totalKills, level_.totalEnemies,
                           bestKills_[li], clearCounts_[li]};
    payout_ = computePayout(result_, level_.killValue, level_.depthBonusWeight,
                            tree_.bonuses().scrapMult);

    scrap_ += payout_.scrap;
    if (totalKills > bestKills_[li]) bestKills_[li] = totalKills;
    if (result_.victory) ++clearCounts_[li];

    hasResult_ = true;
    phase_ = Phase::Report;
    saveNow();
}

void Session::retry() {
    resetWorld();
    hasResult_ = false;
    airstrikeCd_ = 0.0f;
    overchargeCd_ = 0.0f;
    phase_ = Phase::Battle;
}

void Session::openTree() {
    if (phase_ == Phase::Report) phase_ = Phase::Tree;
}

void Session::backToReport() {
    if (phase_ == Phase::Tree) phase_ = Phase::Report;
}

void Session::backToPrepare() {
    resetWorld();
    hasResult_ = false;
    phase_ = Phase::Prepare;
}

void Session::buy(NodeId node) {
    if (phase_ != Phase::Tree) return;
    if (tree_.purchase(node, scrap_)) {
        rebuildEffects();
        saveNow();
    }
}

void Session::respec() {
    if (phase_ != Phase::Tree) return;
    tree_.respecAll(scrap_);
    rebuildEffects();
    saveNow();
}

void Session::saveNow() const {
    if (savePath_.empty()) return;
    SaveData d;
    d.version = 1u;
    d.scrap = scrap_;
    for (size_t i = 0; i < kNodeCount; ++i) {
        d.nodeLevels[i] = tree_.level(static_cast<NodeId>(i));
    }
    d.bestKills = bestKills_;
    d.clearCounts = clearCounts_;
    ls::save(d, savePath_.c_str());
}

}  // namespace ls
