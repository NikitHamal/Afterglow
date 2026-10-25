#include "scene/character.h"
#include "scene/identity.h"
#include "gfx/texture.h"
#include "core/paths.h"
#include "core/log.h"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace ag {

static std::string lowerStr(std::string s) { for (auto& c : s) c = (char)std::tolower((unsigned char)c); return s; }

std::string Character::own(const std::string& suffix) const {
    std::string a = paths::join(dir_, asset + suffix);
    return asset != id && paths::exists(a) ? a : paths::join(dir_, id + suffix);
}

bool Character::load(const std::string& charId, const std::string& path, float targetHeight, const std::string& assetId) {
    destroy();
    id = charId;
    asset = assetId.empty() ? charId : assetId;
    if (!model.load(path)) return false;
    dir_ = paths::dirname(path);
    // another face on this body: its identity deltas go in before anything measures the bind mesh
    if (asset != id && !applyIdentityPack(model, paths::join(dir_, asset + "_identity.bin"))) {
        AG_WARN("AG: %s: no identity pack for %s, showing the base face", id.c_str(), asset.c_str());
    }
    anim.init(&model);
    for (size_t mi = 0; mi < model.meshes.size(); ++mi)
        for (size_t t = 0; t < model.meshes[mi].targetNames.size(); ++t)
            morphIndex_[model.meshes[mi].targetNames[t]].push_back({(int)mi, (int)t});
    baseOverride_.assign(model.materials.size(), 0);
    // optional skin support maps next to the GLB (linear data)
    std::string maskFile = own("_skin_mask.png"), detailFile = paths::join(dir_, "skin_detail_n.png");
    if (paths::exists(maskFile)) skinMask_ = textureFromFile(maskFile, false);
    if (paths::exists(detailFile)) skinDetail_ = textureFromFile(detailFile, false);
    // optional eye albedo next to the GLB (tools/model/native/eye_tex.py: limbal ring, richer iris, toned sclera)
    std::string eyeFile = own("_eye.png");
    if (paths::exists(eyeFile))
        for (size_t mi = 0; mi < model.materials.size(); ++mi)
            if (model.materials[mi].shading == Shading::Eye) {
                GLuint t = 0;
                auto it = toneCache_.find(eyeFile);
                if (it != toneCache_.end()) t = it->second;
                else if ((t = textureFromFile(eyeFile, true)) != 0) toneCache_[eyeFile] = t;
                if (t) replaceBase(mi, t);
            }

    float mn = 0.f, mx = 0.f;
    measureBindHeight(mn, mx);
    float h = mx - mn;
    autoScale_ = 1.f;
    if (h > 0.3f && std::fabs(h - targetHeight) / targetHeight > 0.03f) autoScale_ = targetHeight / h;
    footY_ = mn;
    AG_INFO("AG: character %s: bind height %.3f m -> scale %.3f (target %.2f m)", id.c_str(), h, autoScale_, targetHeight);
    procedural.init(anim, autoScale_);
    rom.build(model);          // anatomical ranges widened to the clips' own envelope
    touch.init(*this);         // body zones from the bind mesh
    model.releaseCpuData();   // bind height measured, zones found: the GPU has the rest
    loaded_ = true;
    return true;
}

void Character::destroy() {
    hair_.reset();
    tone_.clear();   // a reloaded model gets its tone applied again
    outfits_.clear();
    outfitCur_ = -1;
    hideGenitals = false;
    for (auto& kv : toneCache_) { GLuint t = kv.second; textureDestroy(t); }
    toneCache_.clear();
    textureDestroy(skinMask_);
    textureDestroy(skinDetail_);
    anim.destroy();
    model.destroy();
    morphIndex_.clear(); masks_.clear(); bodyValues_.clear(); baseOverride_.clear();
    for (auto& l : layers_) l = Layer{};
    loaded_ = false;
}

