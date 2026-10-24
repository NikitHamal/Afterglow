#include "anim/rom.h"
#include "core/log.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace ag {

bool JointLimits::enabled = true;
bool JointLimits::log = false;

namespace {

constexpr float kRad = kPi / 180.f, kDeg = 180.f / kPi;
constexpr float kSoft = 6.f;      // degrees over which a limit eases in
constexpr float kMargin = 9.f;    // authored envelope + margin: authored keys stay below the soft knee

// anatomical ranges (degrees) by bone name without the side: swing cone, twist about the bone axis; hinges bend
// [hingeMin, hingeMax] about their main axis with a little off-axis play. The rest pose is the anatomical neutral
// (standing, arms relaxed down), as in the content's rigkit (首 40/45, 頭 32/38, 手首 65/85, 足首 82/35).
struct Anat { const char* name; float swing, twist; bool hinge; float hMin, hMax, off; };
const Anat kAnat[] = {
    {"上半身", 60.f, 40.f, false, 0, 0, 0},
    {"上半身2", 40.f, 30.f, false, 0, 0, 0},
    {"首", 40.f, 45.f, false, 0, 0, 0},
    {"頭", 32.f, 38.f, false, 0, 0, 0},
    {"肩", 25.f, 15.f, false, 0, 0, 0},
    {"腕", 175.f, 100.f, false, 0, 0, 0},
    {"ひじ", 0.f, 75.f, true, -5.f, 150.f, 25.f},      // twist = the forearm's share of pronation / supination; off-axis = carrying angle + IK play
    {"手首", 70.f, 88.f, false, 0, 0, 0},
    {"親指０", 50.f, 25.f, false, 0, 0, 0},
    {"親指１", 60.f, 15.f, false, 0, 0, 0},
    {"親指２", 80.f, 10.f, false, 0, 0, 0},
    {"人指０", 95.f, 20.f, false, 0, 0, 0}, {"人指１", 105.f, 10.f, false, 0, 0, 0},
    {"人指２", 95.f, 10.f, false, 0, 0, 0}, {"人指３", 95.f, 10.f, false, 0, 0, 0},
    {"中指０", 95.f, 20.f, false, 0, 0, 0}, {"中指１", 105.f, 10.f, false, 0, 0, 0},
    {"中指２", 95.f, 10.f, false, 0, 0, 0}, {"中指３", 95.f, 10.f, false, 0, 0, 0},
    {"薬指０", 95.f, 20.f, false, 0, 0, 0}, {"薬指１", 105.f, 10.f, false, 0, 0, 0},
    {"薬指２", 95.f, 10.f, false, 0, 0, 0}, {"薬指３", 95.f, 10.f, false, 0, 0, 0},
    {"小指０", 95.f, 20.f, false, 0, 0, 0}, {"小指１", 105.f, 10.f, false, 0, 0, 0},
    {"小指２", 95.f, 10.f, false, 0, 0, 0}, {"小指３", 95.f, 10.f, false, 0, 0, 0},
    {"足", 125.f, 50.f, false, 0, 0, 0},
    {"ひざ", 0.f, 20.f, true, -5.f, 150.f, 10.f},
    {"足首", 82.f, 35.f, false, 0, 0, 0},
    {"つま先", 50.f, 10.f, false, 0, 0, 0},
    {"目", 55.f, 5.f, false, 0, 0, 0},       // the eye roll at climax goes past the gaze range
};

std::string baseName(const std::string& n) {
    if (n.size() > 2 && n[n.size() - 2] == '.' && (n.back() == 'L' || n.back() == 'R')) return n.substr(0, n.size() - 2);
    return n;
}

// rotation relative to rest, split: d = swing * twist (twist about +Y), swing as a rotation vector in the XZ plane
void split(const Quat& rest, const Quat& pose, V3& swingVec, float& twistDeg) {
    Quat d = qnormalize(qmul(qconj(rest), pose));
    if (d.w < 0.f) d = Quat{-d.x, -d.y, -d.z, -d.w};
    Quat tw{0.f, d.y, 0.f, d.w};
    float tl = std::sqrt(tw.y * tw.y + tw.w * tw.w);
    tw = tl > 1e-9f ? Quat{0.f, tw.y / tl, 0.f, tw.w / tl} : Quat{};
    twistDeg = 2.f * std::atan2(tw.y, tw.w) * kDeg;
    if (twistDeg > 180.f) twistDeg -= 360.f;
    if (twistDeg < -180.f) twistDeg += 360.f;
    Quat sw = qmul(d, qconj(tw));
    if (sw.w < 0.f) sw = Quat{-sw.x, -sw.y, -sw.z, -sw.w};
    V3 ax{sw.x, sw.y, sw.z};
    float s = length(ax);
    float ang = 2.f * std::atan2(s, sw.w);
    swingVec = s > 1e-9f ? ax / s * (ang * kDeg) : V3{0, 0, 0};
}

Quat join(const Quat& rest, const V3& swingVec, float twistDeg) {
    float a = length(swingVec);
    Quat sw = a > 1e-6f ? qaxis(swingVec / a, a * kRad) : Quat{};
    Quat tw = qaxis(V3{0, 1, 0}, twistDeg * kRad);
    return qnormalize(qmul(rest, qmul(sw, tw)));
}

// soft limits: identity inside [lo + s, hi - s], easing to (never past) lo / hi
float soft(float x, float lo, float hi) {
    float s = std::min(kSoft, 0.25f * (hi - lo));
    if (x > hi - s) x = hi - s + s * std::tanh((x - (hi - s)) / s);
    else if (x < lo + s) x = lo + s - s * std::tanh(((lo + s) - x) / s);
    return x;
}

}  // namespace

