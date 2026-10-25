#include "game/game.h"
#include "anim/stress.h"
#include "app/platform.h"
#include "app/updater.h"
#include "core/log.h"
#include "core/paths.h"
#include "gfx/gl.h"
#include "gfx/texture.h"
#include "scene/lighting.h"
#include "raylib.h"
#include "rlgl.h"
#include <ctime>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace ag {


// ------------------------------------------------------------------ lifecycle
bool Game::init() {
    settingsPath_ = paths::join(paths::userDir(), "settings.json");
    profilePath_ = paths::join(paths::userDir(), "profile.json");
    bool firstRun = !paths::exists(settingsPath_);
    if (!harness) { settings.load(settingsPath_); profile.load(profilePath_); }
    ui.guard = harness;   // QA: every text / widget checked against the screen and its container
    const char* gpu = (const char*)glGetString(GL_RENDERER);
    std::string g = gpu ? gpu : "";
    AG_INFO("AG: GPU: %s", g.c_str());
    if (firstRun && !harness) {
        // first start: pick a quality that runs smoothly on this GPU (the player can change it any time)
        for (auto& c : g) c = (char)std::tolower((unsigned char)c);
        auto has = [&](const char* k) { return g.find(k) != std::string::npos; };
        if (has("llvmpipe") || has("softpipe") || has("swiftshader") || has("software")) settings.quality = 0;
        else if (has("intel") || has("uhd") || has("iris") || has("radeon(tm) graphics") || has("vega") || has("mali") || has("adreno")) settings.quality = 1;
        AG_INFO("AG: first run: quality %d for this GPU", settings.quality);
    }
    ui.init();
    if (!loadingProgress) loadingProgress = [this](float f, const char* what) { drawLoading(f, what); };
    // while textures decode on worker threads, keep the window responsive and the loader animating
    Model::loadPump = [this]() {
        static double last = 0.0;
        if (GetTime() - last < 0.05) return;
        last = GetTime();
        drawLoading(loadF_, loadWhat_.c_str());
    };
    loadingProgress(0.f, "Opening the door");
    if (!renderer.init()) return false;
    applyQuality(renderer.settings, (Quality)settings.quality);
    // texture detail by quality: 1K maps on Low, 2K on Medium, full 4K skin on High
    setTextureMaxSize(settings.quality == 0 ? 1024 : (settings.quality == 1 ? 2048 : 0));
    if (!scene.load(loadingProgress)) AG_WARN("AG: scene loaded without characters");
    loadingProgress(1.f, "");
    Model::loadPump = nullptr;
    std::string meta = paths::findAsset({"characters/clips.json"});
    if (!meta.empty()) clipsMeta_ = Json::parse(paths::readText(meta));
    std::string presets = paths::findAsset({"characters/presets.json"});
    if (!presets.empty()) clipsPresets_ = Json::parse(paths::readText(presets));
    ensureCast(false);   // the player's models (characters/roster.json); the base GLBs are already in
    platform::trimHeap();   // loading spikes (file images, decode buffers) go back to the OS

    // acts come from the content: every clip family with a base clip (<id>.in = depth-driven, or a loop) is an
    // act, grouped Foreplay / Oral / Positions / Solo; clips.json may name and group them ("label", "group",
    // "her", "him"), otherwise known ids get good names and gains and new ones a sensible default
    struct Known { const char* id; const char* name; const char* group; float her, him; bool pen; };
    static const Known known[] = {
        {"kiss", "Kissing", "Foreplay", 0.45f, 0.25f, false}, {"caress", "Caresses", "Foreplay", 0.55f, 0.25f, false},
        {"oral", "Her Lips", "Oral", 0.3f, 1.25f, false}, {"lick", "His Tongue", "Oral", 1.25f, 0.2f, false},
        {"missionary", "Missionary", "Positions", 1.0f, 1.0f, true}, {"legsup", "Legs Raised", "Positions", 1.1f, 1.1f, true},
        {"doggy", "From Behind", "Positions", 1.05f, 1.2f, true}, {"prone", "Lying Flat", "Positions", 1.0f, 1.15f, true},
        {"cowgirl", "On Top", "Positions", 1.15f, 0.95f, true}, {"revcowgirl", "Reverse", "Positions", 1.0f, 1.1f, true},
        {"spoon", "Spooning", "Positions", 0.95f, 0.9f, true}, {"solo", "Alone", "Solo", 1.0f, 0.f, false},
        {"handjob", "Her Hand", "Foreplay", 0.35f, 1.1f, false}, {"titjob", "Her Breasts", "Foreplay", 0.4f, 1.15f, false},
    };
    // story staging clips (standing, sitting, dancing, undressing, cuddling...) are not acts
    static const char* storyOnly[] = {"stand", "sit", "dance", "undress", "walk", "talk", "listen", "lie", "cuddle", "embrace", "lean", "kneel"};
    const Character& src = scene.vivian.loaded() ? scene.vivian : scene.marcus;
    std::vector<std::string> families;
    for (const std::string& c : src.clipNames()) {
        std::string fam = c.substr(0, c.find('.'));
        if (std::find(families.begin(), families.end(), fam) == families.end()) families.push_back(fam);
    }
    for (const std::string& fam : families) {
        std::string base;
        for (const char* suffix : {".in", ".idle", ".loop"}) if (src.hasClip(fam + suffix)) { base = fam + suffix; break; }
        if (base.empty()) continue;   // layers only (rub / embrace)
        const Json& meta = clipsMeta_["clips"][base];
        bool staging = meta["story"].boolean(false) || !meta["act"].boolean(true);
        for (const char* so : storyOnly) staging = staging || fam == so;
        if (staging) continue;
        PositionInfo p;
        p.id = fam;
        p.baseClip = base;
        p.depthDriven = base.size() > 3 && base.compare(base.size() - 3, 3, ".in") == 0;
        p.solo = fam == "solo" || meta["marcusHidden"].boolean(false);
        const Known* k = nullptr;
        for (const Known& kn : known) if (fam == kn.id) k = &kn;
        std::string g = meta["group"].str();
        if (g.empty()) {
            if (k) g = k->group;
            else if (p.solo) g = "Solo";
            else if (fam.find("oral") != std::string::npos || fam.find("lick") != std::string::npos || fam.find("blow") != std::string::npos ||
                     fam.find("cunni") != std::string::npos || fam.find("69") != std::string::npos) g = "Oral";
            else if (!p.depthDriven) g = "Foreplay";
            else g = "Positions";
        }
        p.group = g;
        std::string label = meta["label"].str(meta["name"].str());
        if (label.empty() && k) label = k->name;
        if (label.empty()) { label = fam; label[0] = (char)std::toupper((unsigned char)label[0]); }
        p.name = label;
        p.her = meta["her"].f(k ? k->her : (g == "Foreplay" ? 0.55f : 1.f));
        p.him = meta["him"].f(k ? k->him : (g == "Foreplay" ? 0.25f : (p.solo ? 0.f : 1.f)));
        p.penetrative = k ? k->pen : (g == "Positions");
        p.canTouch = !p.solo && src.hasClip(fam + ".rub");
        p.canKiss = src.hasClip(fam + ".embrace");
        positions_.push_back(p);
    }
    // display order: Foreplay, Oral, Positions, Solo; known ids in their classic order, new ones after
    auto rank = [&](const PositionInfo& p) {
        const char* order[] = {"Foreplay", "Oral", "Positions", "Solo"};
        int gi = 4;
        for (int i = 0; i < 4; ++i) if (p.group == order[i]) gi = i;
        int ki = 100;
        for (size_t i = 0; i < sizeof(known) / sizeof(known[0]); ++i) if (p.id == known[i].id) ki = (int)i;
        return gi * 1000 + ki;
    };
    std::stable_sort(positions_.begin(), positions_.end(), [&](const PositionInfo& x, const PositionInfo& y) { return rank(x) < rank(y); });
    if (positions_.empty()) { PositionInfo p; p.id = "none"; p.name = "\xe2\x80\x94"; positions_.push_back(p); }
    loadStories();
    sceneReady_ = true;
    camSafety_.boundsMin = scene.info.camMin;
    camSafety_.boundsMax = scene.info.camMax;
    {
        // the bed: the mattress (eye stays above the sheet) and the headboard with the pillows against it
        const RoomInfo& ri = scene.info;
        float hx = ri.bedHalf.x + 0.08f, headZ = ri.bedCenter.z - ri.bedHalf.z, footZ = ri.bedCenter.z + ri.bedHalf.z;
        camSafety_.solids.push_back({V3{-hx, 0.f, headZ - 0.1f}, V3{hx, ri.bedTop + 0.04f, footZ + 0.06f}});
        camSafety_.solids.push_back({V3{-hx - 0.05f, 0.f, headZ - 0.25f}, V3{hx + 0.05f, 1.35f, headZ + 0.22f}});
    }
    camera.solve = [this](const V3& eye, const V3& target, bool fp) {
        return resolveCamera(proxies_, camSafety_, eye, target, fp ? 1 : -1);   // first person sits in his head
    };
    if (enableAudio) initAudio();
    ui.onSound = [this](int k) {
        if (!audio.ready()) return;
        float v = settings.effects * 0.5f;
        if (k == 0) audio.play("ui/hover", v * 0.35f, 1.f);
        else if (k == 1) audio.play("ui/press", v * 0.8f, 1.f);
        else if (k == 2) audio.play("ui/back", v * 0.7f, 1.f);
        else if (k == 3) audio.play("ui/tick", v * 0.25f, 1.f);
    };
    applyLooks();
    stage(Stage::Bedroom);
    enterScreen(Screen::Title);
    fade_ = 1.f; fadeDir_ = -1.f; fadeTime_ = 1.2f;   // fade in from black
    if (updater && settings.checkUpdates && !harness && !Updater::isDevBuild()) updater->checkAsync();
    AG_INFO("AG: game ready: %d positions, RSS %.0f MB, textures %.0f MB, meshes %.0f MB (GPU)", (int)positions_.size(),
            (double)platform::residentBytes() / 1048576.0, (double)textureBytes() / 1048576.0, (double)Model::bufferBytes() / 1048576.0);
    return true;
}

