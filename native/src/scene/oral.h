// Afterglow scene — the oral staging, shared by every path that plays the oral clip (the night, a story act and
// the engine sandbox). The baked clip alone leaves her mouth below his glans, so each path has to run this itself.
#pragma once
#include "core/mathx.h"
#include <string>

namespace ag {

class Character;
struct TouchGoals;

struct OralTuning {
    float open = 2.2f;          // jaw opening, before the girth allowance
    float up = 0.f;             // extra lift of the mouth target along her head's up axis (QA knob)
    float curl = 0.35f;         // finger curl used when the grip solver stands down (QA knob)
    bool grip = true;           // put both her hands on the shaft
    float wristL = 0.f;         // forearm pronation per hand, only when the grip stands down (QA knobs)
    float wristR = 0.f;
};

// The rendered glans: skinned to 竿先, its surface tip moves forward with the length morph. The 竿端 helper carries
// no skin weights, so aiming at it left the lips behind the glans on large custom sizes. `chord` is the straight
// root→tip line; `axis` is the direction the glans itself points, which is what a mouth has to follow when he is
// curved (the two come apart by up to a dozen-odd degrees).
bool oralShaft(const Character& him, V3& root, V3& chord, V3& tip, V3& axis);

// Her jaw open, the lip ring on the shaft axis, the face turned back along it; active = false stands all of it down.
void stageOral(Character& her, Character& him, float depth, const OralTuning& t, bool active);

// Both hands around the shaft. A hand another goal already owns (her own touch) keeps that job.
void oralGrip(Character& her, Character& him, TouchGoals& tg, const OralTuning& t);

}  // namespace ag
