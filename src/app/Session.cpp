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

// Turrets need elbow room, or a player can stack six on one pixel and turn
// positioning into a non-decision. It also has to be comfortably larger than
// the click radius below: when the two were equal, clicking the gap between
// two turrets grabbed one of them instead of deploying into it.
constexpr float kTurretSpacing = 34.0f;
constexpr float kBaseClearance = 18.0f;

// What a turret costs, and how fast that climbs. The same 1.35 curve the
// upgrade tree uses, so the two economies read as one system.
constexpr uint32_t kTurretBaseCost[3] = {60u, 140u, 130u};
// Steep on purpose. More guns is the strongest scaling in the game - each one
// is linear in count AND carries every multiplier the tree has bought - so at
// 1.35 an afternoon's Scrap bought an artillery park and the back half of the
// campaign stopped being a question. A tenth gun should be a campaign
// decision.
constexpr double   kTurretCostGrowth = 1.55;

// The arsenal a new commander starts with: four machine guns, which is what
// the four authored emplacements used to hand out for free.
constexpr uint32_t kStartingMachineGuns = 4u;

// Builds a turret of the given kind from its base stats, then folds in every
// relevant tree effect (scalars and the behaviour toggles). This is what lets
// two different builds feel different: Overclock vs Explosive Shells change a
// turret's fields, not just a damage number.
Turret makeTurret(TurretKind kind, Vec2 pos, const Effects& e) {
    Turret t;
    t.kind = kind;
    t.position = pos;
    // Divides the target's armour rather than multiplying damage, so the
    // node is worth exactly as much as the armour it meets and nothing at all
    // against something unarmoured. 2.0 so the tooltip can say "halves
    // armour" and mean it literally.
    t.armorPierce = e.armorPiercing ? 2.0f : 1.0f;

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
    arsenal_ = saveData_.arsenal;
    stats_ = saveData_.stats;
    if (arsenal_[0] == 0u && arsenal_[1] == 0u && arsenal_[2] == 0u) {
        arsenal_[0] = kStartingMachineGuns;
    }
    clampSettings(settings_);
    // A save that has already seen the tutorial starts with it finished. A
    // fresh one - or an older save, whose spare flag bit reads as zero -
    // starts at step one, which is exactly right for both.
    if (settings_.tutorialDone) tutorial_.skip();
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
    syncWorldTurrets();
}

void Session::defaultLoadout() {
    // Open with the arsenal already deployed rather than an empty map: a
    // first-time player should see a working defence and then rearrange it,
    // not face a blank field and no idea what a turret is.
    loadout_.clear();
    autoDeploy();
}

uint32_t Session::owned(TurretKind kind) const {
    return arsenal_[static_cast<size_t>(kind)];
}

uint32_t Session::placed(TurretKind kind) const {
    uint32_t n = 0u;
    for (const Turret& t : loadout_) {
        if (t.kind == kind) ++n;
    }
    return n;
}

uint32_t Session::available(TurretKind kind) const {
    const uint32_t have = owned(kind);
    const uint32_t out = placed(kind);
    return (have > out) ? (have - out) : 0u;
}

uint32_t Session::turretPrice(TurretKind kind) const {
    const size_t i = static_cast<size_t>(kind);
    return static_cast<uint32_t>(std::llround(
        static_cast<double>(kTurretBaseCost[i]) *
        std::pow(kTurretCostGrowth, static_cast<double>(arsenal_[i]))));
}

bool Session::canAffordTurret(TurretKind kind) const {
    return kindUnlocked(kind) && scrap_ >= turretPrice(kind);
}

bool Session::buyTurret(TurretKind kind) {
    if (!canAffordTurret(kind)) return false;
    scrap_ -= turretPrice(kind);
    ++arsenal_[static_cast<size_t>(kind)];
    ++stats_.turretsBought;
    selectedKind_ = kind;
    saveNow();
    return true;
}

Session::Placement Session::placementAt(Vec2 pos, int ignoreIndex) const {
    const Grid& grid = level_.map.grid;
    int cx = 0;
    int cy = 0;
    if (!grid.worldToCell(pos, cx, cy)) return Placement::OffMap;
    if (!level_.map.isWalkable(cx, cy)) return Placement::OnWall;

    const float baseRange = grid.cellSize() * 1.5f + kBaseClearance;
    if (distanceSq(pos, level_.map.baseCenter()) < baseRange * baseRange) {
        return Placement::TooCloseToBase;
    }
    for (size_t i = 0; i < loadout_.size(); ++i) {
        if (static_cast<int>(i) == ignoreIndex) continue;
        if (distanceSq(pos, loadout_[i].position) <
            kTurretSpacing * kTurretSpacing) {
            return Placement::TooCloseToTurret;
        }
    }
    return Placement::Ok;
}

