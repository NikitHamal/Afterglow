#include "scene/touch.h"
#include "scene/character.h"
#include "core/log.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

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
    ready_ = arms_[0].wrist >= 0 || arms_[1].wrist >= 0;   // zones are optional (world goals work on anyone)
    // ---- where a partner's hands rest (embraces): the outermost trunk skin along a direction from a bone (bind
    // pose). Only skin weighted to the trunk counts: the arms and any surfaces inside the body never do ----
    std::vector<char> trunk;
    if (!m.skins.empty()) {
        trunk.assign(m.skins[0].joints.size(), 0);
        for (const char* b : {"センター", "グルーブ", "腰", "上半身", "上半身2", "下半身", "乳親.L", "乳親.R"}) {
            int nb = node(b);
            for (size_t j = 0; j < m.skins[0].joints.size(); ++j) if (m.skins[0].joints[j] == nb) trunk[j] = 1;
        }
    }
    auto anchor = [&](Zone z, const char* boneName, V3 offset, V3 dir) {
        int bone = node(boneName);
        if (bone < 0 || trunk.empty()) return;
        V3 o = a.global(bone).translation() + offset;
        float best = 0.03f;
        V3 bp{0, 0, 0}, bn{0, 0, 0};
        for (const Node& nd : m.nodes) {
            if (nd.mesh < 0 || nd.name.find("_body") == std::string::npos) continue;
            for (const Primitive& pr : m.meshes[(size_t)nd.mesh].prims) {
                if (pr.joints.empty() || pr.weights.empty() || pr.pos.empty() || pr.nrm.empty()) continue;
                size_t nv = pr.pos.size() / 3;
                for (size_t v = 0; v < nv; ++v) {
                    float tw = 0.f;
                    for (int k = 0; k < 4; ++k) if (pr.joints[v * 4 + k] < trunk.size() && trunk[pr.joints[v * 4 + k]]) tw += pr.weights[v * 4 + k];
                    if (tw < 0.9f) continue;
                    V3 rel = V3{pr.pos[v * 3], pr.pos[v * 3 + 1], pr.pos[v * 3 + 2]} - o;
                    float along = dot(rel, dir);
                    if (along <= best || length(rel - dir * along) > 0.02f) continue;
                    best = along;
                    bp = o + rel;
                    bn = V3{pr.nrm[v * 3], pr.nrm[v * 3 + 1], pr.nrm[v * 3 + 2]};
                }
            }
        }
        if (length(bn) < 0.5f) return;
        M4 inv = inverse(a.global(bone));
        zones_[(int)z] = {bone, xformPoint(inv, bp), normalize(xformDir(inv, bn)), true};
    };
    // ---- the face: lip front at the midline (the vertices the kiss pucker moves), the nose tip, the surface around ----
    int headBone = node("頭");
    const auto* kissT = c.morphTargets("kiss");
    if (headBone >= 0 && kissT) {
        struct LV { V3 p, d; };
        std::vector<LV> lip;
        float maxD = 0.f;
        int bodyMesh = -1;
        for (const auto& mt : *kissT) {
            bool body = false;
            for (const Node& nd : m.nodes) if (nd.mesh == mt.first && nd.name.find("_body") != std::string::npos) body = true;
            if (!body) continue;
            bodyMesh = mt.first;
            for (const Primitive& pr : m.meshes[(size_t)mt.first].prims) {
                if (mt.second >= (int)pr.targets.size() || pr.pos.empty()) continue;
                const Primitive::Sparse& sp = pr.targets[(size_t)mt.second];
                for (size_t k = 0; k < sp.idx.size(); ++k) {
                    uint32_t v = sp.idx[k];
                    V3 d{sp.dp[k * 3], sp.dp[k * 3 + 1], sp.dp[k * 3 + 2]};
                    maxD = std::max(maxD, length(d));
                    lip.push_back({V3{pr.pos[v * 3], pr.pos[v * 3 + 1], pr.pos[v * 3 + 2]}, d});
                }
            }
        }
        lip.erase(std::remove_if(lip.begin(), lip.end(), [&](const LV& l) { return length(l.d) < maxD * 0.4f; }), lip.end());
        if (!lip.empty() && bodyMesh >= 0) {
            float xc = 0.f;
            for (auto& l : lip) xc += l.p.x;
            xc /= (float)lip.size();
            float front = -1e9f, ys = 0.f;
            int ny = 0;
            for (auto& l : lip) if (std::fabs(l.p.x - xc) < 0.006f) { front = std::max(front, l.p.z); ys += l.p.y; ++ny; }
            if (ny > 0) {
                V3 dsum{0, 0, 0};
                int nd = 0;
                for (auto& l : lip) if (std::fabs(l.p.x - xc) < 0.006f && l.p.z > front - 0.004f) { dsum += l.d; ++nd; }
                V3 mp{xc, ys / (float)ny, front};
                M4 inv = inverse(a.global(headBone));
                zones_[(int)Zone::Mouth] = {headBone, xformPoint(inv, mp), normalize(xformDir(inv, V3{0, 0, 1})), true};
                kissDelta_ = xformDir(inv, nd > 0 ? dsum / (float)nd : V3{0, 0, 0});
                // the nose tip: the front-most skin on the midline between the mouth and the eyes; the face surface
                // around (front-facing skin within 9 cm of the mouth, ~4 mm apart) for clearance checks
                V3 nose{xc, mp.y, -1e9f};
                std::unordered_map<long long, int> seen;
                for (const Primitive& pr : m.meshes[(size_t)bodyMesh].prims) {
                    size_t nv = pr.pos.size() / 3;
                    for (size_t v = 0; v < nv; ++v) {
                        V3 p{pr.pos[v * 3], pr.pos[v * 3 + 1], pr.pos[v * 3 + 2]};
                        if (std::fabs(p.x - xc) < 0.004f && p.y > mp.y + 0.012f && p.y < mp.y + 0.06f && p.z > nose.z) nose = p;
                        V3 nr = pr.nrm.empty() ? V3{0, 0, 1} : V3{pr.nrm[v * 3], pr.nrm[v * 3 + 1], pr.nrm[v * 3 + 2]};
                        if (length(p - mp) > 0.09f || nr.z < 0.2f) continue;
                        long long key = ((long long)std::lround(p.x / 0.004f) * 1000003LL + std::lround(p.y / 0.004f)) * 1000003LL + std::lround(p.z / 0.004f);
                        if (!seen.emplace(key, 1).second) continue;
                        faceP_.push_back(xformPoint(inv, p));
                        faceN_.push_back(normalize(xformDir(inv, nr)));
                    }
                }
                if (nose.z > -1e8f) zones_[(int)Zone::Nose] = {headBone, xformPoint(inv, nose), normalize(xformDir(inv, V3{0, 0, 1})), true};
            }
        }
    }
    // bind pose faces +Z; the character's left (.L) is +X
    anchor(Zone::ChestL, "上半身2", V3{0.09f, 0.22f, 0.f}, V3{0.f, 0.f, 1.f});    // over the pectorals
    anchor(Zone::ChestR, "上半身2", V3{-0.09f, 0.22f, 0.f}, V3{0.f, 0.f, 1.f});
    anchor(Zone::WaistL, "下半身", V3{0.f, 0.10f, 0.f}, normalize(V3{1.f, 0.f, 0.15f}));    // the natural waist
    anchor(Zone::WaistR, "下半身", V3{0.f, 0.10f, 0.f}, normalize(V3{-1.f, 0.f, 0.15f}));
    anchor(Zone::Sternum, "上半身2", V3{0.f, 0.29f, 0.f}, V3{0.f, 0.f, 1.f});
    for (int z = 0; z < (int)Zone::Count; ++z) {   // the animator still holds the bind pose
        if (!zones_[z].valid) continue;
        bindPos_[z] = xformPoint(a.global(zones_[z].bone), zones_[z].local);
        bindNrm_[z] = normalize(xformDir(a.global(zones_[z].bone), zones_[z].normalLocal));
    }
    static const char* kZoneName[(int)Zone::Count] = {"vulva", "breast.L", "breast.R", "chest.L", "chest.R", "waist.L", "waist.R",
                                                      "sternum", "mouth", "nose"};
    std::string zs;
    for (int z = 0; z < (int)Zone::Count; ++z) if (zones_[z].valid) zs += std::string(zs.empty() ? "" : " ") + kZoneName[z];
    AG_INFO("AG: touch: zones %s, face %d pts, hand joints L %d-%d R %d-%d", zs.c_str(), (int)faceP_.size(), handJointLo[0], handJointHi[0],
            handJointLo[1], handJointHi[1]);
}

