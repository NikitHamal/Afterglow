// The models (characters/roster.json): every role ("her" = Vivian's body, "him" = Marcus') has several faces that share
// that body, skeleton, clips, contacts and outfits — only the face (identity pack), skin, eyes and hair style differ, so
// every act and story clip fits each of them. Stories keep their own cast; free play and Customize show the player's pick.
#include "game/game.h"
#include "core/log.h"
#include "core/paths.h"

namespace ag {

namespace {
const Json kNullJson;
const char* kRole[2] = {"vivian", "marcus"};
const char* kRoleKey[2] = {"her", "him"};
}

const Json& Game::rosterEntry(int role, const std::string& id) const {
    const Json& list = roster_[kRoleKey[role ? 1 : 0]];
    for (const Json& e : list.arr) if (e["id"].str() == id) return e;
    return kNullJson;
}

const Json& Game::castEntry(int role) const {
    const Character& c = role ? scene.marcus : scene.vivian;
    const Json& e = rosterEntry(role, c.loaded() ? c.asset : kRole[role ? 1 : 0]);
    return e.isNull() ? rosterEntry(role, kRole[role ? 1 : 0]) : e;
}

std::string Game::castName(int role) const {
    if (storyCast_) return role ? "Marcus" : "Vivian";
    const Json& e = castEntry(role);
    return e.isNull() ? (role ? "Marcus" : "Vivian") : e["name"].str(role ? "Marcus" : "Vivian");
}

bool Game::ensureCast(bool story) {
    if (roster_.isNull()) {
        std::string f = paths::findAsset({"characters/roster.json"});
        roster_ = f.empty() ? Json::object() : Json::parse(paths::readText(f));
    }
    storyCast_ = story;
    bool reloaded = false;
    for (int r = 0; r < 2; ++r) {
        std::string want = story ? kRole[r] : (r ? profile.him.model : profile.her.model);
        if (rosterEntry(r, want).isNull()) want = kRole[r];   // unknown (or removed) model: the role's own face
        const Json& e = rosterEntry(r, want);
        Character& c = r ? scene.marcus : scene.vivian;
        if (c.loaded() && c.asset == want && (c.hairStyle() ? c.hairStyle()->style : std::string()) == e["hair"].str("")) continue;
        reloaded |= !c.loaded() || c.asset != want;
        scene.loadRole(kRole[r], want, e["hair"].str(""));
    }
    if (reloaded) {
        AG_INFO("AG: cast %s: %s + %s", story ? "story" : "player", scene.vivian.asset.c_str(), scene.marcus.asset.c_str());
        applyLooks();
    }
    return reloaded;
}

}  // namespace ag
