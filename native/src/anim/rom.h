// Afterglow engine — anatomical joint ranges, enforced on every frame after all layers (clips, pose hooks,
// procedural look/breathing, kiss / touch IK). Each limited bone's rotation relative to its rest frame is split into
// twist (about the bone axis, +Y) and swing (of the axis); swing is held inside a cone — or, for the elbows and knees,
// to their hinge (one bending axis with a small off-axis play) — and twist inside its range, with soft limits (the
// last few degrees ease in, nothing snaps). The range of each bone is the anatomical one, widened to the envelope of
// the character's own clips plus a margin, so authored motion is never altered while no runtime layer can bend a
// joint past what a body can do.
#pragma once
#include "anim/animator.h"
#include <vector>

namespace ag {

// one bone's range as solvers need it (degrees; hinge axis in the bone's rest frame, + = the main bend)
struct JointRange {
    bool valid = false, hinge = false;
    V3 axis{1, 0, 0};
    float hingeMin = -180.f, hingeMax = 180.f, offAxisMax = 180.f, swingMax = 180.f, twistMin = -180.f, twistMax = 180.f;
};

class JointLimits {
public:
    void build(const Model& m);
    // clamps the pose (parents first) and recomputes the globals of what changed; returns how many bones moved
    int apply(Animator& a) const;
    // for solvers that stay inside the ranges by construction (arm / kiss IK): one bone's range, and one bone clamped
    // (soft, like apply) with its subtree recomputed; true when it moved
    JointRange range(int node) const;
    bool clampNode(Animator& a, int node) const;
    bool inRange(int node, const Quat& rest, const Quat& r) const;   // unlimited bones are always in range
    int count() const { return (int)limits_.size(); }
    static bool enabled;                 // QA: rom=0 turns the ranges off for comparisons
    static bool log;                     // QA: romlog=1 logs every bone a range moves (name, degrees)

private:
    struct Limit {
        int node = -1;
        float swingMax = 180.f;          // degrees (cone) — unused for hinges
        float twistMin = -180.f, twistMax = 180.f;
        bool hinge = false;
        V3 axis{1, 0, 0};                // hinge axis in the bone's rest frame (positive = the clips' main bend)
        float hingeMin = -180.f, hingeMax = 180.f, offAxisMax = 180.f;
    };
    std::vector<Limit> limits_;          // parents before children
    static Quat clampRot(const Quat& rest, const Quat& r, const Limit& L);
};

}  // namespace ag
