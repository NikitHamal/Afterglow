// Afterglow — story mode as a film. Every node is a staged scene: a place (the story locations, or the closest
// one that exists), its light and sound, both actors in their story outfits on their marks (stand / sit / lie
// clips or an act's pose), acting while they talk (gaze, gestures, nods, reactions, expressions, lip sync on the
// Kokoro voice of every line), directed by a camera that composes establishing shots, two-shots, over-the-
// shoulder and close-ups with shallow focus, letterboxed with film subtitles. Intimacy is staged in the same
// place (kiss, undressing layer by layer, to bed) and hands over to play with the story's voice over it.
#include "game/game.h"
#include "core/log.h"
#include "core/paths.h"
#include "raylib.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

namespace ag {

namespace {
// staging distances (m): standing, the necks' gap for an embrace (her hands on his chest) and for a kiss; seated,
// the hips' gap where sliding along the bed stops
constexpr float kEmbraceNeck = 0.40f, kKissNeck = 0.30f, kSitHips = 0.38f;
float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
float approachF(float v, float t, float step) { return v < t ? std::min(t, v + step) : std::max(t, v - step); }
V3 flatv(V3 v) { v.y = 0.f; float l = length(v); return l > 1e-4f ? v / l : V3{1, 0, 0}; }

std::string slugOf(const std::string& s) {
    std::string out;
    bool us = false;
    for (unsigned char c : s) {
        if (std::isalnum(c)) { out += (char)std::tolower(c); us = false; }
        else if (!out.empty() && !us) { out += '_'; us = true; }
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    for (const char* pre : {"the_", "a_", "an_"}) if (out.rfind(pre, 0) == 0) out = out.substr(std::strlen(pre));
    return out;
}

float energyOf(const std::string& d) {
    if (d == "whisper") return 0.15f;
    if (d == "soft" || d == "tender") return 0.3f;
    if (d == "breathless") return 0.55f;
    if (d == "playful") return 0.7f;
    if (d == "urgent") return 0.9f;
    return 0.5f;
}

int moodIndex(const std::string& m) { return m == "candle" ? 1 : (m == "day" ? 2 : 0); }

// staging clips by pose name (content story clips; the first one the characters have wins)
std::vector<std::string> poseClips(const std::string& pose) {
    if (pose.find('.') != std::string::npos) return {pose};
    if (pose == "stand") return {"stand.idle"};
    if (pose == "sit") return {"sit.bed", "stand.idle"};
    if (pose == "lie" || pose == "cuddle") return {"cuddle.idle"};
    if (pose == "kiss") return {"kiss.stand", "stand.embrace", "stand.idle"};
    if (pose == "dance") return {"dance.slow", "stand.idle"};
    if (pose == "embrace") return {"stand.embrace", "stand.idle"};
    return {pose + ".idle"};
}

// words of a line, split the way subtitles show them
std::vector<std::string> splitWords(const std::string& t) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : t) {
        if (c == ' ' || c == '\n') { if (!cur.empty()) out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}
}  // namespace

// ------------------------------------------------------------------ entering / leaving the story world
void Game::enterStoryWorld() {
    for (int w = 0; w < 2; ++w) {
        Character& c = w ? scene.marcus : scene.vivian;
        if (!c.loaded()) continue;
        actors_[w].bind(&c);
        c.poseHook = [this, w](Animator& a) { actors_[w].pose(a); };
        c.postHook = [this, w](Animator& a) { actors_[w].postPose(a); };
        c.touchGoals = TouchGoals{};
        c.placement = M4::identity();
    }
    storyStyle_[0] = storyStyle_[1] = "";
    storyOutfit_[0] = storyOutfit_[1] = "dressed";
    storyShow_[0] = storyShow_[1] = true;
    pose_.clear();
    kiss_ = kissTarget_ = near_ = nearTarget_ = 0.f;
    take_ = nullptr;
    speaker_ = -1; prevSpeaker_ = -2;
    voicePlayer_.stop();
    beatNode_.clear();
    shotCount_ = 0;
    letterbox_ = 1.f;
    stagePending_ = playPending_ = false;
    story_.autoMode = settings.storyAuto;
    if (!voices_.ready()) voices_.load(paths::join(paths::join(paths::assetRoot(), "story"), "voice"));
}

void Game::leaveStoryWorld() {
    voicePlayer_.stop();
    take_ = nullptr;
    beatNode_.clear();
    for (int w = 0; w < 2; ++w) {
        Character& c = w ? scene.marcus : scene.vivian;
        if (!c.loaded()) continue;
        actors_[w].releaseFace();
        actors_[w].reset();
        c.setOutfit("");
        c.postHook = nullptr;
        c.touchGoals = TouchGoals{};
        c.placement = M4::identity();
    }
    storyStyle_[0] = storyStyle_[1] = "";
    if (scene.location != "room") {
        scene.setLocation("room");
        camSafety_.boundsMin = scene.info.camMin; camSafety_.boundsMax = scene.info.camMax;
    }
    place_.clear(); pose_.clear(); storyAmb_.clear();
    setAmbience("");
    duckAmbience(0.f);
    renderer.settings.dofAmount = 0.f;
    letterbox_ = 0.f; black_ = 0.f; blackDir_ = 0;
    stagePending_ = playPending_ = false;
    kiss_ = kissTarget_ = near_ = nearTarget_ = 0.f;
    storyPlaying_ = false;
}

int Game::actIndex(const std::string& id) const {
    for (size_t i = 0; i < positions_.size(); ++i) if (positions_[i].id == id) return (int)i;
    return -1;
}

bool Game::storyCharVisible(int who) const {
    const Character& c = who ? scene.marcus : scene.vivian;
    if (!storyShow_[who] || !c.loaded()) return false;
    // dressed / partial needs the outfit asset; without one the character waits off stage
    return storyOutfit_[who] == "nude" || storyOutfit_[who].empty() || c.outfit() != nullptr;
}

// ------------------------------------------------------------------ places
std::string Game::resolvePlace(const StoryNode* n) const {
    std::string want = !n->place.empty() ? n->place : slugOf(n->location);
    if (want.empty()) return place_.empty() ? std::string("room") : place_;
    if (Scene::hasLocation(want)) return want;
    // the closest place that exists: a sibling of the same family first (a door scene in the living room, the
    // beach on the terrace), then the family's bedroom, else where the story already is
    struct F { const char* key; const char* to; };
    static const F fam[] = {{"cabin", "cabin_living"}, {"lodge", "cabin_living"}, {"snow", "cabin_living"},
                            {"spa", "spa_suite"}, {"massage", "spa_suite"},
                            {"beach", "villa_terrace"}, {"bay", "villa_terrace"}, {"sea", "villa_terrace"}, {"villa", "villa_terrace"},
                            {"apartment", "apartment_living"}, {"flat", "apartment_living"}, {"city", "apartment_living"},
                            {"ballroom", "hotel_ballroom"}, {"wedding", "hotel_ballroom"},
                            {"hotel", "hotel_room"}, {"ballroom", "hotel_room"}, {"wedding", "hotel_room"}, {"spa", "hotel_room"},
                            {"cabin", "cabin_bedroom"}, {"lodge", "cabin_bedroom"}, {"snow", "cabin_bedroom"},
                            {"villa", "villa_bedroom"}, {"beach", "villa_bedroom"}, {"bay", "villa_bedroom"}, {"sea", "villa_bedroom"},
                            {"apartment", "room"}, {"flat", "room"}, {"city", "room"}, {"home", "room"}, {"bedroom", "room"}};
    for (const F& f : fam) if (want.find(f.key) != std::string::npos && Scene::hasLocation(f.to)) return f.to;
    return place_.empty() ? std::string("room") : place_;
}

// ------------------------------------------------------------------ staging a node
void Game::stageStoryNode() {
    const StoryNode* n = story_.node();
    if (!n) return;
    const Story* s = story_.story();
    // progress: chapter reached, gallery, endings
    if (!n->chapter.empty()) {
        StoryProgress::add(storyProgress_.chapters[s->id], n->chapter);
        if (n->chapter != storyLastChapter_) { storyLastChapter_ = n->chapter; storyCardT_ = 0.f; storyCardKind_ = 0; }
    }
    if (!n->gallery.empty()) StoryProgress::add(storyProgress_.gallery[s->id], n->gallery);
    if (n->ending) StoryProgress::add(storyProgress_.endings[s->id], n->id);
    if (!n->location.empty() && n->location != storyLocation_) {
        storyLocation_ = n->location;
        if (storyCardKind_ != 0 || storyCardT_ > 0.5f) { storyCardT_ = 0.f; storyCardKind_ = 1; }
    }
    if (!harness) storyProgress_.save(storyProgressPath_);
    storyAutosave();
    // a new place or a new pose: cut through black, stage at the darkest moment
    std::string place = resolvePlace(n);
    std::string pose = !n->pose.empty() ? n->pose : (!n->act.empty() ? n->act : (pose_.empty() ? std::string("stand") : pose_));
    bool dip = place != scene.location || (!pose_.empty() && pose != pose_);
    if (dip && !harness && black_ < 0.99f) { stagePending_ = true; blackDir_ = 1; return; }
    storyStage(n);
}

void Game::setStoryPose(const std::string& pose, float actT) {
    bool changed = pose != pose_;
    pose_ = pose;
    if (changed) { kiss_ = kissTarget_ = near_ = nearTarget_ = 0.f; idlePhase_ = 0.f; }
    int ai = actIndex(pose);
    if (ai >= 0) {
        if (changed || posIndex_ != ai) { posIndex_ = -1; setPosition(ai, true); }
        depth_ = depthTarget_ = dragDepth_ = actT;
    }
}

void Game::storyStage(const StoryNode* n) {
    stagePending_ = false;
    const Story* s = story_.story();
    std::string place = resolvePlace(n);
    bool newPlace = place != scene.location || place_.empty();
    if (place != scene.location) {
        scene.setLocation(place);
        camSafety_.boundsMin = scene.info.camMin; camSafety_.boundsMax = scene.info.camMax;
    }
    if (newPlace) { place_ = place; director_.newScene(); shotCount_ = 0; }
    // a new pose restages them (standing face to face -> sitting side by side): the camera picks its side again
    std::string pose = !n->pose.empty() ? n->pose : (!n->act.empty() ? n->act : (pose_.empty() ? std::string("stand") : pose_));
    if (!newPlace && pose != pose_) director_.newScene();
    // light and sound of the scene
    if (!n->mood.empty()) storyMood_ = moodIndex(n->mood);
    else if (newPlace) storyMood_ = moodIndex(!scene.info.mood.empty() ? scene.info.mood : s->mood);
    if (!n->ambience.empty()) storyAmb_ = n->ambience;
    else if (newPlace) {
        std::string w = !n->place.empty() ? n->place : slugOf(n->location);
        storyAmb_ = w.find("ballroom") != std::string::npos || w.find("party") != std::string::npos || w.find("bar") != std::string::npos ? "party"
                  : (w.find("villa") != std::string::npos || w.find("beach") != std::string::npos || w.find("terrace") != std::string::npos ? "sea"
                  : (w.find("cabin") != std::string::npos ? "rain+fire" : (w.find("spa") != std::string::npos ? "spa"
                  : (w.find("apartment") != std::string::npos || w.find("city") != std::string::npos ? "city" : ""))));
    }
    setAmbience(storyAmb_);
    // who is on stage, in what
    if (n->showSet) { storyShow_[0] = storyShow_[1] = false; for (auto& w : n->show) { if (w == "vivian") storyShow_[0] = true; if (w == "marcus") storyShow_[1] = true; } }
    for (int w = 0; w < 2; ++w) {
        const char* who = w ? "marcus" : "vivian";
        Character& c = w ? scene.marcus : scene.vivian;
        auto wi = n->wardrobe.find(who);
        auto si = s->wardrobe.find(who);
        std::string style = wi != n->wardrobe.end() ? wi->second : (si != s->wardrobe.end() ? si->second : std::string());
        if (style != storyStyle_[w] && c.loaded()) { storyStyle_[w] = style; c.setOutfit(style); }
        auto oi = n->outfit.find(who);
        if (oi != n->outfit.end()) storyOutfit_[w] = oi->second;
        if (Outfit* o = c.outfit()) o->setDress(dressFromName(storyOutfit_[w]), 0.f);
    }
    // the pose
    setStoryPose(pose, n->actT);
    // the opening shot: an establishing view of a new place, else what the node asks for
    if (newPlace && actIndex(pose_) < 0) director_.shot("establish", -1, true, 6.f);
    else if (!n->cam.empty()) { StoryLine l; l.cam = n->cam; storyShot(&l, true); }
    if (black_ > 0.f) blackDir_ = -1;
}

// ------------------------------------------------------------------ a line starts
void Game::storyLineStarted() {
    const StoryLine* l = story_.line();
    const StoryNode* n = story_.node();
    voicePlayer_.stop();
    take_ = nullptr;
    voiceT_ = 0.f;
    lastEnv_ = 0.f;
    story_.textSpeed = 42.f;
    story_.voicedHold = -1.f;
    if (!l || !n) { speaker_ = -1; return; }
    int spk = l->who == "vivian" ? 0 : (l->who == "marcus" ? 1 : -1);
    // the voice: Kokoro take from the manifest (silent when not voiced yet); the typewriter ends with it
    if (l->voice != "none") take_ = voices_.take(story_.story()->id, n->id, story_.lineIndex(), &takeKey_, &l->text);
    if (take_) {
        if (!harness) voicePlayer_.play(voices_.file(takeKey_), settings.voice);
        if (harness) AG_INFO("AG: audio voice line %s %s %.2fs", l->who.c_str(), takeKey_.c_str(), (double)take_->dur);
        int chars = 0;
        for (unsigned char ch : l->text) if ((ch & 0xC0) != 0x80) ++chars;
        story_.textSpeed = std::max(14.f, (float)chars / std::max(0.6f, take_->dur - 0.15f));
        story_.voicedHold = 0.65f;
    }
    // vocal samples and sound effects riding on the line (legacy "voice": sample names too)
    if (!l->vox.empty()) voice(l->vox, 0.8f, 1.f);
    else if (!l->voice.empty() && l->voice != "none" && audio.has("vox/" + l->voice)) voice(l->voice, 0.8f, 1.f);
    if (!l->sfx.empty()) sfx(l->sfx, 0.6f);
    // acting: the speaker plays the line, the other one listens (and reacts)
    float energy = energyOf(l->delivery);
    for (int w = 0; w < 2; ++w) {
        ActingCue cue;
        cue.speaking = w == spk;
        cue.energy = energy;
        cue.look = l->look;
        if (cue.speaking) {
            cue.head = l->head;
            cue.gesture = l->gesture;
            // talk with the hands now and then when the line doesn't say: longer, livelier lines
            if (cue.gesture.empty() && l->text.size() > 70 && energy >= 0.45f && pose_ != "sit")
                cue.gesture = (story_.lineIndex() + (int)l->text.size()) % 3 == 0 ? "open" : "explain";
        } else {
            cue.react = l->react;
            cue.look.clear();
            if (spk < 0 && !l->head.empty()) cue.head = l->head;
        }
        actors_[w].cue(cue);
    }
    // expressions: the speaker's ("smile"), either's ("her.smile" / "him.smile")
    for (auto& e : l->expr) {
        int w = spk;
        std::string k = e.first;
        if (k.rfind("her.", 0) == 0) { w = 0; k = k.substr(4); }
        else if (k.rfind("him.", 0) == 0) { w = 1; k = k.substr(4); }
        if (k == "flush") { (w == 1 ? scene.marcus : scene.vivian).flush = clamp01(e.second); continue; }
        if (w < 0) w = 0;
        actors_[w].expr[k] = e.second;
    }
    prevSpeaker_ = speaker_;
    speaker_ = spk;
    if (!l->action.empty()) storyAction(l->action);
    storyShot(l, false);
}

// ------------------------------------------------------------------ staged actions
void Game::storyAction(const std::string& action) {
    std::vector<std::string> parts;
    size_t i = 0;
    while (i <= action.size()) {
        size_t j = action.find(':', i);
        if (j == std::string::npos) j = action.size();
        parts.push_back(action.substr(i, j - i));
        i = j + 1;
    }
    const std::string& a = parts[0];
    if (a == "kiss") { kissTarget_ = 1.f; nearTarget_ = 1.f; if (audio.ready()) audio.play("fx/kiss", 0.35f * settings.effects); }
    else if (a == "embrace" || a == "step_in") { nearTarget_ = 1.f; kissTarget_ = 0.f; }
    else if (a == "part") { kissTarget_ = 0.f; nearTarget_ = 0.35f; }
    else if (a == "apart") { kissTarget_ = nearTarget_ = 0.f; }
    else if (a == "undress" || a == "dress") {
        bool off = a == "undress";
        std::string who = parts.size() > 1 ? parts[1] : "both";
        std::string layer = parts.size() > 2 ? parts[2] : "";
        for (int w = 0; w < 2; ++w) {
            if (who != "both" && who != (w ? "marcus" : "vivian")) continue;
            Character& c = w ? scene.marcus : scene.vivian;
            Outfit* o = c.outfit();
            if (!o) { storyOutfit_[w] = off ? "nude" : "dressed"; continue; }
            if (layer == "all") o->setDress(off ? Dress::Nude : Dress::Dressed, 2.6f);
            else {
                int l = layer == "under" ? 1 : (layer == "outer" ? 0 : (off ? (o->layerShown(0) ? 0 : 1) : (o->layerShown(1) ? 0 : 1)));
                o->setLayer(l, !off, 1.8f);
                if (off && l == 1) o->setLayer(0, false, 1.8f);
                if (!off && l == 0) o->setLayer(1, true, 1.8f);
            }
            Dress d = o->dress();
            storyOutfit_[w] = d == Dress::Nude ? "nude" : (d == Dress::Partial ? "partial" : "dressed");
            if (audio.ready()) audio.playRandom("fx/rustle", 0.5f * settings.effects, 0.08f);
        }
    }
    else if (a == "to_bed" || a == "sit" || a == "stand" || a == "lie") {
        // a cut through black to the next pose (the node's act when going to bed)
        const StoryNode* n = story_.node();
        std::string pose = a == "to_bed" ? (n && !n->act.empty() ? n->act : std::string("lie")) : a;
        if (pose != pose_) { posePending_ = pose; blackDir_ = 1; }
    }
}

// ------------------------------------------------------------------ shots
void Game::storyShot(const StoryLine* l, bool newNode) {
    std::string cam = l ? l->cam : std::string();
    if (actIndex(pose_) >= 0) {
        // an act's pose: the play cameras (side, front, top, his view, her face)
        if (cam.empty()) { if (newNode || shotCount_ == 0) { storyCamera("side", 1.2f); ++shotCount_; } return; }
        storyCamera(cam == "close" || cam == "xclose" || cam == "ots" || cam == "kiss" ? "face" : (cam == "two" || cam == "wide" || cam == "establish" || cam == "medium" ? "side" : cam), 1.2f);
        ++shotCount_;
        return;
    }
    bool both = storyCharVisible(0) && storyCharVisible(1);
    int spk = speaker_;
    if (cam.empty()) {
        const StoryNode* n = story_.node();
        unsigned h = 2166136261u;
        for (char c : (n ? n->id : std::string())) h = (h ^ (unsigned char)c) * 16777619u;
        h = (h ^ (unsigned)story_.lineIndex()) * 16777619u;
        if (kiss_ > 0.5f || kissTarget_ > 0.5f) cam = "kiss";
        else if (spk < 0) {
            // narration: hold an establishing view a moment, else frame the two of them
            if (director_.kind() == "establish" && director_.held() < 5.f) cam = "establish";
            else cam = both ? ((h % 4 == 0) ? "wide" : "two") : "medium";
        } else {
            const StoryLine& L = *l;
            bool emotional = L.delivery == "soft" || L.delivery == "whisper" || L.delivery == "tender" || L.delivery == "breathless" || !L.expr.empty();
            bool sameSpeaker = spk == prevSpeaker_ && director_.subject() == spk;
            if (sameSpeaker && director_.held() < 8.f) cam = director_.kind();   // hold the shot, the push-in goes on
            else {
                unsigned pick = (h >> 3) % 5u;
                cam = emotional ? (pick < 3 ? "close" : "ots") : (pick < 2 ? "ots" : (pick < 4 ? "close" : "two"));
            }
            if (!both && (cam == "ots" || cam == "two")) cam = "close";
        }
    }
    int subject = cam == "two" || cam == "wide" || cam == "establish" || cam == "kiss" ? -1 : (spk >= 0 ? spk : (storyCharVisible(0) ? 0 : 1));
    bool cut = cam != director_.kind() || subject != director_.subject();
    float hold = take_ ? take_->dur : 4.f;
    director_.shot(cam, subject, cut, hold);
    ++shotCount_;
}

void Game::directStory(float dt) {
    if (actIndex(pose_) >= 0) { renderer.settings.dofAmount = 0.f; return; }   // the play cameras
    ActorView her, him;
    auto view = [&](Character& c, bool shown, ActorView& out) {
        out.shown = shown && c.loaded();
        if (!out.shown) return;
        V3 up;
        c.headAxes(out.head, out.face, up);
        out.hips = c.bonePos("下半身");
        V3 sr = c.bonePos("腕.R") - c.bonePos("腕.L");
        sr.y = 0.f;
        if (length(sr) > 1e-3f) out.body = normalize(cross(V3{0, 1, 0}, normalize(sr)));
    };
    view(scene.vivian, scene.showVivian, her);
    view(scene.marcus, scene.showMarcus, him);
    ShotFrame f = director_.frame(dt, her, him, V3{0.f, 1.2f, 1.4f});
    camera.setDirect(f.eye, f.target, f.fov, director_.takeCut());
    renderer.settings.dofAmount = settings.depthOfField ? f.aperture * 0.75f : 0.f;
    renderer.settings.dofFocus = f.focus;
}

// ------------------------------------------------------------------ the pose, the kiss, the hands
void Game::applyStoryPose(float dt) {
    Character& v = scene.vivian;
    Character& m = scene.marcus;
    int ai = actIndex(pose_);
    if (ai >= 0) {
        const PositionInfo& p = positions_[(size_t)ai];
        idlePhase_ = std::fmod(idlePhase_ + dt / 9.f, 1.f);
        for (Character* c : {&v, &m}) if (c->loaded()) c->clearLayers();
        if (p.solo && v.loaded()) v.setLayer(0, "solo.idle", idlePhase_, 1.f);
        else for (Character* c : {&v, &m}) if (c->loaded()) c->setLayer(0, p.baseClip, p.depthDriven ? depth_ : idlePhase_, 1.f);
        for (Character* c : {&v, &m}) { c->placement = M4::identity(); c->touchGoals = TouchGoals{}; }
        return;
    }
    std::string clip;
    for (const std::string& c : poseClips(pose_)) if (v.hasClip(c) || m.hasClip(c)) { clip = c; break; }
    if (clip.empty() && (pose_ == "lie" || pose_ == "cuddle")) {
        // no cuddle clip yet: lie together spooned, still
        int sp = actIndex("spoon");
        if (sp >= 0) { pose_ = "spoon"; posIndex_ = -1; setPosition(sp, true); depth_ = depthTarget_ = dragDepth_ = 0.f; return; }
    }
    if (clip.empty()) clip = "stand.idle";
    float dur = std::max(0.5f, v.loaded() && v.hasClip(clip) ? v.clipDuration(clip) : 2.f);
    idlePhase_ = std::fmod(idlePhase_ + dt / (dur * 2.f), 1.f);   // the 2 s loops at half speed: unhurried
    for (Character* c : {&v, &m}) {
        if (!c->loaded()) continue;
        c->clearLayers();
        if (c->hasClip(clip)) c->setLayer(0, clip, idlePhase_, 1.f);
    }
    // the kiss / embrace: step in toward each other (seated: slide along the bed until the hips meet), lean in, the
    // lips meet at a point between their mouths, hands on each other
    kiss_ = approachF(kiss_, kissTarget_, dt / 1.1f);
    near_ = approachF(near_, std::max(nearTarget_, kissTarget_), dt / 1.3f);
    float k = smoothstep(0.f, 1.f, kiss_), nr = smoothstep(0.f, 1.f, near_);
    bool sit = pose_ == "sit";
    bool both = v.loaded() && m.loaded();
    V3 axis = flatv(m.bonePos("センター") - v.bonePos("センター"));
    // the gaps without any step (last frame's bones minus last frame's step): standing, the necks end where a
    // natural bend brings the lips together (the embrace leaves room for her hands on his chest); seated, the hips
    // stop just short of each other and the turn + lean + neck do the rest
    float step = 0.f;
    if (both && nr > 0.001f) {
        float neckGap = dot(m.bonePos("首") - v.bonePos("首"), axis) + 2.f * step_;
        float hipGap = dot(m.bonePos("下半身") - v.bonePos("下半身"), axis) + 2.f * step_;
        float want = sit ? 0.5f * (hipGap - kSitHips) : 0.5f * (neckGap - lerpf(kEmbraceNeck, kKissNeck, k));
        step = std::max(0.f, want) * nr;
    }
    step_ = step;
    v.placement = m4translate(axis * step);
    m.placement = m4translate(axis * -step);
    V3 mv = actors_[0].mouthRest, mm = actors_[1].mouthRest;   // last frame, before the kiss IK
    bool meet = both && k > 0.001f && length(mv) > 0.1f && length(mm) > 0.1f;
    V3 u = flatv(mm - mv), meetAt = (mv + mm) * 0.5f;
    meetAt.y = mv.y + 0.45f * (mm.y - mv.y);                    // he bends down a little more than she reaches up
    for (int w = 0; w < 2; ++w) {
        actors_[w].lean = 0.35f * nr + 0.65f * k;
        actors_[w].leanMax = sit ? 0.2f : 0.12f;
        actors_[w].turn = sit ? std::max(nr, k) : 0.f;
        actors_[w].eyesClosed = k > 0.6f;
        actors_[w].kissW = meet ? k : 0.f;
        actors_[w].kissRoll = 0.38f;
        actors_[w].kissTarget = meetAt + u * (w ? 0.001f : -0.001f);
        float& kissExpr = actors_[w].expr["kiss"];
        kissExpr = std::max(kissExpr * (kissTarget_ > 0.5f ? 1.f : 0.f), 0.85f * k);
    }
    // hands: hers flat on his chest, fingers up and a little out toward his shoulder; his at her waist, fingers
    // around toward her back. Each hand on the partner's side facing it (her left on his right: arms never cross),
    // on the real surface (anchors found on the bind mesh), over the clothes' thickness when dressed; elbows low
    TouchGoals tv, tm;
    float hw = smoothstep(0.5f, 1.f, std::max(near_, kiss_));   // hands come with the embrace, not with a half step
    if (hw > 0.01f && !sit && both) {
        V3 hc = m.bonePos("上半身2"), hr = flatv(m.bonePos("腕.R") - m.bonePos("腕.L"));
        V3 hf = flatv(cross(V3{0, 1, 0}, hr));
        V3 vc = v.bonePos("下半身"), vr = flatv(v.bonePos("腕.R") - v.bonePos("腕.L"));
        V3 vf = flatv(cross(V3{0, 1, 0}, vr));
        for (int s = 0; s < 2; ++s) {
            float sg = s ? 1.f : -1.f;   // the hand's own side: -1 left, +1 right
            V3 zp, zn;
            HandGoal g;
            g.active = true; g.world = true; g.flat = true; g.dent = false; g.weight = hw; g.curl = 0.12f; g.press = 0.f;
            g.palmLift = 0.017f;
            Zone hz = s ? Zone::ChestL : Zone::ChestR, vz = s ? Zone::WaistL : Zone::WaistR;
            if (m.touch.zoneWorld(m.anim, hz, zp, zn)) { g.pos = zp + zn * clothOver(1, hz); g.normal = zn; }
            else { g.pos = hc + hf * 0.12f + V3{0.f, 0.1f, 0.f} - hr * (0.085f * sg); g.normal = hf; }
            g.along = normalize(V3{0.f, 1.f, 0.f} + flatv(g.pos - hc) * 0.35f);
            g.elbowHint = normalize(V3{0.f, -1.f, 0.f} + vr * (0.5f * sg) - vf * 0.25f);
            (s ? tv.right : tv.left) = g;
            HandGoal h = g;
            h.palmLift = 0.02f;
            h.curl = 0.2f;
            if (v.touch.zoneWorld(v.anim, vz, zp, zn)) { h.pos = zp + zn * clothOver(0, vz); h.normal = zn; }
            else { h.pos = vc + V3{0.f, 0.06f, 0.f} - vr * (0.15f * sg) - vf * 0.02f; h.normal = vr * -sg; }
            h.along = normalize(vf * -1.f + V3{0.f, -0.2f, 0.f});
            h.elbowHint = normalize(V3{0.f, -1.f, 0.f} + hr * (0.6f * sg) - hf * 0.3f);
            (s ? tm.right : tm.left) = h;
        }
    }
    // seated, kissing: the near hands find each other — hers flat on his chest, his on her waist, each on the side
    // facing the partner (sitting side by side turned in, the far hands stay where the clip rests them)
    if (sit && both && kiss_ > 0.05f) {
        float sw = smoothstep(0.15f, 0.8f, kiss_);
        V3 vr = flatv(v.bonePos("腕.R") - v.bonePos("腕.L")), hr = flatv(m.bonePos("腕.R") - m.bonePos("腕.L"));
        V3 vf = flatv(cross(V3{0, 1, 0}, vr)), hf = flatv(cross(V3{0, 1, 0}, hr));
        V3 toHim = flatv(m.bonePos("センター") - v.bonePos("センター"));
        bool heOnHerRight = dot(toHim, vr) > 0.f, sheOnHisRight = dot(toHim, hr) < 0.f;
        V3 zp, zn;
        Zone hz = sheOnHisRight ? Zone::ChestR : Zone::ChestL, vz = heOnHerRight ? Zone::WaistR : Zone::WaistL;
        if (m.touch.zoneWorld(m.anim, hz, zp, zn)) {
            HandGoal g;
            g.active = true; g.world = true; g.flat = true; g.dent = false; g.weight = sw; g.curl = 0.12f; g.press = 0.f;
            g.palmLift = 0.017f;
            g.pos = zp + zn * clothOver(1, hz);
            g.normal = zn;
            g.along = normalize(V3{0.f, 1.f, 0.f} + flatv(zp - m.bonePos("上半身2")) * 0.35f);
            g.elbowHint = normalize(V3{0.f, -1.f, 0.f} + vf * 0.45f);        // in front of her, low: clear of his arm
            (heOnHerRight ? tv.right : tv.left) = g;
        }
        if (v.touch.zoneWorld(v.anim, vz, zp, zn)) {
            HandGoal h;
            h.active = true; h.world = true; h.flat = true; h.dent = false; h.weight = sw; h.curl = 0.2f; h.press = 0.f;
            h.palmLift = 0.02f;
            h.pos = zp + zn * clothOver(0, vz);
            h.normal = zn;
            h.along = normalize(vf * -1.f + V3{0.f, -0.2f, 0.f});           // fingers around toward her back
            h.elbowHint = normalize(V3{0.f, -1.f, 0.f} - hf * 0.35f);       // low and back: under her forearm
            (sheOnHisRight ? tm.right : tm.left) = h;
        }
    }
    // "me": the right hand laid flat on one's own upper chest, a little to the heart side, fingers toward the left
    // shoulder, the elbow down and out
    for (int w = 0; w < 2; ++w) {
        Character& c = w ? m : v;
        TouchGoals& tg = w ? tm : tv;
        float gw = actors_[w].gestureWeight("chest");
        V3 zp, zn;
        if (gw < 0.01f || tg.right.active || !c.loaded() || !c.touch.zoneWorld(c.anim, Zone::Sternum, zp, zn)) continue;
        V3 cr = flatv(c.bonePos("腕.R") - c.bonePos("腕.L"));
        HandGoal g;
        g.active = true; g.world = true; g.flat = true; g.dent = false; g.weight = gw; g.curl = 0.15f; g.press = 0.f;
        g.palmLift = w ? 0.02f : 0.017f;
        g.pos = zp - cr * 0.025f + zn * clothOver(w, Zone::Sternum);
        g.normal = zn;
        g.along = normalize(V3{0.f, 1.f, 0.f} - cr * 0.45f);
        g.elbowHint = normalize(V3{0.f, -1.f, 0.f} + cr * 0.6f + flatv(cross(V3{0, 1, 0}, cr)) * 0.2f);
        tg.right = g;
    }
    v.touchGoals = tv;
    m.touchGoals = tm;
}

// how far the worn clothes stand off the skin at a body zone: the topmost shown layer's bind shape measured along the
// zone's normal (garments follow the skin, so the rest-shape offset holds in a pose), plus the clearance the renderer
// gives the cloth (u_fitInflate) and a little air. Bare skin: a few mm for the palm's own surface.
float Game::clothOver(int who, Zone z) {
    Character& c = who ? scene.marcus : scene.vivian;
    const Outfit* o = c.outfit();
    if (!o || !o->loaded() || o->visibility(1) < 0.5f) return 0.004f;
    int layers = (o->visibility(0) > 0.5f ? 2 : 0) | 1;
    ClothAt& ca = clothAt_[who];
    if (ca.outfit != o || ca.layers != layers) {
        ca.outfit = o;
        ca.layers = layers;
        for (int zi = 0; zi < (int)Zone::Count; ++zi) {
            V3 bp, bn;
            ca.off[zi] = -1.f;
            if (!c.touch.zoneBind((Zone)zi, bp, bn)) continue;
            for (float r : {0.015f, 0.03f}) {   // the cloth over the zone: vertices around its normal line
                float best = -1.f;
                for (size_t mi = 0; mi < o->model.meshes.size(); ++mi) {
                    int layer = mi < o->meshLayer.size() ? o->meshLayer[mi] : 0;
                    if (layer == 0 && !(layers & 2)) continue;
                    for (const Primitive& p : o->model.meshes[mi].prims)
                        for (size_t k = 0; k + 2 < p.pos.size(); k += 3) {
                            V3 d = V3{p.pos[k], p.pos[k + 1], p.pos[k + 2]} - bp;
                            float t = dot(d, bn);
                            if (t < -0.005f || t > 0.06f) continue;
                            V3 lat = d - bn * t;
                            if (dot(lat, lat) <= r * r) best = std::max(best, t);
                        }
                }
                if (best >= 0.f) { ca.off[zi] = best; break; }
            }
        }
    }
    float off = ca.off[(int)z];
    return off >= 0.f ? off + ((layers & 2) ? 0.003f : 0.0015f) + 0.002f : 0.012f;
}

void Game::presentStory(float dt) {
    Character& v = scene.vivian;
    Character& m = scene.marcus;
    scene.showVivian = v.loaded() && storyCharVisible(0);
    scene.showMarcus = m.loaded() && storyCharVisible(1);
    applyStoryPose(dt);
    bool inAct = actIndex(pose_) >= 0;
    for (int w = 0; w < 2; ++w) {
        Character& c = w ? m : v;
        Character& o = w ? v : m;
        if (!c.loaded()) continue;
        actors_[w].acting = inAct ? 0.f : 1.f;
        actors_[w].sit = pose_ == "sit" ? 1.f : 0.f;
        bool partner = w ? scene.showVivian : scene.showMarcus;
        V3 oh{0, 1.5f, 0}, of{0, 0, 1}, ou{0, 1, 0};
        if (partner) o.headAxes(oh, of, ou);
        if (kiss_ > 0.3f) oh = oh + of * 0.04f + V3{0.f, -0.02f, 0.f};   // toward the partner's mouth
        actors_[w].update(dt, oh, partner, cam_.eye);
        c.proc.breathRate = 0.22f + 0.1f * kiss_;
        c.proc.breathAmp = 1.f + 0.6f * kiss_;
    }
    renderer.settings.flash = 0.f;
    scene.update(dt);
    if (inAct) applyContactTracks();
    if (black_ < 0.999f) directStory(dt);
}

// ------------------------------------------------------------------ update
void Game::updateStory(float dt) {
    if (!story_.active()) return;
    // cuts through black: stage the new place / pose / act at the darkest moment
    if (blackDir_ != 0) {
        black_ = clamp01(black_ + (float)blackDir_ * dt / 0.42f);
        if (blackDir_ > 0 && black_ >= 1.f) {
            if (stagePending_ && story_.node()) storyStage(story_.node());
            if (!posePending_.empty()) { setStoryPose(posePending_, story_.node() ? story_.node()->actT : 0.3f); posePending_.clear(); director_.newScene(); }
            if (playPending_) { playPending_ = false; storyStartPlay(); }
            blackDir_ = -1;
        } else if (blackDir_ < 0 && black_ <= 0.f) blackDir_ = 0;
    }
    if (story_.nodeChanged) { story_.nodeChanged = false; stageStoryNode(); }
    const StoryNode* n = story_.node();
    storyCardT_ += dt;
    bool overlay = storyOverlay_ != 0;
    voicePlayer_.setVolume(settings.voice);
    voicePlayer_.update(dt, overlay);
    letterbox_ = approachF(letterbox_, storyPlaying_ ? 0.f : 1.f, dt / 0.7f);
    if (storyPlaying_) {
        // the night runs until the node's goal, with the story's voice over it, then the story continues
        updatePlay(dt);
        presentPlay(dt);
        storyBeats(dt);
        const std::string& g = n ? n->playGoal : std::string();
        bool done = night_.finished;
        if (g == "her") done = done || night_.herClimaxes > 0;
        else if (g == "him") done = done || night_.finished;
        else if (g.rfind("time:", 0) == 0) done = done || night_.elapsed >= (float)std::atof(g.c_str() + 5);
        if (done && (!night_.finished || afterT_ > 2.5f)) {
            storyPlaying_ = false;
            story_.vars["together"] = night_.together ? 1.f : 0.f;
            story_.vars["herClimaxes"] = (float)night_.herClimaxes;
            story_.vars["connection"] = night_.connection;
            voicePlayer_.stop(); beatNode_.clear();
            for (int w = 0; w < 2; ++w) {
                Character& c = w ? scene.marcus : scene.vivian;
                if (!c.loaded()) continue;
                c.poseHook = [this, w](Animator& a) { actors_[w].pose(a); };
                c.postHook = [this, w](Animator& a) { actors_[w].postPose(a); };
            }
            if (n && !n->playNext.empty()) story_.jump(n->playNext);
        }
        return;
    }
    bool holding = stagePending_ || black_ > 0.6f;
    bool open = storyOverlay_ == 0 && !switching_ && !holding;
    if (open) {
        if (IsKeyPressed(KEY_A)) story_.autoMode = !story_.autoMode;
        story_.skipMode = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || storySkipToggle_;
        if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_H)) storyOverlay_ = 1;
        if (IsKeyPressed(KEY_ESCAPE)) storyOverlay_ = 5;
        std::vector<int> vis = story_.visibleChoices();
        if (story_.atChoices()) {
            story_.skipMode = false;
            for (size_t i = 0; i < vis.size() && i < 4; ++i) if (IsKeyPressed(KEY_ONE + (int)i)) { story_.choose(vis[i]); if (ui.onSound) ui.onSound(1); }
        } else if (story_.atPlay()) {
            if (!playPending_) {
                // into the act: through black when the bodies have to move (standing -> the bed)
                playPending_ = true;
                if (n && n->act != pose_ && !harness) blackDir_ = 1;
                else { playPending_ = false; storyStartPlay(); }
            }
        } else if (!story_.atEnding()) {
            bool click = ui.in.clicked && !CheckCollisionPointRec(ui.in.mouse, storyControlsRect_);
            if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || click || ui.in.wheel < 0.f) story_.advance();
        }
        story_.update(dt);
    }
    if (story_.lineChanged && !holding) { story_.lineChanged = false; storyLineStarted(); }
    // the voice clock + lip sync (the take's loudness, its words, the letters of each word)
    if (!overlay && !holding) voiceT_ += dt;
    const StoryLine* l = story_.line();
    float vt = storyVoiceTime();
    if (take_ && speaker_ >= 0 && l) {
        float env = voices_.envelope(*take_, vt);
        Viseme vz = voices_.viseme(*take_, l->text, vt);
        bool beat = env > 0.5f && lastEnv_ < 0.3f;
        lastEnv_ = env;
        if (vt < take_->dur) actors_[speaker_].speech(env, vz, beat);
        else actors_[speaker_].silence();
    } else if (speaker_ >= 0 && l && !story_.lineDone()) {
        // not voiced (yet): the lips move with the text
        Viseme vz;
        vz.open = 0.35f + 0.25f * std::sin(time_ * 17.f);
        vz.round = 0.2f * std::max(0.f, std::sin(time_ * 7.f));
        actors_[speaker_].speech(0.55f, vz, false);
    } else {
        for (auto& a : actors_) a.silence();
    }
    duckAmbience(take_ && vt < take_->dur ? 1.f : 0.f);
    presentStory(dt);
}

