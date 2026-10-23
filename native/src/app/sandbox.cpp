#include "app/sandbox.h"
#include "core/paths.h"
#include "core/log.h"
#include "raylib.h"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wenum-compare"
#endif
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <algorithm>
#include <cstdio>

namespace ag {

static const char* kTones[3] = {"honey", "caramel", "deep"};
static const std::vector<std::string> kFaceF = {"blink", "smile", "moan", "pleasure", "kiss", "oral"};
static const std::vector<std::string> kFaceM = {"blink", "effort", "pleasure", "moan", "kiss"};
static const std::vector<std::string> kBodyF = {"bust", "hips", "waist", "butt", "weight", "muscle", "belly"};
static const std::vector<std::string> kBodyM = {"muscle", "weight", "belly", "length", "girth"};

bool Sandbox::init() {
    if (!renderer.init()) return false;
    if (!scene.load()) AG_WARN("AG: scene loaded without characters");
    if (enableAudio && audio.init()) audio.loadAll();
    std::string meta = paths::findAsset({"characters/clips.json"});
    if (!meta.empty()) clipsMeta_ = Json::parse(paths::readText(meta));
    // positions = "<pos>.in" clips (+ solo)
    const Character& src = scene.vivian.loaded() ? scene.vivian : scene.marcus;
    for (const std::string& c : src.clipNames()) {
        if (c.size() > 3 && c.compare(c.size() - 3, 3, ".in") == 0) positions_.push_back(c.substr(0, c.size() - 3));
    }
    if (src.hasClip("solo.rub") || src.hasClip("solo.idle")) positions_.push_back("solo");
    for (auto& f : kFaceF) face_[f] = 0.f;
    for (auto& f : kFaceM) mface_[f] = 0.f;
    for (auto& b : kBodyF) body_[b] = 0.f;
    for (auto& b : kBodyM) mbody_[b] = 0.f;
    applyQuality(renderer.settings, Quality::High);
    GuiLoadStyleDefault();
    GuiSetStyle(DEFAULT, TEXT_SIZE, 14);
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, ColorToInt(Color{18, 12, 16, 230}));
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, ColorToInt(Color{40, 26, 34, 255}));
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, ColorToInt(Color{110, 70, 85, 255}));
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, ColorToInt(Color{235, 215, 222, 255}));
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, ColorToInt(Color{90, 40, 60, 255}));
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, ColorToInt(Color{240, 120, 150, 255}));
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, ColorToInt(Color{255, 235, 240, 255}));
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, ColorToInt(Color{200, 80, 115, 255}));
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, ColorToInt(Color{255, 255, 255, 255}));
    GuiSetStyle(DEFAULT, LINE_COLOR, ColorToInt(Color{160, 90, 110, 255}));
    cameraPreset("front", 0.f);
    return true;
}

void Sandbox::shutdown() {
    audio.shutdown();
    renderer.shutdown();
    scene.destroy();
}

void Sandbox::reset() {
    depth_ = 0.55f; rub_ = 0.f; embrace_ = 0.f; autoThrust_ = false; aroused_ = 1.f;
    for (auto& kv : face_) kv.second = 0.f;
    for (auto& kv : mface_) kv.second = 0.f;
    for (auto& kv : body_) kv.second = 0.f;
    for (auto& kv : mbody_) kv.second = 0.f;
    tone_ = mtone_ = 1;
    scene.showVivian = scene.showMarcus = true;
    scene.vivian.flush = scene.vivian.sweat = scene.marcus.flush = scene.marcus.sweat = 0.f;
    flex_ = 0.f;
    forceChars_ = false;
    hairSim_ = breathing_ = lookAt_ = true;
    applyQuality(renderer.settings, Quality::High);
    posIndex_ = 0;
    camPreset_ = "front";
    camPending_ = camFollow_ = false;
    overlay = false;   // harness shots are clean unless the spec asks for debug=1
}

