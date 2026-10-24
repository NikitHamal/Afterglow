#include "game/game.h"
#include "app/platform.h"
#include "core/log.h"
#include "scene/lighting.h"
#include "scene/touch.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ag {

namespace {
float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
float approach(float v, float t, float rate, float dt) { return v + (t - v) * (1.f - std::exp(-rate * dt)); }
float smoothstepF(float a, float b, float x) { float t = clamp01((x - a) / (b - a)); return t * t * (3.f - 2.f * t); }
const float kContentYawOffset = 180.f;   // clips.json yaw is measured from the head side
}  // namespace

// ------------------------------------------------------------------ night start / end
void Game::startNight(bool resume) {
    stage(Stage::Night);
    unsigned seed = (unsigned)(GetTime() * 1000.0) ^ 0x9e3779b9u;
    if (harness) seed = 7;
    if (resume && profile.night.valid) night_.restore(profile.night);
    else night_.begin(seed);
    int start = 0;
    std::string want = resume && profile.night.valid ? profile.night.pos : "missionary";
    for (size_t i = 0; i < positions_.size(); ++i) if (positions_[i].id == want) start = (int)i;
    posIndex_ = -1;
    setPosition(start, true);
    // establishing shot: start wide, ease into the position's camera
    camera.setOrbit(scene.info.bedCenter + V3{0.f, 0.15f, 0.f}, camera.yaw + 0.6f, 0.42f, 3.6f, 0.f);
    cameraForPosition(3.f);
    depth_ = depthTarget_ = dragDepth_ = 0.3f;
    rhythm_ = rhythmToggle_ = false;
    rhythmTempo_ = 0.8f; rhythmAmp_ = 0.5f; rhythmPhase_ = 0.f;
    rub_ = rubTarget_ = embrace_ = embraceTarget_ = 0.f;
    flash_ = climaxGlow_ = 0.f;
    herClimaxT_ = hisClimaxT_ = 99.f;
    afterT_ = 0.f; slowmo_ = 1.f;
    together_ = false;
    sweat_ = resume ? profile.night.sweat : 0.f;
    subtitle_ = resume ? "Where were we\xe2\x80\xa6" : "Come here\xe2\x80\xa6";
    subtitleT_ = 0.f; subtitleDur_ = 4.f;
    shownDesire_ = Desire::None; desireShow_ = 0.f;
    hintsT_ = 0.f; hudT_ = 0.f;
    camUser_ = false; cinematicT_ = 0.f; nextCut_ = 16.f;
    toasts_.clear();
    introCard_ = (!resume && profile.stats.nights == 0) ? 14.f : 0.f;
    profile.night.valid = false;
}

void Game::endNight(bool natural) {
    Results& r = results_;
    r = Results{};
    r.minutes = night_.elapsed / 60.f;
    r.connection = night_.connection;
    r.herClimaxes = night_.herClimaxes;
    r.together = night_.together;
    r.desiresMet = night_.desiresMet; r.desiresMissed = night_.desiresMissed; r.bestStreak = night_.bestStreak;
    float bonus = 1.f + 0.5f * night_.connection;
    r.score = (int)std::lround(night_.score * bonus);
    r.soon = natural && !night_.together && night_.herClimaxes == 0;
    if (!natural) { r.rating = "Until next time"; r.line = "Some nights are for slowing down. She kisses you goodnight."; }
    else if (r.together && r.connection > 0.75f) { r.rating = "Unforgettable"; r.line = "Together, completely. She falls asleep on your chest, smiling."; }
    else if (r.together) { r.rating = "Breathless"; r.line = "You came apart together. She's still catching her breath."; }
    else if (r.herClimaxes >= 2) { r.rating = "Generous"; r.line = "She lost count. You made the night about her."; }
    else if (r.herClimaxes == 1) { r.rating = "Sweet"; r.line = "She found her moment, and you found yours."; }
    else { r.rating = "Too soon"; r.line = "It was over before she got there. Next time, slow down when he's close."; }
    Stats& st = profile.stats;
    ++st.nights;
    if (r.together) ++st.together;
    st.herClimaxes += r.herClimaxes;
    st.bestScore = std::max(st.bestScore, r.score);
    st.bestConnection = std::max(st.bestConnection, r.connection);
    st.totalMinutes += r.minutes;
    profile.night.valid = false;
    if (!harness) profile.save(profilePath_);
}

// ------------------------------------------------------------------ positions + cameras
void Game::setPosition(int index, bool instant) {
    if (index < 0 || index >= (int)positions_.size()) return;
    if (index == posIndex_ && !instant) return;
    if (!instant) {
        if (pendingPos_ >= 0) return;
        pendingPos_ = index;
        posFade_ = 0.0001f;
        return;
    }
    if (posIndex_ >= 0 && posIndex_ != index) lastAct_ = posIndex_;
    posIndex_ = index;
    const PositionInfo& p = positions_[(size_t)index];
    if (!p.canTouch) rubTarget_ = rub_ = 0.f;
    if (!p.canKiss) embraceTarget_ = embrace_ = 0.f;
    if (p.solo && camMode_ == 3) camMode_ = 0;
    depth_ = depthTarget_ = dragDepth_ = std::min(depth_, 0.35f);
    for (Character* c : {&scene.vivian, &scene.marcus}) c->resetSimulation();
    cameraForPosition(0.f);
    camUser_ = false;
    nextCut_ = 14.f + 6.f * (float)(index % 3);
    cinematicT_ = 0.f;
}

