// Story QA: the kiss / embrace measured on the final pose (after every layer and the joint ranges), every frame:
// how far apart the lips are, how deep either face sinks into the other, where the noses are, how far each mouth
// ended from its IK target. Logged as "KISS-QA ..." (harness runs with kissqa=1).
#include "game/game.h"
#include "core/log.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace ag {

void Game::kissQa() {
    Character& v = scene.vivian;
    Character& m = scene.marcus;
    if (!v.loaded() || !m.loaded() || !scene.showVivian || !scene.showMarcus) return;
    V3 mv, mm;
    if (!v.touch.mouthWorld(v.anim, v.morph("kiss"), mv) || !m.touch.mouthWorld(m.anim, m.morph("kiss"), mm)) return;
    std::vector<V3> fv, fm;
    v.touch.faceWorld(v.anim, fv);
    m.touch.faceWorld(m.anim, fm);
    // how deep one face's samples sit behind the other's surface: a point counts only when it is behind a surface
    // sample within 6 mm across it (beside a face is not inside it)
    auto depth = [](const std::vector<V3>& pts, const std::vector<V3>& surf, const std::vector<V3>& nrm) {
        float worst = 0.f;
        for (const V3& q : pts) {
            float best = 1e9f; size_t bi = 0;
            for (size_t i = 0; i < surf.size(); ++i) { V3 d = q - surf[i]; float l = dot(d, d); if (l < best) { best = l; bi = i; } }
            if (surf.empty()) break;
            V3 d = q - surf[bi];
            float s = dot(d, nrm[bi]);
            if (s < 0.f && length(d - nrm[bi] * s) < 0.006f) worst = std::min(worst, s);
        }
        return worst;
    };
    std::vector<V3> nv_, nm_;
    v.touch.faceNormalsWorld(v.anim, nv_);
    m.touch.faceNormalsWorld(m.anim, nm_);
    float herIn = depth(fv, fm, nm_), himIn = depth(fm, fv, nv_);
    V3 nv, nn, nm, nmn;
    float noseV = 1.f, noseM = 1.f;
    if (v.touch.zoneWorld(v.anim, Zone::Nose, nv, nn)) noseV = m.touch.faceDistance(m.anim, nv);
    if (m.touch.zoneWorld(m.anim, Zone::Nose, nm, nmn)) noseM = v.touch.faceDistance(v.anim, nm);
    float rv = actors_[0].kissW > 0.01f ? length(mv - actors_[0].kissTarget) : 0.f;
    float rm = actors_[1].kissW > 0.01f ? length(mm - actors_[1].kissTarget) : 0.f;
    V3 rest = actors_[1].mouthRest - actors_[0].mouthRest, lip = mm - mv;
    AG_INFO("AG: KISS-QA kiss=%.2f near=%.2f lips=%.1fmm (h %.1f, up %.1f) herFaceIn=%.1fmm hisFaceIn=%.1fmm noseHer=%.1fmm noseHis=%.1fmm "
            "missHer=%.1fmm missHis=%.1fmm restGap=%.0fmm (h %.0f, up %.0f) step=%.0fmm shift=%.0fmm", kiss_, near_, length(lip) * 1000.f,
            std::sqrt(lip.x * lip.x + lip.z * lip.z) * 1000.f, lip.y * 1000.f,
            std::min(0.f, herIn) * 1000.f, std::min(0.f, himIn) * 1000.f, noseV * 1000.f, noseM * 1000.f, rv * 1000.f, rm * 1000.f,
            length(rest) * 1000.f, std::sqrt(rest.x * rest.x + rest.z * rest.z) * 1000.f, rest.y * 1000.f, step_ * 1000.f, length(kissShift_) * 1000.f);
}

}  // namespace ag