bool TouchRig::zoneBind(Zone z, V3& pos, V3& normal) const {
    if (!zones_[(int)z].valid) return false;
    pos = bindPos_[(int)z];
    normal = bindNrm_[(int)z];
    return true;
}

bool TouchRig::zoneWorld(const Animator& a, Zone z, V3& pos, V3& normal) const {
    const BodyZone& bz = zones_[(int)z];
    if (!bz.valid) return false;
    const M4& g = a.global(bz.bone);
    pos = xformPoint(g, bz.local);
    normal = normalize(xformDir(g, bz.normalLocal));
    return true;
}

bool TouchRig::mouthWorld(const Animator& a, float kissMorph, V3& pos) const {
    const BodyZone& bz = zones_[(int)Zone::Mouth];
    if (!bz.valid) return false;
    pos = xformPoint(a.global(bz.bone), bz.local + kissDelta_ * clampf(kissMorph, 0.f, 1.f));
    return true;
}

float TouchRig::faceDistance(const Animator& a, const V3& p) const {
    const BodyZone& bz = zones_[(int)Zone::Mouth];
    if (!bz.valid || faceP_.empty()) return 1.f;
    V3 q = xformPoint(inverse(a.global(bz.bone)), p);
    float best = 1e9f;
    size_t bi = 0;
    for (size_t i = 0; i < faceP_.size(); ++i) {
        V3 d = q - faceP_[i];
        float l = dot(d, d);
        if (l < best) { best = l; bi = i; }
    }
    float s = dot(q - faceP_[bi], faceN_[bi]);
    return s < 0.f ? s : std::sqrt(best);
}

