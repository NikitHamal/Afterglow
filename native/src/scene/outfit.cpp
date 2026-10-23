#include "scene/outfit.h"
#include "scene/character.h"
#include "core/log.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace ag {

Dress dressFromName(const std::string& s) {
    if (s == "nude" || s == "naked") return Dress::Nude;
    if (s == "partial" || s == "underwear" || s == "under") return Dress::Partial;
    return Dress::Dressed;
}

namespace {
std::string lowerS(std::string s) { for (auto& c : s) c = (char)std::tolower((unsigned char)c); return s; }
bool has(const std::string& s, const char* k) { return s.find(k) != std::string::npos; }

// plain-coloured garments get a fabric by what they are (style + layer + top/bottom)
void fabricFor(const std::string& style, const std::string& matName, int layer, int& fab, V3& pattern, int& wipeDown) {
    std::string n = lowerS(matName);
    bool bottom = has(n, "bottom");
    wipeDown = 1;
    pattern = {0.f, 0.f, 0.f};
    if (has(n, "lace")) { fab = (int)Fabric::Lace; return; }
    if (style == "evening_dress") { fab = layer == 0 ? (int)Fabric::Satin : (int)Fabric::Lace; pattern = {0.05f, 0.04f, 0.045f}; return; }
    if (style == "knit_sweater_jeans") {
        if (layer == 1) { fab = (int)Fabric::Cotton; return; }
        if (bottom) { fab = (int)Fabric::Denim; return; }
        fab = (int)Fabric::Knit; wipeDown = 0;   // a sweater comes off over the head
        return;
    }
    if (style == "sundress_bikini") { fab = layer == 0 ? (int)Fabric::Cotton : (int)Fabric::Nylon; return; }
    if (style == "flannel_jeans") {
        if (layer == 1) { fab = (int)Fabric::Cotton; return; }
        if (bottom) { fab = (int)Fabric::Denim; return; }
        fab = (int)Fabric::Flannel; pattern = {0.035f, 0.03f, 0.045f};   // red plaid with near-black lines
        return;
    }
    if (style == "suit_open_collar") { fab = layer == 1 ? (int)Fabric::Cotton : (bottom ? (int)Fabric::Wool : (int)Fabric::Cotton); return; }
    if (style == "linen_shirt_shorts") { fab = layer == 1 ? (int)Fabric::Cotton : (bottom ? (int)Fabric::Cotton : (int)Fabric::Linen); return; }
    fab = (int)Fabric::Cotton;
}
}  // namespace