// ------------------------------------------------------------------ into play, and the story's voice over it
void Game::storyStartPlay() {
    const StoryNode* n = story_.node();
    storyPlaying_ = true;
    voicePlayer_.stop();
    take_ = nullptr;
    speaker_ = -1;
    for (int w = 0; w < 2; ++w) {
        Character& c = w ? scene.marcus : scene.vivian;
        if (!c.loaded()) continue;
        actors_[w].silence();
        actors_[w].releaseFace();
        c.postHook = nullptr;
        c.touchGoals = TouchGoals{};
        c.placement = M4::identity();
        if (Outfit* o = c.outfit()) o->setDress(Dress::Nude, 0.f);
        storyOutfit_[w] = "nude";
    }
    scene.vivian.poseHook = [this](Animator& a) { archBody(a); };
    scene.marcus.poseHook = nullptr;
    kiss_ = kissTarget_ = near_ = nearTarget_ = 0.f;
    renderer.settings.dofAmount = 0.f;
    night_.begin((unsigned)(GetTime() * 1000.0) ^ 0x9e3779b9u);
    night_.connection = clamp01(0.45f + 0.06f * (story_.vars.count("trust") ? story_.vars["trust"] : 0.f));
    int ai = n ? actIndex(n->act) : -1;
    if (ai >= 0) { posIndex_ = -1; setPosition(ai, true); pose_ = n->act; }
    depth_ = depthTarget_ = dragDepth_ = 0.3f;
    camUser_ = false; afterT_ = 0.f; slowmo_ = 1.f; together_ = false;
    hudT_ = 0.f; hintsT_ = 0.f;
    cameraForPosition(0.f);
    toasts_.push_back({0.f, "Your turn"});
    beatNode_.clear(); beatLine_ = -1; beatCool_ = 8.f; beatEdge_ = beatPeak_ = false;
    startBeat("start");
}

