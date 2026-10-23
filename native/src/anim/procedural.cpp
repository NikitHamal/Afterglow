#include "anim/procedural.h"
#include "core/log.h"
#include <cmath>
#include <algorithm>

namespace ag {

namespace {
bool startsWith(const std::string& s, const char* p) { return s.rfind(p, 0) == 0; }
V3 pos(const Animator& a, int n) { return a.global(n).translation(); }
// local axis of node n that points along world direction `w` in the rest pose
V3 restLocalDir(const Animator& a, int n, const V3& w) { return normalize(qrotate(qconj(a.worldRot(n)), w)); }
}  // namespace

void Procedural::init(Animator& a, float rootScale) {
    const Model& m = *a.model();
    scale_ = rootScale;
    chest_ = m.findNode("上半身2"); belly_ = m.findNode("上半身");
    neck_ = m.findNode("首"); head_ = m.findNode("頭");
    eyeL_ = m.findNode("目.L"); eyeR_ = m.findNode("目.R");
    shoulderL_ = m.findNode("肩.L"); shoulderR_ = m.findNode("肩.R");

    // rest globals with identity root: forward vectors, collider offsets, chain rest lengths
    a.begin();
    a.computeGlobals(M4::identity());
    const V3 fwd{0, 0, 1};   // glTF characters face +Z
    if (head_ >= 0) { headFwd_ = restLocalDir(a, head_, fwd); headUp_ = restLocalDir(a, head_, V3{0, 1, 0}); }
    if (neck_ >= 0) neckFwd_ = restLocalDir(a, neck_, fwd);
    if (eyeL_ >= 0) eyeFwdL_ = restLocalDir(a, eyeL_, fwd);
    if (eyeR_ >= 0) eyeFwdR_ = restLocalDir(a, eyeR_, fwd);

    // hair colliders from node extras: {"afterglow": "{\"hairColliders\":[{bone,c,r}]}"} (string or object)
    colliders_.clear();
    for (const Node& n : m.nodes) {
        const Json& ag = n.extras["afterglow"];
        Json parsed;
        const Json* src = nullptr;
        if (ag.isString()) { parsed = Json::parse(ag.s); src = &parsed; }
        else if (ag.isObject()) src = &ag;
        if (!src) continue;
        const Json& hc = (*src)["hairColliders"];
        for (size_t i = 0; i < hc.size(); ++i) {
            int b = m.findNode(hc[i]["bone"].str());
            if (b < 0) continue;
            V3 c{hc[i]["c"][0].f(), hc[i]["c"][1].f(), hc[i]["c"][2].f()};
            Collider col{b, xformPoint(inverse(a.global(b)), c), hc[i]["r"].f(0.05f)};
            colliders_.push_back(col);
        }
    }

    // hair chains: bones named 髪* whose parent is not hair
    chains_.clear();
    for (size_t i = 0; i < m.nodes.size(); ++i) {
        const Node& n = m.nodes[i];
        if (!startsWith(n.name, "髪")) continue;
        if (n.parent >= 0 && startsWith(m.nodes[(size_t)n.parent].name, "髪")) continue;
        Chain ch;
        int cur = (int)i;
        while (cur >= 0) {
            ch.bones.push_back(cur);
            int next = -1;
            for (int c : m.nodes[(size_t)cur].children) if (startsWith(m.nodes[(size_t)c].name, "髪")) { next = c; break; }
            cur = next;
        }
        if (ch.bones.size() < 2) continue;
        ch.rest.assign(ch.bones.size(), 0.f);
        ch.axis.assign(ch.bones.size(), V3{0, 1, 0});
        for (size_t k = 1; k < ch.bones.size(); ++k) {
            ch.rest[k] = length(pos(a, ch.bones[k]) - pos(a, ch.bones[k - 1])) * scale_;
            V3 t = a.restT(ch.bones[k]);
            if (length(t) > 1e-6f) ch.axis[k] = normalize(t);
        }
        chains_.push_back(std::move(ch));
    }

    // spring bones
    springs_.clear();
    auto addSpring = [&](const char* name, float k, float c, float maxOff, float sag) {
        int b = m.findNode(name);
        if (b < 0) return;
        Spring s{};
        s.bone = b; s.child = m.nodes[(size_t)b].children.empty() ? -1 : m.nodes[(size_t)b].children[0];
        s.len = s.child >= 0 ? length(pos(a, s.child) - pos(a, b)) * scale_ : 0.08f * scale_;
        if (s.len < 0.02f * scale_) s.len = 0.08f * scale_;
        s.k = k; s.c = c; s.maxOff = maxOff * scale_; s.sag = sag;
        springs_.push_back(s);
    };
    // the content's tuning when the armature carries it (extras.afterglow: hairChains, softBones), else defaults
    const Json* agx = nullptr;
    Json agParsed;
    for (const Node& n : m.nodes) {
        const Json& ag = n.extras["afterglow"];
        if (ag.isString()) { Json pj = Json::parse(ag.s); if (pj["softBones"].isArray() || pj["hairChains"].isArray()) { agParsed = pj; agx = &agParsed; break; } }
        else if (ag.isObject() && (ag["softBones"].isArray() || ag["hairChains"].isArray())) { agx = &ag; break; }
    }
    if (agx && (*agx)["softBones"].isArray()) {
        const Json& sb = (*agx)["softBones"];
        for (size_t i = 0; i < sb.size(); ++i) {
            // stiffness / damping / gravity 0..1 -> spring constant, drag, sag; the swing limit -> tip offset
            float st = sb[i]["stiffness"].f(0.7f), dm = sb[i]["damping"].f(0.3f), gv = sb[i]["gravity"].f(0.5f);
            float ang = sb[i]["maxAngleDeg"].f(15.f) * kPi / 180.f;
            size_t before = springs_.size();
            addSpring(sb[i]["bone"].str().c_str(), 220.f * st, 25.f * dm, 0.f, 0.42f * gv);
            if (springs_.size() > before) springs_.back().maxOff = springs_.back().len * std::sin(ang);
        }
    } else {
        addSpring("乳親.L", 160.f, 7.5f, 0.035f, 0.25f);
        addSpring("乳親.R", 160.f, 7.5f, 0.035f, 0.25f);
        addSpring("玉袋", 90.f, 5.f, 0.03f, 0.6f);
    }
    if (agx && (*agx)["hairChains"].isArray()) {
        const Json& hc = (*agx)["hairChains"];
        for (size_t i = 0; i < hc.size(); ++i) {
            int root = m.findNode(hc[i]["root"].str());
            for (Chain& ch : chains_) {
                if (ch.bones.empty() || ch.bones[0] != root) continue;
                ch.stiff = hc[i]["stiffness"].f(0.82f) / 0.82f;
                ch.drag = clampf(hc[i]["damping"].f(0.55f) * 0.64f, 0.05f, 0.9f);
                ch.grav = hc[i]["gravity"].f(0.35f) / 0.35f;
                ch.maxAng = hc[i]["maxAngleDeg"].f(180.f) * kPi / 180.f;
            }
        }
    }
    AG_INFO("AG: procedural: %d hair chains, %d colliders, %d springs", (int)chains_.size(), (int)colliders_.size(), (int)springs_.size());
    needReset_ = true;
}

void Procedural::apply(Animator& a, float dt, const ProcParams& p) {
    time_ += dt;
    breathing(a, dt, p);
    a.computeGlobals(a.rootMatrix());
    if (p.lookAt) look(a, dt, p);
    if (p.jiggle) jiggle(a, dt, p);
    if (p.hair) hair(a, dt, p);
    needReset_ = false;
}

void Procedural::breathing(Animator& a, float dt, const ProcParams& p) {
    breathPhase_ += dt * p.breathRate * 2.f * kPi;
    float s = std::sin(breathPhase_), amp = p.breathAmp;
    if (amp <= 0.f) return;
    auto tilt = [&](int n, float ang) {
        if (n < 0) return;
        a.r(n) = qnormalize(qmul(a.r(n), qaxis(V3{1, 0, 0}, ang)));
    };
    tilt(chest_, -0.028f * amp * s);
    tilt(belly_, 0.012f * amp * s);
    tilt(neck_, 0.016f * amp * s);
}

void Procedural::look(Animator& a, float dt, const ProcParams& p) {
    if (head_ < 0) return;
    if (needReset_) lookSmooth_ = p.lookTarget;
    lookSmooth_ = lerp(lookSmooth_, p.lookTarget, damp(6.f, dt));
    // head (+ neck share)
    auto aim = [&](int n, const V3& fwdLocal, const V3& target, float maxAng, float weight) {
        if (n < 0 || weight <= 0.f) return;
        Quat w = a.worldRot(n);
        V3 cur = normalize(qrotate(w, fwdLocal));
        V3 want = normalize(target - pos(a, n));
        if (length(want) < 0.5f) return;
        float off = std::acos(clampf(dot(cur, want), -1.f, 1.f));
        float cone = 1.f - smoothstep(1.3f, 2.1f, off);   // don't wrench the neck toward targets behind
        Quat d = qclampAngle(qfromto(cur, want), maxAng);
        d = qslerp(Quat{}, d, clampf(weight * cone, 0.f, 1.f));
        a.setWorldRot(n, qmul(d, w));
        a.recomputeSubtree(n);
    };
    float lw = clampf(p.lookWeight, 0.f, 1.f);
    if (neck_ >= 0) aim(neck_, neckFwd_, lookSmooth_, 0.25f, lw * 0.35f);
    aim(head_, headFwd_, lookSmooth_, 0.55f, lw);
    // eyes: saccades jump, roll-up blends toward the brow line
    saccadeT_ -= dt;
    if (p.saccades && saccadeT_ <= 0.f) {
        saccadeT_ = 0.35f + 2.2f * hash1(time_ * 13.1f);
        float d = length(lookSmooth_ - pos(a, head_));
        saccade_ = V3{vnoise1(time_ * 7.3f), vnoise1(time_ * 5.1f + 3.f) * 0.6f, 0.f} * (0.025f * d);
    }
    V3 up = normalize(qrotate(a.worldRot(head_), headUp_));
    float ew = clampf(p.eyeWeight, 0.f, 1.f);
    for (int k = 0; k < 2; ++k) {
        int e = k == 0 ? eyeL_ : eyeR_;
        if (e < 0) continue;
        V3 tgt = lookSmooth_ + saccade_;
        V3 dir = normalize(tgt - pos(a, e));
        dir = normalize(dir + up * (1.4f * clampf(p.eyeRollUp, 0.f, 1.f)));
        aim(e, k == 0 ? eyeFwdL_ : eyeFwdR_, pos(a, e) + dir, 0.42f + 0.25f * p.eyeRollUp, ew);
    }
}

void Procedural::jiggle(Animator& a, float dt, const ProcParams& p) {
    if (dt <= 0.f) return;
    const V3 g{0, -9.81f, 0};
    for (Spring& s : springs_) {
        V3 o = pos(a, s.bone);
        Quat w = a.worldRot(s.bone);
        V3 dirAnim = s.child >= 0 ? normalize(pos(a, s.child) - o) : normalize(qrotate(w, V3{0, 1, 0}));
        V3 tip = o + dirAnim * s.len;
        if (!s.init || needReset_) { s.x = tip; s.v = {0, 0, 0}; s.init = true; continue; }
        int steps = std::max(1, std::min(4, (int)std::ceil(dt / (1.f / 120.f))));
        float h = dt / (float)steps;
        for (int i = 0; i < steps; ++i) {
            V3 acc = (tip - s.x) * s.k - s.v * s.c + g * (s.sag * 0.12f);
            s.v += acc * h;
            s.x += s.v * h;
        }
        V3 off = s.x - tip;
        float ol = length(off), maxO = s.maxOff * clampf(p.jiggleGain, 0.f, 3.f);
        if (ol > maxO && ol > 1e-6f) { s.x = tip + off * (maxO / ol); s.v *= 0.5f; }
        V3 want = normalize(s.x - o);
        if (length(want) < 0.5f) continue;
        Quat d = qfromto(dirAnim, want);
        a.setWorldRot(s.bone, qmul(d, w));
        a.recomputeSubtree(s.bone);
    }
}

void Procedural::hair(Animator& a, float dt, const ProcParams& p) {
    // Spring-bone chains (the VRM model): every joint is pulled toward its animated direction relative to
    // its *simulated* parent, plus gravity; inertia with drag; length-preserving; pushed out of sphere
    // colliders and above the floor plane. Stiffness falls off toward the tips, so long hair hangs and
    // swings while the roots keep the style on the scalp.
    if (chains_.empty() || dt <= 0.f) return;
    struct WS { V3 c; float r; };
    std::vector<WS> cols;
    cols.reserve(colliders_.size());
    for (auto& c : colliders_) cols.push_back({xformPoint(a.global(c.bone), c.local), c.r * scale_});
    V3 anchor = head_ >= 0 ? pos(a, head_) : pos(a, chains_[0].bones[0]);
    bool teleport = needReset_ || length(anchor - lastAnchor_) > 0.35f;
    lastAnchor_ = anchor;
    const float h = 1.f / 90.f;
    int steps = 0;
    if (teleport) {
        hairAccum_ = 0.f;
        steps = 45;   // settle half a second in the new pose instead of popping from the styled shape
    } else {
        hairAccum_ = std::min(hairAccum_ + dt, 4.f * h);
        steps = (int)(hairAccum_ / h);
        hairAccum_ -= (float)steps * h;
    }
    const V3 down{0, -1, 0};
    const float grav0 = 0.9f * p.hairGravity * scale_, hit = 0.012f * scale_;
    for (Chain& ch : chains_) {
        const size_t n = ch.bones.size();
        const float grav = grav0 * ch.grav, drag = ch.drag;
        if (teleport || ch.p.size() != n) {
            ch.p.resize(n); ch.prev.resize(n);
            for (size_t i = 0; i < n; ++i) ch.p[i] = ch.prev[i] = pos(a, ch.bones[i]);
        }
        const int parent = a.model()->nodes[(size_t)ch.bones[0]].parent;
        const Quat parentW = parent >= 0 ? a.worldRot(parent) : Quat{};
        ch.p[0] = ch.prev[0] = pos(a, ch.bones[0]);
        for (int st = 0; st < steps; ++st) {
            Quat rotPrev = parentW;
            for (size_t i = 1; i < n; ++i) {
                const Quat restW = qmul(rotPrev, a.r(ch.bones[i - 1]));
                const V3 axisW = qrotate(restW, ch.axis[i]);
                const float k = (1.8f - 1.25f * (float)(i - 1) / (float)std::max<size_t>(1, n - 2)) * p.hairStiffness * ch.stiff * scale_;
                const V3 root = ch.p[i - 1];
                auto project = [&](const V3& q) {
                    V3 d = q - root;
                    float l = length(d);
                    return l > 1e-6f ? root + d * (ch.rest[i] / l) : root + axisW * ch.rest[i];
                };
                const V3 cur = ch.p[i];
                V3 next = project(cur + (cur - ch.prev[i]) * (1.f - drag) + axisW * (k * h) + down * (grav * h));
                for (const WS& c : cols) {
                    V3 dc = next - c.c;
                    float r = c.r + hit, d2 = dot(dc, dc);
                    if (d2 < r * r && d2 > 1e-12f) next = project(c.c + dc * (r / std::sqrt(d2)));
                }
                if (next.y < p.floorY + hit) next = project(V3{next.x, p.floorY + hit, next.z});
                // swing limit from the rest direction: a tendril never falls across her face
                if (ch.maxAng < 3.1f) {
                    V3 dn = next - root;
                    float dl = length(dn);
                    if (dl > 1e-6f) {
                        Quat sw = qclampAngle(qfromto(axisW, dn / dl), ch.maxAng);
                        next = root + qrotate(sw, axisW) * ch.rest[i];
                    }
                }
                ch.prev[i] = cur;
                ch.p[i] = next;
                V3 dir = next - root;
                rotPrev = length(dir) > 1e-6f ? qmul(qfromto(axisW, normalize(dir)), restW) : restW;
            }
        }
        // orient each joint toward its simulated tail, root to tip
        Quat rotPrev = parentW;
        for (size_t i = 1; i < n; ++i) {
            const int b = ch.bones[i - 1];
            const Quat restW = qmul(rotPrev, a.r(b));
            const V3 axisW = qrotate(restW, ch.axis[i]);
            V3 want = ch.p[i] - pos(a, b);
            Quat w = length(want) > 1e-6f ? qmul(qfromto(axisW, normalize(want)), restW) : restW;
            a.setWorldRot(b, w);
            a.recomputeSubtree(b);
            rotPrev = w;
        }
    }
}

}  // namespace ag