void TouchRig::faceWorld(const Animator& a, std::vector<V3>& out) const {
    out.clear();
    const BodyZone& bz = zones_[(int)Zone::Mouth];
    if (!bz.valid) return;
    const M4& g = a.global(bz.bone);
    out.reserve(faceP_.size());
    for (const V3& p : faceP_) out.push_back(xformPoint(g, p));
}

void TouchRig::faceNormalsWorld(const Animator& a, std::vector<V3>& out) const {
    out.clear();
    const BodyZone& bz = zones_[(int)Zone::Mouth];
    if (!bz.valid) return;
    const M4& g = a.global(bz.bone);
    out.reserve(faceN_.size());
    for (const V3& n : faceN_) out.push_back(normalize(xformDir(g, n)));
}

void TouchRig::apply(Animator& a, const TouchGoals& goals, const JointLimits* rom) {
    rom_ = JointLimits::enabled ? rom : nullptr;
    contacts.clear();
    if (!ready_) return;
    const HandGoal* gs[2] = {&goals.left, &goals.right};
    for (int s = 0; s < 2; ++s)
        if (gs[s]->active && gs[s]->weight > 0.001f && arms_[s].upper >= 0 && arms_[s].elbow >= 0 && arms_[s].wrist >= 0)
            solveArm(a, arms_[s], *gs[s]);
}