void Game::cameraForPosition(float blend) {
    const PositionInfo& p = positions_[(size_t)std::max(0, posIndex_)];
    const Json& cams = clipsMeta_["clips"][p.baseClip]["cameras"];
    if (camMode_ == 3 && !p.solo && scene.marcus.loaded()) {
        const Json& f = cams["fpv"];
        if (f.isObject() && f["offset_local"].isArray())
            camera.setFirstPersonLocal(&scene.marcus, f["bone"].str("頭"), {f["offset_local"][0].f(), f["offset_local"][1].f(), f["offset_local"][2].f()},
                                       {f["forward_local"][0].f(), f["forward_local"][1].f(), f["forward_local"][2].f(1.f)}, blend);
        else camera.setFirstPerson(&scene.marcus, "頭", {0.f, 0.07f, 0.09f}, blend);
        return;
    }
    if (camMode_ == 5 && scene.vivian.loaded()) {
        // QA: the touched area up close (her vulva zone, from in front and above)
        V3 zp, zn;
        if (scene.vivian.touch.zoneWorld(scene.vivian.anim, Zone::Vulva, zp, zn)) {
            V3 d = normalize(zn + V3{0.f, 0.9f, 0.f});
            camera.setOrbit(zp, std::atan2(d.x, d.z), std::min(std::asin(clampf(d.y, -1.f, 1.f)), 1.3f), 0.62f, blend);
            return;
        }
    }
    if (camMode_ == 4 && scene.vivian.loaded()) {
        // her face: from the front-side, away from him, slightly above, following her head
        V3 hc, hf, hu;
        scene.vivian.headAxes(hc, hf, hu);
        V3 side = normalize(cross(hu, hf));
        if (scene.showMarcus && dot(side, hc - scene.marcus.bonePos("上半身2")) < 0.f) side = side * -1.f;
        V3 d = normalize(hf * 0.85f - hu * 0.1f + side * 0.45f + V3{0.f, 0.25f, 0.f});
        if (d.y < 0.15f) d = normalize(V3{d.x, 0.15f, d.z});
        camera.setOrbit(hc, std::atan2(d.x, d.z), std::min(std::asin(clampf(d.y, -1.f, 1.f)), 1.3f), 0.78f, blend);
        return;
    }
    const char* names[3] = {"orbit_side", "orbit_front", "orbit_top"};
    const Json& c = cams[names[std::max(0, std::min(2, camMode_))]];
    if (c.isObject() && c["target"].isArray()) {
        const float d2r = kPi / 180.f;
        float dist = c["distance"].isNumber() ? c["distance"].f() : c["dist"].f(2.4f);
        camera.setOrbit({c["target"][0].f(), c["target"][1].f(), c["target"][2].f()},
                        (c["yaw"].f() + kContentYawOffset) * d2r, c["pitch"].f(20.f) * d2r, dist, blend);
    } else {
        camera.setOrbit(scene.info.bedCenter + V3{0.f, 0.25f, 0.f}, 1.45f + (float)camMode_ * 0.6f, 0.25f + 0.35f * (float)(camMode_ == 2), 2.4f, blend);
    }
}

// story play offers the acts the chapter allows (play.acts); free play offers everything
bool Game::actAllowed(const std::string& id) const {
    if (screen_ != Screen::Story && !(screen_ == Screen::Pause && pauseReturn_ == Screen::Story)) return true;
    const StoryNode* n = story_.node();
    if (!n || n->playActs.empty()) return true;
    return std::find(n->playActs.begin(), n->playActs.end(), id) != n->playActs.end();
}

void Game::actById(const std::string& id) {
    if (!actAllowed(id)) return;
    if (posIndex_ >= 0 && positions_[(size_t)posIndex_].id == id) {
        if (lastAct_ >= 0 && lastAct_ != posIndex_) setPosition(lastAct_);
        return;
    }
    for (size_t i = 0; i < positions_.size(); ++i)
        if (positions_[i].id == id) { setPosition((int)i); return; }
}

int Game::nextCamMode(int cur, bool solo) const {
    const int order[5] = {0, 1, 2, 4, 3};   // side, front, above, her face, his view
    int at = 0;
    for (int i = 0; i < 5; ++i) if (order[i] == cur) at = i;
    for (int k = 1; k <= 5; ++k) {
        int m = order[(at + k) % 5];
        if (m == 3 && solo) continue;
        return m;
    }
    return 0;
}