void Game::saveAll() {
    if (harness) return;
    settings.save(settingsPath_);
    profile.save(profilePath_);
}

void Game::shutdown() {
    bool inNight = screen_ == Screen::Play || screen_ == Screen::Pause || (screen_ == Screen::Settings && settingsReturn_ == Screen::Pause);
    if (inNight && !night_.finished) profile.night = night_.snapshot();
    saveAll();
    shutdownAudio();
    ui.shutdown();
    renderer.shutdown();
    scene.destroy();
}

void Game::onFocusLost() {
    if (settings.pauseWhenUnfocused && screen_ == Screen::Play) { go(Screen::Pause, 0.15f); }
}

// ------------------------------------------------------------------ flow
void Game::go(Screen s, float t) {
    if (switching_ && pending_ == s) return;
    pending_ = s;
    switching_ = true;
    fadeTime_ = std::max(0.05f, t);
    fadeDir_ = 1.f;
    ui.resetFocus();
}

void Game::enterScreen(Screen s) {
    Screen prev = screen_;
    screen_ = s;
    if (s != Screen::Play && !harness && IsCursorHidden()) ShowCursor();
    blurFresh_ = 0;
    screenTime_ = 0.f;
    ui.resetFocus();
    confirmQuit_ = confirmNew_ = false;
    renderer.uiBlur = s != Screen::Play;
    switch (s) {
        case Screen::Title: case Screen::Menu: case Screen::Stories:
            if (prev == Screen::Story || prev == Screen::Settings || !place_.empty()) leaveStoryWorld();
            if (stage_ != Stage::Bedroom) stage(Stage::Bedroom);
            storyMood_ = -1;
            break;
        case Screen::Story:
            break;
        case Screen::Customize:
            stage(Stage::Showcase);
            break;
        case Screen::Settings:
            break;
        case Screen::Play:
            if (prev == Screen::Pause || prev == Screen::Settings) break;
            break;
        case Screen::Pause:
            barOpen_ = false;
            break;
        case Screen::Results:
            break;
    }
}

