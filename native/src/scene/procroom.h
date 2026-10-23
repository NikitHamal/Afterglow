// Afterglow engine — procedural placeholder bedroom (used until assets/room/room.glb exists).
#pragma once
#include "gfx/model.h"

namespace ag {

struct RoomInfo {
    float bedTop = 0.556f;
    V3 bedCenter{0.f, 0.556f, 0.05f};
    V3 bedHalf{0.95f, 0.f, 1.15f};
    V3 lampPos{-1.35f, 1.02f, -0.85f};
    V3 candlePos[3];
    V3 windowCenter{3.15f, 1.6f, -0.6f};
};

// Builds meshes/materials/textures into `out` (one node per object, static).
void buildPlaceholderRoom(Model& out, RoomInfo& info);

}  // namespace ag