// ------------------------------------------------------------------ input + simulation
void Game::updatePlay(float dt) {
    if (dt <= 0.f || posIndex_ < 0) return;
    const PositionInfo& p = positions_[(size_t)posIndex_];
    hudT_ += dt; hintsT_ += dt;
    if (introCard_ > 0.f) {
        introCard_ -= dt;
        if (night_.sinceStroke < 0.5f && introCard_ > 3.f && night_.elapsed > 4.f) introCard_ = 3.f;   // they've got it
        if (IsKeyPressed(KEY_ENTER)) introCard_ = std::min(introCard_, 0.6f);
    }
    bool overUi = (barOpen_ && ui.in.mouse.y > ui.H - ui.u(250.f)) || CheckCollisionPointRec(ui.in.mouse, actionBarRect_);

    if (!night_.finished) {
        if (IsKeyPressed(KEY_ESCAPE)) { pauseReturn_ = screen_; go(Screen::Pause, 0.15f); return; }
        if (IsKeyPressed(KEY_TAB)) barOpen_ = !barOpen_;
        for (int k = 0; k < 10 && k < (int)positions_.size(); ++k)
            if (IsKeyPressed(k == 9 ? KEY_ZERO : KEY_ONE + k) && actAllowed(positions_[(size_t)k].id)) setPosition(k);
        if ((IsKeyPressed(KEY_R) || IsKeyPressed(KEY_T)) && p.canTouch) rubTarget_ = rubTarget_ > 0.5f ? 0.f : 1.f;
        if ((IsKeyPressed(KEY_E) || IsKeyPressed(KEY_K)) && p.canKiss) embraceTarget_ = embraceTarget_ > 0.5f ? 0.f : 1.f;
        if (IsKeyPressed(KEY_O)) actById("oral");
        if (IsKeyPressed(KEY_L)) actById("lick");
        if (IsKeyPressed(KEY_X)) actById("solo");
        if (IsKeyPressed(KEY_H)) hudHidden_ = !hudHidden_;
        if (IsKeyPressed(KEY_M)) {
            settings.lightMood = (settings.lightMood + 1) % 3;
            toasts_.push_back({0.f, moodName((Mood)settings.lightMood)});
        }
        if (IsKeyPressed(KEY_C)) { camMode_ = nextCamMode(camMode_, p.solo); cameraForPosition(0.9f); camUser_ = false; }
        if (IsKeyPressed(KEY_V) && !p.solo) { camMode_ = camMode_ == 3 ? 0 : 3; cameraForPosition(0.7f); }
        if (settings.holdRhythm) rhythm_ = IsKeyDown(KEY_SPACE) || rhythmToggle_;   // (toggle only set by autoplay)
        else {
            if (IsKeyPressed(KEY_SPACE)) rhythmToggle_ = !rhythmToggle_;
            rhythm_ = rhythmToggle_;
        }
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) rhythmTempo_ = std::min(2.6f, rhythmTempo_ + dt * 0.7f);
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) rhythmTempo_ = std::max(0.3f, rhythmTempo_ - dt * 0.7f);
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) rhythmAmp_ = std::min(0.95f, rhythmAmp_ + dt * 0.5f);
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) rhythmAmp_ = std::max(0.12f, rhythmAmp_ - dt * 0.5f);
    }

    if (autoplay_) autoplayStep(dt);

    if (night_.finished) { rhythm_ = rhythmToggle_ = false; dragDepth_ = depthTarget_ = 0.12f; }   // afterglow: still, close

    // ---- depth: mouse drag (direct), wheel notches (strokes that spring back), or the auto rhythm ----
    float sens = settings.mouseSens * (settings.invertY ? -1.f : 1.f);
    bool dragging = IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !overUi && !ui.anyActive() && !night_.finished;
    bool orbiting = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    float wheel = orbiting ? 0.f : ui.in.wheel;
    // the cursor hides while you move together, and when the mouse rests
    static float still = 0.f;
    still = ui.in.mouseMoved && !dragging ? 0.f : still + dt;
    bool hide = !harness && (dragging || (still > 2.5f && !barOpen_));
    if (hide && !IsCursorHidden()) HideCursor();
    else if (!hide && IsCursorHidden()) ShowCursor();
    if (dragging) {
        dragDepth_ = clamp01(dragDepth_ + GetMouseDelta().y * 0.0042f * sens);
        depthTarget_ = dragDepth_;
        wheelImpulse_ = 0.f;
    } else if (wheel != 0.f && !overUi && !night_.finished) {
        dragDepth_ = clamp01(dragDepth_ - wheel * 0.16f * (settings.invertY ? -1.f : 1.f));
        depthTarget_ = dragDepth_;
        wheelImpulse_ = 0.35f;
    } else if (rhythm_) {
        rhythmPhase_ = std::fmod(rhythmPhase_ + dt * rhythmTempo_, 1.f);
        float wv = 0.5f - 0.5f * std::cos(rhythmPhase_ * 2.f * kPi);
        wv = wv * wv * (3.f - 2.f * wv);                              // dwell a little at both ends
        depthTarget_ = clamp01(rhythmCenter_ + rhythmAmp_ * (wv - 0.5f));
        dragDepth_ = depthTarget_;
    } else if (holdDepth_) {
        depthTarget_ = dragDepth_;
    } else {
        wheelImpulse_ -= dt;
        if (wheelImpulse_ <= 0.f) { depthTarget_ = approach(depthTarget_, std::min(depthTarget_, 0.3f), 1.5f, dt); dragDepth_ = depthTarget_; }
    }
    depth_ = approach(depth_, depthTarget_, dragging ? 22.f : 14.f, dt);
    rub_ = approach(rub_, rubTarget_, 4.f, dt);
    embrace_ = approach(embrace_, embraceTarget_, 3.f, dt);

    // ---- camera: RMB orbits (wheel zooms while orbiting); cinematic drift + cuts otherwise ----
    camera.inputEnabled = orbiting;
    if (orbiting) { camera.handleInput(dt); camUser_ = true; }
    if (!camUser_ && settings.cinematicCamera && camMode_ != 3 && !night_.finished) {
        cinematicT_ += dt;
        if (cinematicT_ > nextCut_) {
            cinematicT_ = 0.f;
            nextCut_ = 13.f + 7.f * hash1(time_);
            camMode_ = (camMode_ + 1) % 3;
            cameraForPosition(2.4f);
        }
    }

    // ---- the night ----
    Night::Input in;
    in.pos = &p;
    in.depth = depth_;
    in.rub = rub_;
    in.embrace = embrace_;
    if (p.solo) { in.depth = clamp01(depth_); in.rub = rhythm_ || dragging || wheelImpulse_ > 0.f ? 1.f : 0.3f; }
    float simDt = dt * slowmo_;
    night_.update(simDt, in);
    if (night_.finished) {
        afterT_ += dt;
        if (afterT_ > 5.5f && screen_ == Screen::Play && !switching_) { endNight(true); go(Screen::Results, 0.9f); }
    }

    // position change dip
    if (pendingPos_ >= 0) {
        posFade_ += dt / 0.2f;
        if (posFade_ >= 1.f) { setPosition(pendingPos_, true); pendingPos_ = -1; posFade_ = 1.f; }
    } else if (posFade_ > 0.f) posFade_ = std::max(0.f, posFade_ - dt / 0.35f);
}