// ------------------------------------------------------------------ staging
void Game::stage(Stage st) {
    stage_ = st;
    Character& v = scene.vivian;
    Character& m = scene.marcus;
    for (Character* c : {&v, &m}) {
        if (!c->loaded()) continue;
        c->clearLayers();
        c->clearBoneOffsets();
        c->poseHook = nullptr;
        c->postHook = nullptr;
        c->touchGoals = TouchGoals{};
        c->placement = M4::identity();
        c->proc.hair = settings.hairPhysics;
        c->proc.lookAt = true;
        c->proc.lookWeight = 0.6f;
        c->proc.eyeRollUp = 0.f;
        c->flush = 0.f; c->sweat = 0.f; c->wet = 0.f;
        // faces and arousal shapes go back to the clips (the night / the story set them by hand)
        for (const char* mn : {"bite_lip", "gasp", "orgasm", "bliss", "vag_aroused", "lids_down", "lids_up", "pout", "shy", "smirk", "wink",
                               "kiss", "oral", "moan", "pleasure", "effort", "blink", "smile", "laugh", "brow_up", "frown", "tender"})
            c->setMorph(mn, -1.f);
        c->resetSimulation();
    }
    postBloomAdd_ = postSatAdd_ = postVigAdd_ = postWarmAdd_ = 0.f; postExpMul_ = 1.f;
    switch (st) {
        case Stage::Bedroom:
            scene.showVivian = v.loaded(); scene.showMarcus = false;
            if (v.loaded() && v.hasClip("solo.idle")) v.setLayer(0, "solo.idle", 0.f, 1.f);
            camera.setOrbit(scene.info.bedCenter + V3{0.f, 0.12f, 0.15f}, 1.05f, 0.3f, 2.9f, 0.f);
            break;
        case Stage::Showcase: {
            Character& c = showcaseWho_ == 0 ? v : m;
            scene.showVivian = showcaseWho_ == 0 && v.loaded();
            scene.showMarcus = showcaseWho_ == 1 && m.loaded();
            standPose(c);
            turntable_ = 0.25f; turnVel_ = 0.f; showcaseZoom_ = 0.f;
            break;
        }
        case Stage::Night:
            scene.showVivian = v.loaded(); scene.showMarcus = m.loaded();
            if (v.loaded()) v.poseHook = [this](Animator& a) { archBody(a); };
            break;
    }
}

void Game::standPose(Character& c) {
    // relaxed standing pose from the A-pose rest: arms lowered, elbows soft, a little weight shift
    c.poseHook = [](Animator& a) {
        const Model* m = a.model();
        auto aim = [&](const char* bone, const V3& axis, float ang) {
            int b = m->findNode(bone);
            if (b < 0) return;
            a.setWorldRot(b, qmul(qaxis(axis, ang), a.worldRot(b)));
            a.recomputeSubtree(b);
        };
        aim("腕.L", {0, 0, 1}, -0.72f); aim("腕.R", {0, 0, 1}, 0.72f);
        aim("腕.L", {0, 1, 0}, 0.12f); aim("腕.R", {0, 1, 0}, -0.12f);
        aim("ひじ.L", {1, 0, 0}, -0.28f); aim("ひじ.R", {1, 0, 0}, -0.28f);
        aim("手首.L", {0, 0, 1}, 0.18f); aim("手首.R", {0, 0, 1}, -0.18f);
        aim("下半身", {0, 0, 1}, 0.03f);
        aim("上半身", {0, 0, 1}, -0.035f);
        aim("足.R", {0, 0, 1}, 0.035f); aim("ひざ.R", {1, 0, 0}, 0.08f);
    };
    c.proc.lookWeight = 0.55f;
}

void Game::applyLooks() {
    // multipliers on the authored espresso hair (linear ~0.042, 0.015, 0.009): espresso, chestnut, copper,
    // auburn, raven, honey
    static const V3 kHair[6] = {{1.f, 1.f, 1.f}, {2.4f, 2.8f, 2.3f}, {5.2f, 4.0f, 2.3f}, {3.3f, 2.0f, 1.75f}, {0.32f, 0.62f, 1.05f}, {9.f, 16.f, 11.6f}};
    Character& v = scene.vivian;
    Character& m = scene.marcus;
    if (v.loaded()) {
        const HerLook& h = profile.her;
        v.setBody("bust", h.bust); v.setBody("hips", h.hips); v.setBody("waist", h.waist); v.setBody("butt", h.butt);
        v.setBody("weight", h.weight); v.setBody("muscle", h.muscle); v.setBody("belly", h.belly);
        // the player's tone and hair colour belong to the model they picked; a cast the story brings in (Vivian while the
        // profile has picked Mei) shows that model's own
        bool own = v.asset == h.model;
        int tone = own ? h.tone : std::max(0, skinToneIndex(castEntry(0)["tone"].str("caramel")));
        applySkin(v, tone);
        // a model with its own face has that face's age: the base's age shapes were measured on the base's face
        v.setBody("age", castEntry(0)["fixedAge"].boolean(false) ? 0.f : ageToMorph(h.age, 42.f));
        bool legacyHair = false;
        for (const Material& mat : v.model.materials)
            if (mat.shading == Shading::Hair && mat.baseColor.x > 0.9f) legacyHair = true;
        int hairIx = own ? h.hair : (int)castEntry(0)["hairColour"].num(0);
        V3 tint = kHair[std::max(0, std::min(5, hairIx))];
        if (legacyHair) tint = V3{std::min(1.f, 0.25f * tint.x), std::min(1.f, 0.09f * tint.y), std::min(1.f, 0.052f * tint.z)};
        v.hairTint = tint;
    }
    if (m.loaded()) {
        const HisLook& h = profile.him;
        m.setBody("muscle", h.muscle); m.setBody("weight", h.weight); m.setBody("belly", h.belly);
        applySkin(m, m.asset == h.model ? h.tone : std::max(0, skinToneIndex(castEntry(1)["tone"].str("caramel"))));
        m.setBody("age", castEntry(1)["fixedAge"].boolean(false) ? 0.f : ageToMorph(h.age, 40.f));
    }
}

// eight skin tones: the painted albedo when the character ships it (Vivian has all eight), else a tint of the
// nearest painted one (Marcus: honey, caramel, deep)
void Game::applySkin(Character& c, int tone) {
    struct T { const char* tex; V3 tint; };
    static const T kFallback[kSkinToneCount] = {
        {"honey", {1.2f, 1.15f, 1.12f}}, {"honey", {1.12f, 1.08f, 1.06f}}, {"honey", {1.f, 1.f, 1.f}}, {"honey", {0.93f, 0.95f, 0.83f}},
        {"caramel", {1.f, 1.f, 1.f}}, {"caramel", {1.f, 0.92f, 0.82f}}, {"deep", {1.f, 1.f, 1.f}}, {"deep", {0.72f, 0.70f, 0.70f}},
    };
    tone = std::max(0, std::min(kSkinToneCount - 1, tone));
    if (c.setTone(skinToneName(tone))) { c.skinTint = {1.f, 1.f, 1.f}; return; }
    if (std::string(skinToneName(tone)) == "caramel") { c.skinTint = {1.f, 1.f, 1.f}; return; }   // the embedded albedo
    c.setTone(kFallback[tone].tex);
    c.skinTint = kFallback[tone].tint;
}

