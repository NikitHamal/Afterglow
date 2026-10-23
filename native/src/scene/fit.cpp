#include "scene/fit.h"
#include "core/mathx.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <queue>
#include <unordered_map>

namespace ag {

namespace {

constexpr float kCap = 0.5f;

V3 at(const std::vector<float>& a, size_t i) { return {a[i * 3], a[i * 3 + 1], a[i * 3 + 2]}; }

// vertices that share a position (UV / normal seams, primitive boundaries) become one: welded id per vertex
std::vector<uint32_t> weld(const std::vector<float>& pos, std::vector<uint32_t>& rep) {
    size_t n = pos.size() / 3;
    std::vector<uint32_t> id(n);
    std::unordered_map<uint64_t, uint32_t> seen;
    seen.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) {
        auto q = [&](float v) { return (uint64_t)(int64_t)std::lround(v * 1e5f) & 0x1fffff; };   // 0.01 mm grid
        uint64_t k = q(pos[i * 3]) | q(pos[i * 3 + 1]) << 21 | q(pos[i * 3 + 2]) << 42;
        auto it = seen.find(k);
        if (it == seen.end()) { it = seen.emplace(k, (uint32_t)rep.size()).first; rep.push_back((uint32_t)i); }
        id[i] = it->second;
    }
    return id;
}

uint64_t ekey(uint32_t a, uint32_t b) { return a < b ? (uint64_t)a << 32 | b : (uint64_t)b << 32 | a; }

uint64_t gkey(int64_t x, int64_t y, int64_t z) {
    return ((uint64_t)(x + (1 << 20)) & 0x1fffff) | ((uint64_t)(y + (1 << 20)) & 0x1fffff) << 21 | ((uint64_t)(z + (1 << 20)) & 0x1fffff) << 42;
}

// points in a uniform grid: every point within r of p
struct PointGrid {
    float cell = 0.02f;
    std::unordered_map<uint64_t, std::vector<uint32_t>> cells;
    int64_t c(float v) const { return (int64_t)std::floor(v / cell); }
    void add(const V3& p, uint32_t i) { cells[gkey(c(p.x), c(p.y), c(p.z))].push_back(i); }
    template <class F> void around(const V3& p, float r, F&& f) const {
        for (int64_t x = c(p.x - r); x <= c(p.x + r); ++x)
            for (int64_t y = c(p.y - r); y <= c(p.y + r); ++y)
                for (int64_t z = c(p.z - r); z <= c(p.z + r); ++z) {
                    auto it = cells.find(gkey(x, y, z));
                    if (it != cells.end()) for (uint32_t i : it->second) f(i);
                }
    }
};

}  // namespace

