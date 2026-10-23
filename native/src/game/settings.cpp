#include "game/settings.h"
#include "core/json.h"
#include "core/paths.h"
#include <algorithm>

namespace ag {

const int Settings::kFpsCaps[5] = {30, 60, 120, 144, 0};

int Settings::fpsLimit() const { return kFpsCaps[std::max(0, std::min(fpsCap, 4))]; }

namespace {
float rd(const Json& j, const char* k, float def, float lo, float hi) { return std::max(lo, std::min(hi, j[k].f(def))); }
int ri(const Json& j, const char* k, int def, int lo, int hi) { return std::max(lo, std::min(hi, (int)j[k].num(def))); }
bool rb(const Json& j, const char* k, bool def) { return j[k].boolean(def); }
}  // namespace

bool Settings::load(const std::string& path) {
    std::string t = paths::readText(path);
    if (t.empty()) return false;
    Json j = Json::parse(t);
    if (!j.isObject()) return false;
    windowMode = ri(j, "windowMode", windowMode, 0, 2);
    winW = ri(j, "winW", winW, 640, 16384);
    winH = ri(j, "winH", winH, 360, 16384);
    vsync = rb(j, "vsync", vsync);
    fpsCap = ri(j, "fpsCap", fpsCap, 0, 4);
    renderScale = rd(j, "renderScale", renderScale, 0.5f, 1.f);
    quality = ri(j, "quality", quality, 0, 2);
    bloom = rb(j, "bloom", bloom);
    ssao = rb(j, "ssao", ssao);
    filmGrain = rb(j, "filmGrain", filmGrain);
    hairPhysics = rb(j, "hairPhysics", hairPhysics);
    master = rd(j, "master", master, 0.f, 1.f);
    voice = rd(j, "voice", voice, 0.f, 1.f);
    effects = rd(j, "effects", effects, 0.f, 1.f);
    ambience = rd(j, "ambience", ambience, 0.f, 1.f);
    music = rd(j, "music", music, 0.f, 1.f);
    mouseSens = rd(j, "mouseSens", mouseSens, 0.2f, 3.f);
    invertY = rb(j, "invertY", invertY);
    holdRhythm = rb(j, "holdRhythm", holdRhythm);
    hints = ri(j, "hints", hints, 0, 2);
    cinematicCamera = rb(j, "cinematicCamera", cinematicCamera);
    checkUpdates = rb(j, "checkUpdates", checkUpdates);
    pauseWhenUnfocused = rb(j, "pauseWhenUnfocused", pauseWhenUnfocused);
    skippedVersion = j["skippedVersion"].str();
    return true;
}

bool Settings::save(const std::string& path) const {
    Json j = Json::object();
    j.set("version", Json::of(1));
    j.set("windowMode", Json::of(windowMode));
    j.set("winW", Json::of(winW));
    j.set("winH", Json::of(winH));
    j.set("vsync", Json::ofBool(vsync));
    j.set("fpsCap", Json::of(fpsCap));
    j.set("renderScale", Json::of(renderScale));
    j.set("quality", Json::of(quality));
    j.set("bloom", Json::ofBool(bloom));
    j.set("ssao", Json::ofBool(ssao));
    j.set("filmGrain", Json::ofBool(filmGrain));
    j.set("hairPhysics", Json::ofBool(hairPhysics));
    j.set("master", Json::of(master));
    j.set("voice", Json::of(voice));
    j.set("effects", Json::of(effects));
    j.set("ambience", Json::of(ambience));
    j.set("music", Json::of(music));
    j.set("mouseSens", Json::of(mouseSens));
    j.set("invertY", Json::ofBool(invertY));
    j.set("holdRhythm", Json::ofBool(holdRhythm));
    j.set("hints", Json::of(hints));
    j.set("cinematicCamera", Json::ofBool(cinematicCamera));
    j.set("checkUpdates", Json::ofBool(checkUpdates));
    j.set("pauseWhenUnfocused", Json::ofBool(pauseWhenUnfocused));
    j.set("skippedVersion", Json::of(skippedVersion));
    return paths::writeAtomic(path, j.dump());
}

bool Profile::load(const std::string& path) {
    std::string t = paths::readText(path);
    if (t.empty()) return false;
    Json j = Json::parse(t);
    if (!j.isObject()) return false;
    const Json& h = j["her"];
    her.bust = rd(h, "bust", 0.f, -1.f, 1.f);
    her.hips = rd(h, "hips", 0.f, -1.f, 1.f);
    her.waist = rd(h, "waist", 0.f, -1.f, 1.f);
    her.butt = rd(h, "butt", 0.f, -1.f, 1.f);
    her.weight = rd(h, "weight", 0.f, -1.f, 1.f);
    her.muscle = rd(h, "muscle", 0.f, 0.f, 1.f);
    her.belly = rd(h, "belly", 0.f, 0.f, 1.f);
    her.tone = ri(h, "tone", 1, 0, 2);
    her.hair = ri(h, "hair", 0, 0, 15);
    const Json& m = j["him"];
    him.muscle = rd(m, "muscle", 0.f, -1.f, 1.f);
    him.weight = rd(m, "weight", 0.f, -1.f, 1.f);
    him.belly = rd(m, "belly", 0.f, 0.f, 1.f);
    him.length = rd(m, "length", 0.f, -1.f, 1.f);
    him.girth = rd(m, "girth", 0.f, -1.f, 1.f);
    him.tone = ri(m, "tone", 1, 0, 2);
    const Json& s = j["stats"];
    stats.nights = ri(s, "nights", 0, 0, 1 << 30);
    stats.together = ri(s, "together", 0, 0, 1 << 30);
    stats.herClimaxes = ri(s, "herClimaxes", 0, 0, 1 << 30);
    stats.bestScore = ri(s, "bestScore", 0, 0, 1 << 30);
    stats.bestConnection = rd(s, "bestConnection", 0.f, 0.f, 1.f);
    stats.totalMinutes = rd(s, "totalMinutes", 0.f, 0.f, 1e9f);
    const Json& n = j["night"];
    night = SavedNight{};
    if (n.isObject() && n["valid"].boolean(false)) {
        night.valid = true;
        night.pos = n["pos"].str("missionary");
        night.her = rd(n, "her", 0.f, 0.f, 0.99f);
        night.him = rd(n, "him", 0.f, 0.f, 0.99f);
        night.connection = rd(n, "connection", 0.5f, 0.f, 1.f);
        night.score = rd(n, "score", 0.f, 0.f, 1e9f);
        night.elapsed = rd(n, "elapsed", 0.f, 0.f, 1e7f);
        night.sweat = rd(n, "sweat", 0.f, 0.f, 1.f);
        night.herClimaxes = ri(n, "herClimaxes", 0, 0, 999);
        night.desiresMet = ri(n, "desiresMet", 0, 0, 1 << 20);
        night.desiresMissed = ri(n, "desiresMissed", 0, 0, 1 << 20);
        night.seed = (unsigned)n["seed"].num(1);
        for (auto& kv : n["novelty"].obj) night.novelty[kv.first] = std::max(0.f, std::min(1.f, kv.second.f(1.f)));
    }
    return true;
}

bool Profile::save(const std::string& path) const {
    Json j = Json::object();
    j.set("version", Json::of(1));
    Json h = Json::object();
    h.set("bust", Json::of(her.bust)); h.set("hips", Json::of(her.hips)); h.set("waist", Json::of(her.waist));
    h.set("butt", Json::of(her.butt)); h.set("weight", Json::of(her.weight)); h.set("muscle", Json::of(her.muscle));
    h.set("belly", Json::of(her.belly)); h.set("tone", Json::of(her.tone)); h.set("hair", Json::of(her.hair));
    j.set("her", h);
    Json m = Json::object();
    m.set("muscle", Json::of(him.muscle)); m.set("weight", Json::of(him.weight)); m.set("belly", Json::of(him.belly));
    m.set("length", Json::of(him.length)); m.set("girth", Json::of(him.girth)); m.set("tone", Json::of(him.tone));
    j.set("him", m);
    Json s = Json::object();
    s.set("nights", Json::of(stats.nights)); s.set("together", Json::of(stats.together));
    s.set("herClimaxes", Json::of(stats.herClimaxes)); s.set("bestScore", Json::of(stats.bestScore));
    s.set("bestConnection", Json::of(stats.bestConnection)); s.set("totalMinutes", Json::of(stats.totalMinutes));
    j.set("stats", s);
    Json n = Json::object();
    n.set("valid", Json::ofBool(night.valid));
    if (night.valid) {
        n.set("pos", Json::of(night.pos)); n.set("her", Json::of(night.her)); n.set("him", Json::of(night.him));
        n.set("connection", Json::of(night.connection)); n.set("score", Json::of(night.score));
        n.set("elapsed", Json::of(night.elapsed)); n.set("sweat", Json::of(night.sweat));
        n.set("herClimaxes", Json::of(night.herClimaxes)); n.set("desiresMet", Json::of(night.desiresMet));
        n.set("desiresMissed", Json::of(night.desiresMissed)); n.set("seed", Json::of((double)night.seed));
        Json nov = Json::object();
        for (auto& kv : night.novelty) nov.set(kv.first, Json::of(kv.second));
        n.set("novelty", nov);
    }
    j.set("night", n);
    return paths::writeAtomic(path, j.dump());
}

}  // namespace ag
