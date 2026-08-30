#include "fx/Particles.h"

#include <cmath>

namespace ls {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDrag = 2.4f;          // spark/smoke velocity damping per second
constexpr float kEmberRise = -18.0f;   // embers drift up-screen

float sizeFor(ParticleKind kind) {
    switch (kind) {
        case ParticleKind::Spark:    return 1.8f;
        case ParticleKind::Ember:    return 2.2f;
        case ParticleKind::Smoke:    return 6.0f;
        case ParticleKind::Flash:    return 9.0f;
        case ParticleKind::ScrapArc: return 3.0f;
    }
    return 2.0f;
}

}  // namespace

ParticlePool::ParticlePool() {
    const size_t cap = static_cast<size_t>(kCapacity);
    position.resize(cap);
    velocity.resize(cap);
    origin.resize(cap);
    target.resize(cap);
    ttl.resize(cap, 0.0f);
    maxTtl.resize(cap, 1.0f);
    size.resize(cap, 1.0f);
    kind.resize(cap, 0u);
}

bool ParticlePool::spawn(Vec2 pos, Vec2 vel, ParticleKind k, float life,
                         float sz) {
    if (count_ >= kCapacity) return false;
    const size_t i = static_cast<size_t>(count_++);
    position[i] = pos;
    velocity[i] = vel;
    origin[i] = pos;
    target[i] = pos;
    ttl[i] = life;
    maxTtl[i] = (life > 0.0f) ? life : 1.0f;
    size[i] = sz;
    kind[i] = static_cast<uint8_t>(k);
    return true;
}

void ParticlePool::kill(uint32_t i) {
    if (i >= count_) return;
    const uint32_t last = count_ - 1u;
    if (i != last) {
        const size_t d = static_cast<size_t>(i);
        const size_t l = static_cast<size_t>(last);
        position[d] = position[l];
        velocity[d] = velocity[l];
        origin[d]   = origin[l];
        target[d]   = target[l];
        ttl[d]      = ttl[l];
        maxTtl[d]   = maxTtl[l];
        size[d]     = size[l];
        kind[d]     = kind[l];
    }
    count_ = last;
}

uint32_t ParticlePool::emitBurst(Vec2 pos, Vec2 direction, uint32_t n,
                                 ParticleKind k, float speed, float life,
                                 Pcg32& rng) {
    const Vec2 dir = normalized(direction);
    const bool directional = lengthSq(dir) > 0.0f;
    uint32_t spawned = 0u;

    for (uint32_t i = 0; i < n; ++i) {
        // A directional burst cones around `direction`; an omnidirectional one
        // takes the whole circle.
        const float base = directional ? std::atan2(dir.y, dir.x) : 0.0f;
        const float spread = directional ? 1.1f : kPi;
        const float angle = base + rng.nextRange(-spread, spread);
        const float mag = speed * rng.nextRange(0.35f, 1.0f);
        const Vec2 vel{std::cos(angle) * mag, std::sin(angle) * mag};
        if (!spawn(pos, vel, k, life * rng.nextRange(0.6f, 1.0f), sizeFor(k))) {
            break;
        }
        ++spawned;
    }
    return spawned;
}

bool ParticlePool::emitFlash(Vec2 pos, Vec2 direction) {
    return spawn(pos, normalized(direction) * 40.0f, ParticleKind::Flash, 0.05f,
                 sizeFor(ParticleKind::Flash));
}

bool ParticlePool::emitScrapArc(Vec2 from, Vec2 to) {
    if (!spawn(from, Vec2{0.0f, 0.0f}, ParticleKind::ScrapArc, 0.7f,
               sizeFor(ParticleKind::ScrapArc))) {
        return false;
    }
    const size_t i = static_cast<size_t>(count_ - 1u);
    origin[i] = from;
    target[i] = to;
    return true;
}

void ParticlePool::update(float dt) {
    // Downward so the swap-remove always moves an already-visited element.
    for (uint32_t i = count_; i-- > 0u;) {
        ttl[i] -= dt;
        if (ttl[i] <= 0.0f) {
            kill(i);
            continue;
        }

        switch (static_cast<ParticleKind>(kind[i])) {
            case ParticleKind::ScrapArc: {
                // Flies to the counter on an eased arc. This is the single
                // most important reward animation in the game (GDD 12.3), so
                // it is the one particle with an authored path.
                const float t = 1.0f - (ttl[i] / maxTtl[i]);
                const float eased = t * t * (3.0f - 2.0f * t);
                const Vec2 flat = lerp(origin[i], target[i], eased);
                const float lift = std::sin(t * kPi) * 40.0f;
                position[i] = Vec2{flat.x, flat.y - lift};
                break;
            }
            case ParticleKind::Ember:
                velocity[i].y += kEmberRise * dt;
                velocity[i] -= velocity[i] * (kDrag * 0.4f * dt);
                position[i] += velocity[i] * dt;
                break;
            case ParticleKind::Flash:
                // Fixed to the barrel for its single frame.
                break;
            default:
                velocity[i] -= velocity[i] * (kDrag * dt);
                position[i] += velocity[i] * dt;
                break;
        }
    }
}

float ParticlePool::progress(uint32_t i) const {
    const float m = maxTtl[i];
    if (m <= 0.0f) return 1.0f;
    const float p = 1.0f - (ttl[i] / m);
    return (p < 0.0f) ? 0.0f : (p > 1.0f ? 1.0f : p);
}

}  // namespace ls
