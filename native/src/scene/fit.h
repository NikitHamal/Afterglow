// Afterglow engine — garment fit, computed once per garment at load from its geometry alone, so any garment or
// accessory the content adds gets it without tuning:
//  - borders:  a garment cut from the body's skin ends in the staircase of the body's triangles. Each layer is
//              taken as one welded surface (seams between its primitives are not borders); every open border is
//              smoothed (real corners such as a V neckline kept), notches are filled by moving their vertices out
//              onto the smoothed line and the shaders trim the teeth beyond it, so hems, cuffs and necklines end
//              in a clean line. `border` is the signed distance to that line (m, + inside the cloth).
//  - drape:    an outer layer bridges the skin's small detail instead of copying it (border kept where it is).
//  - coverage: per skin (or under-garment) vertex, 0..1 how surely an opaque garment lies over it. Covered skin is not
//              drawn while that garment is worn, so the body cannot show through it, in any pose.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace ag {

struct FitSurface {
    const std::vector<float>* pos = nullptr;      // bind-space xyz per vertex
    const std::vector<float>* nrm = nullptr;
    const std::vector<uint32_t>* idx = nullptr;   // triangles
    const std::vector<float>* border = nullptr;   // per vertex: signed distance to the smoothed border
};

// one primitive of a garment layer; pos is edited in place (filled notches, drape), idx grows when holes are closed,
// border is written
struct FitPart {
    std::vector<float>* pos = nullptr;
    const std::vector<float>* nrm = nullptr;
    std::vector<uint32_t>* idx = nullptr;
    std::vector<float> border;
    bool idxChanged = false;
};

// a bone of the wearer in bind space: from a joint to its child
struct FitBone { float a[3], b[3]; };

// holes: where the cloth was cut around a part of the body (the content cuts garments from the skin), it has small open
// borders that no limb passes through. Each such border (perimeter under `maxPerimeter`, crossed by none of `bones` —
// the wearer's long bones the layer is weighted to) is closed with triangles over its own vertices, wound like the
// cloth around it. Returns how many were closed; parts whose idx changed are flagged.
int fillHoles(std::vector<FitPart>& parts, const std::vector<FitBone>& bones, float maxPerimeter = 0.30f);

// borders, notches and drape for one garment layer (all of its primitives); returns a one-line report
std::string fitGarment(std::vector<FitPart>& parts, bool drape);

// per vertex: 0..1 cover by `over` — a garment triangle facing the same way crosses the segment from `inner` below
// the vertex to `reach` above it along its normal, at least margin0 (fully from margin1) inside the smoothed border;
// then eroded over `erode` metres (the hidden region always ends inside the cloth: no holes at the garment's edge).
// Skin under cloth that bridges well clear of it (a dress over the cleavage) stays: the cloth hides it anyway, and a
// hidden patch there would show as a hole when seen down the gap.
std::vector<float> coverage(const std::vector<float>& pos, const std::vector<float>& nrm, const std::vector<FitSurface>& over,
                            float reach = 0.012f, float inner = 0.008f, float margin0 = 0.003f, float margin1 = 0.008f,
                            float erode = 0.01f);

}  // namespace ag
