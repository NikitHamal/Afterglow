// Afterglow engine — garment fit, computed once per garment at load from its geometry alone, so any garment or
// accessory the content adds gets it without tuning:
//  - border:   per garment vertex, the signed distance (m) to a smoothed copy of the garment's open borders (hems,
//              necklines, cuffs). The shaders trim the garment along that smooth line, so an edge no longer follows
//              the staircase of the body's triangles it was cut from.
//  - coverage: per skin (or under-garment) vertex, 0..1 how surely an opaque garment lies over it. Covered skin is not
//              drawn while that garment is worn, so the body cannot show through it, in any pose.
#pragma once
#include <cstdint>
#include <vector>

namespace ag {

struct FitSurface {
    const std::vector<float>* pos = nullptr;      // bind-space xyz per vertex
    const std::vector<float>* nrm = nullptr;
    const std::vector<uint32_t>* idx = nullptr;   // triangles
    const std::vector<float>* border = nullptr;   // per vertex, from borderDistance()
};

// signed distance (m, + inside) from each vertex to the garment's open borders smoothed over `smooth` metres of
// their length; vertices are welded by position first (UV seams are not borders); capped at `cap`
std::vector<float> borderDistance(const std::vector<float>& pos, const std::vector<float>& nrm, const std::vector<uint32_t>& idx,
                                  float smooth = 0.01f, float cap = 0.5f);

// per vertex: 0..1 cover by `over` — a garment triangle facing the same way crosses the segment from `inner` below
// the vertex to `reach` above it along its normal, at least margin0 (fully from margin1) inside the smoothed border;
// then eroded over `erode` metres (the hidden region always ends inside the cloth: no holes at the garment's edge)
std::vector<float> coverage(const std::vector<float>& pos, const std::vector<float>& nrm, const std::vector<FitSurface>& over,
                            float reach = 0.03f, float inner = 0.004f, float margin0 = 0.003f, float margin1 = 0.008f,
                            float erode = 0.01f);

// a garment drapes instead of copying the skin's small detail: Taubin smoothing (no shrinking) of the bind shape,
// vertices near the open border pinned (`border` from borderDistance, full effect from `pin` metres in)
void drape(std::vector<float>& pos, const std::vector<uint32_t>& idx, const std::vector<float>& border, int iterations = 12,
           float pin = 0.02f);

}  // namespace ag