V3 Sandbox::focusPoint() const {
    if (!scene.hasClips) return V3{0.f, 1.05f, 1.72f};
    V3 a = scene.vivian.loaded() ? scene.vivian.bonePos("下半身") : scene.info.bedCenter;
    V3 b = scene.marcus.loaded() && scene.showMarcus ? scene.marcus.bonePos("下半身") : a;
    V3 c = (a + b) * 0.5f;
    return c + V3{0.f, 0.12f, 0.f};
}

// clips.json camera yaw is measured from the head (-Z) side; the engine orbit yaw from +Z (foot side)
static constexpr float kContentYawOffsetDeg = 180.f;

const Json& Sandbox::clipMeta(const std::string& clip) const { return clipsMeta_["clips"][clip]; }

void Sandbox::cameraPreset(const std::string& name, float blend) {
    // applied after the next pose update, so bone-relative shots see the new pose
    camPreset_ = name;
    camBlend_ = blend;
    camPending_ = true;
}

// Bone-relative shots. "close": the face from the front-side, on the side away from the partner;
// "portrait": the whole subject from its front three-quarter.
bool Sandbox::boneShot(const std::string& name, V3& target, float& yaw, float& pitch, float& dist) const {
    if (name != "close" && name != "portrait") return false;
    const Character* c = scene.showVivian && scene.vivian.loaded() ? &scene.vivian : (scene.marcus.loaded() ? &scene.marcus : nullptr);
    if (!c || c->bone("頭") < 0) return false;
    const Character* other = c == &scene.vivian ? &scene.marcus : &scene.vivian;
    bool otherShown = other->loaded() && (other == &scene.marcus ? scene.showMarcus : scene.showVivian);
    V3 center, fwd, up;
    c->headAxes(center, fwd, up);
    V3 side = normalize(cross(up, fwd));
    V3 d;
    if (name == "close") {
        target = center + fwd * 0.03f - up * 0.03f;
        V3 ref = otherShown ? center - other->bonePos("上半身2") : cam_.eye - center;
        if (dot(side, ref) < 0.f) side = side * -1.f;
        d = normalize(fwd * 0.8f - up * 0.35f + side * 0.35f + V3{0, 0.3f, 0});
        dist = 0.62f;
    } else {
        V3 hips = c->bonePos("下半身");
        target = (center + hips) * 0.5f - V3{0, 0.1f, 0};
        V3 h{fwd.x, 0.f, fwd.z};
        if (length(h) < 0.35f) h = V3{side.x, 0.f, side.z};   // lying face-up: from the side
        if (length(h) < 1e-3f) h = V3{0, 0, 1};
        h = qrotate(qaxis({0, 1, 0}, 0.35f), normalize(h));
        d = normalize(h + V3{0, 0.2f, 0});
        dist = 3.0f;
    }
    if (d.y < 0.2f) d = normalize(V3{d.x, 0.2f, d.z});
    float minY = scene.info.bedTop + 0.12f;   // never under the mattress
    if (target.y + d.y * dist < minY) d = normalize(V3{d.x, (minY - target.y) / dist, d.z});
    yaw = std::atan2(d.x, d.z);
    pitch = std::min(std::asin(clampf(d.y, -1.f, 1.f)), 1.35f);
    return true;
}