// A player who listens: rhythm on, tempo/depth eased toward her wishes, touch/kiss/change on request,
// easing off when he is close. Drives the real input path (rhythm, targets, setPosition).
void Game::autoplayStep(float dt) {
    rhythm_ = rhythmToggle_ = true;
    holdDepth_ = false;
    const PositionInfo& p = positions_[(size_t)posIndex_];
    float wantT = night_.wantTempo, wantA = night_.wantAmp;
    if (night_.him > 0.84f && night_.her < 0.85f) wantT = 0.35f;          // edge
    rhythmTempo_ += (wantT - rhythmTempo_) * std::min(1.f, dt * 0.9f);
    rhythmAmp_ += (std::max(0.15f, wantA) - rhythmAmp_) * std::min(1.f, dt * 0.9f);
    if (night_.desire == Desire::Touch && p.canTouch) rubTarget_ = 1.f;
    if (night_.desire == Desire::Kiss && p.canKiss) embraceTarget_ = 1.f;
    if (night_.desire == Desire::Change && pendingPos_ < 0) {
        int next = (posIndex_ + 1) % (int)positions_.size();
        if (positions_[(size_t)next].solo || positions_[(size_t)next].id == "lick" || positions_[(size_t)next].id == "oral") next = 0;
        setPosition(next);
    }
    autoLog_ += dt;
    if (autoLog_ >= 10.f) {
        autoLog_ = 0.f;
        AG_INFO("AG: autoplay t=%.0fs pos=%s tempo=%.2f/%.2f amp=%.2f/%.2f sync=%.2f her=%.2f him=%.2f conn=%.2f desires %d/%d climaxes %d score %.0f",
                (double)night_.elapsed, night_.pos.c_str(), (double)night_.tempo, (double)night_.wantTempo, (double)night_.amplitude,
                (double)night_.wantAmp, (double)night_.sync, (double)night_.her, (double)night_.him, (double)night_.connection,
                night_.desiresMet, night_.desiresMet + night_.desiresMissed, night_.herClimaxes, (double)night_.score);
    }
}

