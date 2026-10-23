// Afterglow engine — story outfits: garments skinned to a character's skeleton (bound by joint name), carrying
// the body's shape morphs, worn in two layers (outer over under) that dissolve away when a character undresses.
// The content ships them as characters/outfits/<who>_<style>.glb: the character's armature + meshes
// <who>_outfit_<style>_outer / _under (node extras afterglow.outfit = {style, layer, hidesWhen}).
#pragma once
#include "gfx/model.h"
#include <string>
#include <vector>

namespace ag {

class Character;

// what a character wears: every layer, the under layer only, or nothing
enum class Dress { Dressed = 0, Partial = 1, Nude = 2 };
Dress dressFromName(const std::string& s);   // "dressed" | "partial" | "nude" (anything else: dressed)

// procedural fabric looks for the plain-coloured garments (pbr.frag u_fabric)
enum class Fabric { Cotton = 0, Satin = 1, Lace = 2, Knit = 3, Denim = 4, Flannel = 5, Linen = 6, Nylon = 7, Wool = 8 };

class Outfit {
public:
    bool load(const std::string& path, const std::string& style, const Character& owner);
    void destroy();
    // which layers show; seconds > 0 dissolves the change in (undressing top to bottom)
    void setDress(Dress d, float seconds);
    // one layer on its own (undressing in steps: outer first, then under)
    void setLayer(int layer, bool shown, float seconds);
    void update(float dt);
    void hold(int layer, float vis) { if (layer >= 0 && layer < 2) vis_[layer] = target_[layer] = vis; }   // stills (QA)
    // body shape (and the arousal shapes) follow the character's current morph weights
    void syncMorphs(const Character& owner);
    float visibility(int layer) const { return layer >= 0 && layer < 2 ? vis_[layer] : 1.f; }
    bool layerShown(int layer) const { return layer >= 0 && layer < 2 && target_[layer] > 0.5f; }
    bool busy() const;
    bool loaded() const { return loaded_; }
    Dress dress() const { return target_[1] < 0.5f ? Dress::Nude : (target_[0] < 0.5f ? Dress::Partial : Dress::Dressed); }

    std::string style;
    Model model;
    std::vector<int> meshLayer;          // per mesh: 0 outer, 1 under
    std::vector<int> fabric;             // per material: Fabric
    std::vector<V3> pattern;             // per material: second colour (plaid lines, lace ground)
    std::vector<V2> heightRange;         // per material: bind-space height span (undress wipe)
    std::vector<int> wipeDown;           // per material: 1 = comes off from the top (dress, trousers), 0 = from below (sweater)

    // garment fit (scene/fit.h), per-vertex textures by mesh then primitive (0: none). A garment's: x = signed distance
    // to its smoothed border (m), y = cover by the outer layer; the wearer's skin: x = cover by outer, y = cover by under
    GLuint fitTexture(int mesh, size_t prim) const;
    GLuint bodyFitTexture(int mesh, size_t prim) const;

private:
    bool loaded_ = false;
    float vis_[2] = {1.f, 1.f}, target_[2] = {1.f, 1.f}, rate_[2] = {1.f, 1.f};
    struct Link { int mesh = -1, target = -1; };
    std::vector<std::vector<Link>> links_;   // per outfit mesh, per target: the character's (mesh, target)
    std::vector<std::vector<float>> lastW_;
    std::vector<std::vector<GLuint>> fit_, bodyFit_;
    void computeFit(const Character& owner);
};

}  // namespace ag
