// Afterglow engine — procedural animation layers applied on top of clips:
// breathing, head/eye look-at with saccades (+ eye roll), verlet hair chains with sphere colliders,
// spring "jiggle" bones.
#pragma once
#include "anim/animator.h"
#include <string>
#include <vector>

namespace ag {

struct ProcParams {
    float breathRate = 0.24f;      // breaths per second
    float breathAmp = 1.f;         // 0 = none, 1 = calm, 2+ = panting
    bool lookAt = true;
    V3 lookTarget{0.f, 1.5f, 2.f}; // world
    float lookWeight = 1.f;        // head/neck share
    float eyeWeight = 1.f;
    float eyeRollUp = 0.f;         // 0..1, eyes roll upward (climax)
    bool saccades = true;
    bool hair = true;
    float hairGravity = 1.f, hairStiffness = 1.f;
    bool jiggle = true;
    float jiggleGain = 1.f;
    float floorY = -1e9f;          // hair floor plane (mattress top)
};

class Procedural {
public:
    void init(Animator& a, float rootScale);
    void apply(Animator& a, float dt, const ProcParams& p);
    void reset() { needReset_ = true; }
    int hairChainCount() const { return (int)chains_.size(); }
    int colliderCount() const { return (int)colliders_.size(); }

    struct Collider { int bone; V3 local; float r; };
    std::vector<Collider> colliders_;

private:
    void breathing(Animator& a, float dt, const ProcParams& p);
    void look(Animator& a, float dt, const ProcParams& p);
    void jiggle(Animator& a, float dt, const ProcParams& p);
    void hair(Animator& a, float dt, const ProcParams& p);

    float scale_ = 1.f;
    bool needReset_ = true;
    float breathPhase_ = 0.f, time_ = 0.f;
    int chest_ = -1, belly_ = -1, neck_ = -1, head_ = -1, eyeL_ = -1, eyeR_ = -1, shoulderL_ = -1, shoulderR_ = -1;
    V3 headFwd_{0, 0, 1}, headUp_{0, 1, 0}, neckFwd_{0, 0, 1}, eyeFwdL_{0, 0, 1}, eyeFwdR_{0, 0, 1};
    V3 lookSmooth_{0, 1.5f, 2.f};
    V3 saccade_{0, 0, 0};
    float saccadeT_ = 0.f;

    struct Spring { int bone; int child; float len; V3 x, v; bool init = false; float k, c, maxOff, sag; };
    std::vector<Spring> springs_;

    struct Chain {
        std::vector<int> bones;          // root .. tip leaf
        std::vector<float> rest;         // rest length bone[i-1] -> bone[i]
        std::vector<V3> p, prev;
    };
    std::vector<Chain> chains_;
    V3 lastAnchor_{0, 0, 0};
    float hairAccum_ = 0.f;
};

}  // namespace ag
