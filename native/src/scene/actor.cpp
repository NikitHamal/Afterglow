#include "scene/actor.h"
#include "scene/character.h"
#include <algorithm>
#include <cmath>

namespace ag {

namespace {
constexpr float kD2R = 0.0174533f;
V3 P(const Animator& a, int n) { return a.global(n).translation(); }
// additive rotation about a world axis (the subtree follows)
void rot(Animator& a, int n, const V3& axis, float ang) {
    if (n < 0 || std::fabs(ang) < 1e-5f) return;
    a.setWorldRot(n, qnormalize(qmul(qaxis(axis, ang), a.worldRot(n))));
    a.recomputeSubtree(n);
}
float bump(float t, float len) { return t >= 0.f && t < len ? std::sin(t / len * kPi) : 0.f; }   // one smooth hump
}  // namespace

void Actor::bind(Character* c) {
    c_ = c;
    female_ = c && c->id == "vivian";
    reset();
    if (!c) return;
    head = c->bone("頭"); neck = c->bone("首"); upper = c->bone("上半身"); chest = c->bone("上半身2");
    const char* sx[2] = {".L", ".R"};
    for (int s = 0; s < 2; ++s) {
        sh[s] = c->bone(std::string("肩") + sx[s]);
        arm[s] = c->bone(std::string("腕") + sx[s]);
        elbow[s] = c->bone(std::string("ひじ") + sx[s]);
        wrist[s] = c->bone(std::string("手首") + sx[s]);
    }
}

void Actor::reset() {
    gesture_.clear(); head_.clear(); look_.clear(); react_.clear();
    lineT_ = 0.f; reactT_ = headT_ = beatT_ = 99.f;
    speaking_ = false; speakW_ = gestW_ = loud_ = 0.f;
    vis_ = visS_ = Viseme{};
    expr.clear(); cur_.clear();
    lean = 0.f; eyesClosed = false; closed_ = 0.f;
}

void Actor::releaseFace() {
    if (!c_) return;
    for (const char* m : {"blink", "moan", "gasp", "smile", "pout", "kiss", "bite_lip", "shy", "lids_down", "bliss", "pleasure", "smirk", "wink", "effort"})
        c_->setMorph(m, -1.f);
    for (auto& kv : cur_) c_->setMorph(kv.first, -1.f);
    cur_.clear();
}

void Actor::cue(const ActingCue& c) {
    lineT_ = 0.f;
    energy_ = clampf(c.energy, 0.f, 1.f);
    look_ = c.look;
    if (c.speaking) {
        gesture_ = c.gesture == "none" ? std::string() : c.gesture;
        head_ = c.head; headT_ = 0.f;
        react_.clear();
        speaking_ = true;
    } else {
        gesture_.clear();
        speaking_ = false;
        if (!c.react.empty()) { react_ = c.react; reactT_ = 0.f; }
        if (!c.head.empty()) { head_ = c.head; headT_ = 0.f; }
    }
    tiltSide_ = hash1(time_ * 3.1f) > 0.5f ? 1.f : -1.f;
}

void Actor::speech(float loud, const Viseme& v, bool beat) {
    speaking_ = true;
    loud_ = loud;
    vis_ = v;
    if (beat) beatT_ = 0.f;
}

void Actor::update(float dt, const V3& partnerHead, bool hasPartner, const V3& camEye) {
    if (!c_) return;
    time_ += dt; lineT_ += dt; reactT_ += dt; headT_ += dt; beatT_ += dt;
    partner_ = partnerHead; hasPartner_ = hasPartner; cam_ = camEye;
    speakW_ += ((speaking_ ? 1.f : 0.f) - speakW_) * damp(4.f, dt);
    bool gest = !gesture_.empty() && (speaking_ || lineT_ < 1.2f);
    gestW_ += ((gest ? 1.f : 0.f) - gestW_) * damp(gest ? 3.2f : 1.7f, dt);
    if (!speaking_) loud_ = 0.f;
    // gaze: the partner's eyes by default; away / down / the camera when the line says so
    V3 hc, hf, hu;
    c_->headAxes(hc, hf, hu);
    V3 flat = normalize(V3{hf.x, 0.f, hf.z});
    if (length(flat) < 0.5f) flat = {0, 0, 1};
    V3 side = normalize(cross(V3{0, 1, 0}, flat));
    V3 tgt = hasPartner_ ? partner_ + V3{0.f, 0.02f, 0.f} : hc + flat * 2.f;
    if (look_ == "camera") tgt = cam_;
    else if (look_ == "away") tgt = hc + flat * 1.2f + side * (0.9f * tiltSide_) + V3{0.f, -0.25f, 0.f};
    else if (look_ == "down") tgt = hc + flat * 0.7f + V3{0.f, -0.75f, 0.f};
    if (react_ == "shy" && reactT_ < 3.f) tgt = lerp(tgt, hc + flat * 0.8f + side * (0.5f * tiltSide_) + V3{0.f, -0.6f, 0.f}, smoothstep(0.f, 0.4f, reactT_) * (1.f - smoothstep(2.4f, 3.f, reactT_)));
    c_->proc.lookAt = true;
    c_->proc.lookTarget = tgt;
    // (close in for a kiss the neck does less of the work: the lean and the step bring the faces together)
    c_->proc.lookWeight = ((look_ == "camera" ? 0.5f : 0.62f) * acting + 0.25f) * (1.f - 0.45f * lean);
    c_->proc.eyeWeight = 1.f;
    c_->proc.saccades = !eyesClosed;
    face(dt);
}

void Actor::face(float dt) {
    std::map<std::string, float> tgt = expr;
    auto atLeast = [&](const char* k, float v) { float& t = tgt[k]; t = std::max(t, v); };
    float rk = reactT_ < 2.8f ? smoothstep(0.f, 0.25f, reactT_) * (1.f - smoothstep(2.f, 2.8f, reactT_)) : 0.f;
    if (rk > 0.f) {
        if (react_ == "smile") atLeast("smile", 0.65f * rk);
        else if (react_ == "laugh") { atLeast("smile", 0.9f * rk); atLeast("moan", 0.12f * rk); }
        else if (react_ == "gasp") atLeast("gasp", 0.55f * rk);
        else if (react_ == "shy") { atLeast("shy", 0.7f * rk); atLeast("smile", 0.3f * rk); atLeast("lids_down", 0.35f * rk); }
        else if (react_ == "sigh") atLeast("bliss", 0.35f * rk);
        else if (react_ == "frown") atLeast("pout", 0.3f * rk);
    }
    for (auto& kv : tgt) cur_.emplace(kv.first, 0.f);
    float k = damp(5.f, dt);
    for (auto& kv : cur_) {
        auto it = tgt.find(kv.first);
        kv.second += ((it == tgt.end() ? 0.f : it->second) - kv.second) * k;
    }
    // speech: visemes scaled by loudness, smoothed (~40 ms) so the mouth never snaps
    float kv = damp(24.f, dt);
    float g = speaking_ ? 0.35f + 0.65f * loud_ : 0.f;
    visS_.open += (vis_.open * g - visS_.open) * kv;
    visS_.wide += (vis_.wide * g - visS_.wide) * kv;
    visS_.round += (vis_.round * g - visS_.round) * kv;
    visS_.bite += (vis_.bite * g - visS_.bite) * kv;
    visS_.closed += ((speaking_ ? vis_.closed : 0.f) - visS_.closed) * kv;
    if (!speaking_) vis_ = Viseme{};
    float open = visS_.open * (1.f - visS_.closed);
    std::map<std::string, float> out = cur_;
    if (female_) {
        out["moan"] += open * 0.3f; out["gasp"] += open * 0.42f; out["smile"] += visS_.wide * 0.35f;
        out["pout"] += visS_.round * 0.5f; out["kiss"] += visS_.round * 0.22f; out["bite_lip"] += visS_.bite * 0.7f;
    } else {
        out["moan"] += open * 0.6f; out["kiss"] += visS_.round * 0.45f;
    }
    // blinks (more right after a line starts), lids closed for a kiss
    blinkT_ -= dt;
    if (blinkT_ <= 0.f) { blink_ = 1.f; blinkT_ = 1.6f + 3.4f * hash1(time_ * 1.37f + (female_ ? 0.f : 5.f)); }
    if (lineT_ < dt * 1.5f && hash1(time_) > 0.55f) blink_ = 1.f;
    blink_ += (0.f - blink_) * damp(15.f, dt);
    closed_ += ((eyesClosed ? 1.f : 0.f) - closed_) * damp(6.f, dt);
    out["blink"] = std::max(out["blink"], std::max(blink_, closed_ * 0.92f));
    for (auto& o : out) c_->setMorph(o.first, clampf(o.second, 0.f, 1.f));
}

void Actor::gestureTargets(Arm& l, Arm& r) const {
    l = r = Arm{};
    const std::string& g = gesture_;
    if (g == "open") { r = {20, 12, 65, 70, 0}; l = r; }
    else if (g == "explain") { r = {25, 8, 80, 45, 0}; l = {6, 3, 15, 10, 0}; }
    else if (g == "point") { r = {50, 5, 15, 0, 0}; }
    else if (g == "shrug") { r = {10, 18, 55, 80, 10}; l = r; }
    else if (g == "reach") { r = {40, 0, 30, 35, 0}; }
    else if (g == "wave") { r = {35, 35, 110, 20, 0}; }
    else if (g == "chest") { r = {28, -12, 115, 10, 0}; }
    else if (g == "hips") { r = {-5, 25, 70, -40, 0}; l = r; }
    if (!female_ && g == "explain") { r.elbow = 70; r.flex = 22; }
}

void Actor::pose(Animator& a) {
    if (!c_ || arm[0] < 0 || arm[1] < 0 || acting <= 0.001f) return;
    V3 up{0, 1, 0};
    V3 r = P(a, arm[1]) - P(a, arm[0]);
    r.y = 0.f;
    if (length(r) < 1e-3f) return;
    r = normalize(r);
    V3 f = normalize(cross(up, r));
    float w = acting;
    // torso: a slow weight shift, leaning in a little while speaking, more for a kiss
    rot(a, upper, f, 0.022f * std::sin(time_ * 0.55f + (female_ ? 0.f : 2.f)) * (1.f - sit) * w);
    rot(a, upper, r, -(0.045f * speakW_ * (0.4f + energy_) + 0.12f * lean) * w);
    rot(a, chest, r, -0.05f * lean * w);
    // arms: talk gestures (additive to the clip), with a little life from the speech itself
    if (gestW_ < 0.002f) return;
    Arm L, R;
    gestureTargets(L, R);
    for (int s = 0; s < 2; ++s) {
        Arm g = s == 1 ? R : L;
        if (g.flex == 0.f && g.elbow == 0.f && g.abduct == 0.f) continue;
        float sg = s == 1 ? 1.f : -1.f;
        float k = gestW_ * w * kD2R;
        float live = (std::sin(time_ * 2.3f + (float)s * 1.7f) * 5.f * loud_ - 9.f * bump(beatT_, 0.3f)) * speakW_;
        if (sit > 0.5f) { g.flex *= 0.3f; g.abduct *= 0.3f; g.shrug *= 0.5f; }
        rot(a, sh[s], f, -sg * g.shrug * k);
        rot(a, arm[s], r, g.flex * k);
        rot(a, arm[s], f, -sg * g.abduct * k);
        if (elbow[s] < 0 || wrist[s] < 0) continue;
        V3 ua = normalize(P(a, elbow[s]) - P(a, arm[s]));
        V3 h = cross(ua, f);
        h = length(h) > 1e-3f ? normalize(h) : r;
        rot(a, elbow[s], h, (g.elbow + live) * k);
        V3 fa = normalize(P(a, wrist[s]) - P(a, elbow[s]));
        rot(a, wrist[s], fa, sg * g.supin * k);
    }
}

// head motions go on after the look-at (which would otherwise straighten them out again)
void Actor::postPose(Animator& a) {
    if (!c_ || arm[0] < 0 || arm[1] < 0 || acting <= 0.001f) return;
    V3 up{0, 1, 0};
    V3 r = P(a, arm[1]) - P(a, arm[0]);
    r.y = 0.f;
    if (length(r) < 1e-3f) return;
    r = normalize(r);
    V3 f = normalize(cross(up, r));
    float w = acting;
    // head: nods / shakes / tilts, a small dip on stressed words, listener nods
    float hp = 0.f, hy = 0.f, hr = 0.f;
    if (head_ == "nod") hp -= 0.11f * bump(headT_, 0.45f) + 0.06f * bump(headT_ - 0.45f, 0.4f);
    else if (head_ == "shake") hy += 0.09f * std::sin(headT_ * 6.2832f * 2.4f) * (1.f - smoothstep(0.5f, 1.1f, headT_));
    else if (head_ == "tilt") hr += 0.11f * tiltSide_ * smoothstep(0.f, 0.45f, headT_) * (speaking_ || lineT_ < 3.f ? 1.f : 0.f);
    else if (head_ == "down") hp -= 0.18f * smoothstep(0.f, 0.5f, headT_);
    else if (head_ == "up") hp += 0.1f * smoothstep(0.f, 0.5f, headT_);
    hp -= 0.05f * (0.4f + energy_) * bump(beatT_, 0.28f) * speakW_;
    if (react_ == "nod") hp -= 0.1f * bump(reactT_, 0.45f) + 0.05f * bump(reactT_ - 0.45f, 0.4f);
    else if (react_ == "laugh") hp += 0.035f * std::sin(reactT_ * 6.2832f * 3.2f) * (1.f - smoothstep(0.f, 1.4f, reactT_));
    else if (react_ == "gasp") hp += 0.06f * bump(reactT_, 0.9f);
    else if (react_ == "shy") hp -= 0.1f * smoothstep(0.f, 0.4f, reactT_) * (1.f - smoothstep(2.4f, 3.f, reactT_));
    int hb = neck >= 0 ? neck : head;
    rot(a, hb, r, hp * 0.4f * w);
    rot(a, head, r, hp * 0.6f * w);
    rot(a, head, up, hy * w);
    rot(a, head, f, hr * w);
}

}  // namespace ag