int Session::turretIndexAt(Vec2 pos, float radius) const {
    int best = -1;
    float bestD = radius * radius;
    for (size_t i = 0; i < loadout_.size(); ++i) {
        const float d = distanceSq(loadout_[i].position, pos);
        if (d <= bestD) {
            bestD = d;
            best = static_cast<int>(i);
        }
    }
    return best;
}

bool Session::placeTurretAt(Vec2 pos) {
    if (phase_ != Phase::Prepare) return false;
    if (available(selectedKind_) == 0u) return false;
    if (!canPlaceAt(pos)) return false;
    loadout_.push_back(makeTurret(selectedKind_, pos, effects_));
    ++placementsMade_;
    syncWorldTurrets();
    return true;
}

bool Session::moveTurret(int index, Vec2 pos) {
    if (phase_ != Phase::Prepare) return false;
    if (index < 0 || index >= static_cast<int>(loadout_.size())) return false;
    if (!canPlaceAt(pos, index)) return false;
    loadout_[static_cast<size_t>(index)].position = pos;
    ++placementsMade_;
    syncWorldTurrets();
    return true;
}

void Session::recallTurret(int index) {
    if (phase_ != Phase::Prepare) return;
    if (index < 0 || index >= static_cast<int>(loadout_.size())) return;
    loadout_.erase(loadout_.begin() + index);
    syncWorldTurrets();
}

