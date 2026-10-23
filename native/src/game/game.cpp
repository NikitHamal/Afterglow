#include "game/game.h"
#include "app/platform.h"
#include "app/updater.h"
#include "core/log.h"
#include "core/paths.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ag {

namespace {
const char* kToneNames[3] = {"honey", "caramel", "deep"};
}

// ------------------------------------------------------------------ lifecycle
bool Game::init() {
    settingsPath_ = paths::join(paths::userDir(), "settings.json");
    profilePath_ = paths::join(paths::userDir(), "profile.json");
    if (!harness) { settings.load(settingsPath_); profile.load(profilePath_); }
    ui.init();
    if (!loadingProgress) loadingProgress = [this](float f, const char* what) { drawLoading(f, what); };
    loadingProgress(0.f, "Opening the door");
    if (!renderer.init()) return false;
    applyQuality(renderer.settings, (Quality)settings.quality);
    if (!scene.load(loadingProgress)) AG_WARN("AG: scene loaded without characters");
    loadingProgress(1.f, "");
    std::string meta = paths::findAsset({"characters/clips.json"});
    if (!meta.empty()) clipsMeta_ = Json::parse(paths::readText(meta));

    // positions the night can use (only those the content ships clips for)
    struct Def { const char* id; const char* name; float her, him; bool pen; };
    const Def defs[] = {
        {"missionary", "Missionary", 1.0f, 1.0f, true}, {"legsup", "Legs Raised", 1.1f, 1.1f, true},
        {"doggy", "From Behind", 1.05f, 1.2f, true},    {"prone", "Lying Flat", 1.0f, 1.15f, true},
        {"cowgirl", "On Top", 1.15f, 0.95f, true},      {"revcowgirl", "Reverse", 1.0f, 1.1f, true},
        {"spoon", "Spooning", 0.95f, 0.9f, true},       {"oral", "Her Lips", 0.3f, 1.25f, false},
        {"lick", "His Tongue", 1.25f, 0.2f, false},      {"solo", "Alone", 1.0f, 0.f, false},
    };
    const Character& src = scene.vivian.loaded() ? scene.vivian : scene.marcus;
    for (const Def& d : defs) {
        std::string id = d.id;
        bool solo = id == "solo";
        if (!src.hasClip(solo ? "solo.idle" : id + ".in")) continue;
        PositionInfo p;
        p.id = id; p.name = d.name; p.her = d.her; p.him = d.him; p.penetrative = d.pen;
        p.solo = solo;
        p.canTouch = !solo && src.hasClip(id + ".rub");
        p.canKiss = src.hasClip(id + ".embrace");
        positions_.push_back(p);
    }
    if (positions_.empty()) { PositionInfo p; p.id = "none"; p.name = "\xe2\x80\x94"; positions_.push_back(p); }
    sceneReady_ = true;
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
    AG_INFO("AG: game ready: %d positions, RSS %.0f MB", (int)positions_.size(), (double)platform::residentBytes() / 1048576.0);
    return true;
}

void Game::saveAll() {
    if (harness) return;
    settings.save(settingsPath_);
    profile.save(profilePath_);
}

void Game::shutdown() {
    if (screen_ == Screen::Play || screen_ == Screen::Pause) profile.night = night_.snapshot();
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
    screenTime_ = 0.f;
    ui.resetFocus();
    confirmQuit_ = confirmNew_ = false;
    renderer.uiBlur = s != Screen::Play;
    switch (s) {
        case Screen::Title: case Screen::Menu:
            if (stage_ != Stage::Bedroom) stage(Stage::Bedroom);
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
        c->placement = M4::identity();
        c->proc.hair = settings.hairPhysics;
        c->proc.lookAt = true;
        c->proc.lookWeight = 0.6f;
        c->proc.eyeRollUp = 0.f;
        c->flush = 0.f; c->sweat = 0.f;
        c->resetSimulation();
    }
    // lights back to the room defaults (the showcase moves the key)
    Lights base = scene.lights;
    base.keyPos = scene.info.lampPos;
    base.keyDir = normalize(scene.info.bedCenter + V3{0.f, 0.25f, 0.2f} - scene.info.lampPos);
    base.keyIntensity = 9.f; base.keyInner = 0.55f; base.keyOuter = 1.15f;
    base.fillIntensity = 0.42f;
    scene.lights = base;
    scene.dust = true;
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
            // studio key from the front left, softer room
            scene.lights.keyPos = {-1.5f, 2.3f, 2.4f};
            scene.lights.keyDir = normalize(V3{0.f, 1.1f, 0.f} - scene.lights.keyPos);
            scene.lights.keyInner = 0.35f; scene.lights.keyOuter = 0.75f; scene.lights.keyIntensity = 26.f;
            scene.lights.fillIntensity = 0.9f;
            scene.dust = false;
            turntable_ = 0.25f; turnVel_ = 0.f; showcaseZoom_ = 0.f;
            break;
        }
        case Stage::Night:
            scene.showVivian = v.loaded(); scene.showMarcus = m.loaded();
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
    static const V3 kHair[6] = {{1.f, 1.f, 1.f}, {2.6f, 1.9f, 1.55f}, {3.4f, 2.0f, 1.2f}, {4.6f, 1.9f, 1.25f}, {0.55f, 0.52f, 0.55f}, {7.5f, 5.2f, 3.2f}};
    Character& v = scene.vivian;
    Character& m = scene.marcus;
    if (v.loaded()) {
        const HerLook& h = profile.her;
        v.setBody("bust", h.bust); v.setBody("hips", h.hips); v.setBody("waist", h.waist); v.setBody("butt", h.butt);
        v.setBody("weight", h.weight); v.setBody("muscle", h.muscle); v.setBody("belly", h.belly);
        v.setTone(kToneNames[std::max(0, std::min(2, h.tone))]);
        bool legacyHair = false;
        for (const Material& mat : v.model.materials)
            if (mat.shading == Shading::Hair && mat.baseColor.x > 0.9f) legacyHair = true;
        V3 tint = kHair[std::max(0, std::min(5, h.hair))];
        v.hairTint = legacyHair ? V3{0.42f * tint.x, 0.21f * tint.y, 0.11f * tint.z} : tint;
    }
    if (m.loaded()) {
        const HisLook& h = profile.him;
        m.setBody("muscle", h.muscle); m.setBody("weight", h.weight); m.setBody("belly", h.belly);
        m.setTone(kToneNames[std::max(0, std::min(2, h.tone))]);
    }
}