void Sandbox::applyCameraPreset() {
    camPending_ = false;
    camFollow_ = false;
    const std::string name = camPreset_;
    const float blend = camBlend_;
    std::string pos = currentPos();
    std::string clip = pos == "solo" ? "solo.idle" : pos + ".in";
    const Json& cams = clipMeta(clip)["cameras"];
    if (name == "fpv" && scene.marcus.loaded()) {
        const Json& f = cams["fpv"];
        if (f.isObject() && f["offset_local"].isArray()) {
            camera.setFirstPersonLocal(&scene.marcus, f["bone"].str("頭"), {f["offset_local"][0].f(), f["offset_local"][1].f(), f["offset_local"][2].f()},
                                       {f["forward_local"][0].f(), f["forward_local"][1].f(), f["forward_local"][2].f(1.f)}, blend);
        } else {
            camera.setFirstPerson(&scene.marcus, "頭", {0.f, 0.07f, 0.09f}, blend);
        }
        return;
    }
    const Json& c = cams["orbit_" + name];
    if (c.isObject() && c["target"].isArray()) {
        const float d2r = kPi / 180.f;
        float dist = c["distance"].isNumber() ? c["distance"].f() : c["dist"].f(2.4f);
        camera.setOrbit({c["target"][0].f(), c["target"][1].f(), c["target"][2].f()},
                        (c["yaw"].f() + kContentYawOffsetDeg) * d2r, c["pitch"].f(20.f) * d2r, dist, blend);
        return;
    }
    V3 t; float yaw, pitch, dist;
    if (boneShot(name, t, yaw, pitch, dist)) {
        camera.setOrbit(t, yaw, pitch, dist, blend);
        camFollow_ = name == "close";
        return;
    }
    V3 f = focusPoint();
    if (name == "side") camera.setOrbit(f, 1.45f, 0.22f, scene.hasClips ? 2.3f : 3.0f, blend);
    else if (name == "top") camera.setOrbit(f, 0.2f, 1.18f, 2.3f, blend);
    else if (name == "wide") camera.setOrbit(f, 0.55f, 0.36f, 4.3f, blend);
    else camera.setOrbit(f, 0.32f, 0.26f, scene.hasClips ? 2.5f : 3.1f, blend);
}

// clips.json morph rules: numbers, or "smoothstep(a, b, depth)"; anything else is left to gameplay
static float evalMorphRule(const Json& rule, float depth, bool& ok) {
    ok = true;
    if (rule.isNumber()) return rule.f();
    if (rule.isString() && rule.s.rfind("smoothstep(", 0) == 0) {
        float a = 0.f, b = 1.f;
        if (std::sscanf(rule.s.c_str(), "smoothstep(%f , %f", &a, &b) == 2 || std::sscanf(rule.s.c_str(), "smoothstep(%f,%f", &a, &b) == 2)
            return smoothstep(a, b, depth);
    }
    ok = false;
    return 0.f;
}

void Sandbox::applySpec(const Spec& s) {
    if (s.has("pos")) {
        std::string p = s.get("pos");
        for (size_t i = 0; i < positions_.size(); ++i) if (positions_[i] == p) posIndex_ = (int)i;
    }
    depth_ = s.num("depth", depth_);
    rub_ = s.num("rub", rub_);
    embrace_ = s.num("embrace", embrace_);
    aroused_ = s.num("aroused", aroused_);
    autoThrust_ = s.num("auto", autoThrust_ ? 1.f : 0.f) > 0.5f;
    for (auto& kv : Spec::pairs(s.get("face"))) face_[kv.first] = kv.second;
    for (auto& kv : Spec::pairs(s.get("mface"))) mface_[kv.first] = kv.second;
    for (auto& kv : Spec::pairs(s.get("body"))) body_[kv.first] = kv.second;
    for (auto& kv : Spec::pairs(s.get("mbody"))) mbody_[kv.first] = kv.second;
    for (int i = 0; i < 3; ++i) {
        if (s.get("tone") == kTones[i]) tone_ = i;
        if (s.get("mtone") == kTones[i]) mtone_ = i;
    }
    std::string q = s.get("quality");
    if (q == "low") applyQuality(renderer.settings, Quality::Low);
    else if (q == "medium") applyQuality(renderer.settings, Quality::Medium);
    else if (q == "high") applyQuality(renderer.settings, Quality::High);
    if (s.has("debug")) overlay = s.num("debug", 1.f) > 0.5f;
    std::string ch = s.get("char");
    forceChars_ = !ch.empty();
    if (ch == "vivian") { scene.showVivian = true; scene.showMarcus = false; }
    else if (ch == "marcus") { scene.showVivian = false; scene.showMarcus = true; }
    else if (ch == "both") scene.showVivian = scene.showMarcus = true;
    if (s.has("hair")) hairSim_ = s.num("hair", 1.f) > 0.5f;
    if (s.has("exposure")) renderer.settings.exposure = s.num("exposure", 1.f);
    if (s.has("flush")) scene.vivian.flush = s.num("flush", 0.f);
    if (s.has("sweat")) scene.vivian.sweat = scene.marcus.sweat = s.num("sweat", 0.f);
    flex_ = s.num("flex", flex_);
    applyPose();
    scene.update(0.f);   // settle poses so camera focus uses them
    std::string cam = s.get("cam", "front");
    if (cam.rfind("orbit:", 0) == 0) {
        std::vector<std::string> v = Spec::split(cam.substr(6), ',');
        V3 f = focusPoint();
        if (v.size() >= 6) f = {(float)std::atof(v[3].c_str()), (float)std::atof(v[4].c_str()), (float)std::atof(v[5].c_str())};
        camera.setOrbit(f, v.size() > 0 ? (float)std::atof(v[0].c_str()) : 0.3f, v.size() > 1 ? (float)std::atof(v[1].c_str()) : 0.25f,
                        v.size() > 2 ? (float)std::atof(v[2].c_str()) : 2.5f, 0.f);
        camPending_ = camFollow_ = false;
        camPreset_ = "orbit";
    } else {
        cameraPreset(cam, 0.f);
    }
    for (Character* c : {&scene.vivian, &scene.marcus}) c->resetSimulation();
}

