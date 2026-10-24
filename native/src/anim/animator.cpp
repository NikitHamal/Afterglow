#include "anim/animator.h"
#include "core/log.h"
#include <algorithm>
#include <cmath>

namespace ag {

int Animator::skinMode = 1;

void Animator::init(Model* m) {
    destroy();
    model_ = m;
    size_t n = m->nodes.size();
    rest_.t.resize(n); rest_.r.resize(n); rest_.s.resize(n);
    for (size_t i = 0; i < n; ++i) { rest_.t[i] = m->nodes[i].t; rest_.r[i] = m->nodes[i].r; rest_.s[i] = m->nodes[i].s; }
    pose_ = rest_;
    global_.assign(n, M4::identity());
    // parents before children
    order_.clear();
    std::vector<char> seen(n, 0);
    std::vector<int> stack;
    for (int r : m->roots) stack.push_back(r);
    for (size_t i = 0; i < n; ++i) if (m->nodes[i].parent < 0) stack.push_back((int)i);
    while (!stack.empty()) {
        int cur = stack.back(); stack.pop_back();
        if (cur < 0 || cur >= (int)n || seen[(size_t)cur]) continue;
        seen[(size_t)cur] = 1; order_.push_back(cur);
        for (auto it = m->nodes[(size_t)cur].children.rbegin(); it != m->nodes[(size_t)cur].children.rend(); ++it) stack.push_back(*it);
    }
    morphAnim_.assign(m->meshes.size(), {});
    morphUser_.assign(m->meshes.size(), {});
    morphLast_.assign(m->meshes.size(), {});
    for (size_t i = 0; i < m->meshes.size(); ++i) {
        size_t tc = m->meshes[i].targetNames.size();
        for (auto& p : m->meshes[i].prims) tc = std::max(tc, (size_t)p.targetCount);
        morphAnim_[i].assign(tc, 0.f);
        for (size_t k = 0; k < tc && k < m->meshes[i].defaultWeights.size(); ++k) morphAnim_[i][k] = m->meshes[i].defaultWeights[k];
        morphUser_[i].assign(tc, -1.f);
        morphLast_[i].assign(tc, -999.f);
    }
    // bone texture: one row per skin, kTexelsPerJoint RGBA32F texels per joint (see animator.h)
    if (!m->skins.empty()) {
        texW_ = std::max(kTexelsPerJoint, m->maxJoints * kTexelsPerJoint);
        texH_ = (int)m->skins.size();
        boneData_.assign((size_t)texW_ * (size_t)texH_ * 4, 0.f);
        glGenTextures(1, &boneTex_);
        glBindTexture(GL_TEXTURE_2D, boneTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, texW_, texH_, 0, GL_RGBA, GL_FLOAT, boneData_.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void Animator::destroy() {
    if (boneTex_) glDeleteTextures(1, &boneTex_);
    boneTex_ = 0;
    model_ = nullptr;
}

void Animator::begin() {
    pose_ = rest_;
    for (size_t i = 0; i < morphAnim_.size(); ++i) {
        auto& w = morphAnim_[i];
        std::fill(w.begin(), w.end(), 0.f);
        const auto& def = model_->meshes[i].defaultWeights;
        for (size_t k = 0; k < w.size() && k < def.size(); ++k) w[k] = def[k];
    }
}

void Animator::sampleChannel(const Channel& ch, float time, float* out) const {
    const int c = ch.comps;
    const size_t nk = ch.times.size();
    const int stride = ch.interp == 2 ? 3 * c : c;
    auto val = [&](size_t k) -> const float* { return &ch.values[k * (size_t)stride + (ch.interp == 2 ? (size_t)c : 0)]; };
    if (nk == 0) return;
    if (nk == 1 || time <= ch.times[0]) { std::copy(val(0), val(0) + c, out); return; }
    if (time >= ch.times[nk - 1]) { std::copy(val(nk - 1), val(nk - 1) + c, out); return; }
    size_t k = (size_t)(std::upper_bound(ch.times.begin(), ch.times.end(), time) - ch.times.begin()) - 1;
    float t0 = ch.times[k], t1 = ch.times[k + 1], dt = t1 - t0;
    float u = dt > 1e-9f ? (time - t0) / dt : 0.f;
    const float* a = val(k);
    const float* b = val(k + 1);
    if (ch.interp == 1) { std::copy(a, a + c, out); return; }
    if (ch.interp == 2) {
        const float* outTan = &ch.values[k * (size_t)stride + 2 * (size_t)c];
        const float* inTan = &ch.values[(k + 1) * (size_t)stride];
        float u2 = u * u, u3 = u2 * u;
        float h00 = 2 * u3 - 3 * u2 + 1, h10 = u3 - 2 * u2 + u, h01 = -2 * u3 + 3 * u2, h11 = u3 - u2;
        for (int i = 0; i < c; ++i) out[i] = h00 * a[i] + h10 * dt * outTan[i] + h01 * b[i] + h11 * dt * inTan[i];
        if (ch.path == 1) { Quat q = qnormalize({out[0], out[1], out[2], out[3]}); out[0] = q.x; out[1] = q.y; out[2] = q.z; out[3] = q.w; }
        return;
    }
    if (ch.path == 1) {
        Quat q = qslerp({a[0], a[1], a[2], a[3]}, {b[0], b[1], b[2], b[3]}, u);
        out[0] = q.x; out[1] = q.y; out[2] = q.z; out[3] = q.w;
        return;
    }
    for (int i = 0; i < c; ++i) out[i] = a[i] + (b[i] - a[i]) * u;
}

void Animator::applyClip(int ci, float time, float weight, const BoneMask* mask) {
    if (!model_ || ci < 0 || ci >= (int)model_->clips.size() || weight <= 0.f) return;
    const Clip& clip = model_->clips[(size_t)ci];
    for (const Channel& ch : clip.channels) {
        if (ch.node < 0 || ch.node >= (int)pose_.t.size()) continue;
        float w = weight;
        if (mask && !mask->empty()) w *= (*mask)[(size_t)ch.node];
        if (w <= 0.f) continue;
        scratch_.resize((size_t)std::max(ch.comps, 4));
        sampleChannel(ch, time, scratch_.data());
        const float* v = scratch_.data();
        size_t n = (size_t)ch.node;
        switch (ch.path) {
            case 0: pose_.t[n] = w >= 1.f ? V3{v[0], v[1], v[2]} : lerp(pose_.t[n], V3{v[0], v[1], v[2]}, w); break;
            case 1: pose_.r[n] = w >= 1.f ? qnormalize({v[0], v[1], v[2], v[3]}) : qslerp(pose_.r[n], qnormalize({v[0], v[1], v[2], v[3]}), w); break;
            case 2: pose_.s[n] = w >= 1.f ? V3{v[0], v[1], v[2]} : lerp(pose_.s[n], V3{v[0], v[1], v[2]}, w); break;
            case 3: {
                int mesh = model_->nodes[n].mesh;
                if (mesh < 0) break;
                auto& mw = morphAnim_[(size_t)mesh];
                for (int k = 0; k < ch.comps && k < (int)mw.size(); ++k) mw[(size_t)k] = lerpf(mw[(size_t)k], v[k], std::min(w, 1.f));
                break;
            }
        }
    }
}

void Animator::globalOf(int n) {
    const Node& nd = model_->nodes[(size_t)n];
    M4 local = m4trs(pose_.t[(size_t)n], pose_.r[(size_t)n], pose_.s[(size_t)n]);
    global_[(size_t)n] = nd.parent >= 0 ? global_[(size_t)nd.parent] * local : root_ * local;
}

void Animator::computeGlobals(const M4& root) {
    root_ = root;
    for (int n : order_) globalOf(n);
}

void Animator::recomputeSubtree(int n) {
    globalOf(n);
    for (int c : model_->nodes[(size_t)n].children) recomputeSubtree(c);
}

Quat Animator::worldRot(int n) const {
    const M4& g = global_[(size_t)n];
    M4 r = M4::identity();
    for (int c = 0; c < 3; ++c) {
        V3 col = normalize(g.col3(c));
        r.m[c * 4] = col.x; r.m[c * 4 + 1] = col.y; r.m[c * 4 + 2] = col.z;
    }
    return quatFromM3(r);
}

void Animator::setWorldRot(int n, const Quat& w) {
    int p = model_->nodes[(size_t)n].parent;
    Quat pw;
    if (p >= 0) pw = worldRot(p);
    else {
        M4 r = M4::identity();
        for (int c = 0; c < 3; ++c) { V3 col = normalize(root_.col3(c)); r.m[c * 4] = col.x; r.m[c * 4 + 1] = col.y; r.m[c * 4 + 2] = col.z; }
        pw = quatFromM3(r);
    }
    pose_.r[(size_t)n] = qnormalize(qmul(qconj(pw), w));
}

void Animator::setMorphOverride(int mesh, int target, float w) {
    if (mesh < 0 || mesh >= (int)morphUser_.size()) return;
    auto& u = morphUser_[(size_t)mesh];
    if (target < 0 || target >= (int)u.size()) return;
    u[(size_t)target] = w;
}

float Animator::morphWeight(int mesh, int target) const {
    if (mesh < 0 || mesh >= (int)morphAnim_.size() || target < 0 || target >= (int)morphAnim_[(size_t)mesh].size()) return 0.f;
    float u = morphUser_[(size_t)mesh][(size_t)target];
    return u >= 0.f ? u : morphAnim_[(size_t)mesh][(size_t)target];
}

// the rigid part of a skin matrix as a unit dual quaternion (real, dual), its uniform scale, and whether the joint must
// blend linearly instead (non-uniform scale / shear / mirroring, or linear skinning asked for)
void Animator::writeDualQuat(const M4& m, float* o) {
    V3 c0 = m.col3(0), c1 = m.col3(1), c2 = m.col3(2);
    float l0 = length(c0), l1 = length(c1), l2 = length(c2);
    float sc = (l0 + l1 + l2) / 3.f;
    Quat q;
    bool rigid = false;
    if (sc > 1e-6f) {
        V3 x = c0 / std::max(l0, 1e-9f);
        V3 y = normalize(c1 - x * dot(x, c1));
        V3 z = cross(x, y);
        M4 r = M4::identity();
        r.m[0] = x.x; r.m[1] = x.y; r.m[2] = x.z;
        r.m[4] = y.x; r.m[5] = y.y; r.m[6] = y.z;
        r.m[8] = z.x; r.m[9] = z.y; r.m[10] = z.z;
        q = qnormalize(quatFromM3(r));
        float tol = 0.02f * sc;
        rigid = std::fabs(l0 - sc) < tol && std::fabs(l1 - sc) < tol && std::fabs(l2 - sc) < tol &&
                std::fabs(dot(c0, c1)) < tol * sc && std::fabs(dot(c1, c2)) < tol * sc && std::fabs(dot(c0, c2)) < tol * sc &&
                dot(cross(c0, c1), c2) > 0.f;
    }
    V3 t = m.translation();
    Quat d = qmul(Quat{t.x, t.y, t.z, 0.f}, q);
    o[0] = q.x; o[1] = q.y; o[2] = q.z; o[3] = q.w;
    o[4] = 0.5f * d.x; o[5] = 0.5f * d.y; o[6] = 0.5f * d.z; o[7] = 0.5f * d.w;
    o[8] = sc; o[9] = (rigid && skinMode == 1) ? 0.f : 1.f; o[10] = 0.f; o[11] = 0.f;
}

void Animator::finish() {
    if (!model_) return;
    if (boneTex_) {
        for (size_t s = 0; s < model_->skins.size(); ++s) {
            const Skin& sk = model_->skins[s];
            float* row = &boneData_[s * (size_t)texW_ * 4];
            for (size_t j = 0; j < sk.joints.size(); ++j) {
                M4 m = global_[(size_t)sk.joints[j]] * sk.invBind[j];
                float* o = row + j * (size_t)kTexelsPerJoint * 4;
                std::copy(m.m, m.m + 16, o);
                writeDualQuat(m, o + 16);
            }
        }
        glBindTexture(GL_TEXTURE_2D, boneTex_);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texW_, texH_, GL_RGBA, GL_FLOAT, boneData_.data());
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    std::vector<float> w;
    for (size_t mi = 0; mi < morphAnim_.size(); ++mi) {
        size_t tc = morphAnim_[mi].size();
        if (!tc) continue;
        w.resize(tc);
        bool changed = false;
        for (size_t k = 0; k < tc; ++k) {
            float u = morphUser_[mi][k];
            w[k] = u >= 0.f ? u : morphAnim_[mi][k];
            if (std::fabs(w[k] - morphLast_[mi][k]) > 1e-3f) changed = true;
        }
        if (!changed) continue;
        morphLast_[mi] = w;
        for (auto& p : model_->meshes[mi].prims) Model::updateMorph(p, w.data(), (int)tc);
    }
}

BoneMask Animator::makeMask(const std::vector<std::string>& roots) const {
    BoneMask m(model_->nodes.size(), 0.f);
    std::vector<int> stack;
    for (auto& r : roots) { int i = model_->findNode(r); if (i >= 0) stack.push_back(i); }
    while (!stack.empty()) {
        int n = stack.back(); stack.pop_back();
        m[(size_t)n] = 1.f;
        for (int c : model_->nodes[(size_t)n].children) stack.push_back(c);
    }
    return m;
}

}  // namespace ag
