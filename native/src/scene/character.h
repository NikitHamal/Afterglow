// Afterglow engine — a rigged character: model + animator + procedural layers + look customisation.
// This is the main object the gameplay code drives.
#pragma once
#include "gfx/model.h"
#include "anim/animator.h"
#include "anim/procedural.h"
#include "scene/touch.h"
#include "scene/outfit.h"
#include "scene/hairstyle.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace ag {

class Character {
public:
    // targetHeight: real-world height in metres; legacy models of another scale are fitted to it.
    // id is the role ("vivian" / "marcus": clips, contacts, outfits and story casting key off it); asset names the model's
    // own files when it is another face on the same body (characters/<asset>_identity.bin, <asset>_skin_<tone>.jpg,
    // <asset>_eye.png, <asset>_skin_mask.png — each falls back to the role's file).
    bool load(const std::string& id, const std::string& glbPath, float targetHeight, const std::string& asset = "");
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

    // ---- bone offsets: additive local rotations applied after clips (pose tweaks) ----
    void setBoneOffset(const std::string& bone, const V3& eulerRad);   // XYZ order, local space
    void clearBoneOffsets() { boneOffsets_.clear(); }

    // ---- morph targets (by name, across all meshes that carry it) ----
    void setMorph(const std::string& name, float w);      // w < 0 releases to animated value
    float morph(const std::string& name) const;
    std::vector<std::string> morphNames() const;
    bool hasMorph(const std::string& name) const { return morphIndex_.count(name) > 0; }
    const std::vector<std::pair<int, int>>* morphTargets(const std::string& name) const {
        auto it = morphIndex_.find(name);
        return it == morphIndex_.end() ? nullptr : &it->second;
    }
    // body slider -1..1: drives <p>_up / <p>_dn pairs, or legacy body_<p> (positive only)
    void setBody(const std::string& param, float v);
    float body(const std::string& param) const;

    // ---- looks ----
    bool setTone(const std::string& tone);                 // honey | caramel | deep
    const std::string& tone() const { return tone_; }
    V3 hairTint{1, 1, 1};                                  // linear multiplier on hair albedo
    V3 skinTint{1, 1, 1};                                  // linear multiplier on skin (+ genital) albedo: tone variants
    float flush = 0.f;                                     // 0..1 arousal flush
    float sweat = 0.f;                                     // 0..1 sweat sheen
    float wet = 0.f;                                       // 0..1 arousal wetness (skin mask alpha)

    // ---- clothes (story mode): characters/outfits/<id>_<style>.glb, "" = none ----
    bool setOutfit(const std::string& style);
    Outfit* outfit() { return outfitCur_ >= 0 ? outfits_[(size_t)outfitCur_].get() : nullptr; }
    const Outfit* outfit() const { return outfitCur_ >= 0 ? outfits_[(size_t)outfitCur_].get() : nullptr; }
    bool hasOutfit(const std::string& style) const;
    bool hideGenitals = false;                             // his genitals stay hidden under clothes

    // ---- hair style: characters/hair/<style>.glb replaces the GLB's own hair ("" = the embedded hair) ----
    bool setHairStyle(const std::string& style);
    HairStyle* hairStyle() { return hair_ && hair_->loaded() ? hair_.get() : nullptr; }
    const HairStyle* hairStyle() const { return hair_ && hair_->loaded() ? hair_.get() : nullptr; }

    // ---- placement + procedural ----
    M4 placement = M4::identity();                         // world transform of the model root
    ProcParams proc;
    // optional pose edit after the clips (world-space bone aims); runs before the procedural layers
    std::function<void(Animator&)> poseHook;
    // optional edit after the procedural layers (look-at, breathing), before touch IK
    std::function<void(Animator&)> postHook;
    // touch: hands onto her own body zones (IK after the procedural layers) + soft-tissue contacts
    TouchRig touch;
    TouchGoals touchGoals;
    void resetSimulation() { procedural.reset(); }

    void update(float dt);

    // ---- queries (valid after update) ----
    int bone(const std::string& name) const { return model.findNode(name); }
    V3 bonePos(const std::string& name) const;
    M4 boneWorld(const std::string& name) const;
    V3 forward() const;                                    // model facing (+Z) in world
    V3 hairCenter() const { return hairCenter_; }          // skull centre (world)
    void headAxes(V3& center, V3& faceFwd, V3& crownUp) const;   // world head frame (cameras, gaze)
    float autoScale() const { return autoScale_; }
    float footOffset() const { return footY_ * autoScale_; }
    M4 rootMatrix() const;

    // per-material texture overrides (tone swaps); 0 = use model texture
    GLuint skinMask() const { return skinMask_; }        // content support maps (0 when absent)
    GLuint skinDetail() const { return skinDetail_; }
    GLuint baseOverride(int material) const { return material >= 0 && material < (int)baseOverride_.size() ? baseOverride_[(size_t)material] : 0; }

    std::string id;
    std::string asset;                                     // model id (files); == id for the base characters
    Model model;
    Animator anim;
    Procedural procedural;

private:
    const BoneMask* mask(const std::string& name);
    void measureBindHeight(float& minY, float& maxY);
    void replaceBase(size_t material, GLuint tex);

    bool loaded_ = false;
    float autoScale_ = 1.f, footY_ = 0.f;
    V3 hairCenter_{0, 1.6f, 0};
    GLuint skinMask_ = 0, skinDetail_ = 0;
    std::string dir_, tone_;
    struct Layer { int clip = -1; float t = 0.f, w = 0.f; std::string mask; };
    Layer layers_[kLayers];
    std::unordered_map<std::string, BoneMask> masks_;
    std::unordered_map<std::string, std::vector<std::pair<int, int>>> morphIndex_;   // name -> (mesh, target)
    std::unordered_map<std::string, float> bodyValues_;
    std::unordered_map<int, Quat> boneOffsets_;
    std::vector<GLuint> baseOverride_;
    std::unordered_map<std::string, GLuint> toneCache_;
    std::vector<std::unique_ptr<Outfit>> outfits_;           // loaded styles (at most two kept)
    std::unique_ptr<HairStyle> hair_;
    std::string own(const std::string& suffix) const;        // <dir>/<asset><suffix> if it exists, else <dir>/<id><suffix>
    int outfitCur_ = -1;
};

}  // namespace ag
