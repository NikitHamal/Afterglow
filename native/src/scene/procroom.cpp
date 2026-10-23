#include "scene/procroom.h"
#include "gfx/texture.h"
#include <cmath>
#include <functional>
#include <vector>

namespace ag {

namespace {

struct MeshData { std::vector<float> pos, nrm, uv; std::vector<uint32_t> idx; };

float hash2(int x, int y) { unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u; h = (h ^ (h >> 13)) * 1274126177u; return (float)((h ^ (h >> 16)) & 0xffffff) / 16777215.f; }
float vnoise(float x, float y) {
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float fx = x - (float)xi, fy = y - (float)yi;
    float ux = fx * fx * (3 - 2 * fx), uy = fy * fy * (3 - 2 * fy);
    float a = hash2(xi, yi), b = hash2(xi + 1, yi), c = hash2(xi, yi + 1), d = hash2(xi + 1, yi + 1);
    return lerpf(lerpf(a, b, ux), lerpf(c, d, ux), uy);
}
float fbm(float x, float y, int oct) { float s = 0, a = 0.5f, f = 1; for (int i = 0; i < oct; ++i) { s += a * vnoise(x * f, y * f); f *= 2.03f; a *= 0.5f; } return s; }

void recomputeNormals(MeshData& m) {
    m.nrm.assign(m.pos.size(), 0.f);
    for (size_t t = 0; t + 2 < m.idx.size(); t += 3) {
        uint32_t a = m.idx[t], b = m.idx[t + 1], c = m.idx[t + 2];
        V3 pa{m.pos[a * 3], m.pos[a * 3 + 1], m.pos[a * 3 + 2]}, pb{m.pos[b * 3], m.pos[b * 3 + 1], m.pos[b * 3 + 2]}, pc{m.pos[c * 3], m.pos[c * 3 + 1], m.pos[c * 3 + 2]};
        V3 n = cross(pb - pa, pc - pa);
        for (uint32_t v : {a, b, c}) { m.nrm[v * 3] += n.x; m.nrm[v * 3 + 1] += n.y; m.nrm[v * 3 + 2] += n.z; }
    }
    for (size_t v = 0; v * 3 < m.nrm.size(); ++v) {
        V3 n = normalize(V3{m.nrm[v * 3], m.nrm[v * 3 + 1], m.nrm[v * 3 + 2]});
        m.nrm[v * 3] = n.x; m.nrm[v * 3 + 1] = n.y; m.nrm[v * 3 + 2] = n.z;
    }
}

// flip winding/normals so they agree with expected(p) on average
void orientBy(MeshData& m, const std::function<V3(V3)>& expected) {
    float sum = 0.f;
    for (size_t v = 0; v * 3 < m.pos.size(); ++v) {
        V3 p{m.pos[v * 3], m.pos[v * 3 + 1], m.pos[v * 3 + 2]}, n{m.nrm[v * 3], m.nrm[v * 3 + 1], m.nrm[v * 3 + 2]};
        sum += dot(n, expected(p));
    }
    if (sum >= 0.f) return;
    for (size_t t = 0; t + 2 < m.idx.size(); t += 3) std::swap(m.idx[t + 1], m.idx[t + 2]);
    for (auto& x : m.nrm) x = -x;
}
MeshData facing(MeshData m, V3 dir) { orientBy(m, [&](V3) { return dir; }); return m; }

// grid over parameter domain, positions from f(u,v) -> V3 (smooth normals recomputed)
MeshData grid(int nu, int nv, const std::function<V3(float, float)>& f, float uvScaleU, float uvScaleV) {
    MeshData m;
    for (int j = 0; j <= nv; ++j)
        for (int i = 0; i <= nu; ++i) {
            float u = (float)i / (float)nu, v = (float)j / (float)nv;
            V3 p = f(u, v);
            m.pos.insert(m.pos.end(), {p.x, p.y, p.z});
            m.uv.insert(m.uv.end(), {u * uvScaleU, v * uvScaleV});
        }
    for (int j = 0; j < nv; ++j)
        for (int i = 0; i < nu; ++i) {
            uint32_t a = (uint32_t)(j * (nu + 1) + i), b = a + 1, c = a + (uint32_t)(nu + 1), d = c + 1;
            m.idx.insert(m.idx.end(), {a, c, b, b, c, d});
        }
    recomputeNormals(m);
    return m;
}

void addFace(MeshData& m, V3 o, V3 du, V3 dv, V3 n, float uu, float vv) {
    uint32_t base = (uint32_t)(m.pos.size() / 3);
    V3 ps[4] = {o, o + du, o + du + dv, o + dv};
    float uvs[8] = {0, 0, uu, 0, uu, vv, 0, vv};
    for (int k = 0; k < 4; ++k) {
        m.pos.insert(m.pos.end(), {ps[k].x, ps[k].y, ps[k].z});
        m.nrm.insert(m.nrm.end(), {n.x, n.y, n.z});
        m.uv.insert(m.uv.end(), {uvs[k * 2], uvs[k * 2 + 1]});
    }
    // orient CCW around n
    if (dot(cross(du, dv), n) >= 0) m.idx.insert(m.idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    else m.idx.insert(m.idx.end(), {base, base + 2, base + 1, base, base + 3, base + 2});
}

MeshData box(V3 c, V3 s, float uvScale = 1.f) {
    MeshData m;
    V3 h = s * 0.5f;
    V3 X{s.x, 0, 0}, Y{0, s.y, 0}, Z{0, 0, s.z};
    addFace(m, c + V3{-h.x, -h.y, h.z}, X, Y, {0, 0, 1}, s.x * uvScale, s.y * uvScale);
    addFace(m, c + V3{h.x, -h.y, -h.z}, X * -1.f, Y, {0, 0, -1}, s.x * uvScale, s.y * uvScale);
    addFace(m, c + V3{h.x, -h.y, h.z}, Z * -1.f, Y, {1, 0, 0}, s.z * uvScale, s.y * uvScale);
    addFace(m, c + V3{-h.x, -h.y, -h.z}, Z, Y, {-1, 0, 0}, s.z * uvScale, s.y * uvScale);
    addFace(m, c + V3{-h.x, h.y, h.z}, X, Z * -1.f, {0, 1, 0}, s.x * uvScale, s.z * uvScale);
    addFace(m, c + V3{-h.x, -h.y, -h.z}, X, Z, {0, -1, 0}, s.x * uvScale, s.z * uvScale);
    return m;
}

MeshData cylinder(V3 base, float r0, float r1, float hgt, int seg, bool caps) {
    MeshData m = grid(seg, 1, [&](float u, float v) {
        float a = u * 2.f * kPi, r = lerpf(r0, r1, v);
        return base + V3{std::cos(a) * r, v * hgt, std::sin(a) * r};
    }, 1.f, 1.f);
    orientBy(m, [&](V3 p) { return V3{p.x - base.x, 0.f, p.z - base.z}; });
    if (caps) {
        for (int cap = 0; cap < 2; ++cap) {
            float y = cap ? hgt : 0.f, r = cap ? r1 : r0;
            uint32_t center = (uint32_t)(m.pos.size() / 3);
            V3 n{0, cap ? 1.f : -1.f, 0};
            m.pos.insert(m.pos.end(), {base.x, base.y + y, base.z}); m.nrm.insert(m.nrm.end(), {n.x, n.y, n.z}); m.uv.insert(m.uv.end(), {0.5f, 0.5f});
            for (int i = 0; i <= seg; ++i) {
                float a = (float)i / (float)seg * 2.f * kPi;
                m.pos.insert(m.pos.end(), {base.x + std::cos(a) * r, base.y + y, base.z + std::sin(a) * r});
                m.nrm.insert(m.nrm.end(), {n.x, n.y, n.z});
                m.uv.insert(m.uv.end(), {0.5f + 0.5f * std::cos(a), 0.5f + 0.5f * std::sin(a)});
            }
            for (int i = 0; i < seg; ++i) {
                uint32_t a = center + 1 + (uint32_t)i, b = a + 1;
                if (cap) m.idx.insert(m.idx.end(), {center, b, a});
                else m.idx.insert(m.idx.end(), {center, a, b});
            }
        }
    }
    return m;
}

float spow(float v, float e) { return (v < 0 ? -1.f : 1.f) * std::pow(std::fabs(v), e); }
MeshData superEllipsoid(V3 c, V3 r, float e1, float e2, int seg, const std::function<V3(V3)>& deform = nullptr) {
    MeshData m = grid(seg * 2, seg, [&](float u, float v) {
        float th = (u - 0.5f) * 2.f * kPi, ph = (v - 0.5f) * kPi;
        V3 p{r.x * spow(std::cos(ph), e1) * spow(std::cos(th), e2), r.y * spow(std::sin(ph), e1), r.z * spow(std::cos(ph), e1) * spow(std::sin(th), e2)};
        if (deform) p = deform(p);
        return c + p;
    }, 2.f, 1.f);
    orientBy(m, [&](V3 p) { return p - c; });
    return m;
}

void append(MeshData& a, const MeshData& b) {
    uint32_t off = (uint32_t)(a.pos.size() / 3);
    a.pos.insert(a.pos.end(), b.pos.begin(), b.pos.end());
    a.nrm.insert(a.nrm.end(), b.nrm.begin(), b.nrm.end());
    a.uv.insert(a.uv.end(), b.uv.begin(), b.uv.end());
    for (uint32_t i : b.idx) a.idx.push_back(i + off);
}

// ---- procedural textures (512^2, RGBA8) ----
using Px = std::vector<unsigned char>;
void put(Px& px, int w, int x, int y, V3 c, float a = 1.f) {
    size_t i = ((size_t)y * (size_t)w + (size_t)x) * 4;
    px[i] = (unsigned char)(clampf(c.x, 0, 1) * 255.f); px[i + 1] = (unsigned char)(clampf(c.y, 0, 1) * 255.f);
    px[i + 2] = (unsigned char)(clampf(c.z, 0, 1) * 255.f); px[i + 3] = (unsigned char)(clampf(a, 0, 1) * 255.f);
}
GLuint texWood() {
    const int N = 512; Px px((size_t)N * N * 4);
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x) {
            float fy = (float)y / N * 6.f; int plank = (int)fy; float inPlank = fy - (float)plank;
            float shift = hash2(plank, 3) * 3.f;
            float gx = (float)x / N * 2.f + shift;
            float grain = std::sin((gx * 22.f + fbm(gx * 3.f, fy * 9.f, 4) * 6.f)) * 0.5f + 0.5f;
            V3 c = lerp(V3{0.30f, 0.17f, 0.10f}, V3{0.46f, 0.28f, 0.17f}, hash2(plank, 7) * 0.7f + grain * 0.3f);
            c = c * (0.85f + 0.3f * fbm(gx * 8.f, fy * 30.f, 3));
            float seam = smoothstep(0.0f, 0.03f, inPlank) * smoothstep(1.0f, 0.97f, inPlank);
            float joint = std::fabs(std::fmod(gx + hash2(plank, 11) * 2.f, 1.f) - 0.5f) > 0.495f ? 0.5f : 1.f;
            put(px, N, x, y, c * (0.55f + 0.45f * seam) * joint);
        }
    return textureFromRGBA(px.data(), N, N, true);
}
GLuint texWallpaper() {
    const int N = 512; Px px((size_t)N * N * 4);
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x) {
            float u = (float)x / N, v = (float)y / N;
            float du = std::fabs(std::fmod(u * 4.f + 0.5f, 1.f) - 0.5f), dv = std::fabs(std::fmod(v * 3.f + 0.5f, 1.f) - 0.5f);
            float diamond = smoothstep(0.42f, 0.38f, du + dv * 0.75f) - smoothstep(0.30f, 0.26f, du + dv * 0.75f);
            float stripe = smoothstep(0.02f, 0.0f, std::fabs(std::fmod(u * 8.f, 1.f) - 0.5f) - 0.47f);
            V3 base{0.23f, 0.28f, 0.285f};
            V3 c = base * (0.9f + 0.2f * fbm(u * 40.f, v * 40.f, 3)) + V3{0.07f, 0.055f, 0.03f} * (diamond * 0.6f + stripe * 0.25f);
            put(px, N, x, y, c);
        }
    return textureFromRGBA(px.data(), N, N, true);
}
GLuint texSatin() {
    const int N = 512; Px px((size_t)N * N * 4);
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x) {
            float u = (float)x / N, v = (float)y / N;
            // integer frequencies only, so the texture tiles without seams
            const float T = 2.f * kPi;
            float mott = 0.5f + 0.2f * std::sin(T * 2.f * u + 1.3f * std::sin(T * v)) + 0.16f * std::sin(T * 3.f * v + 1.7f * std::sin(T * 2.f * u))
                       + 0.08f * std::sin(T * (5.f * u + 4.f * v));
            float weave = std::sin(T * (96.f * u + 96.f * v)) * 0.5f + 0.5f;
            float k = 0.90f + 0.10f * mott + 0.025f * weave;
            put(px, N, x, y, V3{k, k * 0.985f, k * 0.97f});
        }
    return textureFromRGBA(px.data(), N, N, true);
}
GLuint texRug() {
    const int N = 512; Px px((size_t)N * N * 4);
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x) {
            float u = (float)x / N, v = (float)y / N;
            float b = std::min(std::min(u, 1.f - u), std::min(v, 1.f - v));
            float border = smoothstep(0.08f, 0.075f, b) - smoothstep(0.045f, 0.04f, b);
            float r = length(V3{(u - 0.5f) * 1.4f, (v - 0.5f), 0});
            float med = smoothstep(0.24f, 0.22f, r) * (0.5f + 0.5f * std::sin(std::atan2(v - 0.5f, u - 0.5f) * 8.f + r * 30.f));
            V3 c{0.30f, 0.235f, 0.20f};
            c = c * (0.85f + 0.3f * fbm(u * 90.f, v * 90.f, 2));
            c = lerp(c, V3{0.52f, 0.38f, 0.18f}, border * 0.8f + med * 0.35f);
            put(px, N, x, y, c);
        }
    return textureFromRGBA(px.data(), N, N, true);
}
GLuint texNight() {
    const int N = 512; Px px((size_t)N * N * 4);
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x) {
            float u = (float)x / N, v = 1.f - (float)y / N;
            V3 c = lerp(V3{0.010f, 0.016f, 0.045f}, V3{0.04f, 0.07f, 0.16f}, v);
            float star = hash2(x, y) > 0.9985f ? hash2(y, x) : 0.f;
            c += V3{star, star, star * 1.1f};
            float d = length(V3{u - 0.68f, v - 0.74f, 0});
            float moon = smoothstep(0.075f, 0.068f, d);
            float halo = std::exp(-d * 9.f) * 0.35f;
            c = c + V3{0.85f, 0.9f, 1.0f} * (moon * (0.85f + 0.15f * fbm(u * 50.f, v * 50.f, 3))) + V3{0.25f, 0.3f, 0.45f} * halo;
            // city glow along the bottom
            c += V3{0.30f, 0.16f, 0.08f} * std::exp(-v * 9.f) * (0.6f + 0.4f * vnoise(u * 60.f, 0.f));
            put(px, N, x, y, c);
        }
    return textureFromRGBA(px.data(), N, N, true);
}

}  // namespace

