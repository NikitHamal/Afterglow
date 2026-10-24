#include "anim/stress.h"
#include <algorithm>
#include <cmath>

namespace ag {

namespace {

constexpr float kDeg = kPi / 180.f;

struct Ctx {
    Animator& a;
    const Model& m;
    Quat rootRot;
    float k;   // amount 0..1
};

Quat rotOf(const M4& g) {
    M4 r = M4::identity();
    for (int c = 0; c < 3; ++c) {
        V3 col = normalize(g.col3(c));
        r.m[c * 4] = col.x; r.m[c * 4 + 1] = col.y; r.m[c * 4 + 2] = col.z;
    }
    return quatFromM3(r);
}

// world rotation of node n in the rest pose (model space, then the character's root rotation)
Quat restWorld(const Ctx& c, int n) {
    Quat q;
    std::vector<int> chain;
    for (int k = n; k >= 0; k = c.m.nodes[(size_t)k].parent) chain.push_back(k);
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) q = qmul(q, c.a.restR(*it));
    return qmul(c.rootRot, q);
}

V3 bodyDir(const Ctx& c, const V3& d) { return normalize(qrotate(c.rootRot, normalize(d))); }
int node(const Ctx& c, const std::string& bone) { return c.m.findNode(bone); }
V3 axisOf(const Ctx& c, int n) { return normalize(qrotate(c.a.worldRot(n), V3{0, 1, 0})); }

// how node n's frame turned since the rest pose, applied to a body-space direction (e.g. the limb's front)
V3 carried(const Ctx& c, int n, const V3& dirBody) {
    Quat since = qmul(c.a.worldRot(n), qconj(restWorld(c, n)));
    return normalize(qrotate(since, bodyDir(c, dirBody)));
}

void setWorld(Ctx& c, int n, const Quat& target) {
    Quat cur = c.a.worldRot(n);
    c.a.setWorldRot(n, qslerp(cur, qnormalize(target), c.k));
    c.a.recomputeSubtree(n);
}

// bone axis (+Y) onto a world direction by the minimal rotation, then a twist about the new axis
void aimWorld(Ctx& c, int n, const V3& want, float twistDeg = 0.f) {
    if (n < 0) return;
    Quat w = c.a.worldRot(n);
    Quat q = qmul(qfromto(axisOf(c, n), normalize(want)), w);
    if (twistDeg != 0.f) q = qmul(qaxis(normalize(want), twistDeg * kDeg), q);
    setWorld(c, n, q);
}
void aim(Ctx& c, const std::string& bone, const V3& dirBody, float twistDeg = 0.f) { aimWorld(c, node(c, bone), bodyDir(c, dirBody), twistDeg); }

// rotate a bone about a body-space axis
void turn(Ctx& c, const std::string& bone, const V3& axisBody, float deg) {
    int n = node(c, bone);
    if (n < 0) return;
    setWorld(c, n, qmul(qaxis(bodyDir(c, axisBody), deg * kDeg), c.a.worldRot(n)));
}

// about the bone's own axis (pronation / supination, humeral rotation)
void twist(Ctx& c, const std::string& bone, float deg) {
    int n = node(c, bone);
    if (n < 0) return;
    setWorld(c, n, qmul(qaxis(axisOf(c, n), deg * kDeg), c.a.worldRot(n)));
}

// hinge flexion: the child segment turns `deg` away from its parent's axis toward the parent's front (elbow) or back
// (knee); the parent's front is the body front it carried from the rest pose
void flex(Ctx& c, const std::string& parent, const std::string& hinge, float deg, bool towardFront) {
    int p = node(c, parent), h = node(c, hinge);
    if (p < 0 || h < 0) return;
    V3 u = axisOf(c, p);
    V3 front = carried(c, p, V3{0, 0, 1});
    V3 a = normalize(front - u * dot(front, u));
    if (!towardFront) a = a * -1.f;
    aimWorld(c, h, u * std::cos(deg * kDeg) + a * std::sin(deg * kDeg));
}

// keep the feet on the floor: lower the body by how far the lower ankle rose above its rest height
void plant(Ctx& c) {
    int ctr = node(c, "\xe3\x82\xbb\xe3\x83\xb3\xe3\x82\xbf\xe3\x83\xbc");   // センター
    int fl = node(c, "\xe8\xb6\xb3\xe9\xa6\x96.L"), fr = node(c, "\xe8\xb6\xb3\xe9\xa6\x96.R");   // 足首.L / .R
    if (ctr < 0 || fl < 0 || fr < 0) return;
    // rest ankle height (model space): chain of rest transforms
    auto restY = [&](int n) {
        M4 g = M4::identity();
        std::vector<int> chain;
        for (int k = n; k >= 0; k = c.m.nodes[(size_t)k].parent) chain.push_back(k);
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) g = g * m4trs(c.a.restT(*it), c.a.restR(*it), V3{1, 1, 1});
        return g.translation().y;
    };
    float rest = std::min(restY(fl), restY(fr));
    float base = c.a.rootMatrix().translation().y;
    float now = std::min(c.a.global(fl).translation().y, c.a.global(fr).translation().y) - base;
    float drop = now - rest;
    if (std::fabs(drop) < 1e-4f) return;
    c.a.t(ctr).y -= drop;   // センター sits under 全ての親 (identity frame): body-space height
    c.a.recomputeSubtree(ctr);
}