void Game::startBeat(const std::string& trigger) {
    const StoryNode* n = story_.node();
    if (!n) return;
    auto it = n->beats.find(trigger);
    if (it == n->beats.end()) return;
    const StoryNode* b = story_.story()->node(it->second);
    if (!b || b->lines.empty()) return;
    beatNode_ = b->id;
    beatLine_ = -1;
    beatT_ = 99.f;
    if (harness) AG_INFO("AG: story beat %s -> %s", trigger.c_str(), b->id.c_str());
}

void Game::storyBeats(float dt) {
    beatCool_ = std::max(0.f, beatCool_ - dt);
    for (const NightEvent& e : night_.events) {
        if (e.type == NightEvent::DesireNew && beatCool_ <= 0.f && beatNode_.empty()) { startBeat("desire"); beatCool_ = 24.f; }
        if (e.type == NightEvent::HerClimax && !beatPeak_) { beatPeak_ = true; startBeat("her_peak"); }
    }
    if (!beatEdge_ && night_.him > 0.86f) { beatEdge_ = true; if (beatNode_.empty()) startBeat("edge"); }
    beatMouth_ = 0.f;
    if (beatNode_.empty()) return;
    const StoryNode* b = story_.story()->node(beatNode_);
    if (!b) { beatNode_.clear(); return; }
    beatT_ += dt;
    float len = beatTake_ ? beatTake_->dur + 0.6f : 1.8f + 0.045f * (float)beatText_.size();
    if (beatLine_ < 0 || beatT_ > len) {
        ++beatLine_;
        if (beatLine_ >= (int)b->lines.size()) { beatNode_.clear(); beatTake_ = nullptr; return; }
        const StoryLine& L = b->lines[(size_t)beatLine_];
        std::string key;
        beatTake_ = L.voice == "none" ? nullptr : voices_.take(story_.story()->id, b->id, beatLine_, &key, &L.text);
        if (beatTake_ && !harness) voicePlayer_.play(voices_.file(key), settings.voice);
        if (!L.vox.empty()) voice(L.vox, 0.8f, 1.f);
        beatWho_ = L.who;
        beatText_ = L.text;
        beatT_ = 0.f;
    }
    if (beatTake_ && beatWho_ == "vivian" && beatT_ < beatTake_->dur) beatMouth_ = voices_.envelope(*beatTake_, beatT_);
    duckAmbience(beatTake_ && beatT_ < beatTake_->dur ? 1.f : 0.f);
}