void buildPlaceholderRoom(Model& out, RoomInfo& info) {
    out.destroy();
    out.path = "<procedural room>";
    auto mat = [&](const char* name, V4 base, float rough, float metal, GLuint tex = 0, V3 emissive = {0, 0, 0}, GLuint emisTex = 0) {
        Material m;
        m.name = name; m.baseColor = base; m.roughness = rough; m.metallic = metal; m.texBase = tex;
        m.emissive = emissive; m.texEmis = emisTex; m.shading = Shading::Room;
        if (tex) out.textures.push_back(tex);
        if (emisTex && emisTex != tex) out.textures.push_back(emisTex);
        out.materials.push_back(m);
        return (int)out.materials.size() - 1;
    };
    auto add = [&](const char* name, MeshData md, int material, bool cast = true) {
        Primitive p;
        p.pos = md.pos; p.nrm = md.nrm; p.idx = md.idx; p.material = material;
        computeTangents(p.pos, p.nrm, md.uv, p.idx, p.tan);
        p.bmin = {1e9f, 1e9f, 1e9f}; p.bmax = {-1e9f, -1e9f, -1e9f};
        for (size_t v = 0; v * 3 < p.pos.size(); ++v) {
            V3 q{p.pos[v * 3], p.pos[v * 3 + 1], p.pos[v * 3 + 2]};
            p.bmin = {std::min(p.bmin.x, q.x), std::min(p.bmin.y, q.y), std::min(p.bmin.z, q.z)};
            p.bmax = {std::max(p.bmax.x, q.x), std::max(p.bmax.y, q.y), std::max(p.bmax.z, q.z)};
        }
        Model::uploadPrimitive(p, md.uv, {});
        Mesh mesh; mesh.name = name; mesh.prims.push_back(std::move(p));
        out.meshes.push_back(std::move(mesh));
        Node n; n.name = name; n.mesh = (int)out.meshes.size() - 1;
        out.nodes.push_back(n);
        out.roots.push_back((int)out.nodes.size() - 1);
        out.nodeIndex[name] = (int)out.nodes.size() - 1;
        out.materials[(size_t)material].castShadow = out.materials[(size_t)material].castShadow && cast;
    };

    // ---- layout (matches the content authoring bed: centre (0, 0.604, -1.22), 1.80 x 2.00 m, head at -Z) ----
    const float bedTop = 0.604f, CZ = -1.22f, HW = 0.90f, HL = 1.00f;
    const float headZ = CZ - HL, footZ = CZ + HL;
    const float backZ = -2.75f, sideX = 3.0f;
    info.bedTop = bedTop;
    info.bedCenter = {0.f, bedTop, CZ};
    info.bedHalf = {HW, 0.f, HL};

    int mWood = mat("floor_wood", {1, 1, 1, 1}, 0.42f, 0.f, texWood());
    int mWall = mat("wallpaper", {1, 1, 1, 1}, 0.85f, 0.f, texWallpaper());
    int mPanel = mat("wainscot", {0.085f, 0.055f, 0.04f, 1}, 0.5f, 0.f);
    int mFrame = mat("bed_frame", {0.11f, 0.055f, 0.035f, 1}, 0.36f, 0.f);
    int mMattress = mat("mattress", {0.34f, 0.275f, 0.21f, 1}, 0.8f, 0.f);
    int mSheet = mat("sheet_satin", {0.34f, 0.275f, 0.21f, 1}, 0.62f, 0.f, texSatin());
    int mPillow = mat("pillow", {0.78f, 0.73f, 0.66f, 1}, 0.7f, 0.f);
    int mHead = mat("headboard_velvet", {0.11f, 0.105f, 0.11f, 1}, 0.72f, 0.f);
    int mThrow = mat("throw", {0.30f, 0.045f, 0.07f, 1}, 0.62f, 0.f);
    int mStand = mat("nightstand", {0.10f, 0.05f, 0.032f, 1}, 0.33f, 0.f);
    int mBrass = mat("brass", {0.62f, 0.45f, 0.22f, 1}, 0.28f, 1.f);
    int mShade = mat("lamp_shade", {0.95f, 0.78f, 0.55f, 1}, 0.8f, 0.f, 0, {5.5f, 3.4f, 1.6f});
    int mWax = mat("candle_wax", {0.85f, 0.78f, 0.66f, 1}, 0.6f, 0.f, 0, {0.25f, 0.12f, 0.04f});
    int mFlame = mat("candle_flame", {1, 1, 1, 1}, 1.f, 0.f, 0, {9.f, 4.6f, 1.4f});
    int mRug = mat("rug", {1, 1, 1, 1}, 0.95f, 0.f, texRug());
    GLuint night = texNight();
    int mWindow = mat("window_night", {0, 0, 0, 1}, 1.f, 0.f, 0, {1.4f, 1.4f, 1.4f}, night);
    int mCurtain = mat("curtain_velvet", {0.035f, 0.085f, 0.09f, 1}, 0.78f, 0.f);
    out.materials[(size_t)mFlame].castShadow = false;
    out.materials[(size_t)mShade].castShadow = false;
    out.materials[(size_t)mWindow].castShadow = false;

    // floor + walls (floor runs from the back wall to well past the foot of the bed)
    add("floor", facing(grid(1, 1, [&](float u, float v) { return V3{-sideX + u * 2.f * sideX, 0.f, 3.5f - v * (3.5f - backZ)}; }, 4.f, 4.f), {0, 1, 0}), mWood, false);
    add("wall_back", facing(grid(1, 1, [&](float u, float v) { return V3{-sideX + u * 2.f * sideX, v * 3.2f, backZ}; }, 8.f, 4.f), {0, 0, 1}), mWall, false);
    add("wall_left", facing(grid(1, 1, [&](float u, float v) { return V3{-sideX, v * 3.2f, 3.5f - u * (3.5f - backZ)}; }, 8.f, 4.f), {1, 0, 0}), mWall, false);
    add("wall_right", facing(grid(1, 1, [&](float u, float v) { return V3{sideX, v * 3.2f, backZ + u * (3.5f - backZ)}; }, 8.f, 4.f), {-1, 0, 0}), mWall, false);
    MeshData wain = box({0.f, 0.45f, backZ + 0.03f}, {2.f * sideX, 0.9f, 0.05f});
    append(wain, box({-sideX + 0.03f, 0.45f, (3.5f + backZ) * 0.5f}, {0.05f, 0.9f, 3.5f - backZ}));
    append(wain, box({sideX - 0.03f, 0.45f, (3.5f + backZ) * 0.5f}, {0.05f, 0.9f, 3.5f - backZ}));
    add("wainscot", wain, mPanel, false);

    // bed: frame + mattress (top at 0.585), sheet surface at 0.604
    MeshData frame = box({0.f, 0.25f, CZ}, {2.f * HW + 0.14f, 0.30f, 2.f * HL + 0.12f});
    for (int i = 0; i < 4; ++i) append(frame, box({(i & 1) ? HW + 0.03f : -HW - 0.03f, 0.05f, (i & 2) ? footZ + 0.03f : headZ - 0.03f}, {0.09f, 0.1f, 0.09f}));
    add("bed_frame", frame, mFrame);
    // mattress top sits 19 mm under the sheet surface: wrinkles never let it poke through
    add("mattress", box({0.f, 0.4925f, CZ}, {2.f * HW, 0.185f, 2.f * HL}), mMattress);
    add("sheet", facing(grid(72, 84, [&](float u, float v) {
        float x = -(HW + 0.12f) + u * 2.f * (HW + 0.12f), z = headZ - 0.02f + v * (2.f * HL + 0.16f);
        float edgeX = std::max(0.f, std::fabs(x) - HW), edgeZ = std::max(0.f, z - footZ);
        float drop = (edgeX + edgeZ) * 1.9f;
        float wr = (fbm(x * 2.2f, z * 2.2f, 4) - 0.5f) * 0.010f + std::sin(x * 9.f + fbm(z * 3.f, x, 2) * 4.f) * 0.0025f;
        float y = bedTop + wr - drop * drop * 0.9f - drop * 0.12f;
        float pinch = 1.f - std::min(1.f, drop * 0.9f);
        return V3{x * (0.94f + 0.06f * pinch), std::max(y, 0.36f), z};
    }, 6.f, 6.f), V3{0, 1, 0}), mSheet);
    for (int s = -1; s <= 1; s += 2)
        add(s < 0 ? "pillow_l" : "pillow_r", superEllipsoid({0.42f * (float)s, bedTop + 0.075f, headZ + 0.30f}, {0.32f, 0.085f, 0.20f}, 0.45f, 0.3f, 18,
            [](V3 p) { p.y *= 1.f - 0.25f * std::fabs(p.x) / 0.32f; return p; }), mPillow);
    // throw folded over the foot edge: only 9 cm lies on the mattress, so feet and shins stay clear of it
    MeshData drape = grid(48, 12, [&](float u, float v) {
        float x = -(HW + 0.05f) + u * 2.f * (HW + 0.05f);
        float s = v * 0.48f, ripple = 0.006f * std::sin(x * 11.f + v * 3.f);
        if (s < 0.09f) return V3{x, bedTop + 0.014f + ripple * 0.3f, footZ - 0.09f + s};
        float t = s - 0.09f, bend = std::min(1.f, t / 0.05f);
        return V3{x, bedTop + 0.014f - t, footZ + 0.004f + 0.022f * bend + ripple};
    }, 5.f, 1.f);
    orientBy(drape, [&](V3 q) { return q.y > bedTop ? V3{0, 1, 0} : V3{0, 0, 1}; });
    add("throw", drape, mThrow);
    add("headboard", facing(grid(48, 30, [&](float u, float v) {
        float x = -(HW + 0.10f) + u * 2.f * (HW + 0.10f), y = 0.42f + v * 1.25f;
        float tuft = std::cos(x * 26.f) * std::cos(y * 26.f + (std::fmod(std::floor(y * 4.1f), 2.f) * 3.14f));
        return V3{x, y, headZ - 0.06f + 0.009f * tuft + 0.04f * std::sin(u * kPi)};
    }, 3.f, 2.f), V3{0, 0, 1}), mHead);
    add("headboard_back", box({0.f, 1.04f, headZ - 0.12f}, {2.f * HW + 0.30f, 1.30f, 0.08f}), mFrame);

    // nightstands, lamp (left), candles (right)
    const float nsZ = headZ + 0.25f, nsX = HW + 0.45f, nsTop = 0.62f;
    for (int s = -1; s <= 1; s += 2) add(s < 0 ? "nightstand_l" : "nightstand_r", box({nsX * (float)s, nsTop * 0.5f, nsZ}, {0.56f, nsTop, 0.46f}), mStand);
    MeshData lampBase = cylinder({-nsX, nsTop, nsZ}, 0.085f, 0.05f, 0.08f, 24, true);
    append(lampBase, cylinder({-nsX, nsTop + 0.08f, nsZ}, 0.018f, 0.018f, 0.26f, 12, false));
    add("lamp_base", lampBase, mBrass);
    add("lamp_shade", cylinder({-nsX, nsTop + 0.30f, nsZ}, 0.21f, 0.14f, 0.24f, 32, false), mShade, false);
    info.lampPos = {-nsX, nsTop + 0.42f, nsZ};
    const V3 cpos[3] = {{nsX - 0.14f, nsTop, nsZ + 0.06f}, {nsX + 0.04f, nsTop, nsZ - 0.06f}, {nsX + 0.16f, nsTop, nsZ + 0.10f}};
    const float ch[3] = {0.16f, 0.11f, 0.08f};
    MeshData wax, flame;
    for (int i = 0; i < 3; ++i) {
        append(wax, cylinder(cpos[i], 0.034f, 0.032f, ch[i], 18, true));
        V3 fp = cpos[i] + V3{0, ch[i] + 0.025f, 0};
        append(flame, superEllipsoid(fp, {0.009f, 0.024f, 0.009f}, 0.8f, 1.f, 8, [](V3 p) { if (p.y > 0) p.x *= 1.f - p.y * 25.f, p.z *= 1.f - p.y * 25.f; return p; }));
        info.candlePos[i] = fp;
    }
    add("candles", wax, mWax);
    add("candle_flames", flame, mFlame, false);

    // window (night sky) + curtains on the right wall, beside the bed
    add("window", facing(grid(1, 1, [&](float u, float v) { return V3{sideX - 0.02f, 0.95f + v * 1.45f, -0.4f - u * 1.5f}; }, 1.f, 1.f), {-1, 0, 0}), mWindow, false);
    info.windowCenter = {sideX - 0.02f, 1.67f, -1.15f};
    MeshData curtain;
    for (int s = 0; s < 2; ++s) {
        float z0 = s ? -1.95f : -0.15f;
        append(curtain, facing(grid(24, 8, [&](float u, float v) {
            float z = z0 - u * 0.55f, y = 0.05f + v * 2.95f;
            return V3{sideX - 0.07f - 0.035f * (std::sin(u * 18.f) * 0.5f + 0.5f), y, z};
        }, 2.f, 3.f), V3{-1, 0, 0}));
    }
    add("curtains", curtain, mCurtain);

    add("rug", facing(grid(1, 1, [&](float u, float v) { return V3{-1.6f + u * 3.2f, 0.004f, footZ + 1.6f - v * 2.6f}; }, 1.f, 1.f), {0, 1, 0}), mRug, false);
}

}  // namespace ag
