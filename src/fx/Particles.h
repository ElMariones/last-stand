#pragma once
#include <cstdint>
#include <vector>

#include "math/Rng.h"
#include "math/Vec2.h"

namespace ls {

enum class ParticleKind : uint8_t {
    Spark = 0,    // impact: fast, short, bright
    Ember,        // burn: slow, drifting, orange
    Smoke,        // detonation: expanding, dark, slow
    Flash,        // muzzle: one frame of white at the barrel
    ScrapArc,     // kill -> the Scrap counter. The reward animation.
};

// Fixed-capacity SoA pool, swap-removed exactly like EnemyPool. Sized once at
// construction, so a battle never allocates (GDD 14.3) — and the cap is what
// keeps a 4,000-kills-per-minute endgame from turning into a particle storm
// that costs more than the simulation it is decorating.
class ParticlePool {
public:
    static constexpr uint32_t kCapacity = 4096u;

    ParticlePool();

    // Returns the number actually spawned, which is fewer than asked for when
    // the pool is full. Dropping particles is the correct failure mode.
    uint32_t emitBurst(Vec2 origin, Vec2 direction, uint32_t count,
                       ParticleKind kind, float speed, float ttl, Pcg32& rng);
    bool     emitFlash(Vec2 origin, Vec2 direction);
    bool     emitScrapArc(Vec2 origin, Vec2 target);

    void update(float dt);
    void clear() { count_ = 0u; }

    uint32_t count() const { return count_; }

    // 0 at spawn, 1 at expiry. Renderers fade and scale on this.
    float progress(uint32_t i) const;

    std::vector<Vec2>    position;
    std::vector<Vec2>    velocity;
    std::vector<Vec2>    origin;     // ScrapArc only
    std::vector<Vec2>    target;     // ScrapArc only
    std::vector<float>   ttl;
    std::vector<float>   maxTtl;
    std::vector<float>   size;
    std::vector<uint8_t> kind;

private:
    bool spawn(Vec2 pos, Vec2 vel, ParticleKind kind, float ttl, float size);
    void kill(uint32_t i);

    uint32_t count_ = 0u;
};

}  // namespace ls