void Game::updateMenuCamera(float dt) {
    if (stage_ == Stage::Bedroom) {
        float t = time_ * 0.045f;
        V3 tgt = scene.info.bedCenter + V3{0.05f * std::sin(t * 1.7f), 0.1f, 0.12f};
        float yaw = 1.0f + 0.32f * std::sin(t), pitch = 0.3f + 0.05f * std::sin(t * 1.3f), dist = 2.75f + 0.25f * std::sin(t * 0.8f);
        camera.setOrbit(tgt, yaw, pitch, dist, 0.f);
    } else if (stage_ == Stage::Showcase) {
        Character& c = showcaseWho_ == 0 ? scene.vivian : scene.marcus;
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

    if (screen_ == Screen::Play || screen_ == Screen::Pause || screen_ == Screen::Results ||
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
    rs.renderScale = settings.renderScale * (settings.quality == 0 ? 0.85f : 1.f);
    rs.bloom = settings.bloom;
    rs.ssao = settings.ssao && settings.quality > 0;
    rs.grain = settings.filmGrain ? 0.024f : 0.f;
    renderer.render(scene, cam_, w, h, time_);
    ui.blurTex = renderer.blurTexture();
    ui.blurW = renderer.blurWidth(); ui.blurH = renderer.blurHeight();

    ui.alpha = 1.f;
    switch (screen_) {
        case Screen::Title: drawTitle(); break;
        case Screen::Menu: drawMenu(); break;
        case Screen::Customize: drawCustomize(); break;
        case Screen::Settings: drawSettings(); break;
        case Screen::Play: drawHud(); break;
        case Screen::Pause: drawHud(); drawPause(); break;
        case Screen::Results: drawResults(); break;
    }
    if (debug_) drawDebug();
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
    ui.end();
    EndDrawing();
}

void Game::drawDebug() {
    char b[256];
    std::snprintf(b, sizeof b, "%d fps  %.2f ms  draws %d  tris %ldk  RSS %.0f MB  %s",
                  GetFPS(), (double)GetFrameTime() * 1000.0, renderer.drawCalls, renderer.triangles / 1000,
                  (double)platform::residentBytes() / 1048576.0, Updater::current().c_str());
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
    for (int i = 0; i < 3; ++i) {
        if (s.get("tone") == kToneNames[i]) profile.her.tone = i;
        if (s.get("mtone") == kToneNames[i]) profile.him.tone = i;
    }
    if (s.has("hair")) profile.her.hair = (int)s.num("hair", 0.f);
    std::string q = s.get("quality");
    if (q == "low") settings.quality = 0; else if (q == "medium") settings.quality = 1; else if (q == "high") settings.quality = 2;
    applyQuality(renderer.settings, (Quality)settings.quality);
    if (s.has("debug")) debug_ = s.num("debug", 1.f) > 0.5f;
    if (s.has("hints")) settings.hints = (int)s.num("hints", 2.f);
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
        if (d.empty()) night_.nextDesire = 999.f;
        std::string cm = s.get("cam", "side");
        camMode_ = cm == "front" ? 1 : (cm == "top" ? 2 : (cm == "fpv" ? 3 : 0));
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
    screen_ = target;
    enterScreen(target);
    if (target == Screen::Customize) { showcaseWho_ = custTab_; stage(Stage::Showcase); }
    fade_ = 0.f; fadeDir_ = 0.f; switching_ = false;
    screenTime_ = s.num("t", 3.f);   // past the entrance animations
    introT_ = 10.f;
    for (Character* c : {&scene.vivian, &scene.marcus}) c->resetSimulation();
}

}  // namespace ag