void JointLimits::build(const Model& m) {
    limits_.clear();
    // envelope of the character's own clips, per node: swing, twist, hinge projections (rest-relative)
    struct Env { bool any = false; float swing = 0.f, tMin = 0.f, tMax = 0.f; std::vector<V3> sw; };
    std::vector<Env> env(m.nodes.size());
    for (const Clip& c : m.clips)
        for (const Channel& ch : c.channels) {
            if (ch.path != 1 || ch.node < 0 || ch.node >= (int)m.nodes.size()) continue;
            Env& e = env[(size_t)ch.node];
            const Quat& rest = m.nodes[(size_t)ch.node].r;
            size_t stride = ch.interp == 2 ? 12 : 4, off = ch.interp == 2 ? 4 : 0;
            for (size_t k = 0; k + off + 4 <= ch.values.size(); k += stride) {
                const float* v = &ch.values[k + off];
                V3 sv; float tw;
                split(rest, qnormalize(Quat{v[0], v[1], v[2], v[3]}), sv, tw);
                if (!e.any) { e.tMin = e.tMax = tw; e.any = true; }
                e.swing = std::max(e.swing, length(sv));
                e.tMin = std::min(e.tMin, tw); e.tMax = std::max(e.tMax, tw);
                e.sw.push_back(sv);
            }
        }
    for (size_t i = 0; i < m.nodes.size(); ++i) {
        std::string b = baseName(m.nodes[i].name);
        const Anat* an = nullptr;
        for (const Anat& a : kAnat) if (b == a.name) { an = &a; break; }
        if (!an) continue;
        const Env& e = env[i];
        Limit L;
        L.node = (int)i;
        L.twistMin = -an->twist; L.twistMax = an->twist;
        if (e.any) { L.twistMin = std::min(L.twistMin, e.tMin - kMargin); L.twistMax = std::max(L.twistMax, e.tMax + kMargin); }
        if (L.twistMax - L.twistMin >= 356.f) { L.twistMin = -180.f; L.twistMax = 180.f; }
        if (an->hinge) {
            L.hinge = true;
            // main bending axis = principal direction of the clips' swing vectors (local X when there are none)
            V3 axis{1, 0, 0};
            if (e.sw.size() >= 4) {
                // power iteration on the 3x3 scatter matrix
                float S[9] = {0};
                for (const V3& s : e.sw) {
                    float v[3] = {s.x, s.y, s.z};
                    for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) S[r * 3 + c] += v[r] * v[c];
                }
                V3 x{1.f, 0.1f, 0.1f};
                for (int it = 0; it < 32; ++it) {
                    V3 y{S[0] * x.x + S[1] * x.y + S[2] * x.z, S[3] * x.x + S[4] * x.y + S[5] * x.z, S[6] * x.x + S[7] * x.y + S[8] * x.z};
                    if (length(y) < 1e-9f) break;
                    x = normalize(y);
                }
                float mean = 0.f;
                for (const V3& s : e.sw) mean += dot(s, x);
                axis = mean < 0.f ? x * -1.f : x;
            }
            L.axis = axis;
            L.hingeMin = an->hMin; L.hingeMax = an->hMax;
            L.offAxisMax = an->off;
            for (const V3& s : e.sw) {
                float along = dot(s, axis);
                L.hingeMin = std::min(L.hingeMin, along - kMargin);
                L.hingeMax = std::max(L.hingeMax, along + kMargin);
                L.offAxisMax = std::max(L.offAxisMax, length(s - axis * along) + kMargin);
            }
            L.hingeMin = std::max(L.hingeMin, -180.f); L.hingeMax = std::min(L.hingeMax, 180.f);
        } else {
            L.swingMax = std::min(180.f, std::max(an->swing, e.any ? e.swing + kMargin : 0.f));
        }
        limits_.push_back(L);
    }
    // parents before children: sort by depth
    auto depth = [&](int n) { int d = 0; for (int p = m.nodes[(size_t)n].parent; p >= 0; p = m.nodes[(size_t)p].parent) ++d; return d; };
    std::stable_sort(limits_.begin(), limits_.end(), [&](const Limit& a, const Limit& b) { return depth(a.node) < depth(b.node); });
}

