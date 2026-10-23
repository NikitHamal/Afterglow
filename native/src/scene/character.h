// Afterglow engine — a rigged character: model + animator + procedural layers + look customisation.
// This is the main object the gameplay code drives.
#pragma once
#include "gfx/model.h"
#include "anim/animator.h"
#include "anim/procedural.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace ag {

class Character {
public:
    // targetHeight: real-world height in metres; legacy models of another scale are fitted to it.
    bool load(const std::string& id, const std::string& glbPath, float targetHeight);
    void destroy();
    bool loaded() const { return loaded_; }

    // ---- animation layers (evaluated in order; layer 0 usually the base clip) ----
    static constexpr int kLayers = 6;
    // t01: normalized clip time 0..1 (for ".in" clips this is penetration depth; for loops, the phase)
    void setLayer(int layer, const std::string& clip, float t01, float weight = 1.f, const std::string& mask = "all");
    void clearLayer(int layer);
    void clearLayers();
    bool hasClip(const std::string& name) const { return model.findClip(name) >= 0; }
    float clipDuration(const std::string& name) const;
    std::vector<std::string> clipNames() const;

    // ---- morph targets (by name, across all meshes that carry it) ----
    void setMorph(const std::string& name, float w);      // w < 0 releases to animated value
    float morph(const std::string& name) const;
    std::vector<std::string> morphNames() const;
    bool hasMorph(const std::string& name) const { return morphIndex_.count(name) > 0; }
    // body slider -1..1: drives <p>_up / <p>_dn pairs, or legacy body_<p> (positive only)
    void setBody(const std::string& param, float v);
    float body(const std::string& param) const;

    // ---- looks ----
    bool setTone(const std::string& tone);                 // honey | caramel | deep
    const std::string& tone() const { return tone_; }
    V3 hairTint{1, 1, 1};                                  // linear multiplier on hair albedo
    float flush = 0.f;                                     // 0..1 arousal flush
    float sweat = 0.f;                                     // 0..1 sweat sheen

    // ---- placement + procedural ----
    M4 placement = M4::identity();                         // world transform of the model root
    ProcParams proc;
    void resetSimulation() { procedural.reset(); }

    void update(float dt);

    // ---- queries (valid after update) ----
    int bone(const std::string& name) const { return model.findNode(name); }
    V3 bonePos(const std::string& name) const;
    M4 boneWorld(const std::string& name) const;
    V3 forward() const;                                    // model facing (+Z) in world
    float autoScale() const { return autoScale_; }
    float footOffset() const { return footY_ * autoScale_; }
    M4 rootMatrix() const;

    // per-material texture overrides (tone swaps); 0 = use model texture
    GLuint baseOverride(int material) const { return material >= 0 && material < (int)baseOverride_.size() ? baseOverride_[(size_t)material] : 0; }

    std::string id;
    Model model;
    Animator anim;
    Procedural procedural;

private:
    const BoneMask* mask(const std::string& name);
    void measureBindHeight(float& minY, float& maxY);

    bool loaded_ = false;
    float autoScale_ = 1.f, footY_ = 0.f;
    std::string dir_, tone_;
    struct Layer { int clip = -1; float t = 0.f, w = 0.f; std::string mask; };
    Layer layers_[kLayers];
    std::unordered_map<std::string, BoneMask> masks_;
    std::unordered_map<std::string, std::vector<std::pair<int, int>>> morphIndex_;   // name -> (mesh, target)
    std::unordered_map<std::string, float> bodyValues_;
    std::vector<GLuint> baseOverride_;
    std::unordered_map<std::string, GLuint> toneCache_;
};

}  // namespace ag
