// Afterglow native — ag_3d_todo.cpp
// 3D is fully ported end-to-end (engine/glb/cast/animfx/app). Only
// Camera::reset stays here (trivial one-liner).
//
// PORT_STATUS (final 3D):
//   DONE: engine3d (camera rig + PBR-lite + buckets), glbModel (cgltf load,
//         repair, CPU skinning), chars3d+poses3d (registry, 10 pose table),
//         anim3d+fx3d (curves + additive sprites), app3d (frame + GL HUD).
//   TODO(platform): declare d3SetCamMode/d3PushKey/d3PushWheel/d3AddShake/
//         d3FxPass in ag_3d.h and hook SDL input + G.shake (see engine file).
#include "ag_3d.h"

namespace ag::d3 {

void Camera::reset() { *this = Camera(); }

} // namespace ag::d3
