// Afterglow engine — touch: body zones on the real surface (found on the bind mesh), arm IK that puts a hand
// onto a zone (fingertips resting on the skin, palm facing the body, fingers conforming), rubbing / kneading
// motion, and finger contact spheres that dent the soft tissue in the vertex shader.
#pragma once
#include "anim/rom.h"
#include "core/mathx.h"
#include <string>
#include <vector>

namespace ag {

class Animator;
class Character;

// L / R are the character's own sides (MMD .L / .R); Chest* / Waist* are where a partner's hands rest in an embrace,
// Sternum the upper chest at the midline (one's own hand: "me"); Mouth is the front of the lips at the midline, Nose
// its tip (both ride on the head)
enum class Zone { Vulva = 0, BreastL, BreastR, ChestL, ChestR, WaistL, WaistR, Sternum, Mouth, Nose, Count };

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
    // world goals: a flat palm centred on `pos` (the wrist `palmLift` off the surface) instead of fingertips resting
    // beyond it, and where the elbow should point (zero: keep the clip's bend)
    bool flat = false;
    float palmLift = 0.02f;
    V3 elbowHint{0, 0, 0};
};

struct TouchGoals { HandGoal left, right; };

class TouchRig {
public:
    // zones from the bind mesh (needs the model's CPU vertex data: call before releaseCpuData)
    void init(Character& c);
    bool zoneWorld(const Animator& a, Zone z, V3& pos, V3& normal) const;
    // the zone on the skin's rest shape (bind space): where a garment's own bind shape is measured against it
    bool zoneBind(Zone z, V3& pos, V3& normal) const;
    // the lip front with the kiss pucker at `kissMorph` (lips pushed forward)
    bool mouthWorld(const Animator& a, float kissMorph, V3& pos) const;
    // signed distance (m, + outside) from a world point to this character's face around the mouth and nose
    float faceDistance(const Animator& a, const V3& p) const;
    // the face surface samples (front-facing skin within 9 cm of the mouth, ~4 mm apart) in world space
    void faceWorld(const Animator& a, std::vector<V3>& out) const;
    void faceNormalsWorld(const Animator& a, std::vector<V3>& out) const;   // ... and their outward normals
    // IK + fingers onto the zones (runs after the procedural layers); fills `contacts`. With the character's joint
    // ranges, hands laid on a partner (world goals) are solved inside them (hinge elbows, twist in range)
    void apply(Animator& a, const TouchGoals& goals, const JointLimits* rom = nullptr);
    void curlHands(Animator& a, float amount);

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
    bool solveArmRanged(Animator& a, const Arm& arm, const V3& wristT, const V3& elbowHint, float weight);
    const JointLimits* rom_ = nullptr;   // this frame's ranges (apply)
    BodyZone zones_[(int)Zone::Count];
    V3 bindPos_[(int)Zone::Count], bindNrm_[(int)Zone::Count];
    Arm arms_[2];
    V3 kissDelta_{0, 0, 0};              // the lip front's move under the kiss morph (head frame)
    std::vector<V3> faceP_, faceN_;      // face surface samples around mouth and nose (head frame)
    bool ready_ = false;
};

}  // namespace ag