void Sandbox::applyPose() {
    Character& v = scene.vivian;
    Character& m = scene.marcus;
    std::string pos = currentPos();
    for (Character* c : {&v, &m}) {
        if (!c->loaded()) continue;
        c->clearLayers();
        c->proc.hair = hairSim_;
        c->proc.breathAmp = breathing_ ? 1.f : 0.f;
        c->proc.lookAt = lookAt_;
    }
    auto charOf = [&](const std::string& id) -> Character* { return id == "vivian" ? &v : (id == "marcus" ? &m : nullptr); };
    // layer with per-character masks from clips.json "layerBones" ({char: mask}); falls back to the given default
    auto layered = [&](int layer, const std::string& clip, float t, float w, const char* defChar, const char* defMask) {
        if (w <= 0.f) return;
        const Json& lb = clipMeta(clip)["layerBones"];
        if (lb.isObject() && lb.size()) {
            for (auto& kv : lb.obj) {
                Character* c = charOf(kv.first);
                if (c && c->loaded()) c->setLayer(layer, clip, t, w, kv.second.str("all"));
            }
        } else if (defChar) {
            Character* c = charOf(defChar);
            if (c && c->loaded()) c->setLayer(layer, clip, t, w, defMask);
        } else {
            for (Character* c : {&v, &m}) if (c->loaded()) c->setLayer(layer, clip, t, w, defMask);
        }
    };
    std::string baseClip;
    if (!pos.empty()) {
        if (pos == "solo") {
            baseClip = "solo.idle";
            for (Character* c : {&v, &m}) if (c->loaded()) c->setLayer(0, "solo.idle", idlePhase_, 1.f);
            for (Character* c : {&v, &m}) if (c->loaded() && rub_ > 0.f) c->setLayer(1, "solo.rub", rubPhase_, rub_);
        } else {
            baseClip = pos + ".in";
            for (Character* c : {&v, &m}) if (c->loaded()) c->setLayer(0, baseClip, depth_, 1.f);
            layered(1, pos + ".rub", rubPhase_, rub_, "vivian", "arm.R");
            layered(2, pos + ".embrace", 0.f, embrace_, nullptr, "upper");
        }
        if (!forceChars_) scene.showMarcus = !clipMeta(baseClip)["marcusHidden"].boolean(false);
    }
    for (auto& kv : face_) if (v.loaded()) v.setMorph(kv.first, kv.second > 0.001f ? kv.second : -1.f);
    for (auto& kv : mface_) if (m.loaded()) m.setMorph(kv.first, kv.second > 0.001f ? kv.second : -1.f);
    for (auto& kv : body_) if (v.loaded()) v.setBody(kv.first, kv.second);
    for (auto& kv : mbody_) if (m.loaded()) m.setBody(kv.first, kv.second);
    if (m.loaded() && m.hasMorph("aroused")) m.setMorph("aroused", aroused_);
    // per-clip morph rules (vag_open with depth, aroused = 1 in sex clips, ...)
    const Json& rules = clipMeta(baseClip)["morphs"];
    for (auto& who : rules.obj) {
        Character* c = charOf(who.first);
        if (!c || !c->loaded()) continue;
        for (auto& r : who.second.obj) {
            bool ok = false;
            float w = evalMorphRule(r.second, depth_, ok);
            if (ok && c->hasMorph(r.first)) c->setMorph(r.first, w);
        }
    }
    // Marcus' length/girth are erect-state deltas: scale by 0.35 + 0.65 * aroused
    if (m.loaded() && m.hasMorph("aroused")) {
        float k = 0.35f + 0.65f * m.morph("aroused");
        for (const char* p : {"length", "girth"}) m.setBody(p, mbody_[p] * k);
    }
    if (v.loaded()) v.setTone(kTones[tone_]);
    if (m.loaded()) m.setTone(kTones[mtone_]);
    // gaze: she looks at him (or the camera when alone), he looks at her face
    bool together = !pos.empty() && pos != "solo" && m.loaded() && scene.showMarcus;
    for (Character* c : {&v, &m}) c->proc.lookWeight = together ? 0.45f : 0.9f;
    if (v.loaded()) v.proc.lookTarget = together ? m.bonePos("頭") : cam_.eye;
    if (m.loaded()) m.proc.lookTarget = v.loaded() ? v.bonePos("頭") : cam_.eye;
}