// a stretch of border (2 mm samples, inward directions) made clean, staircase-aware: a broad reference line (8 cm of
// path), the border's offset from it binned by the reference's own length (a staircase path is up to twice as long as
// the line it follows), median over +-3 cm of that length (the steps go, the garment's shape stays), gaussian 1 cm
static void cleanChain(const std::vector<V3>& qp, const std::vector<V3>& qt, bool cyclic, float ds, std::vector<V3>& outP, std::vector<V3>& outIn) {
    const long K = (long)qp.size();
    auto at_ = [&](long k) { return cyclic ? (size_t)(((k % K) + K) % K) : (size_t)std::max(0L, std::min(K - 1, k)); };
    std::vector<V3> ref((size_t)K), rin((size_t)K);
    std::vector<float> keep((size_t)K, 1.f);   // 0 where the border folds back on itself (a strap): left as it is
    {
        float sg = 0.08f / ds;
        long r = (long)std::ceil(3.f * sg);
        for (long k = 0; k < K; ++k) {
            V3 a{0, 0, 0}, b{0, 0, 0};
            float ws = 0.f;
            for (long o = -r; o <= r; ++o) {
                if (!cyclic && (k + o < 0 || k + o >= K)) continue;
                float g = std::exp(-0.5f * (float)(o * o) / (sg * sg));
                size_t j = at_(k + o);
                a += qp[j] * g; b += qt[j] * g; ws += g;
            }
            ref[(size_t)k] = a / ws;
            rin[(size_t)k] = normalize(b);
            keep[(size_t)k] = smoothstep(0.2f, 0.35f, length(b) / ws);   // a strap's two sides face each other
        }
    }
    std::vector<float> u((size_t)K, 0.f), off((size_t)K);
    for (long k = 1; k < K; ++k) u[(size_t)k] = u[(size_t)k - 1] + length(ref[(size_t)k] - ref[(size_t)k - 1]);
    float U = u[(size_t)K - 1] + (cyclic ? length(ref[0] - ref[(size_t)K - 1]) : 0.f);
    for (long k = 0; k < K; ++k) off[(size_t)k] = dot(qp[(size_t)k] - ref[(size_t)k], rin[(size_t)k]);
    const float bstep = 0.002f;
    long KB = std::max(1L, (long)std::ceil(U / bstep));
    float db = U > 1e-6f ? U / (float)KB : bstep;
    std::vector<long> bin((size_t)K);
    std::vector<std::vector<float>> lists((size_t)KB);
    for (long k = 0; k < K; ++k) {
        bin[(size_t)k] = std::min(KB - 1, U > 1e-6f ? (long)(u[(size_t)k] / U * (float)KB) : 0L);
        lists[(size_t)bin[(size_t)k]].push_back(off[(size_t)k]);
    }
    auto bat = [&](long b) { return cyclic ? (size_t)(((b % KB) + KB) % KB) : (size_t)std::max(0L, std::min(KB - 1, b)); };
    std::vector<float> med((size_t)KB, 0.f), win;
    long mr = std::max(1L, (long)std::lround(0.03f / db));
    for (long b = 0; b < KB; ++b) {
        win.clear();
        for (long o = -std::min(mr, cyclic ? KB / 2 : mr); o <= std::min(mr, cyclic ? (KB - 1) / 2 : mr); ++o) {
            if (!cyclic && (b + o < 0 || b + o >= KB)) continue;
            const auto& l = lists[bat(b + o)];
            win.insert(win.end(), l.begin(), l.end());
        }
        if (win.empty()) { med[(size_t)b] = b > 0 ? med[(size_t)b - 1] : 0.f; continue; }
        std::nth_element(win.begin(), win.begin() + (long)(win.size() / 2), win.end());
        med[(size_t)b] = win[win.size() / 2];
    }
    std::vector<float> sm((size_t)KB);
    {
        float sg = 0.01f / db;
        long r = (long)std::ceil(3.f * sg);
        for (long b = 0; b < KB; ++b) {
            float a = 0.f, ws = 0.f;
            for (long o = -r; o <= r; ++o) {
                if (!cyclic && (b + o < 0 || b + o >= KB)) continue;
                float g = std::exp(-0.5f * (float)(o * o) / (sg * sg));
                a += med[bat(b + o)] * g; ws += g;
            }
            sm[(size_t)b] = a / ws;
        }
    }
    outP.resize((size_t)K); outIn.resize((size_t)K);
    for (long k = 0; k < K; ++k) {
        float w = keep[(size_t)k];
        outP[(size_t)k] = lerp(qp[(size_t)k], ref[(size_t)k] + rin[(size_t)k] * sm[(size_t)bin[(size_t)k]], w);
        outIn[(size_t)k] = normalize(lerp(qt[(size_t)k], rin[(size_t)k], w));
    }
}