// ------------------------------------------------------------------ drawing
void Game::drawSubtitle(const std::string& who, const std::string& text, int shown, float a, float cy) {
    ui::Ui& U = ui;
    bool narr = who.empty() || who == "narrator";
    ui::Face f = narr ? ui::Face::SerifItalic : ui::Face::Serif;
    float px = U.u(31.f), maxW = std::min(U.W - U.u(220.f), U.u(1340.f));
    std::vector<std::string> words = splitWords(text);
    float spaceW = U.measure(f, px, " ").x;
    std::vector<std::pair<size_t, size_t>> lines;   // [first, last) word per line
    std::vector<float> widths;
    size_t start = 0;
    float w = 0.f;
    for (size_t i = 0; i < words.size(); ++i) {
        float ww = U.measure(f, px, words[i]).x;
        if (i > start && w + spaceW + ww > maxW) { lines.push_back({start, i}); widths.push_back(w); start = i; w = ww; }
        else w += (i > start ? spaceW : 0.f) + ww;
    }
    if (start < words.size()) { lines.push_back({start, words.size()}); widths.push_back(w); }
    float lineH = px * 1.3f, nameH = narr ? 0.f : U.u(30.f);
    float y = cy - (nameH + lineH * (float)lines.size()) * 0.5f;
    float a0 = U.alpha;
    U.alpha = a0 * a;
    if (!narr) {
        std::string name = who == "vivian" ? "VIVIAN" : (who == "marcus" ? "MARCUS" : who);
        for (auto& ch : name) ch = (char)std::toupper((unsigned char)ch);
        Color c = who == "vivian" ? U.th.rose : (who == "marcus" ? U.th.gold : U.th.accent);
        U.text(ui::Face::SansBold, U.u(14.f), name, U.W * 0.5f + U.u(1.5f), y + U.u(2.f), Color{0, 0, 0, 150}, ui::Center, 0.3f);
        U.text(ui::Face::SansBold, U.u(14.f), name, U.W * 0.5f, y, c, ui::Center, 0.3f);
        y += nameH;
    }
    Color ink = narr ? Color{236, 226, 218, 255} : U.th.ink;
    for (size_t li = 0; li < lines.size(); ++li) {
        float x = U.W * 0.5f - widths[li] * 0.5f;
        for (size_t i = lines[li].first; i < lines[li].second; ++i) {
            float ww = U.measure(f, px, words[i]).x;
            if ((int)i < shown) {
                U.text(f, px, words[i], x + U.u(1.6f), y + U.u(2.2f), Color{0, 0, 0, 170}, ui::Left);
                U.text(f, px, words[i], x, y, ink, ui::Left);
            }
            x += ww + spaceW;
        }
        y += lineH;
    }
    U.alpha = a0;
}