const std::string kArm = "\xe8\x85\x95", kElbow = "\xe3\x81\xb2\xe3\x81\x98", kWrist = "\xe6\x89\x8b\xe9\xa6\x96";   // 腕 ひじ 手首
const std::string kLeg = "\xe8\xb6\xb3", kKnee = "\xe3\x81\xb2\xe3\x81\x96", kAnkle = "\xe8\xb6\xb3\xe9\xa6\x96";   // 足 ひざ 足首
const std::string kSpine = "\xe4\xb8\x8a\xe5\x8d\x8a\xe8\xba\xab", kChest = "\xe4\xb8\x8a\xe5\x8d\x8a\xe8\xba\xab" "2";   // 上半身 上半身2
const std::string kNeck = "\xe9\xa6\x96", kHead = "\xe9\xa0\xad";   // 首 頭

std::string R(const std::string& b) { return b + ".R"; }

// arms hanging by the sides (from the A-pose rest), the default for poses that do not use them
void armsDown(Ctx& c) {
    for (int s : {1, -1}) {
        const std::string sd = s > 0 ? ".L" : ".R";
        aim(c, kArm + sd, V3{0.22f * (float)s, -1.f, 0.04f});
        flex(c, kArm + sd, kElbow + sd, 12.f, true);
    }
}

}  // namespace

const std::vector<std::string>& stressPoseNames() {
    static const std::vector<std::string> names = {"arms_up", "arms_cross", "elbows", "wrists", "squat", "knee",
                                                   "split", "twist", "neck", "behind"};
    return names;
}