int fillHoles(std::vector<FitPart>& parts, const std::vector<FitBone>& bones, float maxPerimeter) {
    std::vector<float> P;
    std::vector<uint32_t> I, part;
    std::vector<size_t> base;
    for (size_t pi = 0; pi < parts.size(); ++pi) {
        FitPart& f = parts[pi];
        base.push_back(P.size() / 3);
        if (!f.pos || !f.idx) continue;
        P.insert(P.end(), f.pos->begin(), f.pos->end());
        part.resize(P.size() / 3, (uint32_t)pi);
        for (uint32_t i : *f.idx) I.push_back(i + (uint32_t)base[pi]);
    }
    if (P.empty() || I.size() < 3) return 0;
    std::vector<uint32_t> rep;
    std::vector<uint32_t> w = weld(P, rep);
    std::unordered_map<uint64_t, int> uses;
    for (size_t t = 0; t + 2 < I.size(); t += 3)
        for (int e = 0; e < 3; ++e) {
            uint32_t a = w[I[t + e]], b = w[I[t + (e + 1) % 3]];
            if (a != b) ++uses[ekey(a, b)];
        }
    // border half-edges as their triangle runs them (the cloth on their left), with the primitive that owns them
    std::unordered_map<uint32_t, std::vector<uint32_t>> next;
    std::unordered_map<uint64_t, uint32_t> owner;
    std::unordered_map<uint64_t, uint32_t> local;   // (primitive, welded vertex) -> that primitive's vertex
    for (size_t t = 0; t + 2 < I.size(); t += 3)
        for (int e = 0; e < 3; ++e) {
            uint32_t ga = I[t + e], gb = I[t + (e + 1) % 3], a = w[ga], b = w[gb];
            if (a == b || uses[ekey(a, b)] != 1) continue;
            next[a].push_back(b);
            uint32_t pi = part[ga];
            owner[(uint64_t)a << 32 | b] = pi;
            local.emplace((uint64_t)pi << 32 | a, ga - (uint32_t)base[pi]);
            local.emplace((uint64_t)pi << 32 | b, gb - (uint32_t)base[part[gb]]);
        }
    std::unordered_map<uint32_t, char> done;
    int filled = 0;
    for (auto& kv : next) {
        uint32_t s = kv.first;
        if (done[s] || kv.second.size() != 1) continue;
        std::vector<uint32_t> L{s};
        done[s] = 1;
        bool closed = false;
        for (uint32_t cur = kv.second[0], guard = 0; guard < 4096; ++guard) {
            if (cur == s) { closed = true; break; }
            auto it = next.find(cur);
            if (done[cur] || it == next.end() || it->second.size() != 1) break;   // touching loops: leave them alone
            done[cur] = 1;
            L.push_back(cur);
            cur = it->second[0];
        }
        if (!closed || L.size() < 3) continue;
        // small, and of one primitive
        const size_t Ln = L.size();
        float per = 0.f;
        V3 c{0, 0, 0};
        uint32_t pi = UINT32_MAX;
        bool onePart = true;
        for (size_t i = 0; i < Ln; ++i) {
            V3 a = at(P, rep[L[i]]), b = at(P, rep[L[(i + 1) % Ln]]);
            per += length(b - a);
            c += a;
            auto o = owner.find((uint64_t)L[i] << 32 | L[(i + 1) % Ln]);
            uint32_t op = o == owner.end() ? UINT32_MAX : o->second;
            if (pi == UINT32_MAX) pi = op;
            onePart = onePart && op == pi && op != UINT32_MAX;
        }
        if (per > maxPerimeter || !onePart) continue;
        c = c / (float)Ln;
        // the fill runs the border backwards (its inside then lies on its left, like the cloth's): Newell normal
        std::vector<V3> Q(Ln);
        for (size_t i = 0; i < Ln; ++i) Q[i] = at(P, rep[L[Ln - 1 - i]]);
        V3 N{0, 0, 0};
        float rad = 0.f;
        for (size_t i = 0; i < Ln; ++i) {
            const V3 &a = Q[i], &b = Q[(i + 1) % Ln];
            N += V3{(a.y - b.y) * (a.z + b.z), (a.z - b.z) * (a.x + b.x), (a.x - b.x) * (a.y + b.y)};
            rad = std::max(rad, length(a - c));
        }
        if (length(N) < 1e-9f) continue;
        N = normalize(N);
        // a limb through it: an opening (a cuff, a hem, the neck), not a hole
        bool opening = false;
        for (const FitBone& bn : bones) {
            V3 a{bn.a[0], bn.a[1], bn.a[2]}, b{bn.b[0], bn.b[1], bn.b[2]};
            float d0 = dot(a - c, N), d1 = dot(b - c, N);
            if (d0 * d1 > 0.f || std::fabs(d0 - d1) < 1e-9f) continue;
            V3 hit = a + (b - a) * (d0 / (d0 - d1));
            if (length(hit - c) < rad) { opening = true; break; }
        }
        if (opening) continue;
        // minimum-area triangulation over the border's own vertices (a border around a part of the body is far from
        // flat, so no projection is needed); ties go to short edges. Triangles (i, k, j), i < k < j, run like Q.
        std::vector<float> best(Ln * Ln, 0.f);
        std::vector<int> split(Ln * Ln, -1);
        auto chord = [&](size_t a, size_t b) {   // a diagonal the cloth already has: closing over it would double that edge
            size_t d = a > b ? a - b : b - a;
            return d != 1 && d != Ln - 1 && uses.count(ekey(L[Ln - 1 - a], L[Ln - 1 - b])) > 0;
        };
        auto cost = [&](size_t a, size_t b, size_t d) {
            V3 e1 = Q[b] - Q[a], e2 = Q[d] - Q[a], e3 = Q[d] - Q[b];
            float c = 0.5f * length(cross(e1, e2)) + 0.002f * (dot(e1, e1) + dot(e2, e2) + dot(e3, e3));
            return chord(a, b) || chord(b, d) || chord(a, d) ? c + 1e6f : c;
        };
        for (size_t len = 2; len < Ln; ++len)
            for (size_t i = 0; i + len < Ln; ++i) {
                size_t j = i + len;
                float bv = 1e30f;
                for (size_t k = i + 1; k < j; ++k) {
                    float v = best[i * Ln + k] + best[k * Ln + j] + cost(i, k, j);
                    if (v < bv) { bv = v; split[i * Ln + j] = (int)k; }
                }
                best[i * Ln + j] = bv;
            }
        std::vector<uint32_t> tris;
        std::vector<std::pair<size_t, size_t>> todo{{0, Ln - 1}};
        while (!todo.empty()) {
            auto [i, j] = todo.back();
            todo.pop_back();
            if (j < i + 2) continue;
            size_t k = (size_t)split[i * Ln + j];
            tris.insert(tris.end(), {(uint32_t)i, (uint32_t)k, (uint32_t)j});
            todo.push_back({i, k});
            todo.push_back({k, j});
        }
        // back to the primitive's own vertices
        std::vector<uint32_t> out;
        bool ok = true;
        for (uint32_t q : tris) {
            auto it = local.find((uint64_t)pi << 32 | L[Ln - 1 - q]);
            if (it == local.end()) { ok = false; break; }
            out.push_back(it->second);
        }
        if (!ok) continue;
        parts[pi].idx->insert(parts[pi].idx->end(), out.begin(), out.end());
        parts[pi].idxChanged = true;
        ++filled;
    }
    return filled;
}