int JointLimits::apply(Animator& a) const {
    if (!enabled) return 0;
    std::vector<int> moved;
    for (const Limit& L : limits_) {
        const Quat& rest = a.restR(L.node);
        Quat& r = a.r(L.node);
        V3 sv; float tw;
        split(rest, r, sv, tw);
        V3 sv2 = sv;
        float tw2 = L.twistMax - L.twistMin >= 356.f ? tw : soft(tw, L.twistMin, L.twistMax);
        if (L.hinge) {
            float along = dot(sv, L.axis);
            V3 off = sv - L.axis * along;
            float ol = length(off);
            float along2 = soft(along, L.hingeMin, L.hingeMax);
            float ol2 = ol > 1e-6f ? soft(ol, -L.offAxisMax, L.offAxisMax) : 0.f;
            sv2 = L.axis * along2 + (ol > 1e-6f ? off * (ol2 / ol) : V3{0, 0, 0});
        } else {
            float sa = length(sv);
            if (sa > 1e-6f && L.swingMax < 179.f) sv2 = sv * (soft(sa, -L.swingMax, L.swingMax) / sa);
        }
        if (length(sv2 - sv) < 0.05f && std::fabs(tw2 - tw) < 0.05f) continue;   // inside the range: untouched
        if (log) AG_INFO("AG: rom %s swing %.1f -> %.1f, twist %.1f -> %.1f", a.model()->nodes[(size_t)L.node].name.c_str(),
                         length(sv), length(sv2), tw, tw2);
        r = join(rest, sv2, tw2);
        moved.push_back(L.node);
    }
    for (int n : moved) a.recomputeSubtree(n);
    return (int)moved.size();
}

}  // namespace ag