// ------------------------------------------------------------------ presentation
void Game::presentPlay(float dt) {
    if (posIndex_ < 0) { scene.update(dt); return; }
    const PositionInfo& p = positions_[(size_t)posIndex_];
    Character& v = scene.vivian;
    Character& m = scene.marcus;
    const std::string& baseClip = p.baseClip;
    const Json& meta = clipsMeta_["clips"][baseClip];
    float her = night_.her, him = night_.him;

    // events -> feedback
    for (const NightEvent& e : night_.events) {
        switch (e.type) {
            case NightEvent::Stroke: moanPulse_ = std::min(1.f, moanPulse_ + 0.35f + 0.4f * her); break;
            case NightEvent::DesireNew:
                if (settings.hints > 0) { subtitle_ = Night::desireLine(e.desire, night_.stroking()); subtitleT_ = 0.f; subtitleDur_ = night_.desireTimeout; }
                shownDesire_ = e.desire;
                break;
            case NightEvent::DesireMet:
                subtitle_ = Night::metLine((unsigned)night_.desiresMet); subtitleT_ = 0.f; subtitleDur_ = 2.6f;
                smilePulse_ = 1.f;
                toasts_.push_back({0.f, e.value >= 3.f ? "In sync \xc3\x97" + std::to_string((int)e.value) : "She loves that"});
                break;
            case NightEvent::DesireMissed:
                if (subtitleDur_ > 3.f) { subtitleDur_ = std::min(subtitleDur_, subtitleT_ + 0.8f); }
                break;
            case NightEvent::HerClimax:
                herClimaxT_ = 0.f; flash_ = std::max(flash_, 0.55f * e.value); climaxGlow_ = 1.f;
                subtitle_ = "Oh\xe2\x80\xa6 oh, yes\xe2\x80\xa6 yes!"; subtitleT_ = 0.f; subtitleDur_ = 3.5f;
                toasts_.push_back({0.f, "She came"});
                break;
            case NightEvent::HisClimax:
                hisClimaxT_ = 0.f; flash_ = std::max(flash_, 0.7f); climaxGlow_ = 1.f; slowmo_ = 0.35f;
                break;
            case NightEvent::Together:
                together_ = true;
                subtitle_ = "Together\xe2\x80\xa6"; subtitleT_ = 0.f; subtitleDur_ = 4.f;
                toasts_.push_back({0.f, "Together"});
                break;
            case NightEvent::EdgeStart: toasts_.push_back({0.f, "Holding back\xe2\x80\xa6"}); break;
            case NightEvent::Favorite:
                subtitle_ = "This\xe2\x80\xa6 this is my favourite."; subtitleT_ = 0.f; subtitleDur_ = 3.f;
                break;
        }
    }
    playAudioEvents();
    if (night_.desire == Desire::None) shownDesire_ = Desire::None;
    desireShow_ = approach(desireShow_, shownDesire_ != Desire::None ? 1.f : 0.f, 5.f, dt);
    subtitleT_ += dt;
    for (auto& t : toasts_) t.first += dt;
    toasts_.erase(std::remove_if(toasts_.begin(), toasts_.end(), [](const std::pair<float, std::string>& t) { return t.first > 2.4f; }), toasts_.end());
    herClimaxT_ += dt; hisClimaxT_ += dt;
    slowmo_ = approach(slowmo_, 1.f, night_.finished ? 0.25f : 1.2f, dt);
    flash_ = approach(flash_, 0.f, 2.2f, dt);
    climaxGlow_ = approach(climaxGlow_, 0.f, 0.45f, dt);
    moanPulse_ = approach(moanPulse_, 0.f, 3.5f, dt);
    smilePulse_ = approach(smilePulse_, 0.f, 1.2f, dt);
    sweat_ = clamp01(std::max(sweat_, std::min(0.5f, night_.elapsed / 700.f + 0.22f * her)));   // a dewy glow, not oil

    // ---- layers ----
    // her touch keeps time with you when she is alone, with her own pace otherwise
    float rubRate = p.solo ? std::max(0.5f, std::min(2.2f, night_.tempo + 0.2f)) : 0.8f + 0.9f * her;
    rubPhase_ = std::fmod(rubPhase_ + dt * rubRate, 1.f);
    idlePhase_ = std::fmod(idlePhase_ + dt / 9.f, 1.f);
    auto charOf = [&](const std::string& id) -> Character* { return id == "vivian" ? &v : (id == "marcus" ? &m : nullptr); };
    auto layered = [&](int layer, const std::string& clip, float t, float w, const char* defChar, const char* defMask) {
        if (w <= 0.001f) { for (Character* c : {&v, &m}) if (c->loaded()) c->clearLayer(layer); return; }
        const Json& lb = clipsMeta_["clips"][clip]["layerBones"];
        if (lb.isObject() && lb.size()) {
            for (auto& kv : lb.obj) { Character* c = charOf(kv.first); if (c && c->loaded()) c->setLayer(layer, clip, t, w, kv.second.str("all")); }
        } else if (defChar) {
            Character* c = charOf(defChar);
            if (c && c->loaded()) c->setLayer(layer, clip, t, w, defMask);
        } else {
            for (Character* c : {&v, &m}) if (c->loaded()) c->setLayer(layer, clip, t, w, defMask);
        }
    };
    for (Character* c : {&v, &m}) if (c->loaded()) c->clearLayers();
    if (p.solo) {
        if (v.loaded()) {
            v.setLayer(0, "solo.idle", idlePhase_, 1.f);
            float rubW = clamp01(0.35f + 0.65f * depth_);
            if (v.hasClip("solo.rub")) v.setLayer(1, "solo.rub", rubPhase_, rubW);
        }
        scene.showMarcus = false;
    } else {
        // depth acts scrub their clip with the played depth; loop acts (foreplay) run at the played tempo
        loopPhase_ = std::fmod(loopPhase_ + dt * std::max(0.35f, std::min(2.f, night_.tempo + 0.15f)), 1.f);
        float t = p.depthDriven ? depth_ : loopPhase_;
        for (Character* c : {&v, &m}) if (c->loaded()) c->setLayer(0, baseClip, t, 1.f);
        if (p.canTouch) layered(1, p.id + ".rub", rubPhase_, rub_, "vivian", "arm.R");
        if (p.canKiss) layered(2, p.id + ".embrace", 0.f, embrace_, nullptr, "upper");
        scene.showMarcus = m.loaded() && !meta["marcusHidden"].boolean(false);
    }
    scene.showVivian = v.loaded();

    // ---- touch: her hands find the real surface; being touched makes her react ----
    if (v.loaded()) {
        TouchGoals tg;
        if (p.solo) {
            float inten = clamp01(0.35f + 0.65f * depth_);
            tg.right.active = true; tg.right.zone = Zone::Vulva; tg.right.weight = 0.92f; tg.right.phase = rubPhase_;
            tg.right.amplitude = 0.006f + 0.01f * inten; tg.right.curl = 0.28f; tg.right.press = 0.003f + 0.003f * inten;
            tg.left.active = true; tg.left.zone = Zone::BreastL; tg.left.weight = 0.85f * clamp01(0.45f + her);
            tg.left.phase = std::fmod(rubPhase_ * 0.5f + 0.25f, 1.f); tg.left.amplitude = 0.01f; tg.left.curl = 0.6f; tg.left.press = 0.007f;
        } else if (p.canTouch && rub_ > 0.02f) {
            tg.right.active = true; tg.right.zone = Zone::Vulva; tg.right.weight = 0.95f * rub_; tg.right.phase = rubPhase_;
            tg.right.amplitude = 0.012f; tg.right.curl = 0.28f; tg.right.press = 0.004f;
        }
        v.touchGoals = tg;
        // sensitivity by zone: vulva 1, breast 0.55
        float act = (tg.right.active ? tg.right.weight : 0.f) + (tg.left.active ? tg.left.weight * 0.55f : 0.f);
        float before = touchAct_;
        touchAct_ = approach(touchAct_, clamp01(act), 6.f, dt);
        if (dt > 0.f && before < 0.25f && touchAct_ >= 0.25f) {
            // a touch begins: a small flinch, a caught breath, a jolt through the breasts
            flinch_ = 1.f;
            breath(true, 0.55f);
            voice("moan02", 0.35f + 0.3f * her, 1.25f);
            for (const char* b : {"乳親.L", "乳親.R"}) { int bi = v.bone(b); if (bi >= 0) v.procedural.impulse(bi, V3{0.f, 0.25f, 0.1f}); }
        }
        flinch_ = approach(flinch_, 0.f, 3.5f, dt);
    }

    // ---- bodies, faces, skin ----
    float climaxH = std::exp(-herClimaxT_ * 0.9f) * (herClimaxT_ < 6.f ? 1.f : 0.f);
    float climaxM = std::exp(-hisClimaxT_ * 0.9f) * (hisClimaxT_ < 6.f ? 1.f : 0.f);
    if (v.loaded()) {
        if (flinch_ > 0.9f) herBlink_.trigger(0.7f);                      // a touch lands: the eyes flutter
        float blinkV = herBlink_.update(dt, 1.f + 0.6f * her);             // she blinks more as it builds
        float eyesClosed = clamp01(0.25f * her * her + 0.8f * climaxH);
        v.setMorph("blink", clamp01(std::max(blinkV, eyesClosed)));
        v.setMorph("pleasure", clamp01(0.15f + 0.75f * std::pow(her, 0.8f) + 0.4f * climaxH + 0.25f * touchAct_ + 0.3f * flinch_));
        mouthT_ = std::max(0.f, mouthT_ - dt);
        float u = mouthDur_ > 0.f ? 1.f - mouthT_ / mouthDur_ : 1.f;   // 0 -> 1 over the line
        float voiceOpen = mouthT_ > 0.f ? std::sin(3.14159f * std::min(1.f, u * 1.15f)) * (0.55f + 0.15f * std::sin(u * 23.f)) : 0.f;
        v.setMorph("moan", clamp01(moanPulse_ * (0.25f + 0.7f * her) + 0.85f * climaxH + voiceOpen + 0.55f * beatMouth_));
        v.setMorph("smile", clamp01(0.12f + 0.35f * night_.connection * (1.f - her) + 0.5f * smilePulse_ + 0.6f * std::exp(-afterT_ * 0.2f) * (night_.finished ? 1.f : 0.f)));
        v.setMorph("kiss", clamp01(embrace_ * 0.8f));
        if (v.hasMorph("oral")) v.setMorph("oral", p.id == "oral" ? 1.f : 0.f);
        // the v3 face: her lip bitten as it builds, a gasp when a touch lands, the climax face, bliss afterwards
        float after = night_.finished ? std::exp(-afterT_ * 0.12f) : 0.f;
        v.setMorph("bite_lip", clamp01((smoothstepF(0.45f, 0.8f, her) * (0.35f + 0.25f * std::sin(time_ * 0.7f)) * (1.f - climaxH)) * (1.f - moanPulse_)));
        v.setMorph("gasp", clamp01(0.7f * flinch_));
        v.setMorph("orgasm", clamp01(1.1f * climaxH));
        v.setMorph("bliss", clamp01(0.7f * after + 0.25f * smoothstepF(0.7f, 1.f, her) * (1.f - climaxH)));
        // arousal: she swells and glistens (morph + the skin mask's wetness channel)
        v.setMorph("vag_aroused", clamp01(0.2f + 0.9f * her + 0.3f * climaxH));
        v.wet = clamp01(0.15f + 1.1f * her);
        v.flush = clamp01(0.15f + 0.8f * her + 0.3f * climaxH);
        v.sweat = sweat_;
        v.proc.breathRate = 0.22f + 0.38f * her + 0.3f * climaxH + 0.12f * touchAct_;
        v.proc.breathAmp = 1.f + 1.4f * her + 1.5f * climaxH + 0.8f * touchAct_ + 1.2f * flinch_;
        arch_ = 0.07f * touchAct_ * (0.5f + her) + 0.06f * flinch_ + 0.1f * climaxH;
        v.proc.eyeRollUp = clamp01(0.85f * climaxH);
        v.proc.jiggleGain = 0.8f + 0.9f * std::min(1.f, night_.energy);
        v.proc.hair = settings.hairPhysics;
        bool together = scene.showMarcus;
        // gaze: toward him only when he is in front of her face; behind her (from behind, prone, spooning)
        // she keeps the pose's own head and looks ahead - never a craned neck
        V3 hc, hf, hu;
        v.headAxes(hc, hf, hu);
        V3 tgt = together && m.loaded() ? m.bonePos("頭") : cam_.eye;
        V3 to = tgt - hc;
        float off = length(to) > 1e-3f ? std::acos(clampf(dot(normalize(to), hf), -1.f, 1.f)) : 0.f;
        float front = 1.f - smoothstepF(0.9f, 1.4f, off);
        v.proc.lookWeight = (together ? 0.45f : 0.8f) * front;
        v.proc.eyeWeight = (1.f - 0.7f * climaxH) * (0.35f + 0.65f * front);
        v.proc.lookTarget = front > 0.01f ? tgt : hc + hf;
        // the upper lids follow where the eyes look (v3 gaze lids)
        float pitch = std::asin(clampf(dot(normalize(v.proc.lookTarget - hc), hu), -1.f, 1.f));   // in her head's frame
        float ld, lu;
        lidsFollow(pitch, ld, lu);
        v.setMorph("lids_down", ld);
        v.setMorph("lids_up", lu);
    }
    if (m.loaded()) {
        float mBlink = hisBlink_.update(dt, 0.8f + 0.4f * him);
        m.setMorph("blink", clamp01(std::max(mBlink, 0.7f * climaxM)));
        m.setMorph("effort", clamp01(0.6f * std::min(1.f, night_.energy) + 0.5f * him * him));
        m.setMorph("pleasure", clamp01(0.7f * std::pow(him, 0.9f) + 0.5f * climaxM));
        m.setMorph("moan", clamp01(0.9f * climaxM));
        if (m.hasMorph("tender")) {   // his expression set: warmth, holding back, the moment, the afterglow, gaze lids
            float afterM = night_.finished ? std::exp(-afterT_ * 0.2f) : 0.f;
            m.setMorph("smile", clamp01(0.06f + 0.3f * night_.connection * (1.f - him) + 0.55f * afterM));
            m.setMorph("tender", clamp01(0.55f * embrace_ + 0.45f * afterM + 0.15f * night_.connection));
            m.setMorph("frown", clamp01((night_.edging ? 0.45f : 0.f) * smoothstepF(0.75f, 0.9f, him) * (1.f - climaxM)));
            m.setMorph("bite_lip", clamp01(smoothstepF(0.5f, 0.85f, him) * (0.25f + 0.2f * std::sin(time_ * 0.6f + 1.7f)) * (1.f - climaxM)));
            m.setMorph("gasp", clamp01(0.45f * climaxM));
            V3 hc, hf, hu;
            m.headAxes(hc, hf, hu);
            V3 to = m.proc.lookTarget - hc;
            float pitch = length(to) > 1e-3f ? std::asin(clampf(dot(normalize(to), hu), -1.f, 1.f)) : 0.f;
            float ld, lu;
            lidsFollow(pitch, ld, lu);
            m.setMorph("lids_down", ld);
            m.setMorph("lids_up", lu);
        }
        // clip morph rules (aroused, vag_open with depth)
        const Json& rules = meta["morphs"];
        for (auto& who : rules.obj) {
            Character* c = charOf(who.first);
            if (!c || !c->loaded()) continue;
            for (auto& r : who.second.obj) {
                float w = 0.f;
                bool ok = false;
                if (r.second.isNumber()) { w = r.second.f(); ok = true; }
                else if (r.second.isString() && r.second.s.rfind("smoothstep(", 0) == 0) {
                    float a = 0.f, b = 1.f;
                    if (std::sscanf(r.second.s.c_str(), "smoothstep(%f , %f", &a, &b) == 2 || std::sscanf(r.second.s.c_str(), "smoothstep(%f,%f", &a, &b) == 2) {
                        w = smoothstepF(a, b, depth_); ok = true;
                    }
                } else if (r.second.isString() && r.second.s.find("arousal") != std::string::npos) { w = clamp01(0.2f + him); ok = true; }
                if (ok && c->hasMorph(r.first)) c->setMorph(r.first, w);
            }
        }
        if (m.hasMorph("aroused")) {
            float k = 0.35f + 0.65f * m.morph("aroused");
            m.setBody("length", profile.him.length * k);
            m.setBody("girth", profile.him.girth * k);
        }
        m.flush = clamp01(0.1f + 0.5f * him);
        m.sweat = clamp01(sweat_ * 0.9f);
        m.proc.breathRate = 0.2f + 0.3f * him + 0.4f * climaxM;
        m.proc.breathAmp = 1.f + 1.2f * him;
        m.proc.lookWeight = 0.45f;
        m.proc.lookTarget = v.loaded() ? v.bonePos("頭") : cam_.eye;
        m.proc.hair = settings.hairPhysics;
    }

    if (camMode_ == 4 && v.loaded() && camera.mode() == CameraRig::Mode::Orbit) {
        V3 hc, hf, hu;
        v.headAxes(hc, hf, hu);
        camera.follow(hc);
    }
    if (camMode_ == 5 && v.loaded() && camera.mode() == CameraRig::Mode::Orbit) {
        V3 zp, zn;
        if (v.touch.zoneWorld(v.anim, Zone::Vulva, zp, zn)) camera.follow(zp);
    }
    // ---- camera: drift, climax push-in, afterglow pull-back ----
    camera.sway = settings.cinematicCamera ? 0.6f + 0.6f * him : 0.f;
    if (night_.finished && camera.mode() == CameraRig::Mode::Orbit && afterT_ > 2.f && afterT_ < 2.2f && !camUser_) {
        camera.setOrbit(scene.info.bedCenter + V3{0.f, 0.2f, 0.f}, camera.yaw + 0.5f, 0.42f, 2.9f, 3.2f);
    }

    // ---- post: arousal warms and saturates, climax blooms ----
    // the grade follows the night (applied on top of the player's lighting mood)
    float glow = clamp01(climaxGlow_);
    renderer.settings.flash = clamp01(flash_);
    postBloomAdd_ = 0.03f * her + 0.16f * glow;
    postWarmAdd_ = 0.04f * her;
    postSatAdd_ = 0.06f * her + 0.08f * glow;
    postVigAdd_ = 0.08f * him + 0.08f * glow;
    postExpMul_ = 1.f + 0.06f * glow;

    scene.update(dt * slowmo_);
    applyContactTracks();
}