// years -> the content's age morph (age_up / age_dn, when shipped): the base model is the base age
// the content's age morph: -1 = 25 years, 0 = the character's own age, +1 = 60 (adult shapes only; 21..25 read as 25)
float Game::ageToMorph(float years, float base) {
    years = std::max(kMinAge, std::min(kMaxAge, years));
    const float young = 25.f;
    return years < base ? std::max(-1.f, (years - base) / (base - young)) : (years - base) / (kMaxAge - base);
}

void Game::updateMenuCamera(float dt) {
    if (stage_ == Stage::Bedroom) {
        float t = time_ * 0.045f;
        V3 tgt = scene.info.bedCenter + V3{0.05f * std::sin(t * 1.7f), 0.1f, 0.12f};
        float yaw = 1.0f + 0.32f * std::sin(t), pitch = 0.3f + 0.05f * std::sin(t * 1.3f), dist = 2.75f + 0.25f * std::sin(t * 0.8f);
        camera.setOrbit(tgt, yaw, pitch, dist, 0.f);
    } else if (stage_ == Stage::Showcase) {
        Character& c = showcaseWho_ == 0 ? scene.vivian : scene.marcus;
        if (!qaFocus_.empty()) {   // QA: a fixed body, the camera on one joint (or the whole figure)
            c.placement = M4::identity();
            bool whole = qaFocus_ == "full" || c.bone(qaFocus_) < 0;
            V3 tgt = whole ? V3{0.f, (showcaseWho_ == 0 ? 1.68f : 1.83f) * 0.5f, 0.f} : c.bonePos(qaFocus_);
            camera.setOrbit(tgt, qaYaw_, qaPitch_, whole ? std::max(qaDist_, 2.6f) : qaDist_, 0.f);
            return;
        }
        // turntable: drag to spin with inertia, wheel to zoom toward the torso
        bool overPanel = ui.in.mouse.x > ui.W * 0.56f;
        if (!overPanel && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !ui.anyActive()) turnVel_ = GetMouseDelta().x * 0.45f * settings.mouseSens;
        else turnVel_ *= std::exp(-dt * 3.5f);
        if (!overPanel) showcaseZoom_ = clampf(showcaseZoom_ + GetMouseWheelMove() * 0.12f, 0.f, 1.f);
        turntable_ += turnVel_ * dt;
        c.placement = m4trs({0.f, 0.f, 0.f}, qaxis({0, 1, 0}, turntable_), {1, 1, 1});
        float hgt = showcaseWho_ == 0 ? 1.68f : 1.83f;
        float z = ui::easeInOut(showcaseZoom_);
        V3 tgt{0.62f - 0.25f * z, hgt * (0.53f + 0.2f * z), 0.f};
        camera.setOrbit(tgt, 0.f, 0.06f, 3.6f - 1.8f * z, 0.f);
        if (qaPortrait_ > 0.f) {
            // QA portraits: head and shoulders, framed left of the panel (portrait=1 head, 0.5 bust)
            V3 hc = c.hairCenter();
            float d = 0.55f / qaPortrait_;
            camera.setOrbit(hc + V3{0.16f * d, -0.05f - 0.12f * (d - 0.55f), 0.f}, 0.f, 0.03f, d, 0.f);
        }
        c.proc.lookTarget = cam_.eye;
    }
}

// ------------------------------------------------------------------ update / render
void Game::update(float dt) {
    frameDt_ = dt;
    time_ += dt;
    screenTime_ += dt;
    introT_ += dt;
    ui.begin(dt, true);
    if (IsKeyPressed(KEY_F1)) debug_ = !debug_;
    if (IsKeyPressed(KEY_F12) && !harness) shotPending_ = true;
    shotToastT_ = std::max(0.f, shotToastT_ - dt);
    if (IsKeyPressed(KEY_F5)) renderer.reloadShaders();

    // fades between screens
    if (fadeDir_ != 0.f) {
        fade_ += fadeDir_ * dt / fadeTime_;
        if (fadeDir_ > 0.f && fade_ >= 1.f) {
            fade_ = 1.f;
            if (switching_) { switching_ = false; enterScreen(pending_); }
            fadeDir_ = -1.f;
        } else if (fadeDir_ < 0.f && fade_ <= 0.f) { fade_ = 0.f; fadeDir_ = 0.f; }
    }

    if (screen_ == Screen::Story) {
        updateStory(dt);
    } else if (screen_ == Screen::Settings && settingsReturn_ == Screen::Story) {
        presentStory(0.f);
    } else if (screen_ == Screen::Play || screen_ == Screen::Pause || screen_ == Screen::Results ||
        (screen_ == Screen::Settings && settingsReturn_ == Screen::Pause)) {
        updatePlay(screen_ == Screen::Play ? dt : 0.f);
        presentPlay(screen_ == Screen::Play ? dt : (screen_ == Screen::Results ? dt : 0.f));
    } else {
        updateMenuCamera(dt);
        if (stage_ == Stage::Bedroom && scene.vivian.loaded()) {
            idlePhase_ = std::fmod(idlePhase_ + dt / 14.f, 1.f);
            scene.vivian.setLayer(0, "solo.idle", idlePhase_, 1.f);
            scene.vivian.proc.lookTarget = cam_.eye;
            scene.vivian.proc.lookWeight = 0.35f;
            scene.vivian.setMorph("smile", 0.25f);
        }
        renderer.settings.flash = 0.f;
        scene.update(dt);
    }
    {
        const Character* cs[2] = {&scene.vivian, &scene.marcus};
        bool shown[2] = {scene.showVivian, scene.showMarcus};
        proxies_.build(cs, shown, 2);
    }
    updateAudio(dt);

    // autosave the night now and then (crash safety)
    saveTimer_ += dt;
    if (saveTimer_ > 20.f && (screen_ == Screen::Play || screen_ == Screen::Pause)) {
        saveTimer_ = 0.f;
        profile.night = night_.snapshot();
        if (!harness) profile.save(profilePath_);
    }
}

