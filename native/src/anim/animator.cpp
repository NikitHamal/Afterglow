#include "anim/animator.h"
#include "core/log.h"
#include <algorithm>
#include <cmath>

namespace ag {

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
    // bone texture: one row per skin, 4 RGBA32F texels (matrix columns) per joint
    if (!m->skins.empty()) {
        texW_ = std::max(4, m->maxJoints * 4);
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
    guardWarned_ = false;
}

void Animator::begin() {
    pose_ = rest_;
    guardWarned_ = false;
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
            case 0: {
                V3 x{v[0], v[1], v[2]};
                if (!std::isfinite(x.x) || !std::isfinite(x.y) || !std::isfinite(x.z)) break;
                // A corrupt translation can fling a limb across the scene and
                // poison contacts.  The envelope is intentionally broad: it
                // catches bad exports without limiting real authored travel.
                x.x = clampf(x.x, -4.f, 4.f); x.y = clampf(x.y, -4.f, 4.f); x.z = clampf(x.z, -4.f, 4.f);
                pose_.t[n] = w >= 1.f ? x : lerp(pose_.t[n], x, w); break;
            }
            case 1: {
                Quat x{v[0], v[1], v[2], v[3]};
                float ql = std::sqrt(qdot(x, x));
                if (!std::isfinite(ql) || ql < 1e-6f) break;
                x = qnormalize(x);
                pose_.r[n] = w >= 1.f ? x : qslerp(pose_.r[n], x, w); break;
            }
            case 2: {
                V3 x{v[0], v[1], v[2]};
                if (!std::isfinite(x.x) || !std::isfinite(x.y) || !std::isfinite(x.z)) break;
                x.x = clampf(x.x, 0.05f, 8.f); x.y = clampf(x.y, 0.05f, 8.f); x.z = clampf(x.z, 0.05f, 8.f);
                pose_.s[n] = w >= 1.f ? x : lerp(pose_.s[n], x, w); break;
            }
            case 3: {
                int mesh = model_->nodes[n].mesh;
                if (mesh < 0) break;
                auto& mw = morphAnim_[(size_t)mesh];
                for (int k = 0; k < ch.comps && k < (int)mw.size(); ++k) {
                    if (!std::isfinite(v[k])) continue;
                    mw[(size_t)k] = lerpf(mw[(size_t)k], clampf(v[k], 0.f, 1.f), std::min(w, 1.f));
                }
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
    sanitizePose();
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
    u[(size_t)target] = w < 0.f ? -1.f : clampf(w, 0.f, 1.f);
}

float Animator::morphWeight(int mesh, int target) const {
    if (mesh < 0 || mesh >= (int)morphAnim_.size() || target < 0 || target >= (int)morphAnim_[(size_t)mesh].size()) return 0.f;
    float u = morphUser_[(size_t)mesh][(size_t)target];
    return u >= 0.f ? u : morphAnim_[(size_t)mesh][(size_t)target];
}

void Animator::finish() {
    if (!model_) return;
    if (boneTex_) {
        for (size_t s = 0; s < model_->skins.size(); ++s) {
            const Skin& sk = model_->skins[s];
            float* row = &boneData_[s * (size_t)texW_ * 4];
            for (size_t j = 0; j < sk.joints.size(); ++j) {
                M4 m = global_[(size_t)sk.joints[j]] * sk.invBind[j];
                std::copy(m.m, m.m + 16, row + j * 16);
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
            float a = morphAnim_[mi][k];
            w[k] = clampf(std::isfinite(u) && u >= 0.f ? u : a, 0.f, 1.f);
            if (std::fabs(w[k] - morphLast_[mi][k]) > 1e-3f) changed = true;
        }
        if (!changed) continue;
        morphLast_[mi] = w;
        for (auto& p : model_->meshes[mi].prims) Model::updateMorph(p, w.data(), (int)tc);
    }
}

float Animator::jointLimit(const std::string& name) {
    // Limits are local-rotation envelopes, deliberately softer than anatomy.
    // They stop a malformed clip from folding a limb while leaving expressive
    // authored poses room to breathe.
    if (name == "首") return 0.85f;
    if (name == "頭") return 1.15f;
    if (name.find("ひじ") == 0 || name.find("ひざ") == 0) return 2.85f;
    if (name.find("手首") == 0 || name.find("足首") == 0) return 2.65f;
    if (name.find("肩") == 0 || name.find("腕") == 0 || name.find("足") == 0) return 3.05f;
    if (name.find("指") != std::string::npos || name.find("中指") != std::string::npos || name.find("薬指") != std::string::npos || name.find("小指") != std::string::npos) return 2.75f;
    if (name == "竿根" || name == "竿中" || name == "竿先" || name == "玉袋") return 1.65f;
    return 3.10f;
}

void Animator::sanitizePose() {
    if (!model_) return;
    for (size_t i = 0; i < pose_.t.size(); ++i) {
        V3& t = pose_.t[i];
        if (!std::isfinite(t.x) || !std::isfinite(t.y) || !std::isfinite(t.z)) {
            t = rest_.t[i];
            if (!guardWarned_) { AG_WARN("AG: pose guard reset non-finite translation on %s", model_->path.c_str()); guardWarned_ = true; }
        }
        t.x = clampf(t.x, -4.f, 4.f); t.y = clampf(t.y, -4.f, 4.f); t.z = clampf(t.z, -4.f, 4.f);
        Quat& r = pose_.r[i];
        float ql = std::sqrt(qdot(r, r));
        if (!std::isfinite(ql) || ql < 1e-6f) {
            r = rest_.r[i];
            if (!guardWarned_) { AG_WARN("AG: pose guard reset invalid rotation on %s", model_->path.c_str()); guardWarned_ = true; }
        } else {
            r = qnormalize(r);
            Quat delta = qnormalize(qmul(qconj(rest_.r[i]), r));
            delta = qclampAngle(delta, jointLimit(model_->nodes[i].name));
            r = qnormalize(qmul(rest_.r[i], delta));
        }
        V3& s = pose_.s[i];
        if (!std::isfinite(s.x) || !std::isfinite(s.y) || !std::isfinite(s.z)) s = rest_.s[i];
        s.x = clampf(s.x, 0.05f, 8.f); s.y = clampf(s.y, 0.05f, 8.f); s.z = clampf(s.z, 0.05f, 8.f);
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
