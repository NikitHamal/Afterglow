#include "scene/touch.h"
#include "scene/character.h"
#include "core/log.h"
#include <algorithm>
#include <cmath>

namespace ag {

namespace {
V3 P(const Animator& a, int n) { return a.global(n).translation(); }

// rotation taking frame (f0, n0) onto (f1, n1); both pairs orthonormalised here
Quat alignFrames(V3 f0, V3 n0, V3 f1, V3 n1) {
    f0 = normalize(f0); n0 = normalize(n0 - f0 * dot(n0, f0)); V3 b0 = cross(f0, n0);
    f1 = normalize(f1); n1 = normalize(n1 - f1 * dot(n1, f1)); V3 b1 = cross(f1, n1);
    M4 r = M4::identity();
    const V3 c0[3] = {f0, n0, b0}, c1[3] = {f1, n1, b1};
    for (int col = 0; col < 3; ++col)
        for (int row = 0; row < 3; ++row) {
            float v = 0.f;
            for (int i = 0; i < 3; ++i) {
                const float a1[3] = {c1[i].x, c1[i].y, c1[i].z}, a0[3] = {c0[i].x, c0[i].y, c0[i].z};
                v += a1[row] * a0[col];
            }
            r.m[col * 4 + row] = v;
        }
    return qnormalize(quatFromM3(r));
}

void rotateWorld(Animator& a, int n, const Quat& q) {
    if (n < 0) return;
    a.setWorldRot(n, qnormalize(qmul(q, a.worldRot(n))));
    a.recomputeSubtree(n);
}
}  // namespace

void TouchRig::init(Character& c) {
    ready_ = false;
    Model& m = c.model;
    Animator& a = c.anim;
    a.begin();
    a.computeGlobals(M4::identity());
    auto node = [&](const std::string& n) { return m.findNode(n); };

    // ---- vulva: the vertices the vag_open target moves, at rest ----
    if (const auto* targets = c.morphTargets("vag_open")) {
        V3 sum{0, 0, 0}, nsum{0, 0, 0};
        double cnt = 0;
        for (const auto& mt : *targets) {
            for (const Primitive& pr : m.meshes[(size_t)mt.first].prims) {
                if (mt.second >= (int)pr.targets.size() || pr.pos.empty()) continue;
                for (uint32_t v : pr.targets[(size_t)mt.second].idx) {
                    sum += V3{pr.pos[v * 3], pr.pos[v * 3 + 1], pr.pos[v * 3 + 2]};
                    if (!pr.nrm.empty()) nsum += V3{pr.nrm[v * 3], pr.nrm[v * 3 + 1], pr.nrm[v * 3 + 2]};
                    cnt += 1.0;
                }
            }
        }
        int bone = node("下半身");
        if (cnt > 0 && bone >= 0) {
            V3 p = sum * (float)(1.0 / cnt);
            // the opening's normals partly cancel: lean on the direction out from the pelvis
            V3 out = p - a.global(bone).translation();
            V3 n = normalize((length(out) > 1e-4f ? normalize(out) : V3{0, -0.5f, 0.86f}) +
                             (length(nsum) > 1e-6f ? normalize(nsum) * 0.35f : V3{0, 0, 0}));
            M4 inv = inverse(a.global(bone));
            zones_[(int)Zone::Vulva] = {bone, xformPoint(inv, p), normalize(xformDir(inv, n)), true};
        }
    }
    // ---- breasts: the most forward skin weighted to each breast bone (the nipple), at rest ----
    for (int side = 0; side < 2; ++side) {
        int bone = node(side == 0 ? "乳親.L" : "乳親.R");
        if (bone < 0 || m.skins.empty()) continue;
        int joint = -1;
        for (size_t j = 0; j < m.skins[0].joints.size(); ++j) if (m.skins[0].joints[j] == bone) joint = (int)j;
        if (joint < 0) continue;
        struct Cand { V3 p, n; float w; };
        std::vector<Cand> cands;
        float maxW = 0.f;
        for (const Mesh& me : m.meshes)
            for (const Primitive& pr : me.prims) {
                if (pr.joints.empty() || pr.weights.empty() || pr.pos.empty()) continue;
                size_t nv = pr.pos.size() / 3;
                for (size_t v = 0; v < nv; ++v) {
                    float w = 0.f;
                    for (int k = 0; k < 4; ++k) if (pr.joints[v * 4 + k] == joint) w += pr.weights[v * 4 + k];
                    if (w <= 0.f) continue;
                    maxW = std::max(maxW, w);
                    cands.push_back({V3{pr.pos[v * 3], pr.pos[v * 3 + 1], pr.pos[v * 3 + 2]},
                                     pr.nrm.empty() ? V3{0, 0, 1} : V3{pr.nrm[v * 3], pr.nrm[v * 3 + 1], pr.nrm[v * 3 + 2]}, w});
                }
            }
        // the breast proper: skin weighted to the bone at least 60 % as strongly as its strongest vertex
        cands.erase(std::remove_if(cands.begin(), cands.end(), [&](const Cand& c) { return c.w < std::max(0.12f, maxW * 0.6f); }), cands.end());
        if (cands.empty()) continue;
        float best = -1e9f;
        for (auto& cd : cands) best = std::max(best, cd.p.z);
        V3 sum{0, 0, 0}, nsum{0, 0, 0};
        int cnt = 0;
        for (auto& cd : cands) if (cd.p.z > best - 0.004f) { sum += cd.p; nsum += cd.n; ++cnt; }
        V3 p = sum * (1.f / (float)cnt), n = length(nsum) > 1e-6f ? normalize(nsum) : V3{0, 0, 1};
        M4 inv = inverse(a.global(bone));
        zones_[side == 0 ? (int)Zone::BreastL : (int)Zone::BreastR] = {bone, xformPoint(inv, p), normalize(xformDir(inv, n)), true};
    }
    // ---- arms ----
    const char* fing[4] = {"人指", "中指", "薬指", "小指"};
    const char* digits[4] = {"０", "１", "２", "３"};
    for (int s = 0; s < 2; ++s) {
        std::string sx = s == 0 ? ".L" : ".R";
        Arm& arm = arms_[s];
        arm.upper = node("腕" + sx); arm.elbow = node("ひじ" + sx); arm.wrist = node("手首" + sx);
        arm.middle0 = node(std::string("中指") + digits[1] + sx);
        arm.index1 = node(std::string("人指") + digits[1] + sx);
        arm.pinky1 = node(std::string("小指") + digits[1] + sx);
        arm.side = s == 0 ? -1.f : 1.f;
        for (int f = 0; f < 4; ++f) {
            arm.fingers[f].clear();
            for (int d = 1; d <= 3; ++d) { int b = node(std::string(fing[f]) + digits[d] + sx); if (b >= 0) arm.fingers[f].push_back(b); }
        }
        int tip = node(std::string("中指") + digits[3] + sx), mid2 = node(std::string("中指") + digits[2] + sx);
        if (arm.wrist >= 0 && tip >= 0) {
            float last = mid2 >= 0 ? length(P(a, tip) - P(a, mid2)) : 0.02f;
            arm.handLen = length(P(a, tip) - P(a, arm.wrist)) + last * 0.8f;
        }
        // skin joints of this hand (wrist + all finger bones) never get dented by their own contacts
        if (!m.skins.empty()) {
            int lo = 1 << 30, hi = -1;
            for (size_t j = 0; j < m.skins[0].joints.size(); ++j) {
                const std::string& nm = m.nodes[(size_t)m.skins[0].joints[j]].name;
                if (nm.size() < sx.size() || nm.compare(nm.size() - sx.size(), sx.size(), sx) != 0) continue;
                bool hand = nm.rfind("手首", 0) == 0 || nm.rfind("親指", 0) == 0;
                for (const char* f : fing) hand = hand || nm.rfind(f, 0) == 0;
                if (!hand) continue;
                lo = std::min(lo, (int)j); hi = std::max(hi, (int)j);
            }
            handJointLo[s] = hi >= 0 ? lo : -1; handJointHi[s] = hi;
        }
    }
    ready_ = zones_[(int)Zone::Vulva].valid || zones_[(int)Zone::BreastL].valid;
    AG_INFO("AG: touch: vulva %s, breasts %s/%s, hand joints L %d-%d R %d-%d", zones_[0].valid ? "yes" : "no",
            zones_[1].valid ? "yes" : "no", zones_[2].valid ? "yes" : "no", handJointLo[0], handJointHi[0], handJointLo[1], handJointHi[1]);
}

bool TouchRig::zoneWorld(const Animator& a, Zone z, V3& pos, V3& normal) const {
    const BodyZone& bz = zones_[(int)z];
    if (!bz.valid) return false;
    const M4& g = a.global(bz.bone);
    pos = xformPoint(g, bz.local);
    normal = normalize(xformDir(g, bz.normalLocal));
    return true;
}

void TouchRig::apply(Animator& a, const TouchGoals& goals) {
    contacts.clear();
    if (!ready_) return;
    const HandGoal* gs[2] = {&goals.left, &goals.right};
    for (int s = 0; s < 2; ++s)
        if (gs[s]->active && gs[s]->weight > 0.001f && arms_[s].upper >= 0 && arms_[s].elbow >= 0 && arms_[s].wrist >= 0)
            solveArm(a, arms_[s], *gs[s]);
}

void TouchRig::solveArm(Animator& a, const Arm& arm, const HandGoal& g) {
    V3 zp, zn;
    if (!zoneWorld(a, g.zone, zp, zn)) return;
    const Model* m = a.model();
    int chest = m->findNode("上半身2"), hips = m->findNode("下半身");
    V3 up = chest >= 0 && hips >= 0 ? normalize(P(a, chest) - P(a, hips)) : V3{0, 1, 0};
    bool breast = g.zone != Zone::Vulva;
    // fingers lie along the surface: toward the vulva from the belly; up over the breast toward the collarbone
    V3 along = breast ? up : up * -1.f;
    V3 f = along - zn * dot(along, zn);
    f = length(f) > 1e-4f ? normalize(f) : normalize(cross(zn, V3{1, 0, 0}));
    V3 lat = normalize(cross(zn, f));
    float ph = g.phase * 6.2831853f;
    // fingertip pads: on the skin, sunk by `press`, tracing small circles (rub) or squeezing (knead)
    V3 tip = zp + f * (std::sin(ph) * g.amplitude) + lat * (std::cos(ph) * g.amplitude * 0.6f) + zn * (0.007f - g.press);
    if (breast) tip = zp + zn * (0.006f - g.press) + f * (0.035f + 0.008f * std::sin(ph));   // fingertips beyond the nipple
    V3 wristT = breast ? zp + zn * 0.05f - f * 0.055f + lat * (0.01f * std::cos(ph))
                       : tip - f * (arm.handLen * 0.9f) + zn * 0.035f;
    float w = clampf(g.weight, 0.f, 1.f);

    // ---- two-bone IK: shoulder -> elbow -> wrist, keeping the clip's bend plane ----
    V3 S = P(a, arm.upper), E = P(a, arm.elbow), W = P(a, arm.wrist);
    float L1 = length(E - S), L2 = length(W - E);
    V3 T = lerp(W, wristT, w);
    V3 st = T - S;
    float d = length(st);
    if (d < 1e-4f || L1 < 1e-4f || L2 < 1e-4f) return;
    float maxReach = (L1 + L2) * 0.999f;
    if (d > maxReach) { T = S + st * (maxReach / d); st = T - S; d = maxReach; }
    V3 dir = st / d;
    V3 pole = (E - S) - dir * dot(E - S, dir);
    if (length(pole) < 1e-3f) pole = (up * -0.6f) + cross(dir, up) * arm.side;
    pole = normalize(pole);
    float x = (L1 * L1 - L2 * L2 + d * d) / (2.f * d);
    float h = std::sqrt(std::max(0.f, L1 * L1 - x * x));
    V3 E2 = S + dir * x + pole * h;
    rotateWorld(a, arm.upper, qfromto(normalize(E - S), normalize(E2 - S)));
    V3 Ecur = P(a, arm.elbow), Wcur = P(a, arm.wrist);
    rotateWorld(a, arm.elbow, qfromto(normalize(Wcur - Ecur), normalize(T - Ecur)));

    // ---- hand: palm faces the body, fingers point along the surface toward the contact ----
    if (arm.middle0 >= 0 && arm.index1 >= 0 && arm.pinky1 >= 0) {
        V3 Wn = P(a, arm.wrist);
        V3 f0 = normalize(P(a, arm.middle0) - Wn);
        V3 l0 = normalize(P(a, arm.pinky1) - P(a, arm.index1));
        V3 n0 = normalize(cross(f0, l0)) * arm.side;
        V3 f1 = normalize(tip - Wn);
        V3 n1 = zn * -1.f;
        Quat r = alignFrames(f0, n0, f1, n1);
        rotateWorld(a, arm.wrist, qslerp(Quat{}, r, w));
        // fingers conform: curl toward the palm, more for a grab
        V3 fw = normalize(P(a, arm.middle0) - P(a, arm.wrist));
        V3 lw = normalize(P(a, arm.pinky1) - P(a, arm.index1));
        V3 palm = normalize(cross(fw, lw)) * arm.side;
        V3 axis = normalize(cross(fw, palm));
        float curl = g.curl * w;
        for (int fi = 0; fi < 4; ++fi)
            for (size_t k = 0; k < arm.fingers[fi].size(); ++k)
                rotateWorld(a, arm.fingers[fi][k], qaxis(axis, curl * (k == 0 ? 0.7f : 1.f)));
        // contact spheres at the finger pads (+ the palm for a grab)
        for (int fi = 0; fi < 3; ++fi) {
            const std::vector<int>& ch = arm.fingers[fi];
            if (ch.size() < 2) continue;
            V3 a2 = P(a, ch[ch.size() - 2]), a3 = P(a, ch.back());
            V3 pad = a3 + normalize(a3 - a2) * 0.011f - palm * 0.004f;
            contacts.push_back({pad, 0.0095f});
        }
        if (breast) contacts.push_back({P(a, arm.middle0) - palm * 0.012f, 0.026f});
    }
}

}  // namespace ag