void Game::render(int w, int h) {
    fbW_ = w; fbH_ = h;
    float rdt = frameDt_ > 0.f ? frameDt_ : 1.f / 60.f;
    cam_ = camera.update(rdt, (float)w / (float)std::max(1, h));
    RenderSettings& rs = renderer.settings;
    // lighting mood (+ the player's brightness/warmth), then the night's own grade on top
    LightingSetup ls;
    ls.mood = (Mood)settings.lightMood; ls.brightness = settings.brightness; ls.warmth = settings.warmth;
    if ((screen_ == Screen::Story || (screen_ == Screen::Settings && settingsReturn_ == Screen::Story)) && storyMood_ >= 0) ls.mood = (Mood)storyMood_;
    applyLighting(scene, rs, ls, cam_);
    if (screen_ == Screen::Story && !storyPlaying_ && actIndex(pose_) < 0 && (scene.showVivian || scene.showMarcus)) {
        // story staging: the bake was lit for the bed and the actors stand away from the lamps; a film key from the
        // camera side lights their faces, and the room itself comes up a little so scenes read in the dark moods
        Lights& L = scene.lights;
        V3 hv = scene.vivian.bonePos("\xe9\xa0\xad"), hm = scene.marcus.bonePos("\xe9\xa0\xad");
        V3 mid = scene.showVivian && scene.showMarcus ? (hv + hm) * 0.5f : (scene.showVivian ? hv : hm);
        V3 toCam = cam_.eye - mid;
        toCam.y = 0.f;
        toCam = length(toCam) > 1e-3f ? normalize(toCam) : V3{0, 0, 1};
        V3 right = normalize(cross(V3{0, 1, 0}, toCam));
        L.keyPos = mid + toCam * 1.4f + right * 1.0f + V3{0.f, 0.85f, 0.f};
        L.keyDir = normalize(mid - V3{0.f, 0.15f, 0.f} - L.keyPos);
        L.keyInner = 0.45f; L.keyOuter = 0.95f;
        L.keyIntensity *= ls.mood == Mood::Daylight ? 0.6f : 0.85f;
        if (ls.mood != Mood::Daylight) { scene.bakeTint = scene.bakeTint * 1.6f; L.ambient *= 1.3f; }
    }
    bool night = screen_ == Screen::Play || screen_ == Screen::Pause || screen_ == Screen::Results ||
                 (screen_ == Screen::Settings && settingsReturn_ == Screen::Pause) || (screen_ == Screen::Story && storyPlaying_);
    if (night) {
        rs.bloomStrength += postBloomAdd_; rs.saturation += postSatAdd_; rs.vignette += postVigAdd_;
        rs.warmth += postWarmAdd_; rs.exposure *= postExpMul_;
    } else rs.flash = 0.f;
    if (stage_ == Stage::Showcase) {
        // customisation: a soft studio key from the front left so the body reads clearly in any mood
        scene.lights.keyPos = {-1.5f, 2.3f, 2.4f};
        scene.lights.keyDir = normalize(V3{0.f, 1.1f, 0.f} - scene.lights.keyPos);
        scene.lights.keyInner = 0.35f; scene.lights.keyOuter = 0.75f; scene.lights.keyIntensity = 24.f;
        scene.lights.fillIntensity = std::max(scene.lights.fillIntensity, 0.9f);
    }
    rs.renderScale = settings.renderScale * (settings.quality == 0 ? 0.85f : 1.f);
    // 4K panels: shade at most ~1440p worth of pixels (FXAA + bilinear upscale); invisible at laptop sizes
    float px = (float)w * (float)h * rs.renderScale * rs.renderScale;
    if (px > 3.7e6f) rs.renderScale *= std::sqrt(3.7e6f / px);
    rs.bloom = settings.bloom;
    rs.ssao = settings.ssao && settings.quality > 0;
    rs.grain = settings.filmGrain ? 0.024f : 0.f;
    // paused: once the frosted backdrop fully covers the frozen scene, keep the last blur and skip the 3D
    bool pausedBackdrop = screen_ == Screen::Pause || (screen_ == Screen::Settings && settingsReturn_ == Screen::Pause);
    bool frozen = pausedBackdrop && setTab_ != 2 && blurFresh_ >= 2 && screenTime_ > 0.6f && renderer.blurTexture() != 0 &&
                  w == lastRenderW_ && h == lastRenderH_ && !debug_;
    if (frozen) ClearBackground(Color{6, 3, 5, 255});
    else {
        const bool shown[2] = {scene.showVivian, scene.showMarcus};
        if (qaNoActors_) scene.showVivian = scene.showMarcus = false;   // UI layout shots: the room only
        if (qaHideMarcus_) scene.showMarcus = false;                    // QA: her hips / vulva alone
        bool hiddenGenitals = scene.marcus.hideGenitals;
        if (qaHideGenitals_) scene.marcus.hideGenitals = true;          // QA: identify the genital mesh in a close-up
        renderer.render(scene, cam_, w, h, time_);
        scene.marcus.hideGenitals = hiddenGenitals;
        scene.showVivian = shown[0]; scene.showMarcus = shown[1];
        lastRenderW_ = w; lastRenderH_ = h;
        if (renderer.uiBlur) ++blurFresh_;   // blur of this screen's own scene exists now
    }
    ui.blurTex = renderer.blurTexture();
    ui.blurW = renderer.blurWidth(); ui.blurH = renderer.blurHeight();

    ui.alpha = 1.f;
    if (!qaNoUi_) switch (screen_) {
        case Screen::Title: drawTitle(); break;
        case Screen::Menu: drawMenu(); break;
        case Screen::Customize: drawCustomize(); break;
        case Screen::Settings: drawSettings(); break;
        case Screen::Play: drawHud(); break;
        case Screen::Pause: drawHud(); drawPause(); break;
        case Screen::Results: drawResults(); break;
        case Screen::Stories: drawStories(); break;
        case Screen::Story: drawStory(); break;
    }
    if (debug_) drawDebug();
    if (shotPending_) {
        // the frame as the player sees it (UI included), into <user>/screenshots
        shotPending_ = false;
        rlDrawRenderBatchActive();
        Image img = LoadImageFromScreen();
        char name[64];
        std::snprintf(name, sizeof name, "afterglow-%lld.png", (long long)(GetTime() * 1000.0) + (long long)time(nullptr) * 1000LL);
        std::string dir = paths::join(paths::userDir(), "screenshots");
        paths::writeText(paths::join(dir, ".keep"), "");
        std::string file = paths::join(dir, name);
        bool ok = ExportImage(img, file.c_str());
        UnloadImage(img);
        shotToast_ = ok ? "Screenshot saved" : "Couldn't save the screenshot";
        shotToastT_ = 2.2f;
        AG_INFO("AG: screenshot %s %s", file.c_str(), ok ? "saved" : "failed");
    }
    if (shotToastT_ > 0.f) {
        ui.alpha = std::min(1.f, shotToastT_ * 2.f);
        float tw = ui.measure(ui::Face::SansMedium, ui.u(16.f), shotToast_).x + ui.u(40.f);
        ui::Box b; b.radius = ui.u(18.f); b.fill = Color{24, 14, 18, 200}; b.border = std::max(1.f, ui.u(1.f)); b.borderCol = ui.th.hairline;
        ui.box(Rectangle{ui.W - tw - ui.u(40.f), ui.H - ui.u(84.f), tw, ui.u(38.f)}, b);
        ui.text(ui::Face::SansMedium, ui.u(16.f), shotToast_, ui.W - tw * 0.5f - ui.u(40.f), ui.H - ui.u(75.f), ui.th.ink, ui::Center);
        ui.alpha = 1.f;
    }
    // screen fade + position-change dip
    float black = std::max(fade_, posFade_);
    if (black > 0.001f) {
        ui.alpha = 1.f;
        DrawRectangle(0, 0, (int)ui.W + 1, (int)ui.H + 1, Color{6, 3, 5, (unsigned char)std::lround(255.f * ui::easeInOut(black))});
    }
    ui.end();
}

