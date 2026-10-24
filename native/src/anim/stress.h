// Afterglow engine — QA stress poses: extreme-but-anatomical joint configurations (arms overhead, crossed, full
// elbow / knee flexion, wrist twist, deep squat, splits, spine twist, neck turn, arms behind the back) applied over
// the rest pose as a pose hook, so skinning, garments and joint ranges can be checked on every rig the same way.
#pragma once
#include "anim/animator.h"
#include <string>
#include <vector>

namespace ag {

// poses the animator (globals must be current; the root matrix gives the body frame: +Z front, +Y up, +X left).
// Returns false for an unknown name. `amount` 0..1 blends from the rest pose.
bool stressPose(Animator& a, const std::string& name, float amount = 1.f);
const std::vector<std::string>& stressPoseNames();

}  // namespace ag