bool stressPose(Animator& a, const std::string& name, float amount) {
    const Model* m = a.model();
    if (!m) return false;
    Ctx c{a, *m, rotOf(a.rootMatrix()), amount < 0.f ? 0.f : (amount > 1.f ? 1.f : amount)};
    if (name == "arms_up") {
        for (int s : {1, -1}) {
            const std::string sd = s > 0 ? ".L" : ".R";
            aim(c, kArm + sd, V3{0.24f * (float)s, 1.f, 0.08f});
            flex(c, kArm + sd, kElbow + sd, 12.f, true);
        }
    } else if (name == "arms_cross") {
        for (int s : {1, -1}) {
            const std::string sd = s > 0 ? ".L" : ".R";
            aim(c, kArm + sd, V3{-0.32f * (float)s, -0.3f, 0.9f});
            flex(c, kArm + sd, kElbow + sd, 112.f, true);
        }
    } else if (name == "elbows") {
        for (int s : {1, -1}) {
            const std::string sd = s > 0 ? ".L" : ".R";
            aim(c, kArm + sd, V3{0.2f * (float)s, -0.62f, 0.76f});
            flex(c, kArm + sd, kElbow + sd, 148.f, true);
        }
    } else if (name == "wrists") {
        for (int s : {1, -1}) {
            const std::string sd = s > 0 ? ".L" : ".R";
            aim(c, kArm + sd, V3{0.18f * (float)s, -0.35f, 0.92f});
            flex(c, kArm + sd, kElbow + sd, 35.f, true);
            twist(c, kWrist + sd, 80.f * (float)s);           // one hand pronated, the other supinated
            turn(c, kWrist + sd, V3{1.f, 0.f, 0.f}, 45.f);    // and flexed toward the floor
        }
    } else if (name == "squat") {
        for (int s : {1, -1}) {
            const std::string sd = s > 0 ? ".L" : ".R";
            aim(c, kLeg + sd, V3{0.42f * (float)s, 0.02f, 0.9f});
            flex(c, kLeg + sd, kKnee + sd, 128.f, false);
            aim(c, kAnkle + sd, V3{0.1f * (float)s, -0.35f, 0.94f});
            aim(c, kArm + sd, V3{0.12f * (float)s, -0.15f, 1.f});
            flex(c, kArm + sd, kElbow + sd, 25.f, true);
        }
        turn(c, kSpine, V3{1.f, 0.f, 0.f}, 28.f);   // lean forward over the knees
        plant(c);
    } else if (name == "knee") {
        for (int s : {1, -1}) {                         // arms out for balance, clear of the raised leg
            const std::string sd = s > 0 ? ".L" : ".R";
            aim(c, kArm + sd, V3{0.75f * (float)s, -0.35f, 0.45f});
            flex(c, kArm + sd, kElbow + sd, 20.f, true);
        }
        aim(c, R(kLeg), V3{-0.08f, 0.18f, 1.f});      // right thigh up past horizontal
        flex(c, R(kLeg), R(kKnee), 148.f, false);      // heel to the back of the thigh
        aim(c, R(kAnkle), V3{0.f, 0.3f, -0.95f});
    } else if (name == "split") {
        armsDown(c);
        for (int s : {1, -1}) {
            const std::string sd = s > 0 ? ".L" : ".R";
            aim(c, kLeg + sd, V3{0.82f * (float)s, -0.56f, 0.14f});
            flex(c, kLeg + sd, kKnee + sd, 4.f, false);
            aim(c, kAnkle + sd, V3{0.35f * (float)s, -0.4f, 0.85f});
        }
        plant(c);
    } else if (name == "twist") {
        armsDown(c);
        turn(c, kSpine, V3{0.f, 1.f, 0.f}, 34.f);
        turn(c, kSpine, V3{0.f, 0.f, 1.f}, 18.f);    // side bend
        turn(c, kChest, V3{0.f, 1.f, 0.f}, 16.f);
    } else if (name == "neck") {
        armsDown(c);
        turn(c, kNeck, V3{0.f, 1.f, 0.f}, 38.f);
        turn(c, kHead, V3{0.f, 1.f, 0.f}, 28.f);
        turn(c, kHead, V3{0.f, 0.f, 1.f}, 20.f);     // tilt toward the shoulder
        turn(c, kNeck, V3{1.f, 0.f, 0.f}, 14.f);     // chin a little down
    } else if (name == "behind") {
        for (int s : {1, -1}) {
            const std::string sd = s > 0 ? ".L" : ".R";
            aim(c, kArm + sd, V3{0.3f * (float)s, -0.74f, -0.6f});
            twist(c, kArm + sd, 55.f * (float)s);       // humerus turned in: forearms behind the back
            flex(c, kArm + sd, kElbow + sd, 96.f, true);
        }
    } else {
        return false;
    }
    return true;
}

}  // namespace ag
