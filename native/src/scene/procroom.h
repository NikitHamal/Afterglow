// Afterglow engine — procedural placeholder bedroom (used until assets/room/room.glb exists).
#pragma once
#include <string>
#include "gfx/model.h"

namespace ag {

// Matches the content pipeline's authoring bed (tools/model/anim/scene.py): centre (0, 0.604, -1.22),
// mattress 1.80 x 2.00 m, headboard toward -Z.
struct RoomInfo {
    float bedTop = 0.604f;
    V3 bedCenter{0.f, 0.604f, -1.22f};
    V3 bedHalf{0.90f, 0.f, 1.00f};
    V3 lampPos{-1.30f, 1.05f, -1.98f};
    V3 candlePos[3];
    V3 windowCenter{3.15f, 1.6f, -0.6f};
    float bakeScale = 1.f;            // room.json lightmap.scale (baked irradiance multiplier)
    V3 camMin{-2.6f, 0.12f, -2.6f}, camMax{2.6f, 2.7f, 3.2f};   // room.json cameraBounds
    std::string style, mood;          // story locations: hotel | cabin | villa, their default lighting mood
};

// Builds meshes/materials/textures into `out` (one node per object, static).
void buildPlaceholderRoom(Model& out, RoomInfo& info);

}  // namespace ag
