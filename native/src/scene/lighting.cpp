#include "scene/lighting.h"
#include "scene/scene.h"
#include "scene/camera.h"
#include "gfx/renderer.h"
#include <algorithm>
#include <cmath>

namespace ag {

const char* moodName(Mood m) {
    switch (m) {
        case Mood::WarmEvening: return "Warm evening";
        case Mood::Candlelit: return "Candlelit night";
        case Mood::Daylight: return "Bright daylight";
        default: return "";
    }
}

namespace {
struct MoodDef {
    V3 keyColor; float keyIntensity, keyInner, keyOuter;
    bool keyFromWindow;                   // daylight: the "key" is the sun through the window (shadows from there)
    V3 fillColor; float fillIntensity;    // camera-side fill: faces always read
    V3 windowColor; float windowIntensity;
    V3 sky, ground; float ambient;
    float candles, lamp2;                 // point lights by role
    float bake; V3 bakeTint;              // baked room irradiance (x lightmap scale)
    float exposure, warmth, saturation, contrast, vignette, bloom;
};

// Values tuned on the real room (Cycles lightmap) and on the placeholder: skin must read as skin.
const MoodDef kMoods[3] = {
    // warm evening (default): lamp-lit, everything readable, natural skin tones
    {{1.0f, 0.86f, 0.74f}, 12.f, 0.62f, 1.25f, false,
     {1.0f, 0.95f, 0.92f}, 0.85f,
     {0.80f, 0.86f, 1.0f}, 0.32f,
     {0.62f, 0.58f, 0.56f}, {0.26f, 0.21f, 0.18f}, 1.0f,
     0.10f, 0.55f,
     1.3f, {1.0f, 0.95f, 0.91f},
     1.2f, 0.01f, 1.0f, 1.03f, 0.16f, 0.06f},
    // candlelit night: warmer, deeper shadows, faces still clear
    {{1.0f, 0.76f, 0.56f}, 9.f, 0.55f, 1.15f, false,
     {1.0f, 0.90f, 0.84f}, 0.55f,
     {0.62f, 0.72f, 1.0f}, 0.42f,
     {0.44f, 0.39f, 0.41f}, {0.17f, 0.13f, 0.12f}, 0.66f,
     0.16f, 0.4f,
     0.95f, {1.0f, 0.93f, 0.86f},
     1.28f, 0.06f, 1.0f, 1.05f, 0.26f, 0.08f},
    // bright daylight: soft window light, neutral colour, open shadows
    {{1.0f, 0.97f, 0.93f}, 24.f, 0.8f, 1.4f, true,
     {1.0f, 0.99f, 0.97f}, 1.05f,
     {0.95f, 0.97f, 1.0f}, 0.85f,
     {0.84f, 0.88f, 0.94f}, {0.46f, 0.42f, 0.38f}, 1.75f,
     0.0f, 0.0f,
     2.6f, {0.97f, 1.03f, 1.12f},
     1.06f, -0.02f, 1.02f, 1.02f, 0.10f, 0.035f},
};
}  // namespace

void applyLighting(Scene& s, RenderSettings& rs, const LightingSetup& set, const CameraState& cam) {
    const MoodDef& m = kMoods[std::max(0, std::min(2, (int)set.mood))];
    Lights& L = s.lights;
    const RoomInfo& ri = s.info;
    float w = clampf(set.warmth, -1.f, 1.f);
    V3 tint{1.f + 0.07f * w, 1.f + 0.01f * w, 1.f - 0.12f * w};
    V3 aim = ri.bedCenter + V3{0.f, 0.2f, 0.15f};
    if (m.keyFromWindow) {
        V3 in = normalize(aim - ri.windowCenter);
        L.keyPos = ri.windowCenter - in * 0.8f + V3{0.f, 0.45f, 0.f};
    } else {
        L.keyPos = ri.lampPos + V3{0.f, 0.1f, 0.f};
    }
    L.keyDir = normalize(aim - L.keyPos);
    L.keyColor = V3{m.keyColor.x * tint.x, m.keyColor.y * tint.y, m.keyColor.z * tint.z};
    L.keyIntensity = m.keyIntensity;
    L.keyInner = m.keyInner; L.keyOuter = m.keyOuter; L.keyRange = 9.f;
    L.keyShadow = true;
    // fill from just above and beside the camera, travelling with the view
    V3 fwd = cam.target - cam.eye;
    fwd = length(fwd) > 1e-4f ? normalize(fwd) : V3{0, 0, -1};
    V3 right = cross(fwd, V3{0, 1, 0});
    right = length(right) > 1e-4f ? normalize(right) : V3{1, 0, 0};
    L.fillDir = normalize(fwd + V3{0.f, -0.35f, 0.f} + right * 0.25f);
    L.fillColor = V3{m.fillColor.x * tint.x, m.fillColor.y * tint.y, m.fillColor.z * tint.z};
    L.fillIntensity = m.fillIntensity;
    // window light (moonlight at night, soft daylight by day)
    L.moonDir = normalize(ri.bedCenter - ri.windowCenter + V3{0.f, -0.7f, 0.f});
    L.moonColor = m.windowColor; L.moonIntensity = m.windowIntensity;
    L.sky = m.sky; L.ground = m.ground; L.ambient = m.ambient;
    for (int i = 0; i < L.numPoints; ++i) {
        float base = s.pointRole[i] == 0 ? m.candles : (s.pointRole[i] == 1 ? m.lamp2 : m.candles);
        L.points[i].base = base;
        if (L.points[i].flicker <= 0.f) L.points[i].intensity = base;
    }
    s.bakeTint = m.bakeTint * (m.bake * ri.bakeScale);
    rs.exposure = m.exposure * std::pow(2.f, 0.8f * clampf(set.brightness, -1.f, 1.f));
    rs.warmth = m.warmth + 0.1f * w;
    rs.saturation = m.saturation;
    rs.contrast = m.contrast;
    rs.vignette = m.vignette;
    rs.bloomStrength = m.bloom;
}

}  // namespace ag
