// Afterglow engine — lighting moods. One rig, three looks: the room supplies positions (lamp, candles,
// window), the mood supplies colour, intensity, ambience, how bright the baked room reads and the grade.
// A camera-side fill follows the view so faces always read, in every mood. Brightness and warmth are
// player offsets on top.
#pragma once
#include "core/mathx.h"

namespace ag {

class Scene;
struct RenderSettings;
struct CameraState;

enum class Mood { WarmEvening = 0, Candlelit = 1, Daylight = 2, Count };
const char* moodName(Mood m);

struct LightingSetup {
    Mood mood = Mood::WarmEvening;
    float brightness = 0.f;   // -1..1 (about +-0.8 EV)
    float warmth = 0.f;       // -1..1
};

// Sets scene.lights (+ room bake tint) and the colour grade in rs for this setup. Call once per frame
// (cheap): the fill light follows the camera.
void applyLighting(Scene& scene, RenderSettings& rs, const LightingSetup& setup, const CameraState& cam);

}  // namespace ag
