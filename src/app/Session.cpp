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
        settings_    = saveData_.settings;
    } else {
        scrap_ = 0u;
    }
    clampSettings(settings_);
    timeScale_ = settings_.defaultTimeScale;
    // The session opens on the title screen, whose background is a live
    // battle: the simulation is fast enough to be a menu backdrop after M5,
    // so resetWorld below leaves one running behind it.
    juice_.setScale(settings_.shake());
    juice_.setHitstopEnabled(settings_.hitstop);
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
    telemetry_.begin(level_);
    lastShots_ = {0u, 0u, 0u};
    resetPresentation();
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
    telemetry_.begin(level_);
    lastShots_ = {0u, 0u, 0u};
    resetPresentation();
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

    // The emotional peak of a battle deserves to be loud (GDD 10).
    events_.airstrike = true;
    juice_.onDetonation(12.0f);
    for (int i = 0; i < 12; ++i) {
        const float x = world_->map().grid.worldWidth() *
                        (static_cast<float>(i) + 0.5f) / 12.0f;
        particles_.emitBurst(Vec2{x, rowY}, Vec2{0.0f, 0.0f}, 14u,
                             ParticleKind::Smoke, 150.0f, 0.75f, fxRng_);
        particles_.emitBurst(Vec2{x, rowY}, Vec2{0.0f, 0.0f}, 8u,
                             ParticleKind::Ember, 190.0f, 0.55f, fxRng_);
    }
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
    events_.overcharge = true;
    juice_.onDetonation(3.0f);
    particles_.emitBurst(turrets[best].position, Vec2{0.0f, 0.0f}, 16u,
                         ParticleKind::Ember, 120.0f, 0.5f, fxRng_);
}

void Session::updateBattle(float dt) {
    if (phase_ != Phase::Battle) return;
    if (airstrikeCd_ > 0.0f) airstrikeCd_ -= dt;
    if (overchargeCd_ > 0.0f) overchargeCd_ -= dt;

    const uint32_t killsBefore = world_->totalKills();
    const uint32_t arrivalsBefore = world_->totalArrived();

    director_.update(*world_, level_, dt);
    world_->tick(dt);

    telemetry_.sample(*world_, dt);
    emitBattleFx(killsBefore, arrivalsBefore);

    if (world_->isOver()) finishBattle();
}

// Turns what the tick did into particles, corpses, damage numbers, shake and
// the event record the audio engine reads. All of it downstream of the
// simulation and none of it able to reach back into it.
void Session::emitBattleFx(uint32_t killsBefore, uint32_t arrivalsBefore) {
    const uint32_t kills = world_->totalKills() - killsBefore;
    const uint32_t arrivals = world_->totalArrived() - arrivalsBefore;

    events_.kills += kills;
    events_.arrivals += arrivals;

    juice_.onKills(kills);
    juice_.onBaseHit(arrivals);

    // Shots, totalled per turret kind so the mix can tell a machine gun from
    // a cannon without the audio engine knowing what a turret is.
    uint64_t byKind[3] = {0u, 0u, 0u};
    for (const Turret& t : world_->turrets()) {
        byKind[static_cast<size_t>(t.kind)] += t.shotsFired;
    }
    const auto fired = [](uint64_t now, uint64_t& last) -> uint32_t {
        const uint32_t delta =
            (now > last) ? static_cast<uint32_t>(now - last) : 0u;
        last = now;
        return delta;
    };
    events_.gunShots += fired(byKind[0], lastShots_[0]);
    events_.cannonShots += fired(byKind[1], lastShots_[1]);
    events_.flameShots += fired(byKind[2], lastShots_[2]);

    if (kills == 0u && arrivals == 0u) return;

    for (const Death& d : world_->deaths()) {
        corpses_.add(d.position, d.direction, d.type);
        particles_.emitBurst(d.position, -d.direction, 3u, ParticleKind::Spark,
                             90.0f, 0.28f, fxRng_);
        // Scrap arcs are the reward animation; one per kill would be a storm,
        // so they are rationed to a readable trickle.
        if ((fxRng_.nextU32() & 7u) == 0u) {
            particles_.emitScrapArc(d.position, scrapAnchor_);
        }
    }

    if (settings_.damageNumbers) {
        for (const Death& d : world_->deaths()) {
            numbers_.add(d.position, statsFor(static_cast<EnemyType>(d.type)).hp);
        }
    }

    if (arrivals > 0u) {
        particles_.emitBurst(world_->base().position, Vec2{0.0f, 0.0f},
                             10u * arrivals, ParticleKind::Smoke, 70.0f, 0.6f,
                             fxRng_);
    }
}

void Session::resetPresentation() {
    particles_.clear();
    corpses_.clear();
    numbers_.clear();
    juice_.reset();
    reportReveal_ = 0.0f;
    events_ = FrameEvents{};
}

void Session::updatePresentation(float frameSeconds) {
    juice_.update(frameSeconds);
    particles_.update(frameSeconds);
    corpses_.update(frameSeconds);
    numbers_.update(frameSeconds);
    if (phase_ == Phase::Report && reportReveal_ < 1.0f) {
        reportReveal_ += frameSeconds * 1.6f;
        if (reportReveal_ > 1.0f) reportReveal_ = 1.0f;
    }
}

FrameEvents Session::takeEvents() {
    const FrameEvents out = events_;
    events_ = FrameEvents{};
    return out;
}

void Session::applySettings() {
    clampSettings(settings_);
    juice_.setScale(settings_.shake());
    juice_.setHitstopEnabled(settings_.hitstop);
    saveNow();
}

void Session::goMenu() { phase_ = Phase::Menu; }

void Session::goOptions() {
    returnPhase_ = phase_;
    phase_ = Phase::Options;
}

void Session::goLevelSelect() { phase_ = Phase::LevelSelect; }

void Session::pause() {
    if (phase_ == Phase::Battle) phase_ = Phase::Pause;
}

void Session::resume() {
    if (phase_ == Phase::Pause) phase_ = Phase::Battle;
    else if (phase_ == Phase::Options) phase_ = returnPhase_;
}

void Session::abandonBattle() {
    // No payout: leaving a battle is not the same as losing one, and paying
    // for it would make quitting a strategy.
    hasResult_ = false;
    resetWorld();
    phase_ = Phase::Menu;
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

    telemetry_.finish(*world_, result_.victory);
    failure_ = analyse(telemetry_, tree_);

    events_.battleEnded = true;
    events_.victory = result_.victory;
    juice_.onDetonation(result_.victory ? 6.0f : 10.0f);

    hasResult_ = true;
    reportReveal_ = 0.0f;
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
    d.settings = settings_;
    ls::save(d, savePath_.c_str());
}

}  // namespace ls
