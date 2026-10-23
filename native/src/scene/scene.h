// Afterglow engine — the bedroom scene: room, the two characters, lights.
#pragma once
#include "scene/character.h"
#include "scene/procroom.h"
#include <string>

namespace ag {

struct PointLight { V3 pos, color; float intensity = 0.f, range = 3.f, flicker = 0.f, base = 0.f; };

struct Lights {
    V3 keyPos{-1.36f, 1.0f, -0.86f}, keyDir{0.8f, -0.25f, 0.55f}, keyColor{1.0f, 0.70f, 0.42f};
    float keyIntensity = 7.f, keyInner = 0.55f, keyOuter = 1.15f, keyRange = 7.f;   // angles in radians (half-cone)
    bool keyShadow = true;
    V3 moonDir{-0.80f, -0.42f, 0.42f}, moonColor{0.55f, 0.66f, 1.0f};
    float moonIntensity = 0.55f;
    V3 fillDir{0.15f, -0.35f, -0.92f}, fillColor{0.95f, 0.72f, 0.80f};
    float fillIntensity = 0.12f;
    V3 sky{0.30f, 0.20f, 0.24f}, ground{0.10f, 0.05f, 0.05f};
    float ambient = 0.55f;
    PointLight points[6];
    int numPoints = 0;
};

class Scene {
public:
    // Loads room (GLB or placeholder), Vivian and Marcus. Returns false only on fatal errors.
    bool load();
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

    // Stand both characters beside the bed (used when no clips exist yet).
    void standingPlacement();

private:
    void lightsFromRoomGlb();
};

}  // namespace ag