bool Outfit::load(const std::string& path, const std::string& st, const Character& owner) {
    destroy();
    style = st;
    if (!model.load(path)) return false;
    const Model& cm = owner.model;
    if (cm.skins.empty() || model.skins.empty()) { AG_WARN("AG: outfit %s: no skin", path.c_str()); model.destroy(); return false; }
    // ---- bind to the character's skeleton: outfit joint -> the character's skin-0 joint of the same name ----
    std::unordered_map<std::string, int> charJoint;
    const Skin& cs = cm.skins[0];
    for (size_t j = 0; j < cs.joints.size(); ++j) charJoint[cm.nodes[(size_t)cs.joints[j]].name] = (int)j;
    std::vector<std::vector<int>> remap(model.skins.size());
    int missing = 0;
    for (size_t si = 0; si < model.skins.size(); ++si) {
        const Skin& s = model.skins[si];
        remap[si].assign(s.joints.size(), 0);
        for (size_t j = 0; j < s.joints.size(); ++j) {
            // a joint the character lacks rides on its nearest ancestor that it has
            int node = s.joints[j], hit = -1;
            while (node >= 0 && hit < 0) {
                auto it = charJoint.find(model.nodes[(size_t)node].name);
                if (it != charJoint.end()) hit = it->second;
                else node = model.nodes[(size_t)node].parent;
            }
            if (hit < 0) { hit = 0; ++missing; }
            remap[si][j] = hit;
        }
    }
    meshLayer.assign(model.meshes.size(), 0);
    for (const Node& n : model.nodes) {
        if (n.mesh < 0) continue;
        // layer from the node extras (afterglow.outfit.layer), else from the name
        int layer = lowerS(n.name).find("_under") != std::string::npos ? 1 : 0;
        const Json& ex = n.extras["afterglow"];
        Json parsed;
        const Json* ag = &ex;
        if (ex.isString()) { parsed = Json::parse(ex.s); ag = &parsed; }
        const Json& o = (*ag)["outfit"];
        if (o.isObject() && o["layer"].isString()) layer = o["layer"].s == "under" ? 1 : 0;
        meshLayer[(size_t)n.mesh] = layer;
        if (n.skin < 0) continue;
        for (Primitive& p : model.meshes[(size_t)n.mesh].prims) {
            if (p.joints.empty() || !p.vbo[5]) continue;
            for (auto& j : p.joints) j = (uint16_t)(j < remap[(size_t)n.skin].size() ? remap[(size_t)n.skin][j] : 0);
            std::vector<float> jf(p.joints.begin(), p.joints.end());
            glBindBuffer(GL_ARRAY_BUFFER, p.vbo[5]);
            glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(jf.size() * sizeof(float)), jf.data());
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    // the same skin both ways: the character's bone texture skins the garment directly (same bind matrices)
    for (Node& n : model.nodes) if (n.mesh >= 0 && n.skin >= 0) n.skin = 0;
    // ---- materials: procedural fabrics, the undress wipe spans each garment's height ----
    fabric.assign(model.materials.size(), (int)Fabric::Cotton);
    pattern.assign(model.materials.size(), V3{0, 0, 0});
    heightRange.assign(model.materials.size(), V2{0.f, 1.8f});
    wipeDown.assign(model.materials.size(), 1);
    std::vector<V2> span(model.materials.size(), V2{1e9f, -1e9f});
    for (size_t mi = 0; mi < model.meshes.size(); ++mi)
        for (const Primitive& p : model.meshes[mi].prims) {
            if (p.material < 0 || p.material >= (int)span.size()) continue;
            V2& sp = span[(size_t)p.material];
            sp.x = std::min(sp.x, p.bmin.y); sp.y = std::max(sp.y, p.bmax.y);
            int f = 0, wd = 1; V3 pat;
            fabricFor(style, model.materials[(size_t)p.material].name, meshLayer[mi], f, pat, wd);
            fabric[(size_t)p.material] = f; pattern[(size_t)p.material] = pat; wipeDown[(size_t)p.material] = wd;
        }
    for (size_t i = 0; i < span.size(); ++i) if (span[i].y > span[i].x) heightRange[i] = span[i];
    for (Material& m : model.materials) { m.shading = Shading::Cloth; m.doubleSided = true; m.metallic = 0.f; }
    // ---- morphs: each outfit target follows the character's target of the same name ----
    links_.assign(model.meshes.size(), {});
    lastW_.assign(model.meshes.size(), {});
    for (size_t mi = 0; mi < model.meshes.size(); ++mi) {
        const Mesh& me = model.meshes[mi];
        size_t tc = me.targetNames.size();
        for (const Primitive& p : me.prims) tc = std::max(tc, (size_t)p.targetCount);
        links_[mi].assign(tc, Link{});
        lastW_[mi].assign(tc, -1.f);
        for (size_t t = 0; t < me.targetNames.size(); ++t) {
            const auto* ct = owner.morphTargets(me.targetNames[t]);
            if (ct && !ct->empty()) links_[mi][t] = Link{(*ct)[0].first, (*ct)[0].second};
        }
    }
    model.releaseCpuData();
    loaded_ = true;
    vis_[0] = vis_[1] = target_[0] = target_[1] = 1.f;
    AG_INFO("AG: outfit %s for %s: %d meshes, %d materials%s", style.c_str(), owner.id.c_str(), (int)model.meshes.size(),
            (int)model.materials.size(), missing ? " (some joints unmatched)" : "");
    return true;
}

void Outfit::destroy() {
    model.destroy();
    meshLayer.clear(); fabric.clear(); pattern.clear(); heightRange.clear(); wipeDown.clear();
    links_.clear(); lastW_.clear();
    loaded_ = false;
}

void Outfit::setLayer(int layer, bool shown, float seconds) {
    if (layer < 0 || layer > 1) return;
    target_[layer] = shown ? 1.f : 0.f;
    if (seconds <= 0.f) vis_[layer] = target_[layer];
    else rate_[layer] = 1.f / seconds;
}

void Outfit::setDress(Dress d, float seconds) {
    setLayer(0, d == Dress::Dressed, seconds);
    setLayer(1, d != Dress::Nude, seconds);
}

bool Outfit::busy() const { return std::fabs(vis_[0] - target_[0]) > 1e-3f || std::fabs(vis_[1] - target_[1]) > 1e-3f; }

void Outfit::update(float dt) {
    for (int l = 0; l < 2; ++l) {
        // the under layer only starts to go once the outer one is off (and comes back first)
        if (l == 1 && target_[1] < vis_[1] && vis_[0] > 0.02f) continue;
        if (l == 0 && target_[0] > vis_[0] && vis_[1] < 0.98f && target_[1] > 0.5f) continue;
        float d = target_[l] - vis_[l];
        float step = rate_[l] * dt;
        vis_[l] = std::fabs(d) <= step ? target_[l] : vis_[l] + (d > 0.f ? step : -step);
    }
}

void Outfit::syncMorphs(const Character& owner) {
    if (!loaded_) return;
    std::vector<float> w;
    for (size_t mi = 0; mi < model.meshes.size(); ++mi) {
        const std::vector<Link>& ln = links_[mi];
        if (ln.empty()) continue;
        w.assign(ln.size(), 0.f);
        bool changed = false;
        for (size_t t = 0; t < ln.size(); ++t) {
            if (ln[t].mesh >= 0) w[t] = owner.anim.morphWeight(ln[t].mesh, ln[t].target);
            if (std::fabs(w[t] - lastW_[mi][t]) > 1e-3f) changed = true;
        }
        if (!changed) continue;
        lastW_[mi] = w;
        for (Primitive& p : model.meshes[mi].prims) Model::updateMorph(p, w.data(), (int)w.size());
    }
}

}  // namespace ag
