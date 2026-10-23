#include "scene/fit.h"
#include "core/mathx.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <queue>
#include <unordered_map>

namespace ag {

namespace {

V3 at(const std::vector<float>& a, size_t i) { return {a[i * 3], a[i * 3 + 1], a[i * 3 + 2]}; }

// vertices that share a position (UV / normal seams) become one: welded id per vertex
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

}  // namespace

std::vector<float> borderDistance(const std::vector<float>& pos, const std::vector<float>& nrm, const std::vector<uint32_t>& idx,
                                  float smooth, float cap) {
    const size_t n = pos.size() / 3;
    std::vector<float> out(n, cap);
    if (n == 0 || idx.size() < 3) return out;
    std::vector<uint32_t> rep;
    std::vector<uint32_t> w = weld(pos, rep);
    const size_t m = rep.size();
    // edges by use: an edge of one triangle is on the open border; its triangle's third corner points inside
    std::unordered_map<uint64_t, int> uses;
    uses.reserve(idx.size() * 2);
    for (size_t t = 0; t + 2 < idx.size(); t += 3)
        for (int e = 0; e < 3; ++e) {
            uint32_t a = w[idx[t + e]], b = w[idx[t + (e + 1) % 3]];
            if (a != b) ++uses[ekey(a, b)];
        }
    std::vector<std::vector<std::pair<uint32_t, float>>> adj(m);
    std::vector<std::vector<uint32_t>> bnext(m);
    std::vector<V3> inward(m, V3{0, 0, 0});
    for (size_t t = 0; t + 2 < idx.size(); t += 3)
        for (int e = 0; e < 3; ++e) {
            uint32_t a = w[idx[t + e]], b = w[idx[t + (e + 1) % 3]], c = w[idx[t + (e + 2) % 3]];
            if (a == b) continue;
            float len = length(at(pos, rep[a]) - at(pos, rep[b]));
            adj[a].push_back({b, len});
            adj[b].push_back({a, len});
            if (uses[ekey(a, b)] == 1) {
                bnext[a].push_back(b);
                bnext[b].push_back(a);
                V3 mid = (at(pos, rep[a]) + at(pos, rep[b])) * 0.5f, in = at(pos, rep[c]) - mid;
                inward[a] = inward[a] + in;
                inward[b] = inward[b] + in;
            }
        }
    // geodesic distance to the border (Dijkstra over the edges): far from it nothing below matters
    std::vector<float> geo(m, cap);
    using QE = std::pair<float, uint32_t>;
    std::priority_queue<QE, std::vector<QE>, std::greater<QE>> pq;
    for (uint32_t v = 0; v < m; ++v) if (!bnext[v].empty()) { geo[v] = 0.f; pq.push({0.f, v}); }
    if (pq.empty()) return out;   // a closed surface: no border to trim
    while (!pq.empty()) {
        auto [d, v] = pq.top(); pq.pop();
        if (d > geo[v]) continue;
        for (auto& [u, len] : adj[v])
            if (d + len < geo[u]) { geo[u] = d + len; pq.push({d + len, u}); }
    }
    // border loops, resampled every 2 mm, smoothed (gaussian over `smooth` metres) with their inward direction
    struct CurvePt { V3 p, in; };
    std::vector<CurvePt> curve;
    std::vector<char> done(m, 0);
    for (uint32_t s = 0; s < m; ++s) {
        if (bnext[s].empty() || done[s]) continue;
        std::vector<uint32_t> loop{s};
        done[s] = 1;
        uint32_t prev = s, cur = s;
        bool closed = false;
        for (;;) {
            uint32_t nx = UINT32_MAX;
            for (uint32_t c : bnext[cur]) if (c != prev && (!done[c] || (c == s && loop.size() > 2))) { nx = c; break; }
            if (nx == UINT32_MAX) break;
            if (nx == s) { closed = true; break; }
            loop.push_back(nx); done[nx] = 1; prev = cur; cur = nx;
        }
        if (loop.size() < 3) continue;
        // per border vertex: position, inward tangent within the surface (normal x loop tangent, toward the inside)
        size_t L = loop.size();
        std::vector<V3> P(L), T(L);
        for (size_t i = 0; i < L; ++i) {
            P[i] = at(pos, rep[loop[i]]);
            V3 a = at(pos, rep[loop[i == 0 ? (closed ? L - 1 : 0) : i - 1]]), b = at(pos, rep[loop[i + 1 == L ? (closed ? 0 : L - 1) : i + 1]]);
            V3 tan = normalize(b - a), nr = nrm.empty() ? V3{0, 1, 0} : normalize(at(nrm, rep[loop[i]]));
            V3 t = normalize(cross(nr, tan));
            if (dot(t, inward[loop[i]]) < 0.f) t = t * -1.f;
            if (length(t) < 1e-6f) t = normalize(inward[loop[i]]);
            T[i] = t;
        }
        std::vector<float> arc(L + 1, 0.f);
        for (size_t i = 1; i <= L; ++i) arc[i] = arc[i - 1] + length(P[i % L] - P[i - 1]);
        float total = closed ? arc[L] : arc[L - 1];
        if (total < 1e-4f) continue;
        const float step = 0.002f;
        size_t K = std::max<size_t>(8, (size_t)(total / step));
        std::vector<V3> qp(K), qt(K);
        size_t seg = 0;
        for (size_t k = 0; k < K; ++k) {
            float sk = total * (float)k / (float)(closed ? K : K - 1);
            while (seg + 1 < L + (closed ? 1 : 0) && arc[seg + 1] < sk) ++seg;
            size_t i0 = seg % L, i1 = (seg + 1) % L;
            float span = arc[seg + 1] - arc[seg], f = span > 1e-9f ? (sk - arc[seg]) / span : 0.f;
            if (!closed && seg + 1 >= L) { i1 = i0; f = 0.f; }
            qp[k] = lerp(P[i0], P[i1], f);
            qt[k] = lerp(T[i0], T[i1], f);
        }
        float sig = std::max(step, smooth) / (total / (float)K);
        int rad = (int)std::ceil(sig * 3.f);
        for (size_t k = 0; k < K; ++k) {
            V3 sp{0, 0, 0}, st{0, 0, 0};
            float ws = 0.f;
            for (int o = -rad; o <= rad; ++o) {
                long j = (long)k + o;
                if (closed) j = ((j % (long)K) + (long)K) % (long)K;
                else if (j < 0 || j >= (long)K) continue;
                float g = std::exp(-0.5f * (float)(o * o) / (sig * sig));
                sp = sp + qp[(size_t)j] * g; st = st + qt[(size_t)j] * g; ws += g;
            }
            curve.push_back({sp / ws, normalize(st)});
        }
    }
    if (curve.empty()) return out;
    // signed distance near the border: against the nearest smoothed curve point, along its inward direction
    const float near = std::max(0.04f, smooth * 3.f), cell = 0.02f;
    std::unordered_map<uint64_t, std::vector<uint32_t>> grid;
    auto key = [&](const V3& p, int dx, int dy, int dz) {
        auto c = [&](float v, int d) { return (uint64_t)((int64_t)std::floor(v / cell) + d + (1 << 20)) & 0x1fffff; };
        return c(p.x, dx) | c(p.y, dy) << 21 | c(p.z, dz) << 42;
    };
    for (uint32_t i = 0; i < curve.size(); ++i) grid[key(curve[i].p, 0, 0, 0)].push_back(i);
    std::vector<float> sd(m, cap);
    int r = (int)std::ceil(near / cell);
    for (uint32_t v = 0; v < m; ++v) {
        if (geo[v] > near) { sd[v] = std::min(cap, geo[v]); continue; }
        V3 p = at(pos, rep[v]);
        float best = 1e9f, s = geo[v];
        for (int dx = -r; dx <= r; ++dx)
            for (int dy = -r; dy <= r; ++dy)
                for (int dz = -r; dz <= r; ++dz) {
                    auto it = grid.find(key(p, dx, dy, dz));
                    if (it == grid.end()) continue;
                    for (uint32_t ci : it->second) {
                        float d = length(p - curve[ci].p);
                        if (d < best) { best = d; s = dot(p - curve[ci].p, curve[ci].in); }
                    }
                }
        sd[v] = std::min(s, geo[v] + 0.01f);
    }
    for (size_t i = 0; i < n; ++i) out[i] = sd[w[i]];
    return out;
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
    auto key = [&](int64_t x, int64_t y, int64_t z) {
        return ((uint64_t)(x + (1 << 20)) & 0x1fffff) | ((uint64_t)(y + (1 << 20)) & 0x1fffff) << 21 | ((uint64_t)(z + (1 << 20)) & 0x1fffff) << 42;
    };
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

void drape(std::vector<float>& pos, const std::vector<uint32_t>& idx, const std::vector<float>& border, int iterations, float pin) {
    const size_t n = pos.size() / 3;
    if (n == 0 || idx.size() < 3 || border.size() != n) return;
    std::vector<uint32_t> rep;
    std::vector<uint32_t> w = weld(pos, rep);
    const size_t m = rep.size();
    std::vector<std::vector<uint32_t>> nb(m);
    for (size_t t = 0; t + 2 < idx.size(); t += 3)
        for (int e = 0; e < 3; ++e) {
            uint32_t a = w[idx[t + e]], b = w[idx[t + (e + 1) % 3]];
            if (a == b) continue;
            nb[a].push_back(b); nb[b].push_back(a);
        }
    for (auto& v : nb) { std::sort(v.begin(), v.end()); v.erase(std::unique(v.begin(), v.end()), v.end()); }
    std::vector<V3> p(m), q(m);
    std::vector<float> k(m);
    for (uint32_t i = 0; i < m; ++i) { p[i] = at(pos, rep[i]); k[i] = smoothstep(0.f, pin, border[rep[i]]); }
    auto pass = [&](float f) {
        for (uint32_t i = 0; i < m; ++i) {
            if (nb[i].empty() || k[i] <= 0.f) { q[i] = p[i]; continue; }
            V3 c{0, 0, 0};
            for (uint32_t j : nb[i]) c = c + p[j];
            c = c / (float)nb[i].size();
            q[i] = p[i] + (c - p[i]) * (f * k[i]);
        }
        p.swap(q);
    };
    for (int it = 0; it < iterations; ++it) { pass(0.5f); pass(-0.53f); }
    for (size_t i = 0; i < n; ++i) { const V3& v = p[w[i]]; pos[i * 3] = v.x; pos[i * 3 + 1] = v.y; pos[i * 3 + 2] = v.z; }
}

}  // namespace ag