// a hand laid on a partner, inside the joint ranges: the elbow bends about its own hinge only (the shoulder -> wrist
// distance fixes the angle), the whole arm then turns about the shoulder -> wrist line until the elbow points where the
// goal wants it, as far as the upper arm's twist range allows. The palm (solveArm) splits its turn between the forearm
// twist and the wrist inside their ranges. False when the elbow has no hinge range (the plain solve is used).
bool TouchRig::solveArmRanged(Animator& a, const Arm& arm, const V3& wristT, const V3& elbowHint, float weight) {
    JointRange er = rom_->range(arm.elbow);
    if (!er.valid || !er.hinge || arm.upper < 0 || arm.elbow < 0 || arm.wrist < 0) return false;
    const float kD = 0.0174533f;
    V3 S = P(a, arm.upper);
    V3 te = a.restT(arm.elbow), tw = a.restT(arm.wrist);
    float sc = length(P(a, arm.elbow) - S) / std::max(1e-5f, length(te));
    Quat re = a.restR(arm.elbow);
    auto reach = [&](float deg) { return length(te + qrotate(qmul(re, qaxis(er.axis, deg * kD)), tw)) * sc; };
    float d = length(wristT - S);
    float lo = er.hingeMin + 6.f, hi = er.hingeMax - 6.f;
    float best = lo, err = 1e9f;
    for (float p = lo; p <= hi; p += 1.f) { float e = std::fabs(reach(p) - d); if (e < err) { err = e; best = p; } }
    for (float st = 0.5f; st > 0.02f; st *= 0.5f)
        for (float p : {best - st, best + st}) {
            if (p < lo || p > hi) continue;
            float e = std::fabs(reach(p) - d);
            if (e < err) { err = e; best = p; }
        }
    a.r(arm.elbow) = qnormalize(qmul(re, qaxis(er.axis, best * kD)));   // pure hinge (the palm adds the twist)
    a.recomputeSubtree(arm.elbow);
    V3 ax = wristT - S;
    if (length(ax) < 1e-4f) return true;
    ax = normalize(ax);
    rotateWorld(a, arm.upper, qfromto(normalize(P(a, arm.wrist) - S), ax));
    V3 e0 = P(a, arm.elbow) - S, h = elbowHint;
    e0 = e0 - ax * dot(e0, ax); h = h - ax * dot(h, ax);
    if (length(e0) > 1e-4f && length(h) > 1e-4f) {
        e0 = normalize(e0); h = normalize(h);
        float ang = std::atan2(dot(cross(e0, h), ax), dot(e0, h)) * weight;
        Quat baseW = a.worldRot(arm.upper);
        for (float f : {1.f, 0.85f, 0.7f, 0.55f, 0.4f, 0.25f, 0.1f, 0.f}) {   // the upper arm's twist stays in range
            a.setWorldRot(arm.upper, qnormalize(qmul(qaxis(ax, ang * f), baseW)));
            if (rom_->inRange(arm.upper, a.restR(arm.upper), a.r(arm.upper))) break;
        }
        a.recomputeSubtree(arm.upper);
    }
    rom_->clampNode(a, arm.upper);   // anything left over (the swing cone)
    return true;
}