void Game::drawLoading(float f, const char* what) {
    // drawn between loading steps: logo, a thin progress line, what is loading
    static float shown = 0.f;
    shown = std::max(shown, f);
    loadF_ = f;
    if (what != loadWhat_.c_str()) loadWhat_ = what ? what : "";
    ui.begin(1.f / 60.f, false);
    BeginDrawing();
    ClearBackground(Color{7, 4, 6, 255});
    ui.alpha = 1.f;
    ui.glow(ui.W * 0.5f, ui.H * 0.47f, ui.u(260.f), ui::withA(ui.th.accent, 0.06f));
    ui.text(ui::Face::Serif, ui.u(96.f), "Afterglow", ui.W * 0.5f, ui.H * 0.42f, ui.th.ink, ui::Center, 0.02f);
    float w = ui.u(360.f), x = ui.W * 0.5f - w * 0.5f, y = ui.H * 0.42f + ui.u(150.f);
    ui.fillRect(Rectangle{x, y, w, ui.u(2.f)}, Color{255, 236, 224, 30}, ui.u(1.f));
    ui::Box b; b.radius = ui.u(1.f); b.fill = ui.th.accent; b.fill2 = ui.th.rose; b.grad = 1;
    b.shadow = ui.u(8.f); b.shadowCol = ui::withA(ui.th.accent, 0.4f);
    ui.box(Rectangle{x, y, std::max(ui.u(2.f), w * shown), ui.u(2.f)}, b);
    if (what && *what) ui.text(ui::Face::SerifItalic, ui.u(22.f), what, ui.W * 0.5f, y + ui.u(26.f), ui.th.inkFaint, ui::Center);
    // a slow shimmer along the line: alive even while one step takes a while
    float sx = x + w * std::fmod((float)GetTime() * 0.45f, 1.f);
    ui.glow(sx, y + ui.u(1.f), ui.u(26.f), ui::withA(ui.th.accent, 0.35f));
    ui.end();
    EndDrawing();
}

void Game::drawDebug() {
    char b[256];
    std::snprintf(b, sizeof b, "%d fps  %.2f ms  draws %d  tris %ldk  RAM %.0f MB  VRAM~%.0f MB  %s",
                  GetFPS(), (double)GetFrameTime() * 1000.0, renderer.drawCalls, renderer.triangles / 1000,
                  (double)platform::residentBytes() / 1048576.0, (double)(textureBytes() + Model::bufferBytes()) / 1048576.0,
                  Updater::current().c_str());
    ui.alpha = 1.f;
    ui.fillRect(Rectangle{ui.u(16.f), ui.H - ui.u(44.f), ui.measure(ui::Face::Sans, ui.u(15.f), b).x + ui.u(24.f), ui.u(30.f)}, Color{0, 0, 0, 150}, ui.u(8.f));
    ui.text(ui::Face::Sans, ui.u(15.f), b, ui.u(28.f), ui.H - ui.u(37.f), ui.th.ink);
}

// ------------------------------------------------------------------ harness
void Game::reset() {
    profile = Profile{};
    settings = Settings{};
    settings.checkUpdates = false;
    applyQuality(renderer.settings, Quality::High);
    hudHidden_ = barOpen_ = false;
    debug_ = false;
    custTab_ = setTab_ = 0;
    showcaseWho_ = 0;
    flash_ = climaxGlow_ = 0.f;
    subtitle_.clear(); subtitleT_ = 0.f;
    toasts_.clear();
    fade_ = 0.f; fadeDir_ = 0.f; switching_ = false; posFade_ = 0.f; pendingPos_ = -1;
    rub_ = rubTarget_ = embrace_ = embraceTarget_ = 0.f;
    rhythm_ = rhythmToggle_ = false;
    camUser_ = false;
    holdDepth_ = false;
    autoplay_ = false;
    qaNoActors_ = false;
    qaStress_.clear(); qaFocus_.clear(); qaNoUi_ = false; qaKiss_ = false; qaOral_ = false; oralOpen_ = 2.2f; qaHideMarcus_ = false; qaHideGenitals_ = false; oralWristL_ = 0.f; oralWristR_ = -1.5708f; oralCurl_ = 0.35f; oralGrip_ = true; oralUp_ = 0.012f; qaOrbit_ = false;
    Animator::skinMode = 1;
    JointLimits::enabled = true;
    JointLimits::log = false;
}