std::string fitGarment(std::vector<FitPart>& parts, bool drapeIt) {
    // ---- the layer as one welded surface ----
    std::vector<float> P;
    std::vector<uint32_t> I;
    std::vector<size_t> base;
    for (FitPart& f : parts) {
        size_t b = P.size() / 3;
        base.push_back(b);
        f.border.assign(f.pos ? f.pos->size() / 3 : 0, kCap);
        if (!f.pos || !f.idx) continue;
        P.insert(P.end(), f.pos->begin(), f.pos->end());
        for (uint32_t i : *f.idx) I.push_back(i + (uint32_t)b);
    }
    const size_t n = P.size() / 3;
    if (n == 0 || I.size() < 3) return "no geometry";
    std::vector<uint32_t> rep;
    std::vector<uint32_t> w = weld(P, rep);
    const size_t m = rep.size();
    std::vector<V3> X(m), VN(m, V3{0, 0, 0});
    for (size_t i = 0; i < m; ++i) X[i] = at(P, rep[i]);
    std::unordered_map<uint64_t, int> uses;
    uses.reserve(I.size() * 2);
    for (size_t t = 0; t + 2 < I.size(); t += 3) {
        uint32_t a = w[I[t]], b = w[I[t + 1]], c = w[I[t + 2]];
        if (a == b || b == c || c == a) continue;
        V3 fn = cross(X[b] - X[a], X[c] - X[a]);   // by winding, area weighted
        VN[a] += fn; VN[b] += fn; VN[c] += fn;
        ++uses[ekey(a, b)]; ++uses[ekey(b, c)]; ++uses[ekey(c, a)];
    }
    // directed border half-edges (their triangle on the left, by its winding); all edges for the geodesic distance
    struct HalfEdge { uint32_t to; bool used; };
    std::vector<std::vector<HalfEdge>> outB(m);
    std::vector<std::vector<std::pair<uint32_t, float>>> adj(m);
    for (size_t t = 0; t + 2 < I.size(); t += 3) {
        uint32_t v[3] = {w[I[t]], w[I[t + 1]], w[I[t + 2]]};
        if (v[0] == v[1] || v[1] == v[2] || v[2] == v[0]) continue;
        for (int e = 0; e < 3; ++e) {
            uint32_t a = v[e], b = v[(e + 1) % 3];
            float len = length(X[a] - X[b]);
            adj[a].push_back({b, len});
            adj[b].push_back({a, len});
            if (uses[ekey(a, b)] == 1) outB[a].push_back({b, false});
        }
    }
    // ---- border loops: follow the half-edges; where two loops touch at a vertex, take the sharpest right turn
    // (the cloth stays on the left), so touching loops never merge ----
    struct Loop { std::vector<uint32_t> v; bool closed = false; };
    std::vector<Loop> loops;
    for (int pass = 0; pass < 2; ++pass)
        for (uint32_t s = 0; s < m; ++s) {
            if (pass == 0 && outB[s].size() != 1) continue;   // start where a loop passes once
            for (size_t h0 = 0; h0 < outB[s].size(); ++h0) {
                if (outB[s][h0].used) continue;
                Loop L;
                L.v.push_back(s);
                outB[s][h0].used = true;
                uint32_t prev = s, cur = outB[s][h0].to;
                for (size_t guard = 0; guard <= m; ++guard) {
                    if (cur == s) { L.closed = true; break; }
                    L.v.push_back(cur);
                    V3 din = X[cur] - X[prev], nn = normalize(VN[cur]);
                    int best = -1;
                    float bestA = 1e9f;
                    for (size_t k = 0; k < outB[cur].size(); ++k) {
                        if (outB[cur][k].used) continue;
                        V3 dout = X[outB[cur][k].to] - X[cur];
                        float ang = std::atan2(dot(cross(din, dout), nn), dot(din, dout));
                        if (ang < bestA) { bestA = ang; best = (int)k; }
                    }
                    if (best < 0) break;   // an open chain (the winding flips somewhere)
                    outB[cur][(size_t)best].used = true;
                    prev = cur;
                    cur = outB[cur][(size_t)best].to;
                }
                if (L.v.size() >= 3) loops.push_back(std::move(L));
            }
        }
    if (loops.empty()) return "closed surface: no border";
    // ---- each loop made clean: a very smooth reference line, the border's offset from it median-filtered over 8 cm
    // (the steps of the body's triangles go, the garment's own shape stays) and smoothed again; corners (a sharp turn
    // between straight arms, a V neckline) are kept exactly ----
    struct CurvePt { V3 p, in; uint32_t loop; };
    std::vector<CurvePt> curve;
    const float step = 0.002f;
    int nCorners = 0;
    float maxOff = 0.f;
    for (size_t li = 0; li < loops.size(); ++li) {
        const Loop& L = loops[li];
        const size_t Lc = L.v.size();
        std::vector<V3> R(Lc), T(Lc);
        for (size_t i = 0; i < Lc; ++i) {
            R[i] = X[L.v[i]];
            size_t ip = i == 0 ? (L.closed ? Lc - 1 : 0) : i - 1, inx = i + 1 == Lc ? (L.closed ? 0 : Lc - 1) : i + 1;
            T[i] = normalize(cross(normalize(VN[L.v[i]]), normalize(X[L.v[inx]] - X[L.v[ip]])));   // into the cloth
        }
        std::vector<float> arc(Lc + 1, 0.f);
        for (size_t i = 1; i <= Lc; ++i) arc[i] = arc[i - 1] + length(R[i % Lc] - R[i - 1]);
        float total = L.closed ? arc[Lc] : arc[Lc - 1];
        if (total < 1e-3f) continue;
        size_t K = std::max<size_t>(8, (size_t)(total / step));
        std::vector<V3> qp(K), qt(K);
        size_t seg = 0;
        for (size_t k = 0; k < K; ++k) {
            float sk = total * (float)k / (float)(L.closed ? K : K - 1);
            while (seg + 1 < Lc + (L.closed ? 1 : 0) && arc[seg + 1] < sk) ++seg;
            size_t i0 = seg % Lc, i1 = (seg + 1) % Lc;
            float span = arc[seg + 1] - arc[seg], f = span > 1e-9f ? (sk - arc[seg]) / span : 0.f;
            if (!L.closed && seg + 1 >= Lc) { i1 = i0; f = 0.f; }
            qp[k] = lerp(R[i0], R[i1], f);
            qt[k] = lerp(T[i0], T[i1], f);
        }
        const float ds = total / (float)(L.closed ? K : K - 1);
        auto wrap = [&](long k) { return L.closed ? (size_t)(((k % (long)K) + (long)K) % (long)K) : (size_t)std::max(0L, std::min((long)K - 1, k)); };
        // corners (a V neckline): a turn of more than 60 degrees between two arms that are straight (within 15
        // degrees) over 4 and over 6 cm; the steps of a staircase or a rounded neckline never are
        int h = std::max(4, (int)std::lround(0.04f / ds));
        std::vector<float> ta(K, 0.f);
        auto dir = [&](long a0, long a1) { return normalize(qp[wrap(a1)] - qp[wrap(a0)]); };
        if (K > (size_t)(7 * h))
            for (size_t k = 0; k < K; ++k) {
                long kk = (long)k;
                if (!L.closed && (kk < 3 * h || kk >= (long)K - 3 * h)) continue;
                float turnA = std::acos(clampf(dot(dir(kk - h, kk), dir(kk, kk + h)), -1.f, 1.f));
                bool straight = true;
                for (long hh : {(long)h, (long)(3 * h / 2)})
                    straight = straight && dot(dir(kk - 2 * hh, kk - hh), dir(kk - hh, kk)) > 0.966f &&
                               dot(dir(kk, kk + hh), dir(kk + hh, kk + 2 * hh)) > 0.966f;
                if (turnA > 1.047f && straight) ta[k] = turnA;
            }
        std::vector<long> ck;
        for (size_t k = 0; k < K; ++k) {
            if (ta[k] <= 0.f) continue;
            bool peak = true;
            for (int o = -h; o <= h && peak; ++o) {
                if (o == 0) continue;
                long j = (long)k + o;
                if (!L.closed && (j < 0 || j >= (long)K)) continue;
                size_t jj = wrap(j);
                if (ta[jj] > ta[k] || (ta[jj] == ta[k] && o < 0)) peak = false;
            }
            if (peak) ck.push_back((long)k);
        }
        nCorners += (int)ck.size();
        if (!L.closed) { ck.insert(ck.begin(), 0L); ck.push_back((long)K - 1); }
        // chains between corners (the whole loop, cyclic, when it has none), each cleaned; within 1.5 cm of a corner
        // the curve follows the border itself
        std::vector<float> dc(K, 1e9f);
        for (long c : ck)
            for (size_t k = 0; k < K; ++k) {
                long d = std::labs((long)k - c);
                if (L.closed) d = std::min(d, (long)K - d);
                dc[k] = std::min(dc[k], (float)d * ds);
            }
        std::vector<V3> cp(K), cin(K);
        if (ck.empty()) cleanChain(qp, qt, L.closed, ds, cp, cin);
        else {
            for (size_t ci = 0; ci < ck.size(); ++ci) {
                long c0 = ck[ci], c1 = ci + 1 < ck.size() ? ck[ci + 1] : (L.closed ? ck[0] + (long)K : -1);
                if (c1 < 0) break;
                std::vector<V3> sp, st, op, oi;
                for (long k = c0; k <= c1; ++k) { sp.push_back(qp[wrap(k)]); st.push_back(qt[wrap(k)]); }
                if (sp.size() < 3) { for (long k = c0; k <= c1; ++k) { cp[wrap(k)] = qp[wrap(k)]; cin[wrap(k)] = qt[wrap(k)]; } continue; }
                cleanChain(sp, st, false, ds, op, oi);
                for (long k = c0; k <= c1; ++k) { cp[wrap(k)] = op[(size_t)(k - c0)]; cin[wrap(k)] = oi[(size_t)(k - c0)]; }
            }
        }
        for (size_t k = 0; k < K; ++k) {
            float wgt = smoothstep(0.f, 0.015f, dc[k]);   // a corner stays exactly where it is
            maxOff = std::max(maxOff, length(cp[k] - qp[k]) * wgt);
            curve.push_back({lerp(qp[k], cp[k], wgt), normalize(lerp(qt[k], normalize(cin[k]), wgt)), (uint32_t)li});
        }
    }
    // ---- distance to the border (Dijkstra over the edges), carrying which loop is the nearest ----
    std::vector<float> geo(m, kCap);
    std::vector<int> src(m, -1);
    using QE = std::pair<float, uint32_t>;
    std::priority_queue<QE, std::vector<QE>, std::greater<QE>> pq;
    for (size_t li = 0; li < loops.size(); ++li)
        for (uint32_t v : loops[li].v) { geo[v] = 0.f; src[v] = (int)li; pq.push({0.f, v}); }
    while (!pq.empty()) {
        auto [d, v] = pq.top(); pq.pop();
        if (d > geo[v]) continue;
        for (auto& [u, len] : adj[v])
            if (d + len < geo[u]) { geo[u] = d + len; src[u] = src[v]; pq.push({d + len, u}); }
    }
    PointGrid grid;
    for (uint32_t i = 0; i < curve.size(); ++i) grid.add(curve[i].p, i);
    auto nearest = [&](const V3& p, int li, float r, const CurvePt** out) {
        float best = r;
        *out = nullptr;
        grid.around(p, r, [&](uint32_t i) {
            if ((int)curve[i].loop != li) return;
            float d = length(p - curve[i].p);
            if (d < best) { best = d; *out = &curve[i]; }
        });
        return *out != nullptr;
    };
    // ---- the cut sits inside every notch: each loop's clean line moves into the cloth by the notch depth around it
    // (a smooth envelope; a small loop such as a cuff moves as a whole, so it stays a straight cut). Nothing is moved
    // on the garment itself: a vertex slid along the surface would sink under the skin where the body curves in ----
    std::vector<std::vector<uint32_t>> loopCurve(loops.size());
    for (uint32_t i = 0; i < curve.size(); ++i) loopCurve[curve[i].loop].push_back(i);
    float maxShift = 0.f;
    for (size_t li = 0; li < loops.size(); ++li) {
        auto& lc = loopCurve[li];
        if (lc.empty()) continue;
        std::vector<std::pair<V3, float>> notch;   // foot on the line, depth inside it
        for (uint32_t v : loops[li].v) {
            const CurvePt* c = nullptr;
            if (!nearest(X[v], (int)li, 0.12f, &c)) continue;
            float d = dot(X[v] - c->p, c->in);
            if (d > 0.f) notch.push_back({c->p, d});
        }
        if (notch.empty()) continue;
        float len = 0.f;
        for (size_t k = 1; k < lc.size(); ++k) len += length(curve[lc[k]].p - curve[lc[k - 1]].p);
        const float margin = 0.001f, R = 0.15f;
        std::vector<float> e(lc.size(), 0.f);
        if (len < 0.15f) {
            float d = 0.f;
            for (auto& nt : notch) d = std::max(d, nt.second);
            std::fill(e.begin(), e.end(), d + margin);
        } else {
            for (size_t k = 0; k < lc.size(); ++k)
                for (auto& nt : notch) {
                    float dd = length(curve[lc[k]].p - nt.first);
                    if (dd < 0.12f) e[k] = std::max(e[k], nt.second + margin - dd * dd / (2.f * R));
                }
        }
        for (size_t k = 0; k < lc.size(); ++k) {
            curve[lc[k]].p += curve[lc[k]].in * std::min(e[k], 0.035f);
            maxShift = std::max(maxShift, e[k]);
        }
    }
    grid = PointGrid{};
    for (uint32_t i = 0; i < curve.size(); ++i) grid.add(curve[i].p, i);
    // ---- signed distance to the smoothed border (m, + inside), against the vertex's own nearest loop ----
    std::vector<float> sd(m, kCap);
    for (uint32_t v = 0; v < m; ++v) {
        if (src[v] < 0 || geo[v] > 0.08f) { sd[v] = std::min(kCap, geo[v]); continue; }   // far in: only "far" matters
        const CurvePt* c = nullptr;
        if (!nearest(X[v], src[v], 0.12f, &c)) { sd[v] = geo[v]; continue; }
        sd[v] = dot(X[v] - c->p, c->in);   // never the distance to the raw border: that is the staircase again
    }
    // ---- drape: Taubin smoothing (no shrinking) of the bind shape, pinned within 3 cm of the border ----
    float moved = 0.f;
    if (drapeIt) {
        std::vector<std::vector<uint32_t>> nb(m);
        for (uint32_t a = 0; a < m; ++a) {
            for (auto& e : adj[a]) nb[a].push_back(e.first);
            std::sort(nb[a].begin(), nb[a].end());
            nb[a].erase(std::unique(nb[a].begin(), nb[a].end()), nb[a].end());
        }
        std::vector<V3> p = X, q(m);
        std::vector<float> k(m);
        for (uint32_t i = 0; i < m; ++i) k[i] = smoothstep(0.f, 0.03f, sd[i]);
        auto pass = [&](float f) {
            for (uint32_t i = 0; i < m; ++i) {
                if (nb[i].empty() || k[i] <= 0.f) { q[i] = p[i]; continue; }
                V3 c{0, 0, 0};
                for (uint32_t j : nb[i]) c += p[j];
                c = c / (float)nb[i].size();
                q[i] = p[i] + (c - p[i]) * (f * k[i]);
            }
            p.swap(q);
        };
        for (int it = 0; it < 12; ++it) { pass(0.5f); pass(-0.53f); }
        // cloth bridges hollows but never sinks: smoothing pulls convex places (chest, shoulders) inward, and cloth
        // below its own offset surface ends up under the skin, which then shows in front of it. At most 1.5 mm in.
        for (uint32_t i = 0; i < m; ++i) {
            V3 dlt = p[i] - X[i], n = normalize(VN[i]);
            float dn = dot(dlt, n);
            if (dn < -0.0015f) dlt -= n * (dn + 0.0015f);
            moved = std::max(moved, length(dlt));
            X[i] += dlt;
        }
    }
    // ---- back to the primitives ----
    for (size_t pi = 0; pi < parts.size(); ++pi) {
        FitPart& f = parts[pi];
        if (!f.pos) continue;
        size_t nv = f.pos->size() / 3;
        for (size_t i = 0; i < nv; ++i) {
            uint32_t g = w[base[pi] + i];
            (*f.pos)[i * 3] = X[g].x; (*f.pos)[i * 3 + 1] = X[g].y; (*f.pos)[i * 3 + 2] = X[g].z;
            f.border[i] = sd[g];
        }
    }
    char buf[200];
    std::snprintf(buf, sizeof buf, "%zu border loops, %d corners, steps up to %.0f mm, cut moved in by up to %.0f mm%s",
                  loops.size(), nCorners, maxOff * 1000.f, maxShift * 1000.f,
                  drapeIt ? (", draped (up to " + std::to_string((int)std::lround(moved * 1000.f)) + " mm)").c_str() : "");
    return buf;
}

