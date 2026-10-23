#include "game/director.h"
#include <cmath>

namespace ag {

namespace {
V3 flat(V3 v) { v.y = 0.f; float l = length(v); return l > 1e-4f ? v / l : V3{0, 0, 1}; }
}

void Director::shot(const std::string& kind, int subject, bool cut, float hold) {
    bool same = kind == kind_ && subject == subject_;
    kind_ = kind;
    subject_ = subject;
    hold_ = std::max(2.5f, hold);
    if (same && !cut) return;          // the same shot carries on (its push-in continues)
    t_ = 0.f;
    vary_ = hash1((float)(kind.size() * 7 + subject * 3) + vary_ * 13.7f + 1.3f);
    cut_ = true;
    snap_ = true;
}

ShotFrame Director::frame(float dt, const ActorView& her, const ActorView& him, const V3& room) {
    t_ += dt;
    const V3 up{0, 1, 0};
    ShotFrame f;
    float u = smoothstep(0.f, 1.f, t_ / (hold_ + 2.f));     // slow push-ins / drifts over the shot
    bool both = her.shown && him.shown;
    if (!her.shown && !him.shown) {
        // nobody on stage: the place itself, a slow drift across the room
        float a = 0.7f + (vary_ - 0.5f) * 0.6f + 0.18f * u;
        f.target = room + V3{0.f, -0.1f, -1.2f};
        f.eye = f.target + V3{std::sin(a) * 2.9f, 0.55f, std::cos(a) * 2.9f};
        f.fov = 44.f;
        f.focus = 3.f;
        sEye_ = f.eye; sTarget_ = f.target;
        return f;
    }
    const ActorView& S = subject_ == 1 ? (him.shown ? him : her) : (subject_ == 0 ? (her.shown ? her : him) : (her.shown ? her : him));
    const ActorView& L = &S == &her ? him : her;
    V3 mid = both ? (her.head + him.head) * 0.5f : S.head;
    V3 axis = both ? flat(him.head - her.head) : flat(cross(S.face, up));
    V3 perp = normalize(cross(up, axis));
    if (!sideSet_) {
        // in front of them when their bodies face the same way (sitting side by side), else on the open side
        float s1 = dot(perp, flat(her.body) * (her.shown ? 1.f : 0.f) + flat(him.body) * (him.shown ? 1.f : 0.f));
        side_ = std::fabs(s1) > 0.35f ? (s1 > 0.f ? 1.f : -1.f) : (dot(perp, room - mid) >= 0.f ? 1.f : -1.f);
        sideSet_ = true;
    }
    V3 side = perp * side_;
    float sep = both ? length(him.head - her.head) : 0.6f;
    std::string k = kind_;
    if (!both && (k == "two" || k == "ots" || k == "kiss")) k = "medium";
    if (k == "establish") {
        f.eye = mid + side * 3.7f + axis * ((vary_ - 0.5f) * 1.6f - 0.45f * u) + up * 0.42f;
        f.target = mid - up * 0.5f + axis * ((vary_ - 0.5f) * 0.3f);
        f.fov = 44.f;
    } else if (k == "wide") {
        f.eye = mid + side * (3.0f - 0.3f * u) + axis * ((vary_ - 0.5f) * 0.8f) + up * 0.12f;
        f.target = mid - up * 0.55f;
        f.fov = 40.f;
    } else if (k == "two") {
        float d = (1.3f + sep * 0.85f) * (1.f - 0.08f * u);
        f.eye = mid + side * d + axis * ((vary_ - 0.5f) * 0.35f) + up * 0.03f;
        f.target = mid - up * 0.17f;
        f.fov = 34.f;
        f.aperture = 0.15f;
    } else if (k == "kiss") {
        // a tight two-shot across the kiss, a little low, slowly closing in
        float d = 0.95f - 0.12f * u;
        f.eye = mid + side * d + axis * ((vary_ - 0.5f) * 0.2f) - up * 0.04f;
        f.target = mid - up * 0.03f;
        f.fov = 30.f;
        f.aperture = 0.7f;
    } else if (k == "ots") {
        V3 dir = flat(S.head - L.head);
        V3 sl = side - dir * dot(side, dir);
        sl = length(sl) > 1e-3f ? normalize(sl) : side;
        f.eye = L.head - dir * (0.52f - 0.07f * u) + sl * 0.3f + up * 0.06f;
        f.target = S.head - up * 0.07f - dir * 0.02f;   // eyes on the upper third
        f.fov = 30.f;
        f.aperture = 0.9f;
    } else {
        // singles: the speaker, from a little to the camera side of where they look
        V3 looks = both ? flat(L.head - S.head) : flat(S.face);
        float ang = 0.55f + (vary_ - 0.5f) * 0.25f;
        V3 cdir = normalize(looks * std::cos(ang) + side * std::sin(ang));
        // close-up: head and shoulders, the eyes on the upper third; extreme close-up: the face
        float d = k == "xclose" ? 0.58f : (k == "close" ? 0.98f : 1.4f);
        d *= 1.f - (k == "medium" ? 0.05f : 0.1f) * u;
        f.eye = S.head + cdir * d + up * (k == "medium" ? -0.05f : 0.0f);
        f.target = S.head + up * (k == "medium" ? -0.24f : (k == "close" ? -0.075f : -0.03f));
        f.fov = k == "xclose" ? 25.f : (k == "close" ? 28.f : 32.f);
        f.aperture = k == "xclose" ? 1.4f : (k == "close" ? 1.15f : 0.45f);
    }
    // ease within a shot (breathing heads, gestures), snap on a cut
    if (snap_) { sEye_ = f.eye; sTarget_ = f.target; snap_ = false; }
    else {
        float kk = damp(5.f, dt);
        sEye_ = lerp(sEye_, f.eye, kk);
        sTarget_ = lerp(sTarget_, f.target, kk);
    }
    f.eye = sEye_;
    f.target = sTarget_;
    f.focus = length((k == "two" || k == "wide" || k == "establish" || k == "kiss" ? mid : S.head) - f.eye);
    return f;
}

}  // namespace ag
