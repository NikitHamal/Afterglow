// Afterglow engine — character collision proxies (head sphere + capsules along spine and limbs, built from
// the live skeleton) and a camera solver that keeps the eye outside every body and inside the room.
#pragma once
#include "core/mathx.h"
#include <vector>

namespace ag {

class Character;

struct Capsule { V3 a, b; float r; int owner; bool head; };

class BodyProxies {
public:
    // rebuild from the characters' current pose (call after Scene::update)
    void build(const Character* const* chars, const bool* shown, int count);
    const std::vector<Capsule>& capsules() const { return caps_; }
    // closest distance from p to any capsule surface (negative inside); skips the owner's head when asked
    float distance(const V3& p, int skipHeadOf = -1) const;

private:
    std::vector<Capsule> caps_;
};

struct CameraSafety {
    V3 boundsMin{-3, 0.1f, -3}, boundsMax{3, 2.8f, 3.5f};
    float margin = 0.09f;          // eye-to-skin clearance (keeps the near plane out of bodies)
    float nearProbe = 0.12f;       // also clear a point this far ahead of the eye
};

// Moves `eye` (looking at `target`) out of every proxy and into the room. skipHeadOf: index of the
// character whose head the eye legitimately sits in (first person), or -1.
V3 resolveCamera(const BodyProxies& proxies, const CameraSafety& safety, V3 eye, const V3& target, int skipHeadOf);

}  // namespace ag