void TouchRig::solveArm(Animator& a, const Arm& arm, const HandGoal& g) {
    V3 zp, zn;
    if (g.world) { zp = g.pos; zn = normalize(g.normal); }
    else if (!zoneWorld(a, g.zone, zp, zn)) return;
    const Model* m = a.model();
    int chest = m->findNode("上半身2"), hips = m->findNode("下半身");
    V3 up = chest >= 0 && hips >= 0 ? normalize(P(a, chest) - P(a, hips)) : V3{0, 1, 0};
    bool breast = g.world || g.zone != Zone::Vulva;   // a resting palm: fingertips a little beyond the point
    // fingers lie along the surface: toward the vulva from the belly; up over the breast toward the collarbone
    V3 along = g.world ? g.along : (breast ? up : up * -1.f);
    V3 f = along - zn * dot(along, zn);
    f = length(f) > 1e-4f ? normalize(f) : normalize(cross(zn, V3{1, 0, 0}));
    V3 lat = normalize(cross(zn, f));
    float ph = g.phase * 6.2831853f;
    // fingertip pads: on the skin, sunk by `press`, tracing small circles (rub) or squeezing (knead)
    V3 tip = zp + f * (std::sin(ph) * g.amplitude) + lat * (std::cos(ph) * g.amplitude * 0.6f) + zn * (0.007f - g.press);
    if (breast) tip = zp + zn * (0.006f - g.press) + f * (0.035f + 0.008f * std::sin(ph));   // fingertips beyond the nipple
    V3 wristT = breast ? zp + zn * 0.05f - f * 0.055f + lat * (0.01f * std::cos(ph))
                       : tip - f * (arm.handLen * 0.9f) + zn * 0.035f;
    if (g.world && g.flat) {   // a palm laid flat, centred on the point, fingertips resting on the surface
        tip = zp + f * (arm.handLen * 0.55f) + zn * 0.004f;
        wristT = zp - f * (arm.handLen * 0.42f) + zn * g.palmLift;
    }
    float w = clampf(g.weight, 0.f, 1.f);

    // ---- two-bone IK: shoulder -> elbow -> wrist ----
    V3 S = P(a, arm.upper), E = P(a, arm.elbow), W = P(a, arm.wrist);
    bool ranged = false;
    if (g.world && rom_) {
        V3 hint = length(g.elbowHint) > 0.5f ? g.elbowHint : E - (S + W) * 0.5f;   // none: the clip's elbow
        ranged = solveArmRanged(a, arm, lerp(W, wristT, clampf(g.weight, 0.f, 1.f)), hint, clampf(g.weight, 0.f, 1.f));
    }
    if (!ranged) {   // ... keeping the clip's bend plane (zone goals, or no ranges)
        float L1 = length(E - S), L2 = length(W - E);
        V3 T = lerp(W, wristT, w);
        V3 st = T - S;
        float d = length(st);
        if (d < 1e-4f || L1 < 1e-4f || L2 < 1e-4f) return;
        float maxReach = (L1 + L2) * 0.999f;
        if (d > maxReach) { T = S + st * (maxReach / d); st = T - S; d = maxReach; }
        V3 dir = st / d;
        V3 pole = (E - S) - dir * dot(E - S, dir);
        if (length(g.elbowHint) > 0.5f) pole = g.elbowHint - dir * dot(g.elbowHint, dir);   // where the goal wants the elbow
        if (length(pole) < 1e-3f) pole = (up * -0.6f) + cross(dir, up) * arm.side;
        pole = normalize(pole);
        float x = (L1 * L1 - L2 * L2 + d * d) / (2.f * d);
        float h = std::sqrt(std::max(0.f, L1 * L1 - x * x));
        V3 E2 = S + dir * x + pole * h;
        rotateWorld(a, arm.upper, qfromto(normalize(E - S), normalize(E2 - S)));
        V3 Ecur = P(a, arm.elbow), Wcur = P(a, arm.wrist);
        rotateWorld(a, arm.elbow, qfromto(normalize(Wcur - Ecur), normalize(T - Ecur)));
    }

    // ---- hand: palm faces the body, fingers point along the surface toward the contact ----
    if (arm.middle0 >= 0 && arm.index1 >= 0 && arm.pinky1 >= 0) {
        V3 Wn = P(a, arm.wrist);
        V3 f0 = normalize(P(a, arm.middle0) - Wn);
        V3 l0 = normalize(P(a, arm.pinky1) - P(a, arm.index1));
        V3 n0 = normalize(cross(f0, l0)) * arm.side;
        V3 f1 = normalize(tip - Wn);
        V3 n1 = zn * -1.f;
        Quat r = qslerp(Quat{}, alignFrames(f0, n0, f1, n1), w);
        // turning the palm (pronation / supination) is the forearm's: 60 % of the twist about the forearm axis turns
        // the whole forearm from the elbow, the wrist bends and takes the rest (no candy-wrapper collapse at the wrist)
        V3 fa = normalize(Wn - P(a, arm.elbow));
        float tw = 2.f * std::atan2(r.x * fa.x + r.y * fa.y + r.z * fa.z, r.w);
        if (tw > 3.14159265f) tw -= 6.2831853f;
        if (tw < -3.14159265f) tw += 6.2831853f;
        float share = g.world ? tw * 0.6f : 0.f;   // (zone goals keep their tuned wrist-only turn)
        if (ranged) {
            // the forearm (untwisted after the hinge solve) takes as much of the turn as its twist range allows, the
            // wrist the rest; then both are held in range
            JointRange er = rom_->range(arm.elbow);
            share = clampf(tw, (er.twistMin + 6.f) * 0.0174533f, (er.twistMax - 6.f) * 0.0174533f);
        }
        Quat te = qaxis(fa, share);
        rotateWorld(a, arm.elbow, te);
        rotateWorld(a, arm.wrist, qmul(r, qconj(te)));
        if (ranged) { rom_->clampNode(a, arm.elbow); rom_->clampNode(a, arm.wrist); }
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
        for (int fi = 0; fi < 3 && g.dent; ++fi) {
            const std::vector<int>& ch = arm.fingers[fi];
            if (ch.size() < 2) continue;
            V3 a2 = P(a, ch[ch.size() - 2]), a3 = P(a, ch.back());
            V3 pad = a3 + normalize(a3 - a2) * 0.011f - palm * 0.004f;
            contacts.push_back({pad, 0.0095f});
        }
        if (breast && g.dent) contacts.push_back({P(a, arm.middle0) - palm * 0.012f, 0.026f});
    }
}

}  // namespace ag