void Sandbox::update(float dt) {
    time_ += dt;
    // keyboard shortcuts
    if (IsKeyPressed(KEY_F1)) overlay = !overlay;
    if (IsKeyPressed(KEY_F11)) ToggleFullscreen();
    if (IsKeyPressed(KEY_F5)) renderer.reloadShaders();
    if (IsKeyPressed(KEY_ESCAPE)) quit_ = true;
    if (IsKeyPressed(KEY_SPACE)) autoThrust_ = !autoThrust_;
    for (int k = 0; k < 9 && k < (int)positions_.size(); ++k)
        if (IsKeyPressed(KEY_ONE + k)) { posIndex_ = k; cameraPreset(camPreset_); for (Character* c : {&scene.vivian, &scene.marcus}) c->resetSimulation(); }
    if (IsKeyDown(KEY_UP)) depth_ = clampf(depth_ + dt * 0.8f, 0.f, 1.f);
    if (IsKeyDown(KEY_DOWN)) depth_ = clampf(depth_ - dt * 0.8f, 0.f, 1.f);
    if (autoThrust_) { thrustPhase_ += dt * thrustRate_ * 2.f * kPi; depth_ = 0.5f - 0.5f * std::cos(thrustPhase_); }
    rubPhase_ = std::fmod(rubPhase_ + dt * 1.0f, 1.f);
    idlePhase_ = std::fmod(idlePhase_ + dt * 0.12f, 1.f);
    // camera input unless the pointer is over the overlay
    Vector2 mp = GetMousePosition();
    camera.inputEnabled = !(overlay && (mp.x < 330.f || mp.x > (float)GetScreenWidth() - 300.f));
    camera.handleInput(dt);
    applyPose();
    scene.update(dt);
    if (camPending_) applyCameraPreset();
    else if (camFollow_ && camera.mode() == CameraRig::Mode::Orbit) {
        V3 t; float yaw, pitch, dist;
        if (boneShot(camPreset_, t, yaw, pitch, dist)) camera.follow(t);
    }
    audio.update();
}

