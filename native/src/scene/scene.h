// Afterglow engine — the bedroom scene: room, the two characters, lights.
#pragma once
#include "scene/character.h"
#include "scene/procroom.h"
#include "gfx/particles.h"
#include <functional>
#include <string>

namespace ag {

struct PointLight { V3 pos, color; float intensity = 0.f, range = 3.f, flicker = 0.f, base = 0.f; };

struct Lights {
    V3 keyPos{-1.36f, 1.0f, -0.86f}, keyDir{0.8f, -0.25f, 0.55f}, keyColor{1.0f, 0.80f, 0.62f};   // ~3200K bulb
    float keyIntensity = 9.f, keyInner = 0.55f, keyOuter = 1.15f, keyRange = 7.f;   // angles in radians (half-cone)
    bool keyShadow = true;
    V3 moonDir{-0.80f, -0.42f, 0.42f}, moonColor{0.60f, 0.70f, 1.0f};
    float moonIntensity = 0.75f;
    V3 fillDir{0.10f, -0.30f, -0.95f}, fillColor{1.0f, 0.88f, 0.84f};     // soft camera-side fill so faces read
    float fillIntensity = 0.42f;
    V3 sky{0.36f, 0.32f, 0.34f}, ground{0.13f, 0.10f, 0.09f};
    float ambient = 0.6f;
    PointLight points[6];
    int numPoints = 0;
};

class Scene {
public:
    // Loads room (GLB or placeholder), Vivian and Marcus. Returns false only on fatal errors.
    bool load(const std::function<void(float, const char*)>& progress = {});
    void destroy();
    void update(float dt);                  // characters + light flicker
    Character* character(const std::string& id);

    Character vivian, marcus;
    bool showVivian = true, showMarcus = true;
    Model room;
    Animator roomAnim;
    bool roomFromGlb = false, roomBaked = false;
    RoomInfo info;
    Lights lights;
    float time = 0.f;
    bool hasClips = false;                  // content clips present (placement baked in clips)
    Particles particles;                    // shared FX (none by default)
    V3 bakeTint{1, 1, 1};                   // baked room irradiance multiplier (lighting mood x lightmap scale)
    float bakeMix = 1.f, bakeSat = 1.f;     // baked vs live room light; baked room saturation
    int pointRole[6] = {0, 0, 0, 0, 0, 0};  // 0 candle, 1 lamp, 2 other (moods scale them by role)

    // Stand both characters beside the bed (used when no clips exist yet).
    void standingPlacement();

    // the room around the bed: "room" (the default bedroom) or a story location (locations/<id>/<id>.glb,
    // same conventions); all of them are built on the authoring bed, so every clip fits each of them
    bool setLocation(const std::string& id);
    static bool hasLocation(const std::string& id);
    std::string location;

private:
    void lightsFromRoomGlb();
    void loadRoomFrom(const std::string& glbPath, const std::string& jsonPath);
};

}  // namespace ag
