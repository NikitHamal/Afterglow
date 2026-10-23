#include "scene/scene.h"
#include "core/paths.h"
#include "core/log.h"
#include <cctype>

namespace ag {

static std::string lowerS(std::string s) { for (auto& c : s) c = (char)std::tolower((unsigned char)c); return s; }

bool Scene::load(const std::function<void(float, const char*)>& progress) {
    auto step = [&](float f, const char* what) { if (progress) progress(f, what); };
    step(0.02f, "Lighting the candles");
    // room
    std::string roomPath = paths::findAsset({"room/room.glb"});
    if (!roomPath.empty() && room.load(roomPath)) {
        roomFromGlb = true;
        for (auto& m : room.materials) {
            if (m.shading == Shading::Unlit) { roomBaked = true; continue; }
            if (m.texEmis && m.uvEmis == 1) { m.shading = Shading::Room; roomBaked = true; continue; }
            m.shading = Shading::Room;
        }
        const Json& ag = room.sceneExtras["afterglow"];
        Json parsed;
        const Json* ex = &ag;
        if (ag.isString()) { parsed = Json::parse(ag.s); ex = &parsed; }
        if ((*ex)["bedTop"].isNumber()) info.bedTop = (*ex)["bedTop"].f();
        if ((*ex)["bedCenter"].isArray()) info.bedCenter = {(*ex)["bedCenter"][0].f(), (*ex)["bedCenter"][1].f(), (*ex)["bedCenter"][2].f()};
        else info.bedCenter.y = info.bedTop;
        if ((*ex)["lamp"].isArray()) info.lampPos = {(*ex)["lamp"][0].f(), (*ex)["lamp"][1].f(), (*ex)["lamp"][2].f()};
    } else {
        buildPlaceholderRoom(room, info);
        room.releaseCpuData();
        roomFromGlb = false;
    }
    // room.json (content pipeline): bed / lamp metadata, lenient about the exact layout
    std::string rj = paths::findAsset({"room/room.json"});
    if (!rj.empty()) {
        Json j = Json::parse(paths::readText(rj));
        const Json& bed = j["bed"].isObject() ? j["bed"] : j;
        auto v3 = [](const Json& a, V3 def) { return a.isArray() && a.size() >= 3 ? V3{a[0].f(), a[1].f(), a[2].f()} : def; };
        if (bed["top"].isNumber()) info.bedTop = bed["top"].f();
        if (j["bedTop"].isNumber()) info.bedTop = j["bedTop"].f();
        info.bedCenter = v3(bed["center"].isArray() ? bed["center"] : j["bedCenter"], info.bedCenter);
        info.bedCenter.y = info.bedTop;
        const Json& size = bed["size"].isArray() ? bed["size"] : j["bedSize"];
        if (size.isArray() && size.size() >= 2) info.bedHalf = {size[0].f() * 0.5f, 0.f, size[size.size() - 1].f() * 0.5f};
        const Json& lamp = j["lamp"].isObject() ? j["lamp"]["pos"] : j["lamp"];
        info.lampPos = v3(lamp, info.lampPos);
    }
    roomAnim.init(&room);
    roomAnim.begin();
    roomAnim.computeGlobals(M4::identity());

    // default lights relative to the bed / lamp
    lights.keyPos = info.lampPos;
    V3 aim = info.bedCenter + V3{0.f, 0.12f, 0.05f};
    lights.keyDir = normalize(aim - lights.keyPos);
    lights.numPoints = 0;
    if (!roomFromGlb) {
        for (int i = 0; i < 3; ++i) {
            PointLight& p = lights.points[lights.numPoints++];
            p.pos = info.candlePos[i] + V3{0, 0.01f, 0};
            p.color = {1.0f, 0.56f, 0.24f};
            p.base = 0.10f; p.intensity = p.base; p.range = 3.0f; p.flicker = (float)i * 1.7f + 0.3f;
        }
    }
    if (roomFromGlb) lightsFromRoomGlb();

    // characters (real-scale assets preferred; legacy web GLBs are fitted to real height)
    std::string vPath = paths::findAsset({"characters/vivian.glb", "legacy/vivian/vivian.glb", "assets/vivian/vivian.glb"});
    std::string mPath = paths::findAsset({"characters/marcus.glb", "legacy/marcus/marcus.glb", "assets/marcus/marcus.glb"});
    step(0.12f, "Vivian");
    if (vPath.empty() || !vivian.load("vivian", vPath, 1.68f)) AG_ERROR("AG: Vivian not found");
    step(0.62f, "Marcus");
    if (mPath.empty() || !marcus.load("marcus", mPath, 1.83f)) AG_ERROR("AG: Marcus not found");
    step(0.9f, "Warming up");
    hasClips = (vivian.loaded() && !vivian.model.clips.empty()) || (marcus.loaded() && !marcus.model.clips.empty());
    for (Character* c : {&vivian, &marcus}) {
        if (!c->loaded()) continue;
        c->proc.floorY = info.bedTop;
        c->setTone("caramel");
    }
    // content hair carries its colour in baseColorFactor; the legacy web hair is neutral grey with a white
    // factor, so only that one gets a tint (rich dark chestnut)
    if (vivian.loaded()) {
        for (const Material& m : vivian.model.materials)
            if (m.shading == Shading::Hair && m.baseColor.x > 0.9f && m.baseColor.y > 0.9f && m.baseColor.z > 0.9f)
                vivian.hairTint = {0.42f, 0.21f, 0.11f};
    }
    standingPlacement();
    particles.init();
    return vivian.loaded() || marcus.loaded();
}

void Scene::standingPlacement() {
    // characters without clips stand beside the bed, facing the room (+Z), feet on the floor;
    // characters with clips keep an identity root (placement is authored in the clips)
    if (vivian.loaded()) vivian.placement = vivian.model.clips.empty()
        ? m4trs({-0.42f, -vivian.footOffset(), 1.75f}, qaxis({0, 1, 0}, 0.18f), {1, 1, 1}) : M4::identity();
    if (marcus.loaded()) marcus.placement = marcus.model.clips.empty()
        ? m4trs({0.42f, -marcus.footOffset(), 1.70f}, qaxis({0, 1, 0}, -0.18f), {1, 1, 1}) : M4::identity();
}

void Scene::lightsFromRoomGlb() {
    bool haveKey = false;
    int np = 0;
    for (const Light& l : room.lights) {
        if (l.node < 0) continue;
        const M4& g = roomAnim.global(l.node);
        V3 pos = g.translation();
        V3 dir = normalize(xformDir(g, V3{0, 0, -1}));
        std::string n = lowerS(l.name);
        if (l.type == 2 && !haveKey) {
            haveKey = true;
            lights.keyPos = pos; lights.keyDir = dir; lights.keyColor = l.color;
            lights.keyIntensity = l.intensity > 50.f ? l.intensity / 120.f : l.intensity;   // W (Blender) -> engine units
            lights.keyOuter = l.outer > 0.f ? l.outer : lights.keyOuter;
            lights.keyInner = l.inner > 0.f ? l.inner : lights.keyOuter * 0.5f;
            if (l.range > 0.f) lights.keyRange = l.range;
        } else if (l.type == 0) {
            if (n.find("fill") != std::string::npos) { lights.fillDir = dir; lights.fillColor = l.color; lights.fillIntensity = l.intensity; }
            else { lights.moonDir = dir; lights.moonColor = l.color; lights.moonIntensity = l.intensity; }
        } else if (l.type == 1 && np < 6) {
            PointLight& p = lights.points[np++];
            p.pos = pos; p.color = l.color;
            p.base = l.intensity > 50.f ? l.intensity / 400.f : l.intensity;
            p.intensity = p.base;
            p.range = l.range > 0.f ? l.range : 3.f;
            p.flicker = n.find("candle") != std::string::npos ? (float)np * 1.3f + 0.2f : 0.f;
        }
    }
    if (np > 0) lights.numPoints = np;
}

void Scene::destroy() {
    particles.destroy();
    vivian.destroy();
    marcus.destroy();
    roomAnim.destroy();
    room.destroy();
}

void Scene::update(float dt) {
    time += dt;
    for (int i = 0; i < lights.numPoints; ++i) {
        PointLight& p = lights.points[i];
        if (p.flicker <= 0.f) continue;
        float f = 0.82f + 0.12f * vnoise1(time * 7.3f + p.flicker * 11.f) + 0.06f * vnoise1(time * 19.f + p.flicker);
        p.intensity = p.base * f;
    }
    if (vivian.loaded() && showVivian) vivian.update(dt);
    if (marcus.loaded() && showMarcus) marcus.update(dt);
    if (dust) updateDust(dt);
    particles.update(dt, info.bedTop);
}

// Floating dust motes, bright only where the lamp's cone reaches them (light catching the dust).
void Scene::updateDust(float dt) {
    static float acc = 0.f;
    acc += dt;
    int want = dustTarget - particles.count();
    if (want <= 0 || acc < 0.02f) return;
    acc = 0.f;
    V3 a = lights.keyPos, b = info.bedCenter + V3{0.f, 0.35f, 0.f};
    V3 dir = normalize(lights.keyDir);
    float cosOuter = std::cos(lights.keyOuter), cosInner = std::cos(lights.keyInner);
    for (int i = 0; i < std::min(want, 6); ++i) {
        float u = hash1(time * 17.3f + (float)i * 3.1f), v = hash1(time * 11.7f + (float)i * 5.3f), w = hash1(time * 7.1f + (float)i * 9.7f);
        V3 p = lerp(a, b, 0.25f + 0.75f * u) + V3{(v - 0.5f) * 1.6f, (w - 0.5f) * 1.0f, (u - 0.5f) * 1.4f};
        V3 L = p - lights.keyPos;
        float d = length(L);
        float spot = smoothstep(cosOuter, cosInner, dot(L / std::max(d, 1e-3f), dir));
        float bright = spot * lights.keyIntensity * 0.32f / (d * d + 0.3f);
        if (bright < 0.02f) continue;
        EmitParams e;
        e.pos = p;
        e.vel = {(v - 0.5f) * 0.012f, (w - 0.5f) * 0.006f, (u - 0.5f) * 0.012f};
        e.life = 9.f + 6.f * w; e.lifeJitter = 0.2f;
        e.size0 = e.size1 = 0.0018f + 0.0016f * v;
        e.color0 = {lights.keyColor.x * bright, lights.keyColor.y * bright, lights.keyColor.z * bright, 0.9f};
        e.color1 = {e.color0.x, e.color0.y, e.color0.z, 0.f};
        e.additive = true;
        particles.emit(e);
    }
}

Character* Scene::character(const std::string& id) {
    if (id == "vivian") return &vivian;
    if (id == "marcus") return &marcus;
    return nullptr;
}

}  // namespace ag
