#include "scene/hairstyle.h"
#include "scene/character.h"
#include "core/log.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace ag {

bool HairStyle::load(const std::string& path, const std::string& st, const Character& owner) {
    destroy();
    style = st;
    if (!model.load(path)) return false;
    const Model& cm = owner.model;
    if (cm.skins.empty() || model.skins.empty()) { AG_WARN("AG: hair %s: no skin", path.c_str()); model.destroy(); return false; }
    // joint -> the character's skin-0 joint of the same name (a joint it lacks rides its nearest named ancestor)
    std::unordered_map<std::string, int> charJoint;
    const Skin& cs = cm.skins[0];
    for (size_t j = 0; j < cs.joints.size(); ++j) charJoint[cm.nodes[(size_t)cs.joints[j]].name] = (int)j;
    int missing = 0;
    std::vector<std::vector<int>> remap(model.skins.size());
    for (size_t si = 0; si < model.skins.size(); ++si) {
        const Skin& s = model.skins[si];
        remap[si].assign(s.joints.size(), 0);
        for (size_t j = 0; j < s.joints.size(); ++j) {
            int node = s.joints[j], hit = -1;
            while (node >= 0 && hit < 0) {
                auto it = charJoint.find(model.nodes[(size_t)node].name);
                if (it != charJoint.end()) hit = it->second;
                else node = model.nodes[(size_t)node].parent;
            }
            if (hit < 0) {
                auto head = charJoint.find("\xe9\xa0\xad");   // 頭: anything unmatched rides the head, never the root
                hit = head != charJoint.end() ? head->second : 0;
                ++missing;
            }
            remap[si][j] = hit;
        }
    }
    for (Node& n : model.nodes) {
        if (n.mesh < 0 || n.skin < 0) continue;
        for (Primitive& p : model.meshes[(size_t)n.mesh].prims) {
            if (p.joints.empty() || !p.vbo[5]) continue;
            for (auto& j : p.joints) j = (uint16_t)(j < remap[(size_t)n.skin].size() ? remap[(size_t)n.skin][j] : 0);
            std::vector<float> jf(p.joints.begin(), p.joints.end());
            glBindBuffer(GL_ARRAY_BUFFER, p.vbo[5]);
            glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(jf.size() * sizeof(float)), jf.data());
        }
        n.skin = 0;   // the character's bone texture skins the hair directly (same bind space)
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    for (Material& m : model.materials) { m.shading = Shading::Hair; m.metallic = 0.f; m.castShadow = true; }
    links_.assign(model.meshes.size(), {});
    lastW_.assign(model.meshes.size(), {});
    for (size_t mi = 0; mi < model.meshes.size(); ++mi) {
        const Mesh& me = model.meshes[mi];
        links_[mi].assign(me.targetNames.size(), Link{});
        lastW_[mi].assign(me.targetNames.size(), -1.f);
        for (size_t t = 0; t < me.targetNames.size(); ++t) {
            const auto* ct = owner.morphTargets(me.targetNames[t]);
            if (ct && !ct->empty()) links_[mi][t] = Link{(*ct)[0].first, (*ct)[0].second};
        }
    }
    model.releaseCpuData();
    loaded_ = true;
    AG_INFO("AG: hair %s for %s: %d meshes%s", style.c_str(), owner.id.c_str(), (int)model.meshes.size(), missing ? " (some joints unmatched)" : "");
    return true;
}

void HairStyle::destroy() {
    model.destroy();
    links_.clear();
    lastW_.clear();
    loaded_ = false;
}

void HairStyle::syncMorphs(const Character& owner) {
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
