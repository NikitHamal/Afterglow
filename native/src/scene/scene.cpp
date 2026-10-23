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
        // the lightmap rides in the emissive slot: on UV1 for textured materials and on UV0 for plain-coloured
        // ones (their only UV set is their lightmap chart). Any material carrying that same texture is baked.
        GLuint lightmap = 0;
        for (auto& m : room.materials)
            if (m.texEmis && m.uvEmis == 1) { lightmap = m.texEmis; break; }
        for (auto& m : room.materials) {
            if (m.shading == Shading::Unlit) { roomBaked = true; continue; }
            m.shading = Shading::Room;
            if (lightmap && m.texEmis == lightmap) { m.baked = true; roomBaked = true; }
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
        if (j["lightmap"]["scale"].isNumber()) info.bakeScale = j["lightmap"]["scale"].f(1.f);
        info.windowCenter = v3(j["window"]["center"], info.windowCenter);
        info.camMin = v3(j["cameraBounds"]["min"], info.camMin);
        info.camMax = v3(j["cameraBounds"]["max"], info.camMax);
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
            pointRole[np - 1] = n.find("candle") != std::string::npos ? 0 : (n.find("lamp") != std::string::npos ? 1 : 2);
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
    particles.update(dt, info.bedTop);
}


Character* Scene::character(const std::string& id) {
    if (id == "vivian") return &vivian;
    if (id == "marcus") return &marcus;
    return nullptr;
}

}  // namespace ag
