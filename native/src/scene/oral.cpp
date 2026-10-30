// Afterglow scene — see oral.h. Everything here is measured on the final posed skeleton, so it holds for whatever
// pose the clip or the player put the bodies in.
#include "scene/oral.h"
#include "scene/character.h"
#include "scene/touch.h"
#include <algorithm>
#include <cmath>

namespace ag {

namespace {
float clamp01f(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
// the visible glans tip in the male's own bind frame, walked forward by the length morph
constexpr float kTipBase = 0.08832f, kTipTravel = 0.0288f, kTipBack = 0.00583f;
}  // namespace

bool oralShaft(const Character& him, V3& root, V3& chord, V3& tip, V3& axis) {
    if (!him.loaded()) return false;
    float lenUp = clampf(him.morph("length_up"), 0.f, 1.f);
    root = him.bonePos("竿根");
    tip = xformPoint(him.boneWorld("竿先"), V3{0.f, kTipBase + kTipTravel * lenUp, -kTipBack});
    chord = tip - root;
    float len = length(chord);
    if (len <= 1e-3f) return false;
    chord = chord * (1.f / len);
    // the glans points along its own bone, not along the chord: with him curved, a mouth aimed at the chord ends up
    // beside the shaft rather than on it
    axis = normalize(xformDir(him.boneWorld("竿先"), V3{0.f, 1.f, 0.f}));
    if (dot(axis, chord) < 0.2f) axis = chord;
    return true;
}

void stageOral(Character& her, Character& him, float depth, const OralTuning& t, bool active) {
    her.oralW = 0.f;
    if (!her.loaded()) return;
    float girthUp = him.loaded() ? clampf(him.morph("girth_up"), 0.f, 1.f) : 0.f;
    if (her.hasMorph("oral")) her.setMorph("oral", active ? std::max(1.f, t.open) + 0.35f * girthUp : 0.f);
    if (!active) return;

    V3 root, chord, tip, axis;
    if (!oralShaft(him, root, chord, tip, axis)) return;
    // The lip ring's centre rides the shaft. The max-size glans is ~42 mm long, so the shallow end already sits
    // past it: short of that the glans clears the lips at the bottom of every stroke and rests on her nose.
    her.oralTarget = tip - axis * (0.050f + 0.040f * clamp01f(depth));
    if (std::fabs(t.up) > 1e-5f) {
        V3 headUp = qrotate(her.anim.worldRot(her.bone("頭")), her.procedural.headUpLocal());
        her.oralTarget = her.oralTarget + headUp * t.up;
    }
    her.oralFace = axis * -1.f;
    her.oralW = 1.f;
    her.oralCurl = t.grip ? 0.f : t.curl;

    if (!t.grip && !her.postHook && (std::fabs(t.wristL) > 1e-4f || std::fabs(t.wristR) > 1e-4f)) {
        float l = t.wristL, r = t.wristR;
        her.postHook = [l, r](Animator& a) {   // QA knob: pronate her wrists about the forearm
            for (int s = 0; s < 2; ++s) {
                float ang = s ? r : l;
                if (std::fabs(ang) < 1e-4f) continue;
                std::string side = s ? ".R" : ".L";
                int w = a.model()->findNode("手首" + side), e = a.model()->findNode("ひじ" + side);
                if (w < 0 || e < 0) continue;
                V3 ax = a.global(w).translation() - a.global(e).translation();
                if (length(ax) < 1e-4f) continue;
                a.setWorldRot(w, qnormalize(qmul(qaxis(normalize(ax), ang), a.worldRot(w))));
                a.recomputeSubtree(w);
            }
        };
    }
}

void oralGrip(Character& her, Character& him, TouchGoals& tg, const OralTuning& t) {
    if (!t.grip) return;
    V3 root, chord, tip, axis;
    if (!oralShaft(him, root, chord, tip, axis)) return;
    if (!her.loaded()) return;
    V3 ax = chord;   // the hands sit low on the shaft, where the chord is the shaft
    float shaft = length(tip - root);
    float girthUp = clampf(him.morph("girth_up"), 0.f, 1.f);
    auto grip = [&](HandGoal& g, const char* side, float along, float elbowSide) {
        if (g.active) return;   // a hand already somewhere else keeps it
        V3 shoulder = her.bonePos(std::string("腕") + side);
        V3 at = root + ax * along;
        V3 out = shoulder - at;
        out = out - ax * dot(out, ax);
        if (length(out) < 1e-3f) out = V3{elbowSide, 0.f, 0.f};
        out = normalize(out);
        // Palm to the shaft, fingers running around it: lengthwise fingers read as a claw sliding up the shaft.
        V3 toFace = her.bonePos("頭") - at;
        toFace = toFace - ax * dot(toFace, ax);
        V3 tangent = cross(ax, out);
        if (length(tangent) < 1e-3f) tangent = cross(ax, V3{1.f, 0.f, 0.f});
        tangent = normalize(tangent);
        if (dot(tangent, toFace) < 0.f) tangent = tangent * -1.f;
        g.active = true;
        g.world = true;
        g.weight = 1.f;
        g.pos = at + out * (0.031f + 0.004f * girthUp);
        g.normal = out;
        g.along = tangent;
        g.flat = true;
        g.palmLift = 0.012f;
        g.curl = 0.f;                                  // wrapFingers() closes them onto the cylinder instead
        g.press = 0.f;
        g.dent = false;
        g.wrap = true;
        g.wrapAxis = ax;
        g.wrapDir = tangent;
        g.wrapRef = root;
        g.elbowHint = out * 0.65f + V3{0.f, 0.15f, 0.f};
    };
    grip(tg.left, ".L", shaft * 0.27f, -1.f);
    grip(tg.right, ".R", shaft * 0.40f, 1.f);
}

}  // namespace ag