// bind-pose height of the skinned body (positions skinned on the CPU at rest; handles quantized GLBs)
void Character::measureBindHeight(float& mn, float& mx) {
    anim.begin();
    anim.computeGlobals(M4::identity());
    mn = 1e9f; mx = -1e9f;
    for (size_t ni = 0; ni < model.nodes.size(); ++ni) {
        const Node& n = model.nodes[ni];
        if (n.mesh < 0) continue;
        const Mesh& mesh = model.meshes[(size_t)n.mesh];
        for (const Primitive& p : mesh.prims) {
            bool sk = p.skinned && n.skin >= 0;
            const Skin* skin = sk ? &model.skins[(size_t)n.skin] : nullptr;
            M4 nodeM = anim.global((int)ni);
            for (int v = 0; v < p.vertexCount; v += 3) {
                V3 x{p.pos[(size_t)v * 3], p.pos[(size_t)v * 3 + 1], p.pos[(size_t)v * 3 + 2]};
                V3 w;
                if (sk) {
                    int j = p.joints[(size_t)v * 4];
                    if (j >= (int)skin->joints.size()) continue;
                    w = xformPoint(anim.global(skin->joints[(size_t)j]) * skin->invBind[(size_t)j], x);
                } else {
                    w = xformPoint(nodeM, x);
                }
                mn = std::min(mn, w.y); mx = std::max(mx, w.y);
            }
        }
    }
    if (mn > mx) { mn = 0.f; mx = 1.7f; }
}

void Character::setLayer(int i, const std::string& clip, float t01, float weight, const std::string& m) {
    if (i < 0 || i >= kLayers) return;
    Layer& l = layers_[i];
    l.clip = model.findClip(clip);
    l.t = clampf(t01, 0.f, 1.f);
    l.w = weight;
    l.mask = m;
}
void Character::clearLayer(int i) { if (i >= 0 && i < kLayers) layers_[i] = Layer{}; }
void Character::clearLayers() { for (auto& l : layers_) l = Layer{}; }

float Character::clipDuration(const std::string& name) const {
    int c = model.findClip(name);
    return c >= 0 ? model.clips[(size_t)c].duration : 0.f;
}
std::vector<std::string> Character::clipNames() const {
    std::vector<std::string> out;
    for (auto& c : model.clips) out.push_back(c.name);
    return out;
}

const BoneMask* Character::mask(const std::string& name) {
    if (name.empty() || name == "all") return nullptr;
    auto it = masks_.find(name);
    if (it != masks_.end()) return &it->second;
    std::vector<std::string> roots;
    if (name.find('+') != std::string::npos) {           // union, e.g. "arms+head"
        BoneMask u(model.nodes.size(), 0.f);
        size_t i = 0;
        while (i <= name.size()) {
            size_t j = name.find('+', i);
            if (j == std::string::npos) j = name.size();
            const BoneMask* part = mask(name.substr(i, j - i));
            if (!part) { std::fill(u.begin(), u.end(), 1.f); break; }
            for (size_t k = 0; k < u.size() && k < part->size(); ++k) u[k] = std::max(u[k], (*part)[k]);
            i = j + 1;
        }
        auto res = masks_.emplace(name, std::move(u));
        return &res.first->second;
    }
    if (name == "arm.L") roots = {"肩.L"};
    else if (name == "arm.R") roots = {"肩.R"};
    else if (name == "arms") roots = {"肩.L", "肩.R"};
    else if (name == "hand.L") roots = {"手首.L"};
    else if (name == "hand.R") roots = {"手首.R"};
    else if (name == "head") roots = {"首"};
    else if (name == "legs") roots = {"足.L", "足.R"};
    else if (name == "upper") roots = {"上半身"};
    else if (name == "lower") roots = {"下半身"};
    else if (name.rfind("bones:", 0) == 0) {
        std::string rest = name.substr(6);
        size_t i = 0;
        while (i <= rest.size()) {
            size_t j = rest.find(',', i);
            if (j == std::string::npos) j = rest.size();
            if (j > i) roots.push_back(rest.substr(i, j - i));
            i = j + 1;
        }
    } else {
        roots = {name};
    }
    auto res = masks_.emplace(name, anim.makeMask(roots));
    return &res.first->second;
}