void Sandbox::render(int w, int h) {
    fbW_ = w; fbH_ = h;
    cam_ = camera.update(GetFrameTime() > 0.f ? std::min(GetFrameTime(), 0.05f) : 1.f / 60.f, (float)w / (float)std::max(1, h));
    renderer.render(scene, cam_, w, h, time_);
    if (overlay) drawOverlay(GetScreenWidth(), GetScreenHeight());
}

void Sandbox::drawOverlay(int w, int h) {
    char buf[256];
    float x = 12, y = 12, pw = 306;
    GuiPanel(Rectangle{x - 6, y - 6, pw + 12, (float)h - 12}, "Afterglow engine (F1)");
    y += 22;
    std::snprintf(buf, sizeof buf, "%d fps  |  %d draws  |  %.0fk tris", GetFPS(), renderer.drawCalls, (double)renderer.triangles / 1000.0);
    GuiLabel(Rectangle{x, y, pw, 20}, buf); y += 22;
    std::snprintf(buf, sizeof buf, "room: %s%s  clips: %s", scene.roomFromGlb ? "glb" : "placeholder", scene.roomBaked ? " (baked)" : "", scene.hasClips ? "yes" : "none yet");
    GuiLabel(Rectangle{x, y, pw, 20}, buf); y += 26;

    GuiLine(Rectangle{x, y, pw, 12}, "Position"); y += 16;
    if (!positions_.empty()) {
        std::string list;
        for (size_t i = 0; i < positions_.size(); ++i) { if (i) list += ";"; list += positions_[i]; }
        int prev = posIndex_;
        Rectangle dd{x, y, pw, 24};
        y += 30;
        GuiSlider(Rectangle{x + 60, y, pw - 110, 18}, "depth", TextFormat("%.2f", depth_), &depth_, 0.f, 1.f); y += 24;
        GuiCheckBox(Rectangle{x + 60, y, 18, 18}, "auto thrust (Space)", &autoThrust_); y += 24;
        GuiSlider(Rectangle{x + 60, y, pw - 110, 18}, "rate", TextFormat("%.2f", thrustRate_), &thrustRate_, 0.3f, 3.f); y += 24;
        GuiSlider(Rectangle{x + 60, y, pw - 110, 18}, "rub", TextFormat("%.2f", rub_), &rub_, 0.f, 1.f); y += 24;
        GuiSlider(Rectangle{x + 60, y, pw - 110, 18}, "embrace", TextFormat("%.2f", embrace_), &embrace_, 0.f, 1.f); y += 28;
        if (GuiDropdownBox(dd, list.c_str(), &posIndex_, dropdownEdit_)) dropdownEdit_ = !dropdownEdit_;
        if (prev != posIndex_) { cameraPreset(camPreset_); for (Character* c : {&scene.vivian, &scene.marcus}) c->resetSimulation(); }
    } else {
        GuiLabel(Rectangle{x, y, pw, 20}, "no clips yet: characters in bind pose"); y += 26;
    }
    GuiLine(Rectangle{x, y, pw, 12}, "Camera"); y += 16;
    const char* cams[7] = {"front", "side", "top", "close", "portrait", "wide", "fpv"};
    for (int i = 0; i < 7; ++i)
        if (GuiButton(Rectangle{x + (float)(i % 3) * 102, y + (float)(i / 3) * 28, 98, 24}, cams[i])) cameraPreset(cams[i]);
    y += 90;
    GuiLine(Rectangle{x, y, pw, 12}, "Render"); y += 16;
    int q = (int)renderer.settings.quality;
    GuiToggleGroup(Rectangle{x, y, 98, 24}, "low;medium;high", &q); y += 30;
    if (q != (int)renderer.settings.quality) applyQuality(renderer.settings, (Quality)q);
    GuiCheckBox(Rectangle{x, y, 18, 18}, "shadows", &renderer.settings.shadows);
    GuiCheckBox(Rectangle{x + 102, y, 18, 18}, "bloom", &renderer.settings.bloom);
    GuiCheckBox(Rectangle{x + 204, y, 18, 18}, "fxaa", &renderer.settings.fxaa); y += 24;
    GuiCheckBox(Rectangle{x, y, 18, 18}, "hair sim", &hairSim_);
    GuiCheckBox(Rectangle{x + 102, y, 18, 18}, "breathing", &breathing_);
    GuiCheckBox(Rectangle{x + 204, y, 18, 18}, "look-at", &lookAt_); y += 26;
    GuiSlider(Rectangle{x + 70, y, pw - 120, 18}, "exposure", TextFormat("%.2f", renderer.settings.exposure), &renderer.settings.exposure, 0.3f, 3.f); y += 24;
    GuiSlider(Rectangle{x + 70, y, pw - 120, 18}, "warmth", TextFormat("%.2f", renderer.settings.warmth), &renderer.settings.warmth, -1.f, 1.f); y += 24;
    GuiSlider(Rectangle{x + 70, y, pw - 120, 18}, "bloom", TextFormat("%.2f", renderer.settings.bloomStrength), &renderer.settings.bloomStrength, 0.f, 0.4f); y += 24;
    GuiSlider(Rectangle{x + 70, y, pw - 120, 18}, "flush", TextFormat("%.2f", scene.vivian.flush), &scene.vivian.flush, 0.f, 1.f); y += 24;
    float sweat = scene.vivian.sweat;
    GuiSlider(Rectangle{x + 70, y, pw - 120, 18}, "sweat", TextFormat("%.2f", sweat), &sweat, 0.f, 1.f); y += 24;
    scene.vivian.sweat = scene.marcus.sweat = sweat;

    // right panel: characters
    float rx = (float)w - 296;
    y = 12;
    GuiPanel(Rectangle{rx - 6, y - 6, 290, (float)h - 12}, "Characters");
    y += 22;
    GuiLabel(Rectangle{rx, y, 200, 20}, "Vivian tone"); y += 20;
    GuiToggleGroup(Rectangle{rx, y, 88, 22}, "honey;caramel;deep", &tone_); y += 28;
    for (auto& f : kFaceF) {
        if (!scene.vivian.hasMorph(f)) continue;
        GuiSlider(Rectangle{rx + 64, y, 170, 16}, f.c_str(), TextFormat("%.2f", face_[f]), &face_[f], 0.f, 1.f); y += 20;
    }
    y += 6;
    for (auto& b : kBodyF) { GuiSlider(Rectangle{rx + 64, y, 170, 16}, b.c_str(), TextFormat("%+.2f", body_[b]), &body_[b], -1.f, 1.f); y += 20; }
    y += 10;
    GuiCheckBox(Rectangle{rx, y, 18, 18}, "show Vivian", &scene.showVivian);
    GuiCheckBox(Rectangle{rx + 140, y, 18, 18}, "show Marcus", &scene.showMarcus); y += 28;
    GuiLabel(Rectangle{rx, y, 200, 20}, "Marcus tone"); y += 20;
    GuiToggleGroup(Rectangle{rx, y, 88, 22}, "honey;caramel;deep", &mtone_); y += 28;
    if (scene.marcus.hasMorph("aroused")) { GuiSlider(Rectangle{rx + 64, y, 170, 16}, "aroused", TextFormat("%.2f", aroused_), &aroused_, 0.f, 1.f); y += 20; }
    for (auto& f : kFaceM) {
        if (!scene.marcus.hasMorph(f)) continue;
        GuiSlider(Rectangle{rx + 64, y, 170, 16}, f.c_str(), TextFormat("%.2f", mface_[f]), &mface_[f], 0.f, 1.f); y += 20;
    }
    y += 6;
    for (auto& b : kBodyM) { GuiSlider(Rectangle{rx + 64, y, 170, 16}, b.c_str(), TextFormat("%+.2f", mbody_[b]), &mbody_[b], -1.f, 1.f); y += 20; }
}

}  // namespace ag
