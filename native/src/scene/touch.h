// Afterglow engine — touch: body zones on the real surface (found on the bind mesh), arm IK that puts a hand
// onto a zone (fingertips resting on the skin, palm facing the body, fingers conforming), rubbing / kneading
// motion, and finger contact spheres that dent the soft tissue in the vertex shader.
#pragma once
#include "core/mathx.h"
#include <string>
#include <vector>

namespace ag {

class Animator;
class Character;

enum class Zone { Vulva = 0, BreastL, BreastR, Count };

struct BodyZone {
    int bone = -1;              // node the zone rides on
    V3 local{0, 0, 0};          // surface point in the bone's frame (bind)
    V3 normalLocal{0, 0, 1};    // outward normal in the bone's frame
    bool valid = false;
};

// what one hand should do this frame
struct HandGoal {
    bool active = false;
    Zone zone = Zone::Vulva;
    float weight = 0.f;         // 0..1 blend from the clip pose
    float phase = 0.f;          // rub / knead cycle 0..1
    float amplitude = 0.012f;   // metres of rubbing motion along the surface
    float curl = 0.25f;         // finger curl per joint (radians); grabs curl more
    float press = 0.004f;       // how far the fingertips sink into the skin (dent depth)
    // a hand on any surface (the partner's chest, waist, cheek...): world point + outward normal + finger direction
    bool world = false;
    V3 pos{0, 0, 0}, normal{0, 0, 1}, along{0, 1, 0};
    bool dent = true;           // finger pads dent the skin they rest on
};

struct TouchGoals { HandGoal left, right; };

class TouchRig {
public:
    // zones from the bind mesh (needs the model's CPU vertex data: call before releaseCpuData)
    void init(Character& c);
    bool zoneWorld(const Animator& a, Zone z, V3& pos, V3& normal) const;
    // IK + fingers onto the zones (runs after the procedural layers); fills `contacts`
    void apply(Animator& a, const TouchGoals& goals);

    struct Contact { V3 c; float r; };
    std::vector<Contact> contacts;       // finger/palm spheres in world space (soft-tissue dents)
    int handJointLo[2] = {-1, -1}, handJointHi[2] = {-1, -1};   // skin joint ranges never dented (L, R)
    bool ready() const { return ready_; }

private:
    struct Arm {
        int upper = -1, elbow = -1, wrist = -1, middle0 = -1, index1 = -1, pinky1 = -1;
        std::vector<int> fingers[4];     // index, middle, ring, pinky chains (proximal -> distal)
        float handLen = 0.17f;
        float side = 1.f;                // +1 right, -1 left (palm normal sign)
    };
    void solveArm(Animator& a, const Arm& arm, const HandGoal& g);
    BodyZone zones_[(int)Zone::Count];
    Arm arms_[2];
    bool ready_ = false;
};

}  // namespace ag