void Character::setMorph(const std::string& name, float w) {
    auto it = morphIndex_.find(name);
    if (it == morphIndex_.end()) return;
    for (auto& mt : it->second) anim.setMorphOverride(mt.first, mt.second, w);
}
float Character::morph(const std::string& name) const {
    auto it = morphIndex_.find(name);
    if (it == morphIndex_.end() || it->second.empty()) return 0.f;
    return anim.morphWeight(it->second[0].first, it->second[0].second);
}
std::vector<std::string> Character::morphNames() const {
    std::vector<std::string> out;
    for (auto& kv : morphIndex_) out.push_back(kv.first);
    std::sort(out.begin(), out.end());
    return out;
}

void Character::setBody(const std::string& p, float v) {
    v = clampf(v, -1.f, 1.f);
    bodyValues_[p] = v;
    if (hasMorph(p + "_up") || hasMorph(p + "_dn")) {
        setMorph(p + "_up", std::max(v, 0.f));
        setMorph(p + "_dn", std::max(-v, 0.f));
    } else if (hasMorph("body_" + p)) {
        setMorph("body_" + p, std::max(v, 0.f));
    } else if (hasMorph(p)) {
        setMorph(p, std::max(v, 0.f));
    }
}
float Character::body(const std::string& p) const {
    auto it = bodyValues_.find(p);
    return it == bodyValues_.end() ? 0.f : it->second;
}

bool Character::setTone(const std::string& tone) {
    if (tone == tone_) return true;
    bool any = false;
    for (size_t mi = 0; mi < model.materials.size(); ++mi) {
        std::string n = lowerStr(model.materials[mi].name);
        std::string part;
        if (n.find("skin") != std::string::npos) part = "skin";
        else if (n.find("genital") != std::string::npos) part = "genital";
        else continue;
        const std::vector<std::string> cands = {
            paths::join(dir_, asset + "_" + part + "_" + tone + ".jpg"),
            paths::join(dir_, id + "_" + part + "_" + tone + ".jpg"),
            paths::join(dir_, id + "_" + part + "_" + tone + ".png"),
            paths::join(dir_, part + "_" + tone + ".jpg"),
        };
        std::string file;
        for (auto& c : cands) if (paths::exists(c)) { file = c; break; }
        if (file.empty()) continue;
        auto it = toneCache_.find(file);
        GLuint t = 0;
        if (it != toneCache_.end()) t = it->second;
        else { t = textureFromFile(file, true); toneCache_[file] = t; }
        if (!t) continue;
        GLuint prev = baseOverride_[mi];
        replaceBase(mi, t);
        any = true;
        if (prev && prev != t) {
            bool used = false;
            for (GLuint o : baseOverride_) used = used || o == prev;
            if (!used) {
                for (auto c = toneCache_.begin(); c != toneCache_.end();) {
                    if (c->second == prev) c = toneCache_.erase(c); else ++c;
                }
                textureDestroy(prev);
            }
        }
    }
    if (any) tone_ = tone;
    return any;
}

bool Character::setHairStyle(const std::string& style) {
    if (style.empty()) { hair_.reset(); return true; }
    if (hair_ && hair_->loaded() && hair_->style == style) return true;
    std::string file = paths::join(paths::join(dir_, "hair"), style + ".glb");
    if (!paths::exists(file)) { AG_WARN("AG: %s: no hair style %s", id.c_str(), style.c_str()); hair_.reset(); return false; }
    auto h = std::make_unique<HairStyle>();
    if (!h->load(file, style, *this)) { hair_.reset(); return false; }
    hair_ = std::move(h);
    return true;
}

M4 Character::rootMatrix() const { return placement * m4scale({autoScale_, autoScale_, autoScale_}); }

bool Character::hasOutfit(const std::string& style) const {
    if (style.empty()) return false;
    return paths::exists(paths::join(paths::join(dir_, "outfits"), id + "_" + style + ".glb"));
}

