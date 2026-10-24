// Afterglow engine — face identity packs: a model that shares a base character's body, skeleton, clips and outfits and
// differs in the face. characters/<model>_identity.bin (tools/model/identity) holds per-vertex position + normal deltas
// for the base GLB's own meshes; they are added to the bind data right after the GLB loads (before anything measures it).
#pragma once
#include "gfx/model.h"
#include <string>

namespace ag {

// adds the pack's deltas to the model's CPU bind data and re-uploads the touched vertex buffers; false (model untouched)
// when the file is missing or does not fit this model (mesh names / vertex counts)
bool applyIdentityPack(Model& model, const std::string& file);

}  // namespace ag
