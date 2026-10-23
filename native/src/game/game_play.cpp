#include "game/game.h"
#include "app/platform.h"
#include "core/log.h"
#include "scene/lighting.h"
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

const char* desireHint(Desire d) {
    switch (d) {
        case Desire::Faster: return "Quicker strokes  \xc2\xb7  W / scroll faster";
        case Desire::Slower: return "Slow the rhythm  \xc2\xb7  S / gentler strokes";
        case Desire::Deeper: return "Longer, deeper strokes  \xc2\xb7  D";
        case Desire::Gentler: return "Shorter, softer strokes  \xc2\xb7  A";
        case Desire::Touch: return "Let her touch herself  \xc2\xb7  R";
        case Desire::Kiss: return "Hold her close  \xc2\xb7  E";
        case Desire::Change: return "Choose another position  \xc2\xb7  Tab";
        default: return "";
    }
}
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
    std::string clip = p.solo ? "solo.idle" : p.id + ".in";
    const Json& cams = clipsMeta_["clips"][clip]["cameras"];
    if (camMode_ == 3 && !p.solo && scene.marcus.loaded()) {
        const Json& f = cams["fpv"];
        if (f.isObject() && f["offset_local"].isArray())
            camera.setFirstPersonLocal(&scene.marcus, f["bone"].str("頭"), {f["offset_local"][0].f(), f["offset_local"][1].f(), f["offset_local"][2].f()},
                                       {f["forward_local"][0].f(), f["forward_local"][1].f(), f["forward_local"][2].f(1.f)}, blend);
        else camera.setFirstPerson(&scene.marcus, "頭", {0.f, 0.07f, 0.09f}, blend);
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
    bool overUi = barOpen_ && ui.in.mouse.y > ui.H - ui.u(190.f);

    if (!night_.finished) {
        if (IsKeyPressed(KEY_ESCAPE)) { go(Screen::Pause, 0.15f); return; }
        if (IsKeyPressed(KEY_TAB)) barOpen_ = !barOpen_;
        for (int k = 0; k < 10 && k < (int)positions_.size(); ++k)
            if (IsKeyPressed(k == 9 ? KEY_ZERO : KEY_ONE + k)) setPosition(k);
        if (IsKeyPressed(KEY_R) && p.canTouch) rubTarget_ = rubTarget_ > 0.5f ? 0.f : 1.f;
        if (IsKeyPressed(KEY_E) && p.canKiss) embraceTarget_ = embraceTarget_ > 0.5f ? 0.f : 1.f;
        if (IsKeyPressed(KEY_H)) hudHidden_ = !hudHidden_;
        if (IsKeyPressed(KEY_M)) {
            settings.lightMood = (settings.lightMood + 1) % 3;
            toasts_.push_back({0.f, moodName((Mood)settings.lightMood)});
        }
        if (IsKeyPressed(KEY_C)) { camMode_ = (camMode_ + 1) % (p.solo ? 3 : 4); cameraForPosition(0.9f); camUser_ = false; }
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
    std::string baseClip = p.solo ? "solo.idle" : p.id + ".in";
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
        for (Character* c : {&v, &m}) if (c->loaded()) c->setLayer(0, baseClip, depth_, 1.f);
        if (p.canTouch) layered(1, p.id + ".rub", rubPhase_, rub_, "vivian", "arm.R");
        if (p.canKiss) layered(2, p.id + ".embrace", 0.f, embrace_, nullptr, "upper");
        scene.showMarcus = m.loaded() && !meta["marcusHidden"].boolean(false);
    }
    scene.showVivian = v.loaded();

    // ---- bodies, faces, skin ----
    float climaxH = std::exp(-herClimaxT_ * 0.9f) * (herClimaxT_ < 6.f ? 1.f : 0.f);
    float climaxM = std::exp(-hisClimaxT_ * 0.9f) * (hisClimaxT_ < 6.f ? 1.f : 0.f);
    if (v.loaded()) {
        blinkT_ -= dt;
        if (blinkT_ <= 0.f) { blinkV_ = 1.f; blinkT_ = 1.8f + 3.5f * hash1(time_ * 3.1f) * (1.f - 0.5f * her); }
        blinkV_ = approach(blinkV_, 0.f, 14.f, dt);
        float eyesClosed = clamp01(0.25f * her * her + 0.8f * climaxH);
        v.setMorph("blink", clamp01(std::max(blinkV_, eyesClosed)));
        v.setMorph("pleasure", clamp01(0.15f + 0.75f * std::pow(her, 0.8f) + 0.4f * climaxH));
        mouthT_ = std::max(0.f, mouthT_ - dt);
        float u = mouthDur_ > 0.f ? 1.f - mouthT_ / mouthDur_ : 1.f;   // 0 -> 1 over the line
        float voiceOpen = mouthT_ > 0.f ? std::sin(3.14159f * std::min(1.f, u * 1.15f)) * (0.55f + 0.15f * std::sin(u * 23.f)) : 0.f;
        v.setMorph("moan", clamp01(moanPulse_ * (0.25f + 0.7f * her) + 0.85f * climaxH + voiceOpen));
        v.setMorph("smile", clamp01(0.12f + 0.35f * night_.connection * (1.f - her) + 0.5f * smilePulse_ + 0.6f * std::exp(-afterT_ * 0.2f) * (night_.finished ? 1.f : 0.f)));
        v.setMorph("kiss", clamp01(embrace_ * 0.8f));
        if (v.hasMorph("oral")) v.setMorph("oral", p.id == "oral" ? 1.f : 0.f);
        v.flush = clamp01(0.15f + 0.8f * her + 0.3f * climaxH);
        v.sweat = sweat_;
        v.proc.breathRate = 0.22f + 0.38f * her + 0.3f * climaxH;
        v.proc.breathAmp = 1.f + 1.4f * her + 1.5f * climaxH;
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
    }
    if (m.loaded()) {
        mBlinkT_ -= dt;
        if (mBlinkT_ <= 0.f) { mBlinkV_ = 1.f; mBlinkT_ = 2.5f + 4.f * hash1(time_ * 5.7f); }
        mBlinkV_ = approach(mBlinkV_, 0.f, 14.f, dt);
        m.setMorph("blink", clamp01(std::max(mBlinkV_, 0.7f * climaxM)));
        m.setMorph("effort", clamp01(0.6f * std::min(1.f, night_.energy) + 0.5f * him * him));
        m.setMorph("pleasure", clamp01(0.7f * std::pow(him, 0.9f) + 0.5f * climaxM));
        m.setMorph("moan", clamp01(0.9f * climaxM));
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
}

// ------------------------------------------------------------------ HUD
void Game::drawHud() {
    if (posIndex_ < 0) return;
    ui::Ui& U = ui;
    const PositionInfo& p = positions_[(size_t)posIndex_];
    float hudA = hudHidden_ ? 0.f : clamp01(hudT_ * 1.5f);
    if (night_.finished) hudA *= clamp01(1.f - afterT_ * 0.6f);
    U.alpha = hudA;
    if (hudA > 0.01f) {
        // ---- arousal: two slim meters, top left ----
        float x = U.u(56.f), y = U.u(46.f), w = U.u(330.f);
        auto meter = [&](const char* who, float v, Color c, float yy, bool pulse) {
            U.text(ui::Face::SansBold, U.u(14.f), who, x, yy, ui::withA(U.th.ink, 0.85f), ui::Left, 0.22f);
            char b[16]; std::snprintf(b, sizeof b, "%d", (int)std::lround(v * 100.f));
            U.text(ui::Face::Sans, U.u(14.f), b, x + w, yy, U.th.inkFaint, ui::Right);
            float th = U.u(5.f), by = yy + U.u(26.f);
            U.fillRect(Rectangle{x, by, w, th}, Color{255, 236, 224, 34}, th * 0.5f);
            float fw = std::max(th, w * v);
            ui::Box b2; b2.radius = th * 0.5f; b2.fill = ui::withA(c, 0.75f); b2.fill2 = c; b2.grad = 1;
            b2.shadow = U.u(10.f + (pulse ? 8.f * (0.5f + 0.5f * std::sin(time_ * 9.f)) : 0.f)); b2.shadowCol = ui::withA(c, 0.45f);
            U.box(Rectangle{x, by, fw, th}, b2);
            U.glow(x + fw, by + th * 0.5f, U.u(14.f), ui::withA(c, 0.6f));
        };
        meter("VIVIAN", night_.her, U.th.rose, y, night_.her > 0.85f);
        if (!p.solo) {
            meter("MARCUS", night_.him, U.th.gold, y + U.u(52.f), night_.him > 0.82f);
            if (night_.him > 0.82f && !night_.finished) {
                float a = 0.55f + 0.45f * std::sin(time_ * 5.f);
                U.text(ui::Face::SerifItalic, U.u(22.f), night_.edging ? "Holding back\xe2\x80\xa6" : "He's close \xe2\x80\x94 ease off",
                       x, y + U.u(112.f), ui::withA(U.th.gold, a));
            }
        }
        // ---- connection + score, top right ----
        float rx = U.W - U.u(56.f), ry = U.u(44.f);
        float pulse = 1.f + 0.06f * std::sin(time_ * (2.f + 3.f * night_.her));
        U.glow(rx - U.u(16.f), ry + U.u(16.f), U.u(26.f) * night_.connection, ui::withA(U.th.rose, 0.35f));
        U.heart(rx - U.u(16.f), ry + U.u(16.f), U.u(26.f) * pulse, ui::mixc(Color{120, 80, 90, 255}, U.th.rose, night_.connection));
        char cb[24]; std::snprintf(cb, sizeof cb, "%d%%", (int)std::lround(night_.connection * 100.f));
        U.text(ui::Face::SerifBold, U.u(30.f), cb, rx - U.u(40.f), ry, U.th.ink, ui::Right);
        U.text(ui::Face::SansBold, U.u(12.f), "CONNECTION", rx - U.u(40.f), ry + U.u(36.f), U.th.inkFaint, ui::Right, 0.22f);
        char sb[24]; std::snprintf(sb, sizeof sb, "%d", (int)std::lround(night_.score));
        U.text(ui::Face::Sans, U.u(17.f), sb, rx, ry + U.u(64.f), U.th.inkDim, ui::Right);
        // ---- position name ----
        U.text(ui::Face::SerifItalic, U.u(26.f), p.name, U.W * 0.5f, U.u(40.f), ui::withA(U.th.inkDim, 0.9f), ui::Center);

        // ---- rhythm gauge, right edge: where you are, and (hints) what she wants ----
        if (!p.solo || true) {
            float gx = U.W - U.u(70.f), gy0 = U.H * 0.32f, gh = U.H * 0.36f, gw = U.u(6.f);
            U.fillRect(Rectangle{gx, gy0, gw, gh}, Color{255, 236, 224, 26}, gw * 0.5f);
            if (settings.hints == 2) {
                float c = 0.5f, a = night_.wantAmp;
                float top = gy0 + gh * clamp01(c - a * 0.5f), bot = gy0 + gh * clamp01(c + a * 0.5f);
                ui::Box band; band.radius = U.u(5.f); band.fill = ui::withA(U.th.rose, 0.18f); band.border = std::max(1.f, U.u(1.f));
                band.borderCol = ui::withA(U.th.rose, 0.45f);
                U.box(Rectangle{gx - U.u(6.f), top, gw + U.u(12.f), std::max(U.u(8.f), bot - top)}, band);
            }
            float my = gy0 + gh * clamp01(depth_);
            U.glow(gx + gw * 0.5f, my, U.u(18.f), ui::withA(U.th.accent, 0.55f));
            ui::Box k; k.radius = U.u(7.f); k.fill = Color{255, 244, 236, 255};
            U.box(Rectangle{gx + gw * 0.5f - U.u(7.f), my - U.u(7.f), U.u(14.f), U.u(14.f)}, k);
            // tempo dots: your rhythm vs. hers
            float tx = gx + gw * 0.5f, ty = gy0 + gh + U.u(34.f);
            U.text(ui::Face::SansBold, U.u(11.f), "TEMPO", tx, ty, U.th.inkFaint, ui::Center, 0.2f);
            float yours = night_.tempo, wants = night_.wantTempo;
            char tb[32]; std::snprintf(tb, sizeof tb, "%.1f", (double)yours);
            U.text(ui::Face::SerifBold, U.u(24.f), tb, tx, ty + U.u(16.f), ui::mixc(U.th.inkDim, U.th.ink, night_.sync), ui::Center);
            if (settings.hints == 2 && !p.solo) {
                const char* arrow = yours < wants - 0.25f ? "\xe2\x86\x91" : (yours > wants + 0.3f ? "\xe2\x86\x93" : "\xe2\x80\xa2");
                U.text(ui::Face::Sans, U.u(18.f), arrow, tx, ty + U.u(46.f), ui::withA(U.th.rose, 0.9f), ui::Center);
            }
            if (rhythm_) U.text(ui::Face::SansBold, U.u(11.f), "AUTO", tx, gy0 - U.u(28.f), U.th.accent, ui::Center, 0.2f);
        }

        // ---- her voice (subtitle) + desire hint ----
        float subA = clamp01(subtitleT_ * 2.5f) * clamp01((subtitleDur_ - subtitleT_) * 1.5f);
        if (!subtitle_.empty() && subA > 0.01f) {
            float a0 = U.alpha;
            U.alpha = a0 * subA;
            float sy = U.H - U.u(190.f) - (barOpen_ ? U.u(150.f) : 0.f);
            Vector2 m = U.measure(ui::Face::SerifItalic, U.u(34.f), subtitle_);
            U.glow(U.W * 0.5f, sy + U.u(20.f), m.x * 0.35f, Color{0, 0, 0, 120});
            U.text(ui::Face::SerifItalic, U.u(34.f), "\xe2\x80\x9c" + subtitle_ + "\xe2\x80\x9d", U.W * 0.5f, sy, U.th.ink, ui::Center);
            U.alpha = a0;
        }
        if (settings.hints == 2 && desireShow_ > 0.01f && shownDesire_ != Desire::None) {
            float a0 = U.alpha;
            U.alpha = a0 * desireShow_;
            const char* h = desireHint(shownDesire_);
            float hy = U.H - U.u(136.f) - (barOpen_ ? U.u(150.f) : 0.f);
            float hw = U.measure(ui::Face::SansMedium, U.u(17.f), h).x + U.u(44.f);
            float prog = night_.desireTimeout > 0.f ? clamp01(1.f - night_.desireAge / night_.desireTimeout) : 0.f;
            Rectangle r{U.W * 0.5f - hw * 0.5f, hy, hw, U.u(38.f)};
            ui::Box b; b.radius = r.height * 0.5f; b.fill = Color{30, 16, 22, 150}; b.border = std::max(1.f, U.u(1.f));
            b.borderCol = ui::withA(U.th.rose, 0.55f);
            U.box(r, b);
            ui::Box pr; pr.radius = U.u(1.5f); pr.fill = ui::withA(U.th.rose, 0.8f);
            U.box(Rectangle{r.x + r.height * 0.5f, r.y + r.height - U.u(5.f), (r.width - r.height) * prog, U.u(2.f)}, pr);
            U.text(ui::Face::SansMedium, U.u(17.f), h, U.W * 0.5f, hy + U.u(9.f), U.th.ink, ui::Center);
            U.alpha = a0;
        }
        // ---- toasts ----
        for (size_t i = 0; i < toasts_.size(); ++i) {
            float t = toasts_[i].first;
            float a = clamp01(t * 4.f) * clamp01((2.4f - t) * 1.6f);
            float a0 = U.alpha;
            U.alpha = a0 * a;
            U.text(ui::Face::SerifBold, U.u(28.f), toasts_[i].second, U.W * 0.5f, U.H * 0.36f - U.u(30.f) * t - U.u(36.f) * (float)i,
                   U.th.accent, ui::Center);
            U.alpha = a0;
        }
        // ---- key hints (first minute, or while nothing happens) ----
        float idleHints = night_.sinceStroke > 6.f ? 1.f : 0.f;
        float hintA = std::max(clamp01((45.f - hintsT_) * 0.5f), idleHints) * (barOpen_ ? 0.f : 1.f);
        if (hintA > 0.01f) {
            float a0 = U.alpha;
            U.alpha = a0 * hintA;
            float hx = U.u(56.f), hy = U.H - U.u(64.f), px = 15.f;
            hx += U.keyHint(hx, hy, "Drag", p.solo ? "Tease" : "Thrust", px) + U.u(26.f);
            hx += U.keyHint(hx, hy, "Space", "Rhythm", px) + U.u(26.f);
            hx += U.keyHint(hx, hy, "W/S", "Tempo", px) + U.u(26.f);
            hx += U.keyHint(hx, hy, "A/D", "Depth", px) + U.u(26.f);
            if (p.canTouch) hx += U.keyHint(hx, hy, "R", "Touch", px) + U.u(26.f);
            if (p.canKiss) hx += U.keyHint(hx, hy, "E", "Embrace", px) + U.u(26.f);
            hx += U.keyHint(hx, hy, "Tab", "Positions", px) + U.u(26.f);
            hx += U.keyHint(hx, hy, "C", "Camera", px) + U.u(26.f);
            U.keyHint(hx, hy, "Esc", "Pause", px);
            U.alpha = a0;
        }
    }
    U.alpha = 1.f;
    if (introCard_ > 0.f && !hudHidden_) drawIntroCard(clamp01(std::min(introCard_ / 0.6f, (14.f - introCard_) / 0.8f)));
    barT_ = approach(barT_, barOpen_ ? 1.f : 0.f, 10.f, frameDt_);
    if (barT_ > 0.01f) drawPositionBar(barT_);
    // climax veil
    if (climaxGlow_ > 0.02f) {
        U.alpha = 1.f;
        U.glow(U.W * 0.5f, U.H * 0.5f, U.W * 0.45f * climaxGlow_, ui::withA(Color{255, 200, 170, 255}, 0.12f * climaxGlow_));
    }
}

void Game::drawIntroCard(float a) {
    ui::Ui& U = ui;
    U.alpha = a;
    float w = U.u(620.f), h = U.u(250.f), x = U.W * 0.5f - w * 0.5f, y = U.H * 0.5f - h * 0.5f - U.u(40.f) + (1.f - a) * U.u(16.f);
    U.panel(Rectangle{x, y, w, h}, 24.f, 0.6f);
    U.text(ui::Face::SansBold, U.u(13.f), "YOUR FIRST NIGHT", x + w * 0.5f, y + U.u(26.f), U.th.accent, ui::Center, 0.24f);
    U.paragraph(ui::Face::SerifItalic, U.u(25.f), "Move together, then listen. She'll tell you what she wants \xe2\x80\x94 and he'll tell you when to slow down.",
                x + U.u(40.f), y + U.u(58.f), w - U.u(80.f), U.th.ink, 1.3f, ui::Center);
    float hx = x + U.u(44.f), hy = y + h - U.u(64.f);
    hx += U.keyHint(hx, hy, "Drag", "Move", 16.f) + U.u(24.f);
    hx += U.keyHint(hx, hy, "Space", "Rhythm", 16.f) + U.u(24.f);
    hx += U.keyHint(hx, hy, "Tab", "Positions", 16.f) + U.u(24.f);
    U.keyHint(hx, hy, "Enter", "Got it", 16.f);
    U.alpha = 1.f;
}

void Game::drawPositionBar(float a) {
    ui::Ui& U = ui;
    U.alpha = a;
    int n = (int)positions_.size();
    float cw = U.u(150.f), ch = U.u(112.f), gap = U.u(14.f);
    float total = (float)n * cw + (float)(n - 1) * gap;
    float x0 = U.W * 0.5f - total * 0.5f, y0 = U.H - U.u(40.f) - ch + (1.f - ui::easeOut(a)) * U.u(60.f);
    U.panel(Rectangle{x0 - U.u(22.f), y0 - U.u(46.f), total + U.u(44.f), ch + U.u(66.f)}, 24.f, 0.62f);
    U.text(ui::Face::SansBold, U.u(13.f), "POSITIONS", x0, y0 - U.u(30.f), U.th.accent, ui::Left, 0.22f);
    U.text(ui::Face::Sans, U.u(13.f), "Number keys or click \xc2\xb7 Tab to close", x0 + total, y0 - U.u(30.f), U.th.inkFaint, ui::Right);
    for (int i = 0; i < n; ++i) {
        const PositionInfo& p = positions_[(size_t)i];
        Rectangle r{x0 + (float)i * (cw + gap), y0, cw, ch};
        bool cur = i == posIndex_;
        bool over = CheckCollisionPointRec(U.in.mouse, r);
        float h = U.anim("posbar#" + std::to_string(i), over ? 1.f : 0.f, 14.f);
        if (over && U.in.clicked) { setPosition(i); barOpen_ = false; }
        ui::Box b; b.radius = U.u(16.f);
        b.fill = cur ? ui::withA(U.th.accent, 0.2f) : Color{255, 236, 224, (unsigned char)(10 + 20 * h)};
        b.border = std::max(1.f, U.u(1.2f));
        b.borderCol = cur ? ui::withA(U.th.accent, 0.9f) : ui::mixc(U.th.hairline, ui::withA(U.th.accent, 0.6f), h);
        b.shadow = U.u(14.f) * (h + (cur ? 0.6f : 0.f)); b.shadowCol = ui::withA(U.th.accent, 0.2f);
        U.box(Rectangle{r.x, r.y - h * U.u(4.f), r.width, r.height}, b);
        char key[4]; std::snprintf(key, sizeof key, "%d", (i + 1) % 10);
        U.text(ui::Face::SansBold, U.u(13.f), key, r.x + U.u(14.f), r.y + U.u(12.f) - h * U.u(4.f), cur ? U.th.accent : U.th.inkFaint);
        float nov = night_.novelty.count(p.id) ? night_.novelty.at(p.id) : 1.f;
        for (int d = 0; d < 3; ++d) {
            bool lit = nov > 0.34f * (float)d + 0.1f;
            U.fillRect(Rectangle{r.x + r.width - U.u(16.f) - (float)d * U.u(9.f), r.y + U.u(15.f) - h * U.u(4.f), U.u(5.f), U.u(5.f)},
                       lit ? ui::withA(U.th.rose, 0.85f) : Color{255, 236, 224, 40}, U.u(2.5f));
        }
        U.paragraph(ui::Face::Serif, U.u(23.f), p.name, r.x + U.u(12.f), r.y + U.u(42.f) - h * U.u(4.f), r.width - U.u(24.f),
                    cur ? U.th.ink : ui::mixc(U.th.inkDim, U.th.ink, h), 1.0f);
    }
    U.alpha = 1.f;
}

}  // namespace ag