std::vector<float> coverage(const std::vector<float>& pos, const std::vector<float>& nrm, const std::vector<FitSurface>& over,
                            float reach, float inner, float margin0, float margin1, float erode) {
    const size_t n = pos.size() / 3;
    std::vector<float> cov(n, 0.f);
    if (n == 0 || nrm.size() != pos.size() || over.empty()) return cov;
    // every garment triangle in a uniform grid
    struct Tri { V3 a, e1, e2, nrm; float b0, b1, b2; };
    std::vector<Tri> tris;
    for (const FitSurface& s : over) {
        if (!s.pos || !s.idx || !s.border) continue;
        for (size_t t = 0; t + 2 < s.idx->size(); t += 3) {
            uint32_t i0 = (*s.idx)[t], i1 = (*s.idx)[t + 1], i2 = (*s.idx)[t + 2];
            V3 a = at(*s.pos, i0), b = at(*s.pos, i1), c = at(*s.pos, i2);
            V3 fn = cross(b - a, c - a);
            if (length(fn) < 1e-12f) continue;
            // the garment's outward side: its vertex normals (garments may be wound either way)
            V3 vn = s.nrm && s.nrm->size() == s.pos->size() ? at(*s.nrm, i0) + at(*s.nrm, i1) + at(*s.nrm, i2) : fn;
            tris.push_back({a, b - a, c - a, normalize(length(vn) > 1e-9f ? vn : fn), (*s.border)[i0], (*s.border)[i1], (*s.border)[i2]});
        }
    }
    if (tris.empty()) return cov;
    const float cell = 0.025f;
    auto ci = [&](float v) { return (int64_t)std::floor(v / cell); };
    auto key = [&](int64_t x, int64_t y, int64_t z) { return gkey(x, y, z); };
    std::unordered_map<uint64_t, std::vector<uint32_t>> grid;
    for (uint32_t t = 0; t < tris.size(); ++t) {
        const Tri& T = tris[t];
        V3 b = T.a + T.e1, c = T.a + T.e2;
        V3 lo{std::min({T.a.x, b.x, c.x}), std::min({T.a.y, b.y, c.y}), std::min({T.a.z, b.z, c.z})};
        V3 hi{std::max({T.a.x, b.x, c.x}), std::max({T.a.y, b.y, c.y}), std::max({T.a.z, b.z, c.z})};
        for (int64_t x = ci(lo.x); x <= ci(hi.x); ++x)
            for (int64_t y = ci(lo.y); y <= ci(hi.y); ++y)
                for (int64_t z = ci(lo.z); z <= ci(hi.z); ++z) grid[key(x, y, z)].push_back(t);
    }
    std::vector<uint32_t> stamp(tris.size(), UINT32_MAX);
    for (size_t v = 0; v < n; ++v) {
        V3 p = at(pos, v), d = normalize(at(nrm, v));
        if (length(d) < 0.5f) continue;
        V3 o = p - d * inner;
        float len = inner + reach;
        V3 e = o + d * len;
        float best = 0.f;
        for (int64_t x = ci(std::min(o.x, e.x)); x <= ci(std::max(o.x, e.x)); ++x)
            for (int64_t y = ci(std::min(o.y, e.y)); y <= ci(std::max(o.y, e.y)); ++y)
                for (int64_t z = ci(std::min(o.z, e.z)); z <= ci(std::max(o.z, e.z)); ++z) {
                    auto it = grid.find(key(x, y, z));
                    if (it == grid.end()) continue;
                    for (uint32_t t : it->second) {
                        if (stamp[t] == (uint32_t)v) continue;
                        stamp[t] = (uint32_t)v;
                        const Tri& T = tris[t];
                        if (dot(T.nrm, d) < 0.5f) continue;            // a garment facing the other way (an arm's sleeve)
                        // Moller-Trumbore
                        V3 pv = cross(d, T.e2);
                        float det = dot(T.e1, pv);
                        if (std::fabs(det) < 1e-12f) continue;
                        float inv = 1.f / det;
                        V3 tv = o - T.a;
                        float u = dot(tv, pv) * inv;
                        if (u < 0.f || u > 1.f) continue;
                        V3 qv = cross(tv, T.e1);
                        float w = dot(d, qv) * inv;
                        if (w < 0.f || u + w > 1.f) continue;
                        float tt = dot(T.e2, qv) * inv;
                        if (tt < 0.f || tt > len) continue;
                        float b = T.b0 * (1.f - u - w) + T.b1 * u + T.b2 * w;
                        best = std::max(best, smoothstep(margin0, margin1, b));
                    }
                }
        cov[v] = best;
    }
    if (erode <= 0.f) return cov;
    // erosion: a vertex keeps the least cover found within `erode` of it
    std::unordered_map<uint64_t, std::vector<uint32_t>> vg;
    auto vc = [&](float v) { return (int64_t)std::floor(v / erode); };
    for (uint32_t v = 0; v < n; ++v) vg[key(vc(pos[v * 3]), vc(pos[v * 3 + 1]), vc(pos[v * 3 + 2]))].push_back(v);
    std::vector<float> er(cov);
    for (uint32_t v = 0; v < n; ++v) {
        if (cov[v] <= 0.f) continue;
        V3 p = at(pos, v);
        int64_t x0 = vc(p.x), y0 = vc(p.y), z0 = vc(p.z);
        for (int64_t x = x0 - 1; x <= x0 + 1; ++x)
            for (int64_t y = y0 - 1; y <= y0 + 1; ++y)
                for (int64_t z = z0 - 1; z <= z0 + 1; ++z) {
                    auto it = vg.find(key(x, y, z));
                    if (it == vg.end()) continue;
                    for (uint32_t u : it->second)
                        if (cov[u] < er[v] && length(at(pos, u) - p) <= erode) er[v] = cov[u];
                }
    }
    return er;
}

}  // namespace ag