bool Character::setOutfit(const std::string& style) {
    if (style.empty()) { outfitCur_ = -1; hideGenitals = false; return true; }
    for (size_t i = 0; i < outfits_.size(); ++i)
        if (outfits_[i]->style == style) { outfitCur_ = (int)i; return true; }
    std::string file = paths::join(paths::join(dir_, "outfits"), id + "_" + style + ".glb");
    if (!paths::exists(file)) { AG_WARN("AG: outfit %s missing", file.c_str()); outfitCur_ = -1; return false; }
    auto o = std::make_unique<Outfit>();
    if (!o->load(file, style, *this)) { outfitCur_ = -1; return false; }
    // VRAM: keep the worn style and one more (a story changes clothes between days at most)
    if (outfits_.size() >= 2) {
        size_t drop = outfitCur_ == 0 ? 1 : 0;
        outfits_.erase(outfits_.begin() + (long)drop);
    }
    outfits_.push_back(std::move(o));
    outfitCur_ = (int)outfits_.size() - 1;
    return true;
}

void Character::update(float dt) {
    if (!loaded_) return;
    anim.begin();
    for (int i = 0; i < kLayers; ++i) {
        Layer& l = layers_[i];
        if (l.clip < 0 || l.w <= 0.f) continue;
        const Clip& c = model.clips[(size_t)l.clip];
        anim.applyClip(l.clip, l.t * c.duration, l.w, mask(l.mask));
    }
    for (auto& kv : boneOffsets_) anim.r(kv.first) = qnormalize(qmul(anim.r(kv.first), kv.second));
    anim.computeGlobals(rootMatrix());
    if (poseHook) poseHook(anim);
    procedural.apply(anim, dt, proc);
    if (postHook) postHook(anim);
    if (oralW > 0.001f) oralIK(anim);
    if (touch.ready() && (touchGoals.left.active || touchGoals.right.active)) touch.apply(anim, touchGoals, &rom);
    else touch.contacts.clear();
    if (touch.ready() && oralW > 0.001f && std::fabs(oralCurl) > 1e-3f) touch.curlHands(anim, oralCurl);
    rom.apply(anim);           // no layer may bend a joint past its range (clips are inside it by construction)
    anim.guardPose();          // last gate: no NaN, zero scale or broken quaternion reaches the skin
    anim.finish();
    if (Outfit* o = outfit()) {
        o->update(dt);
        o->syncMorphs(*this);
        hideGenitals = o->visibility(0) > 0.5f || o->visibility(1) > 0.5f;
    } else hideGenitals = false;
    if (HairStyle* h = hairStyle()) h->syncMorphs(*this);
    // skull centre: hair shading bends card normals around it
    int hb = bone("頭");
    if (hb >= 0) hairCenter_ = xformPoint(anim.global(hb), procedural.headUpLocal() * 0.085f + procedural.headForwardLocal() * 0.01f);
}

