#include "scene/collision.h"
#include "scene/character.h"
#include <algorithm>
#include <cmath>

namespace ag {

namespace {
V3 closestOnSegment(const V3& p, const V3& a, const V3& b) {
    V3 ab = b - a;
    float l2 = dot(ab, ab);
    if (l2 < 1e-10f) return a;
    float t = clampf(dot(p - a, ab) / l2, 0.f, 1.f);
    return a + ab * t;
}
}  // namespace

void BodyProxies::build(const Character* const* chars, const bool* shown, int count) {
    caps_.clear();
    for (int i = 0; i < count; ++i) {
        const Character* c = chars[i];
        if (!c || !c->loaded() || !shown[i]) continue;
        float k = c->autoScale();
        bool male = c->id == "marcus";
        auto bone = [&](const char* n, V3& out) {
            int b = c->bone(n);
            if (b < 0) return false;
            out = c->bonePos(n);
            return true;
        };
        auto cap = [&](const char* a, const char* b, float r) {
            V3 pa, pb;
            if (bone(a, pa) && bone(b, pb)) caps_.push_back({pa, pb, r * k, i, false});
        };
        // head: a sphere around the skull centre (covers the face and mouth)
        V3 hc, hf, hu;
        c->headAxes(hc, hf, hu);
        caps_.push_back({hc - hu * 0.02f + hf * 0.015f, hc - hu * 0.02f + hf * 0.015f, (male ? 0.115f : 0.105f) * k, i, true});
        cap("首", "頭", male ? 0.06f : 0.052f);
        cap("下半身", "上半身", male ? 0.15f : 0.155f);
        cap("上半身", "上半身2", male ? 0.15f : 0.135f);
        cap("上半身2", "首", male ? 0.14f : 0.12f);
        for (const char* side : {".L", ".R"}) {
            std::string s = side;
            cap(("腕" + s).c_str(), ("ひじ" + s).c_str(), male ? 0.05f : 0.042f);
            cap(("ひじ" + s).c_str(), ("手首" + s).c_str(), male ? 0.04f : 0.034f);
            cap(("足" + s).c_str(), ("ひざ" + s).c_str(), male ? 0.08f : 0.078f);
            cap(("ひざ" + s).c_str(), ("足首" + s).c_str(), male ? 0.055f : 0.05f);
        }
    }
}

float BodyProxies::distance(const V3& p, int skipHeadOf) const {
    float best = 1e9f;
    for (const Capsule& c : caps_) {
        if (c.head && c.owner == skipHeadOf) continue;
        best = std::min(best, length(p - closestOnSegment(p, c.a, c.b)) - c.r);
    }
    return best;
}

V3 resolveCamera(const BodyProxies& proxies, const CameraSafety& s, V3 eye, const V3& target, int skipHeadOf) {
    // a few relaxation passes: push the eye (and a probe just ahead of it, so the near plane stays
    // out of skin) to `margin` outside every capsule, then clamp to the room
    for (int it = 0; it < 4; ++it) {
        bool moved = false;
        V3 fwd = target - eye;
        float fl = length(fwd);
        fwd = fl > 1e-4f ? fwd / fl : V3{0, 0, -1};
        for (const Capsule& c : proxies.capsules()) {
            if (c.head && c.owner == skipHeadOf) continue;
            for (int probe = 0; probe < 2; ++probe) {
                V3 p = probe == 0 ? eye : eye + fwd * std::min(s.nearProbe, fl * 0.5f);
                V3 q = closestOnSegment(p, c.a, c.b);
                V3 d = p - q;
                float dl = length(d), want = c.r + s.margin;
                if (dl >= want) continue;
                V3 n = dl > 1e-5f ? d / dl : fwd * -1.f;
                if (probe == 1 && dot(n, fwd) > 0.f) n = fwd * -1.f;   // the probe blocked ahead: step back
                eye = eye + n * (want - dl);
                moved = true;
            }
        }
        eye = V3{clampf(eye.x, s.boundsMin.x, s.boundsMax.x), clampf(eye.y, s.boundsMin.y, s.boundsMax.y),
                 clampf(eye.z, s.boundsMin.z, s.boundsMax.z)};
        if (!moved) break;
    }
    return eye;
}

}  // namespace ag
