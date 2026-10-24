// Afterglow engine — hair styles: characters/hair/<style>.glb, a hair mesh skinned to a character's skeleton (bound by
// joint name, like outfits) that replaces the hair embedded in the character's GLB. Rigid parts ride 頭; loose locks ride
// the character's own simulated hair chains (髪.<n>.a..d), so the same spring bones and colliders move them. Morph
// targets with the character's names (weight_up / weight_dn) follow its head shape.
#pragma once
#include "gfx/model.h"
#include <string>
#include <vector>

namespace ag {

class Character;

class HairStyle {
public:
    bool load(const std::string& path, const std::string& style, const Character& owner);
    void destroy();
    void syncMorphs(const Character& owner);
    bool loaded() const { return loaded_; }
    std::string style;
    Model model;

private:
    bool loaded_ = false;
    struct Link { int mesh = -1, target = -1; };
    std::vector<std::vector<Link>> links_;
    std::vector<std::vector<float>> lastW_;
};

}  // namespace ag