void Session::syncWorldTurrets() {
    // The loadout is built before the first world exists (the constructor
    // arranges a starting defence, then creates the world it goes in), so
    // this has to tolerate being called early.
    if (world_ == nullptr) return;
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

bool Session::isLevelUnlocked(int idx) const {
    if (idx < 0 || idx >= kLevelCount) return false;
    if (levelTier(idx) == 0) return true;

    // ANY parent, not all of them. The campaign is a graph the player picks a
    // route through: requiring every parent would quietly turn it back into a
    // corridor with extra steps.
    int parents[kMaxParents];
    const int n = levelParents(idx, parents);
    for (int i = 0; i < n; ++i) {
        if (clearCountFor(parents[i]) > 0u) return true;
    }
    return false;
}

int Session::furthestUnlockedLevel() const {
    int furthest = 0;
    for (int i = 1; i < kLevelCount; ++i) {
        if (isLevelUnlocked(i)) furthest = i;
    }
    return furthest;
}

int Session::suggestedNextLevel() const {
    // A child of what was just held, if one opened: that is the sector this
    // victory actually earned, and it is what the report should offer.
    for (int i = 0; i < kLevelCount; ++i) {
        if (clearCountFor(i) > 0u || !isLevelUnlocked(i)) continue;
        int parents[kMaxParents];
        const int n = levelParents(i, parents);
        for (int p = 0; p < n; ++p) {
            if (parents[p] == levelIndex_) return i;
        }
    }
    // Otherwise the shallowest thing still standing, so a player who doubles
    // back to farm an old sector is not sent to the end of the campaign.
    for (int i = 0; i < kLevelCount; ++i) {
        if (isLevelUnlocked(i) && clearCountFor(i) == 0u) return i;
    }
    return levelIndex_;
}

bool Session::canAdvance() const {
    return hasResult_ && result_.victory &&
           suggestedNextLevel() != levelIndex_;
}

int Session::sectorsOpenedHere() const {
    int opened = 0;
    for (int i = 0; i < kLevelCount; ++i) {
        if (clearCountFor(i) > 0u || !isLevelUnlocked(i)) continue;
        int parents[kMaxParents];
        const int n = levelParents(i, parents);
        for (int p = 0; p < n; ++p) {
            if (parents[p] == levelIndex_) { ++opened; break; }
        }
    }
    return opened;
}

void Session::skipTutorial() {
    tutorial_.skip();
    settings_.tutorialDone = true;
    saveNow();
}

void Session::restartTutorial() {
    tutorial_.restart();
    settings_.tutorialDone = false;
    saveNow();
}

void Session::grantScrap(uint32_t amount) { scrap_ += amount; }

void Session::openTreeDirect() { phase_ = Phase::Tree; }

void Session::selectLevelUnchecked(int idx) {
    idx = std::clamp(idx, 0, kLevelCount - 1);
    levelIndex_ = idx;
    level_ = makeLevelByIndex(idx);
    defaultLoadout();
    resetWorld();
    phase_ = Phase::Prepare;
    hasResult_ = false;
    airstrikeCd_ = 0.0f;
    overchargeCd_ = 0.0f;
}

void Session::selectLevel(int idx) {
    idx = std::clamp(idx, 0, kLevelCount - 1);
    // Locked sectors are not reachable, from the map or from anywhere else.
    if (!isLevelUnlocked(idx)) return;
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
    world_->setHealthMultiplier(level_.enemyHealthMult);

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

void Session::toggleTurretAt(Vec2 worldPos, float radius) {
    if (phase_ != Phase::Prepare) return;

    // On a turret: swap its kind if a different one is selected and spare,
    // otherwise recall it. On open ground: deploy, if there is one to deploy.
    const int existing = turretIndexAt(worldPos, radius);
    if (existing >= 0) {
        Turret& t = loadout_[static_cast<size_t>(existing)];
        if (t.kind != selectedKind_ && available(selectedKind_) > 0u) {
            t = makeTurret(selectedKind_, t.position, effects_);
        } else {
            loadout_.erase(loadout_.begin() + existing);
        }
        syncWorldTurrets();
        return;
    }
    placeTurretAt(worldPos);
}

void Session::removeTurretAt(Vec2 worldPos, float radius) {
    recallTurret(turretIndexAt(worldPos, radius));
}

void Session::autoDeploy() {
    // The "I do not want to arrange this" button. Everything still in the
    // crate goes out around the map's deploy anchor; a player who does want
    // to arrange it drags them where they belong.
    int spare = 0;
    for (int k = 0; k < 3; ++k) {
        spare += static_cast<int>(available(static_cast<TurretKind>(k)));
    }
    if (spare <= 0) return;

    const std::vector<Vec2> spots =
        defaultDeployPositions(level_.map, turretCount() + spare);
    for (const Vec2& at : spots) {
        if (!canPlaceAt(at)) continue;

        TurretKind kind = selectedKind_;
        if (available(kind) == 0u) {
            bool found = false;
            for (int k = 0; k < 3 && !found; ++k) {
                const auto candidate = static_cast<TurretKind>(k);
                if (available(candidate) > 0u) {
                    kind = candidate;
                    found = true;
                }
            }
            if (!found) break;
        }
        loadout_.push_back(makeTurret(kind, at, effects_));
    }
    syncWorldTurrets();
}

void Session::clearLoadout() {
    if (phase_ != Phase::Prepare) return;
    loadout_.clear();
    syncWorldTurrets();
}

void Session::selectKind(TurretKind kind) {
    if (!kindUnlocked(kind)) return;
    selectedKind_ = kind;
}

TargetingMode Session::targetingMode() const {
    return loadout_.empty() ? TargetingMode::First : loadout_.front().mode;
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
    timeScale_ = (timeScale_ >= 4) ? 1 : (timeScale_ + 1);
}

void Session::setTimeScale(int scale) {
    timeScale_ = std::clamp(scale, 1, 4);
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
    // The tutorial only ever watches. It never touches the simulation and it
    // never blocks anything - a player who ignores it entirely plays a normal
    // game, which is the only kind of tutorial worth having.
    if (tutorial_.active()) {
        tutorial_.observe(*this, frameSeconds);
        if (!tutorial_.active()) {
            settings_.tutorialDone = true;
            saveNow();
        }
    }
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

void Session::goStats() {
    returnPhase_ = phase_;
    phase_ = Phase::Stats;
}

void Session::pause() {
    if (phase_ == Phase::Battle) phase_ = Phase::Pause;
}

void Session::resume() {
    if (phase_ == Phase::Pause) phase_ = Phase::Battle;
    else if (phase_ == Phase::Options || phase_ == Phase::Stats) {
        phase_ = returnPhase_;
    }
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

    // Lifetime totals. Recorded here rather than in the world, because they
    // outlive every battle and belong to the save.
    ++stats_.runs;
    if (result_.victory) ++stats_.victories;
    stats_.kills += totalKills;
    stats_.scrapEarned += payout_.scrap;
    stats_.secondsPlayed +=
        static_cast<uint32_t>(telemetry_.elapsedSeconds());
    stats_.bestRunKills = std::max(stats_.bestRunKills, totalKills);
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
        ++stats_.nodesBought;
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

uint32_t Session::bestKillsFor(int level) const {
    if (level < 0 || static_cast<size_t>(level) >= kSaveLevels) return 0u;
    return bestKills_[static_cast<size_t>(level)];
}

uint32_t Session::clearCountFor(int level) const {
    if (level < 0 || static_cast<size_t>(level) >= kSaveLevels) return 0u;
    return clearCounts_[static_cast<size_t>(level)];
}

bool Session::hasProgress() const {
    if (scrap_ > 0u || tree_.totalSpent() > 0u) return true;
    for (const uint32_t best : bestKills_) {
        if (best > 0u) return true;
    }
    return false;
}

void Session::newGame() {
    scrap_ = 0u;
    arsenal_ = {kStartingMachineGuns, 0u, 0u};
    uint32_t refund = 0u;
    tree_.respecAll(refund);      // zeroes every node; the refund is discarded
    bestKills_.fill(0u);
    clearCounts_.fill(0u);
    stats_ = Stats{};
    levelIndex_ = 0;
    level_ = makeLevel1();
    selectedKind_ = TurretKind::MachineGun;
    hasResult_ = false;
    rebuildEffects();
    defaultLoadout();
    resetWorld();
    phase_ = Phase::Prepare;
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
    d.arsenal = arsenal_;
    d.stats = stats_;
    ls::save(d, savePath_.c_str());
}

}  // namespace ls