// the oral correction: the procedural gaze may turn the head after the clip, so put the lip front back on the shaft
// axis, turn the face along the shaft, and roll the crown upright about the lip line. Every joint stays in range and
// only makes a bounded correction to the authored pose.
void Character::oralIK(Animator& a) {
    int hd = bone("頭"), nk = bone("首"), ch = bone("上半身2");
    if (hd < 0 || nk < 0) return;
    const float w = clampf(oralW, 0.f, 1.f);
    V3 mo;
    if (!touch.mouthWorld(a, 0.f, mo)) return;
    const int chain[3] = {hd, nk, ch};
    const int count = ch >= 0 ? 3 : 2;
    const float share[3] = {0.62f, 0.52f, 0.30f};
    const float maxTurn[3] = {0.30f, 0.14f, 0.07f};
    Quat before[3];
    for (int j = 0; j < count; ++j) before[j] = a.r(chain[j]);
    auto P = [&](int n) { return a.global(n).translation(); };
    auto rotq = [&](int n, const Quat& q) {
        if (n < 0) return;
        a.setWorldRot(n, qnormalize(qmul(q, a.worldRot(n))));
        a.recomputeSubtree(n);
    };
    auto clampTurn = [&](int j) {
        int n = chain[j];
        Quat d = qnormalize(qmul(a.r(n), qconj(before[j])));
        float angle = 2.f * std::acos(clampf(std::fabs(d.w), 0.f, 1.f));
        float limit = maxTurn[j] * w;
        if (angle > limit && angle > 1e-5f) {
            a.r(n) = qnormalize(qslerp(before[j], a.r(n), limit / angle));
            a.recomputeSubtree(n);
        }
    };
    auto aim = [&](int iters) {
        for (int it = 0; it < iters; ++it) {
            for (int j = 0; j < count; ++j) {
                int n = chain[j];
                touch.mouthWorld(a, 0.f, mo);
                V3 pj = P(n), v0 = mo - pj, v1 = oralTarget - pj;
                if (length(v0) < 1e-4f || length(v1) < 1e-4f) continue;
                rotq(n, qslerp(Quat{}, qfromto(normalize(v0), normalize(v1)), share[j]));
                clampTurn(j);
                rom.clampNode(a, n);
            }
            touch.mouthWorld(a, 0.f, mo);
            if (length(mo - oralTarget) < 0.001f) break;
        }
    };
    aim(12);
    V3 want = normalize(oralFace);
    V3 face = qrotate(a.worldRot(hd), procedural.headForwardLocal());
    if (length(face) > 1e-4f && length(want) > 1e-4f) {
        rotq(hd, qslerp(Quat{}, qfromto(normalize(face), want), 0.8f * w));
        clampTurn(0);
        rom.clampNode(a, hd);
    }
    aim(6);
    touch.mouthWorld(a, 0.f, mo);
    V3 axis = mo - P(hd);
    if (length(axis) > 1e-4f) {
        axis = normalize(axis);
        V3 cu = qrotate(a.worldRot(hd), procedural.headUpLocal());
        V3 u0 = cu - axis * dot(cu, axis), u1 = V3{0, 1, 0} - axis * axis.y;
        if (length(u0) > 1e-4f && length(u1) > 1e-4f) {
            u0 = normalize(u0);
            u1 = normalize(u1);
            float ang = std::atan2(dot(cross(u0, u1), axis), dot(u0, u1));
            rotq(hd, qaxis(axis, clampf(ang, -0.28f, 0.28f) * w));
            clampTurn(0);
            rom.clampNode(a, hd);
        }
    }
    aim(4);
}

void Character::headAxes(V3& center, V3& faceFwd, V3& crownUp) const {
    int hb = bone("頭");
    if (hb < 0) { center = placement.translation() + V3{0, 1.5f, 0}; faceFwd = forward(); crownUp = {0, 1, 0}; return; }
    const M4& g = anim.global(hb);
    center = hairCenter_;
    faceFwd = normalize(xformDir(g, procedural.headForwardLocal()));
    crownUp = normalize(xformDir(g, procedural.headUpLocal()));
}

void Character::setBoneOffset(const std::string& name, const V3& e) {
    int b = bone(name);
    if (b < 0) return;
    if (std::fabs(e.x) + std::fabs(e.y) + std::fabs(e.z) < 1e-5f) { boneOffsets_.erase(b); return; }
    boneOffsets_[b] = qnormalize(qmul(qmul(qaxis({1, 0, 0}, e.x), qaxis({0, 1, 0}, e.y)), qaxis({0, 0, 1}, e.z)));
}

V3 Character::bonePos(const std::string& name) const {
    int b = bone(name);
    return b >= 0 ? anim.global(b).translation() : placement.translation();
}
M4 Character::boneWorld(const std::string& name) const {
    int b = bone(name);
    return b >= 0 ? anim.global(b) : placement;
}
// VRAM: an override replaces the embedded albedo for good (only the live override stays cached)
void Character::replaceBase(size_t mi, GLuint t) {
    baseOverride_[mi] = t;
    Material& mat = model.materials[mi];
    if (mat.texBase && mat.texBase != t) {
        for (auto& mt : model.textures) if (mt == mat.texBase) mt = 0;
        GLuint dead = mat.texBase;
        for (auto& other : model.materials) if (other.texBase == dead) other.texBase = 0;
        textureDestroy(dead);
    }
}

V3 Character::forward() const { return normalize(xformDir(placement, V3{0, 0, 1})); }

}  // namespace ag