void Game::applySpec(const Spec& s) {
    // looks
    for (auto& kv : Spec::pairs(s.get("body"))) {
        HerLook& h = profile.her;
        if (kv.first == "bust") h.bust = kv.second; else if (kv.first == "hips") h.hips = kv.second;
        else if (kv.first == "waist") h.waist = kv.second; else if (kv.first == "butt") h.butt = kv.second;
        else if (kv.first == "weight") h.weight = kv.second; else if (kv.first == "muscle") h.muscle = kv.second;
        else if (kv.first == "belly") h.belly = kv.second;
    }
    for (auto& kv : Spec::pairs(s.get("mbody"))) {
        HisLook& h = profile.him;
        if (kv.first == "muscle") h.muscle = kv.second; else if (kv.first == "weight") h.weight = kv.second;
        else if (kv.first == "belly") h.belly = kv.second; else if (kv.first == "length") h.length = kv.second;
        else if (kv.first == "girth") h.girth = kv.second;
    }
    if (skinToneIndex(s.get("tone")) >= 0) profile.her.tone = skinToneIndex(s.get("tone"));
    if (skinToneIndex(s.get("mtone")) >= 0) profile.him.tone = skinToneIndex(s.get("mtone"));
    if (s.has("age")) profile.her.age = std::max(kMinAge, s.num("age", 42.f));
    if (s.has("mage")) profile.him.age = std::max(kMinAge, s.num("mage", 40.f));
    if (s.has("hair")) profile.her.hair = (int)s.num("hair", 0.f);
    // models (characters/roster.json): hermodel=<id>, himmodel=<id>; story shots keep the stories' own cast
    if (s.has("hermodel")) pickModel(0, s.get("hermodel"), !s.has("tone"), !s.has("hair"));
    if (s.has("himmodel")) pickModel(1, s.get("himmodel"), !s.has("mtone"), false);
    ensureCast(s.get("screen") == "story");
    std::string q = s.get("quality");
    if (q == "low") settings.quality = 0; else if (q == "medium") settings.quality = 1; else if (q == "high") settings.quality = 2;
    applyQuality(renderer.settings, (Quality)settings.quality);
    if (s.has("debug")) debug_ = s.num("debug", 1.f) > 0.5f;
    qaNoActors_ = s.has("actors") && s.num("actors", 1.f) < 0.5f;
    renderer.settings.hairDebug = s.has("hairdbg") && s.num("hairdbg", 1.f) > 0.5f;   // QA: soft hair vs strand cores
    renderer.settings.skinDebug = (int)s.num("skindbg", 0.f);                           // QA: skin flush / shadow
    qaStress_ = s.get("stress");
    Animator::skinMode = s.get("skin") == "lbs" ? 0 : 1;   // QA: linear blend skinning for comparisons
    JointLimits::enabled = !(s.has("rom") && s.num("rom", 1.f) < 0.5f);   // QA: rom=0, no joint ranges
    JointLimits::log = s.has("romlog");
    qaFocus_ = s.get("focus");
    qaNoUi_ = s.has("ui") && s.num("ui", 1.f) < 0.5f;
    {
        auto v = Spec::pairs(s.get("view"));   // yaw:deg,pitch:deg,dist:m
        qaYaw_ = 0.f; qaPitch_ = 0.1f; qaDist_ = 0.9f;
        for (auto& kv : v) {
            if (kv.first == "yaw") qaYaw_ = kv.second * kPi / 180.f;
            else if (kv.first == "pitch") qaPitch_ = kv.second * kPi / 180.f;
            else if (kv.first == "dist") qaDist_ = kv.second;
        }
    }
    qaKiss_ = s.has("kissqa");
    qaOral_ = s.has("oralqa");
    oralOpen_ = s.num("oralopen", 2.2f);
    qaHideMarcus_ = s.has("hidehim") && s.num("hidehim", 1.f) > 0.5f;
    qaHideGenitals_ = s.has("hidegenitals") && s.num("hidegenitals", 1.f) > 0.5f;
    oralWristL_ = s.num("oralwristl", 0.f);
    oralWristR_ = s.num("oralwristr", -1.5708f);
    oralCurl_ = s.num("oralcurl", 0.35f);
    oralGrip_ = s.num("oralgrip", 1.f) > 0.5f;
    oralUp_ = s.num("oralup", 0.012f);
    qaOrbit_ = s.has("orbit");
    if (qaOrbit_) {
        qaOYaw_ = 0.f; qaOPitch_ = 0.f; qaODist_ = 1.2f;
        for (auto& kv : Spec::pairs(s.get("orbit"))) {
            if (kv.first == "yaw") qaOYaw_ = kv.second * kPi / 180.f;
            else if (kv.first == "pitch") qaOPitch_ = kv.second * kPi / 180.f;
            else if (kv.first == "dist") qaODist_ = kv.second;
        }
    }
    if (s.has("hints")) settings.hints = (int)s.num("hints", 2.f);
    std::string mood = s.get("mood");
    settings.lightMood = mood == "candle" ? 1 : (mood == "day" ? 2 : 0);
    settings.brightness = s.num("bright", 0.f);
    settings.warmth = s.num("warm", 0.f);
    applyLooks();
    if (s.has("update") && updater) updater->checkAsync("file://fake");   // QA: the card's "checking" look

    std::string scr = s.get("screen", "title");
    Screen target = Screen::Title;
    if (scr == "menu") target = Screen::Menu;
    else if (scr == "customize") target = Screen::Customize;
    else if (scr == "settings") target = Screen::Settings;
    else if (scr == "play") target = Screen::Play;
    else if (scr == "pause") target = Screen::Pause;
    else if (scr == "results") target = Screen::Results;
    else if (scr == "stories") target = Screen::Stories;
    else if (scr == "story") target = Screen::Story;
    custTab_ = s.get("tab") == "him" ? 1 : 0;
    showcaseWho_ = custTab_;
    setTab_ = (int)s.num("settab", 0.f);

    if (target == Screen::Play || target == Screen::Pause || target == Screen::Results) {
        std::string pos = s.get("pos", "missionary");
        startNight(false);
        for (size_t i = 0; i < positions_.size(); ++i) if (positions_[i].id == pos) setPosition((int)i, true);
        night_.her = s.num("her", night_.her);
        night_.him = s.num("him", night_.him);
        night_.connection = s.num("connection", night_.connection);
        night_.elapsed = s.num("elapsed", 140.f);
        night_.score = s.num("score", 1840.f);
        introCard_ = s.has("intro") ? 8.f : 0.f;
        hudT_ = hintsT_ = 1.f;   // stills: the HUD has already faded in
        autoplay_ = s.has("autoplay");
        if (autoplay_) { night_.her = 0.f; night_.him = 0.f; night_.elapsed = 0.f; night_.score = 0.f; night_.connection = 0.5f; autoLog_ = 0.f; }
        depth_ = depthTarget_ = dragDepth_ = s.num("depth", 0.6f);
        holdDepth_ = true;
        if (s.has("rhythm")) { rhythm_ = true; rhythmToggle_ = true; rhythmTempo_ = s.num("tempo", 1.1f); }
        rubTarget_ = rub_ = s.num("rub", 0.f);
        embraceTarget_ = embrace_ = s.num("embrace", 0.f);
        std::string d = s.get("desire");
        const char* dn[] = {"", "faster", "slower", "deeper", "gentler", "touch", "kiss", "change"};
        for (int i = 1; i < 8; ++i) if (d == dn[i]) {
            night_.desire = (Desire)i; night_.desireAge = 1.f; night_.desireTimeout = 30.f; night_.nextDesire = 99.f;
            subtitle_ = Night::desireLine((Desire)i, true); subtitleT_ = 0.6f; subtitleDur_ = 30.f;
            shownDesire_ = (Desire)i;
        }
        if (d.empty() && !s.has("autoplay")) night_.nextDesire = 999.f;
        std::string cm = s.get("cam", "side");
        camMode_ = cm == "front" ? 1 : (cm == "top" ? 2 : (cm == "fpv" ? 3 : (cm == "face" ? 4 : (cm == "touch" ? 5 : 0))));
        cameraForPosition(0.f);
        camUser_ = true;   // no cinematic cuts in a still
        if (s.has("hud")) hudHidden_ = s.num("hud", 1.f) < 0.5f;
        if (s.has("bar")) barOpen_ = true;
        std::string cl = s.get("climax");
        if (!cl.empty()) {
            flash_ = 0.55f; climaxGlow_ = 1.f;
            if (cl == "her" || cl == "together") { herClimaxT_ = 0.4f; night_.herClimaxAge = 0.4f; }
            if (cl == "him" || cl == "together") hisClimaxT_ = 0.4f;
        }
        if (target == Screen::Results) {
            night_.together = s.get("result", "together") == "together";
            night_.herClimaxes = night_.together ? 2 : 0;
            night_.desiresMet = 9; night_.desiresMissed = 2; night_.bestStreak = 5; night_.connection = 0.86f;
            endNight(true);
        }
    }
    if (target == Screen::Story) {
        const Story* st = storyById(s.get("story"));
        if (!st && !stories_.empty()) st = &stories_[0];
        if (st) {
            std::string nodeId = s.get("node", st->chapters[0].start);
            stage(Stage::Night);
            enterStoryWorld();
            story_.start(st, nodeId, nullptr, (int)s.num("line", 0.f));
            story_.autoMode = false;
            storyPlaying_ = false; storyOverlay_ = (int)s.num("overlay", 0.f);
            storyCardT_ = s.has("card") ? 0.8f : 99.f;
            story_.nodeChanged = true;
            black_ = 0.f; blackDir_ = 0;
            updateStory(0.f);   // stage the node and start its line now
            for (int k = 0; k < (int)s.num("reveal", 999.f); ++k) story_.update(1.f / 30.f);
            // stills: the voice clock held mid-line (lip sync), a kiss, an undress half done, a forced shot
            voiceHold_ = s.has("vt") ? s.num("vt", 0.f) : -1.f;
            if (s.has("kiss")) { kiss_ = kissTarget_ = near_ = nearTarget_ = s.num("kiss", 1.f); }
            if (s.has("near")) { near_ = nearTarget_ = s.num("near", 1.f); }   // an embrace without the kiss
            std::string und = s.get("undress");
            if (!und.empty()) {
                size_t c = und.find(':');
                std::string who = und.substr(0, c);
                float vis = c == std::string::npos ? 0.5f : (float)std::atof(und.c_str() + c + 1);
                Character& ch = who == "marcus" ? scene.marcus : scene.vivian;
                if (Outfit* o = ch.outfit()) { o->hold(0, vis); }
            }
            std::string sh = s.get("shot");
            if (!sh.empty()) { StoryLine fl = story_.line() ? *story_.line() : StoryLine{}; fl.cam = sh; storyShot(&fl, false); }
            if (s.has("pose")) setStoryPose(s.get("pose"), s.num("actT", 0.3f));
            // the hand-over to play (the node must be a play node): autoplay drives the night
            if (s.has("storyplay") && story_.node() && !story_.node()->playGoal.empty()) {
                storyStartPlay();
                autoplay_ = true;
                night_.nextDesire = 4.f;
            }
        }
    }
    screen_ = target;
    enterScreen(target);
    qaPortrait_ = 0.f;
    if ((target == Screen::Play || target == Screen::Pause) && s.has("dress")) {
        // QA: both in story outfits in act stills (dress=<her style>,<his style>; layer=under: underwear only), so
        // every act can be checked from every side without nudity
        std::string d = s.get("dress");
        size_t comma = d.find(',');
        std::string st[2] = {d.substr(0, comma), comma == std::string::npos ? std::string() : d.substr(comma + 1)};
        for (int w = 0; w < 2; ++w) {
            Character& c = w ? scene.marcus : scene.vivian;
            if (!c.loaded() || st[w].empty()) continue;
            c.setOutfit(st[w]);
            if (Outfit* o = c.outfit()) {
                o->setDress(Dress::Dressed, 0.f);
                if (s.get("layer") == "under") o->hold(0, 0.f);
            }
        }
    }
    if (target == Screen::Customize) {
        showcaseWho_ = custTab_; stage(Stage::Showcase);
        // QA stills of the models: turntable angle, a head-and-shoulders camera, an outfit, held expressions
        if (s.has("turn")) turntable_ = s.num("turn", 0.25f);
        if (s.has("zoom")) showcaseZoom_ = std::max(0.f, std::min(1.f, s.num("zoom", 0.f)));
        qaPortrait_ = s.num("portrait", 0.f);
        Character& c = showcaseWho_ == 0 ? scene.vivian : scene.marcus;
        // a story outfit (layer=under: the outer layer off), none unless asked for
        std::string of = s.get("outfit");
        if (c.loaded()) c.setOutfit(of);
        if (!of.empty()) if (Outfit* o = c.outfit()) {
            o->setDress(Dress::Dressed, 0.f);
            if (s.get("layer") == "under") o->hold(0, 0.f);
        }
        for (auto& kv : Spec::pairs(s.get("face"))) c.setMorph(kv.first, kv.second);
        if (s.has("clip") && c.loaded() && c.hasClip(s.get("clip"))) {   // QA: an authored pose, held
            c.poseHook = nullptr;
            c.setLayer(0, s.get("clip"), s.num("clipT", 0.5f), 1.f);
            c.proc.lookAt = false;
            c.proc.breathAmp = 0.f;
        }
        if (!qaStress_.empty() && c.loaded()) {   // QA: a stress pose (anim/stress.cpp)
            std::string pose = qaStress_;
            c.poseHook = [pose](Animator& a) { stressPose(a, pose); };
            c.proc.lookAt = false;
            c.proc.breathAmp = 0.f;
        }
    }
    fade_ = 0.f; fadeDir_ = 0.f; switching_ = false;
    screenTime_ = s.num("t", 3.f);   // past the entrance animations
    introT_ = 10.f;
    for (Character* c : {&scene.vivian, &scene.marcus}) c->resetSimulation();
}

}  // namespace ag