// the content's hand / mouth contacts for the clip playing now (clips.json contacts): the skin they rest on gives
// a little under them (vertex-shader dents, the same as the touch rig's fingertips)
void Game::applyContactTracks() {
    if (posIndex_ < 0 || posIndex_ >= (int)positions_.size()) return;
    const PositionInfo& p = positions_[(size_t)posIndex_];
    auto charOf = [&](const std::string& id) -> Character* { return id == "vivian" ? &scene.vivian : (id == "marcus" ? &scene.marcus : nullptr); };
    auto run = [&](const std::string& clip, float t, float weight) {
        if (weight < 0.3f) return;
        const Json& cs = clipsMeta_["clips"][clip]["contacts"];
        for (size_t i = 0; i < cs.size(); ++i) {
            const Json& c = cs[i];
            const Json& tr = c["t"];
            if (tr.size() >= 2 && (t < tr[0].f() - 0.03f || t > tr[1].f() + 0.03f)) continue;
            float gmin = c["gap_mm"].size() >= 1 ? c["gap_mm"][0].f(0.f) : 0.f;
            if (gmin > 6.f) continue;   // hovering near the skin, not on it
            Character* eff = charOf(c["char"].str());
            Character* tgt = charOf(c["target"]["char"].str());
            if (!eff || !tgt || !eff->loaded() || !tgt->loaded()) continue;
            if ((tgt == &scene.vivian && !scene.showVivian) || (tgt == &scene.marcus && !scene.showMarcus)) continue;
            int b = eff->bone(c["bone"].str());
            if (b < 0 || tgt->touch.contacts.size() >= 8) continue;
            V3 off{c["offset"][0].f(), c["offset"][1].f(), c["offset"][2].f()};
            V3 pos = xformPoint(eff->anim.global(b), off);
            tgt->touch.contacts.push_back({pos, c["radius"].f(0.012f) + 0.004f - 0.001f * std::max(0.f, gmin)});
        }
    };
    run(p.solo ? std::string("solo.idle") : p.baseClip, p.depthDriven ? depth_ : (p.solo ? idlePhase_ : loopPhase_), 1.f);
    if (p.solo) run("solo.rub", rubPhase_, 1.f);
    else if (p.canTouch) run(p.id + ".rub", rubPhase_, rub_);
}

// her back arches with pleasure: the upper body rotates back about the shoulder line (world space)
void Game::archBody(Animator& a) const {
    if (arch_ < 0.002f) return;
    const Model* m = a.model();
    int upper = m->findNode("上半身"), sl = m->findNode("腕.L"), sr = m->findNode("腕.R");
    if (upper < 0 || sl < 0 || sr < 0) return;
    V3 axis = a.global(sr).translation() - a.global(sl).translation();
    if (length(axis) < 1e-4f) return;
    a.setWorldRot(upper, qnormalize(qmul(qaxis(normalize(axis), arch_), a.worldRot(upper))));
    a.recomputeSubtree(upper);
}

}  // namespace ag