void Game::drawStory() {
    ui::Ui& U = ui;
    if (!story_.active()) return;
    const StoryNode* n = story_.node();
    const Story* s = story_.story();
    if (storyPlaying_) {
        drawHud();
        if (n) {
            const char* g = n->playGoal == "her" ? "Bring her to her peak" : (n->playGoal == "him" ? "Let him have his moment" :
                            (n->playGoal.rfind("time:", 0) == 0 ? "Take your time together" : "Find each other - together"));
            U.alpha = 1.f;
            float w = U.measure(ui::Face::SansMedium, U.u(15.f), g).x + U.u(120.f);
            ui::Box b; b.radius = U.u(16.f); b.fill = Color{24, 14, 18, 170}; b.border = std::max(1.f, U.u(1.f)); b.borderCol = ui::withA(U.th.accent, 0.5f);
            U.box(Rectangle{U.W * 0.5f - w * 0.5f, U.u(74.f), w, U.u(34.f)}, b);
            U.text(ui::Face::SansBold, U.u(12.f), "STORY", U.W * 0.5f - w * 0.5f + U.u(18.f), U.u(84.f), U.th.accent, ui::Left, 0.22f);
            U.text(ui::Face::SansMedium, U.u(15.f), g, U.W * 0.5f - w * 0.5f + U.u(80.f), U.u(82.f), U.th.ink);
        }
        // her (or his) voice over the play, as subtitles above the action bar
        if (!beatNode_.empty() && beatLine_ >= 0) {
            int nw = (int)splitWords(beatText_).size();
            int shown = nw;
            if (beatTake_ && !beatTake_->words.empty() && beatT_ < beatTake_->dur)
                shown = (int)std::ceil((float)voices_.wordsSpoken(*beatTake_, beatT_) / (float)beatTake_->words.size() * (float)nw);
            float a = clamp01(beatT_ * 4.f);
            drawSubtitle(beatWho_, beatText_, shown, a, U.H - U.u(250.f));
        }
        if (black_ > 0.f) U.fillRect(Rectangle{0, 0, U.W, U.H}, Color{0, 0, 0, (unsigned char)(255.f * clamp01(black_))});
        return;
    }
    // letterbox: the frame narrows to about 2.2:1
    float barH = std::max(U.u(64.f), std::min(U.H * 0.16f, (U.H - U.W / 2.2f) * 0.5f)) * ui::easeInOut(letterbox_);
    U.alpha = 1.f;
    if (barH > 0.5f) {
        U.fillRect(Rectangle{0, 0, U.W, barH}, Color{0, 0, 0, 255});
        U.fillRect(Rectangle{0, U.H - barH, U.W, barH}, Color{0, 0, 0, 255});
    }
    // chapter / location card
    if (storyCardT_ < 4.f && n) {
        float a = clamp01(storyCardT_ * 1.6f) * clamp01((4.f - storyCardT_) * 1.2f) * (1.f - clamp01(black_ * 2.f));
        U.alpha = a;
        int ci = storyCardKind_ == 0 ? s->chapterIndex(n->chapter) : -1;
        if (ci >= 0 || !storyLocation_.empty()) {
            float top = U.H * 0.24f + (ci >= 0 ? -U.u(6.f) : U.u(24.f)), hgt = ci >= 0 ? U.u(160.f) : U.u(56.f);
            ui::Box sc; sc.radius = U.u(40.f); sc.shadow = U.u(200.f); sc.shadowCol = Color{8, 4, 6, 170};
            U.box(Rectangle{U.W * 0.5f - U.u(330.f), top, U.u(660.f), hgt}, sc);
        }
        if (ci >= 0) {
            char b[32]; std::snprintf(b, sizeof b, "CHAPTER %d", ci + 1);
            U.text(ui::Face::SansBold, U.u(15.f), b, U.W * 0.5f, U.H * 0.24f, U.th.accent, ui::Center, 0.3f);
            U.text(ui::Face::Serif, U.u(68.f), s->chapters[(size_t)ci].title, U.W * 0.5f, U.H * 0.24f + U.u(30.f), U.th.ink, ui::Center);
        }
        if (!storyLocation_.empty()) {
            std::string loc = storyLocation_;
            if (!loc.empty()) loc[0] = (char)std::toupper((unsigned char)loc[0]);
            U.text(ui::Face::SerifItalic, U.u(24.f), loc, U.W * 0.5f, U.H * 0.24f + (storyCardKind_ == 0 ? U.u(118.f) : U.u(40.f)), U.th.inkDim, ui::Center);
        }
        U.alpha = 1.f;
    }
    // the top bar: where we are
    if (barH > U.u(40.f) && n) {
        int ci = s->chapterIndex(n->chapter);
        std::string where = s->title;
        if (ci >= 0) where += "  \xc2\xb7  " + s->chapters[(size_t)ci].title;
        U.alpha = 0.7f * letterbox_;
        U.text(ui::Face::SansBold, U.u(12.f), where, U.u(44.f), barH * 0.5f - U.u(7.f), U.th.inkFaint, ui::Left, 0.2f);
        U.alpha = 1.f;
    }
    // ending
    if (story_.atEnding()) {
        float a = clamp01(screenTime_);
        U.backdrop(0.8f);
        U.alpha = a;
        U.text(ui::Face::SansBold, U.u(15.f), "AN ENDING", U.W * 0.5f, U.H * 0.3f, U.th.accent, ui::Center, 0.3f);
        U.text(ui::Face::Serif, U.u(76.f), n->endTitle, U.W * 0.5f, U.H * 0.3f + U.u(34.f), U.th.ink, ui::Center);
        U.paragraph(ui::Face::SerifItalic, U.u(26.f), n->endText, U.W * 0.5f - U.u(420.f), U.H * 0.3f + U.u(150.f), U.u(840.f), U.th.inkDim, 1.35f, ui::Center);
        char b[64];
        std::snprintf(b, sizeof b, "%d of %d endings found", (int)storyProgress_.endings[s->id].size(), (int)s->endings.size());
        U.text(ui::Face::Sans, U.u(16.f), b, U.W * 0.5f, U.H * 0.3f + U.u(300.f), U.th.inkFaint, ui::Center);
        if (U.button("end#back", "Back to stories", Rectangle{U.W * 0.5f - U.u(130.f), U.H * 0.3f + U.u(350.f), U.u(260.f), U.u(54.f)}, true)) {
            story_.stop(); go(Screen::Stories, 0.6f);
        }
        U.alpha = 1.f;
        return;
    }
    // choices
    if (story_.atChoices() && black_ < 0.5f) {
        std::vector<int> vis = story_.visibleChoices();
        float cw = U.u(820.f), chh = U.u(64.f), gap = U.u(16.f);
        float total = (float)vis.size() * (chh + gap) - gap;
        float y = U.H * 0.5f - total * 0.5f - U.u(30.f);
        for (size_t i = 0; i < vis.size(); ++i) {
            const StoryChoice& c = n->choices[(size_t)vis[i]];
            Rectangle r{U.W * 0.5f - cw * 0.5f, y + (float)i * (chh + gap), cw, chh};
            bool over = CheckCollisionPointRec(U.in.mouse, r);
            float h = U.anim("ch#" + std::to_string(i), over ? 1.f : 0.f, 14.f);
            float pr = U.anim("chp#" + std::to_string(i), over && U.in.mouseDown ? 1.f : 0.f, 20.f);
            ui::Box b; b.radius = U.u(20.f); b.glass = true; b.glassTint = U.th.glass; b.glassAmt = 0.62f;
            b.fill = ui::mixc(Color{255, 236, 224, 8}, ui::withA(U.th.accent, 0.18f), h);
            b.border = std::max(1.f, U.u(1.2f)); b.borderCol = ui::mixc(U.th.hairline, ui::withA(U.th.accent, 0.9f), h);
            b.shadow = U.u(18.f + 14.f * h); b.shadowCol = ui::withA(U.th.accent, 0.12f + 0.15f * h);
            U.box(Rectangle{r.x - h * U.u(4.f) + pr * U.u(2.f), r.y + pr * U.u(1.5f), r.width + h * U.u(8.f) - pr * U.u(4.f), r.height - pr * U.u(3.f)}, b);
            char k[4]; std::snprintf(k, sizeof k, "%d", (int)i + 1);
            U.text(ui::Face::SansBold, U.u(14.f), k, r.x + U.u(26.f), r.y + (chh - U.u(14.f)) * 0.5f, U.th.accent);
            U.text(ui::Face::Serif, U.u(25.f), c.text, r.x + U.u(58.f), r.y + (chh - U.u(25.f)) * 0.5f - U.u(3.f), ui::mixc(U.th.inkDim, U.th.ink, h));
            if (over && U.in.released && storyOverlay_ == 0) { story_.choose(vis[i]); if (ui.onSound) ui.onSound(1); }
        }
    }
    // subtitles: the line as it is spoken (word by word with the voice), the last line under the choices
    const StoryLine* l = story_.line();
    const StoryLine* shownL = l;
    static StoryLine last;
    if (!shownL && story_.atChoices() && !story_.history.empty()) { last.who = story_.history.back().who; last.text = story_.history.back().text; shownL = &last; }
    if (shownL && black_ < 0.6f && shownL->who != "you") {
        int nw = (int)splitWords(shownL->text).size();
        int shown = nw;
        if (l && !story_.lineDone()) {
            if (take_ && !take_->words.empty()) {
                float vt = storyVoiceTime();
                shown = (int)std::ceil((float)voices_.wordsSpoken(*take_, vt) / (float)take_->words.size() * (float)nw);
                if (vt >= take_->dur) shown = nw;
            } else {
                // typewriter: the words whose last letter is out
                int rev = (int)story_.revealed(), chars = 0;
                shown = 0;
                for (const std::string& w : splitWords(l->text)) {
                    for (unsigned char ch : w) if ((ch & 0xC0) != 0x80) ++chars;
                    if (chars <= rev) ++shown;
                    ++chars;
                }
            }
        }
        float cy = barH > U.u(96.f) ? U.H - barH * 0.5f - U.u(6.f) : U.H - barH - U.u(70.f);
        drawSubtitle(shownL->who, shownL->text, shown, story_.atChoices() ? 0.55f : 1.f, cy);
        if (l && story_.lineDone() && !story_.autoMode) {
            float bl = 0.5f + 0.5f * std::sin(time_ * 4.f);
            U.diamond(U.W - U.u(56.f), U.H - barH * 0.5f, U.u(6.f), ui::withA(U.th.accent, 0.35f + 0.6f * bl));
        }
    }
    // controls, small, in the top bar
    {
        struct C { const char* label; bool on; int id; };
        C cs[6] = {{"Auto", story_.autoMode, 0}, {"Skip", storySkipToggle_, 1}, {"Log", false, 2}, {"Save", false, 3}, {"Load", false, 4}, {"Menu", false, 5}};
        float cx = U.W - U.u(44.f), cy = std::max(U.u(14.f), barH * 0.5f - U.u(7.f));
        float minX = cx;
        for (int i = 5; i >= 0; --i) {
            float tw = U.measure(ui::Face::SansBold, U.u(13.f), cs[i].label, 0.18f).x;
            Rectangle r{cx - tw - U.u(10.f), cy - U.u(8.f), tw + U.u(20.f), U.u(30.f)};
            bool over = CheckCollisionPointRec(U.in.mouse, r);
            if (over) U.fillRect(r, Color{255, 236, 224, 18}, U.u(8.f));
            U.text(ui::Face::SansBold, U.u(13.f), cs[i].label, cx, cy, cs[i].on ? U.th.accent : (over ? U.th.ink : U.th.inkFaint), ui::Right, 0.18f);
            if (over && U.in.clicked && storyOverlay_ == 0) {
                if (ui.onSound) ui.onSound(1);
                if (cs[i].id == 0) story_.autoMode = !story_.autoMode;
                else if (cs[i].id == 1) storySkipToggle_ = !storySkipToggle_;
                else if (cs[i].id == 2) storyOverlay_ = 1;
                else if (cs[i].id == 3) storyOverlay_ = 2;
                else if (cs[i].id == 4) storyOverlay_ = 3;
                else storyOverlay_ = 5;
            }
            cx -= tw + U.u(30.f);
            minX = r.x;
        }
        storyControlsRect_ = Rectangle{minX, cy - U.u(10.f), U.W - minX, U.u(34.f)};
    }
    if (black_ > 0.f) U.fillRect(Rectangle{0, 0, U.W, U.H}, Color{0, 0, 0, (unsigned char)(255.f * clamp01(black_))});
    // overlays
    if (storyOverlay_ == 1) drawStoryLog();
    else if (storyOverlay_ == 2) drawStorySlots(true);
    else if (storyOverlay_ == 3) drawStorySlots(false);
    else if (storyOverlay_ == 5) drawStoryMenu();
}

}  // namespace ag
