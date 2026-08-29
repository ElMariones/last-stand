#include "app/Session.h"

#include <algorithm>

#include "math/Rect.h"

namespace ls {

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
    resetWorld();
}

void Session::resetWorld() {
    world_ = std::make_unique<World>(level_.map, /*seed*/ 0x5EEDu + clearCounts_[0]);
    world_->setLevelTotal(level_.totalEnemies);
    for (const Vec2& hp : level_.map.hardpoints) {
        world_->placeTurret(hp);
    }
    applyBonuses();
    director_ = SpawnDirector{};
}

void Session::applyBonuses() {
    const Bonuses b = tree_.bonuses();
    ls::Base& base = world_->base();
    base.maxHealth += b.baseBonusHp;
    base.health = base.maxHealth;
    base.regenPerSecond = b.baseRegen;

    for (ls::Turret& t : world_->turrets()) {
        t.damage *= b.damageMult;
        t.range *= b.rangeMult;
        t.fireInterval /= b.fireRateMult;
    }
}

bool Session::toggleTurretAt(Vec2 worldPos, float halfW, float halfH) {
    if (phase_ != Phase::Prepare) return false;
    for (size_t i = 0; i < level_.map.hardpoints.size(); ++i) {
        const Rect hit = fromCenter(level_.map.hardpoints[i], halfW, halfH);
        if (contains(hit, worldPos)) {
            // Toggle: remove a turret sitting here, else add one.
            auto& turrets = world_->turrets();
            for (auto it = turrets.begin(); it != turrets.end(); ++it) {
                if (distanceSq(it->position, level_.map.hardpoints[i]) < 1.0f) {
                    turrets.erase(it);
                    return true;
                }
            }
            world_->placeTurret(level_.map.hardpoints[i]);
            applyBonuses();   // a new turret must inherit current bonuses
            return true;
        }
    }
    return false;
}

void Session::startBattle() {
    if (phase_ != Phase::Prepare) return;
    director_ = SpawnDirector{};
    hasResult_ = false;
    phase_ = Phase::Battle;
}

void Session::updateBattle(float dt) {
    if (phase_ != Phase::Battle) return;
    director_.update(*world_, level_, dt);
    world_->tick(dt);
    if (world_->isOver()) finishBattle();
}

void Session::finishBattle() {
    const uint32_t totalKills = world_->totalKills();
    result_ = BattleResult{world_->isVictory(), totalKills, level_.totalEnemies,
                           bestKills_[0], clearCounts_[0]};
    payout_ = computePayout(result_, level_.killValue, level_.depthBonusWeight,
                            tree_.bonuses().scrapMult);

    scrap_ += payout_.scrap;
    if (totalKills > bestKills_[0]) bestKills_[0] = totalKills;
    if (result_.victory) ++clearCounts_[0];

    hasResult_ = true;
    phase_ = Phase::Report;
    saveNow();
}

void Session::retry() {
    resetWorld();
    hasResult_ = false;
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
    if (tree_.purchase(node, scrap_)) saveNow();
}

void Session::respec() {
    if (phase_ != Phase::Tree) return;
    tree_.respecAll(scrap_);
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
