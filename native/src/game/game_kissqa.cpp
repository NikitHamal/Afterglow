// Story QA: the kiss / embrace measured on the final pose (after every layer and the joint ranges), every frame:
// how far apart the lips are, how deep either face sinks into the other, where the noses are, how far each mouth
// ended from its IK target. Logged as "KISS-QA ..." (harness runs with kissqa=1).
#include "game/game.h"
#include "core/log.h"
#include "scene/oral.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace ag {

// Play QA: the mouth and nose against the shaft's own axis, on the final pose. A mouth on the shaft has a small
// radial distance and a mouth-axis position between root and the tip; a nose closer to the shaft than the mouth
// means the pose is aiming the wrong part of the face.
void Game::oralQa() {
    Character& v = scene.vivian;
    Character& m = scene.marcus;
    if (!v.loaded() || !m.loaded() || !scene.showVivian || !scene.showMarcus) return;
    V3 mv, mn, nv, nn, av;
    if (!v.touch.zoneWorld(v.anim, Zone::Mouth, mv, mn) || !v.touch.zoneWorld(v.anim, Zone::Nose, nv, nn)) return;
    if (!v.touch.mouthApertureWorld(v.anim, av)) av = mv;
    V3 root, chord, end, axis;
    if (!oralShaft(m, root, chord, end, axis)) return;
    float len = length(end - root);
    float sM = dot(mv - root, chord), sN = dot(nv - root, axis), sA = dot(av - root, axis);
    float rM = length(mv - (root + chord * sM)), rN = length(nv - (root + axis * sN));
    float rA = length(av - (root + axis * sA));
    float bend = std::acos(clampf(dot(chord, axis), -1.f, 1.f)) * 57.29578f;
    V3 face = qrotate(v.anim.worldRot(v.bone("頭")), v.procedural.headForwardLocal());
    AG_INFO("AG: ORAL-QA depth=%.2f mouthTip=%.1fmm radial=%.1fmm apTip=%.1fmm apAx=%.0f/%.0fmm apRadial=%.1fmm "
            "apMiss=%.1fmm bend=%.0fdeg noseTip=%.1fmm noseAx=%.0fmm noseRadial=%.1fmm faceRoot=%.2f",
            depth_, length(mv - end) * 1000.f, rM * 1000.f, length(av - end) * 1000.f, sA * 1000.f, len * 1000.f,
            rA * 1000.f, length(av - v.oralTarget) * 1000.f, bend, length(nv - end) * 1000.f, sN * 1000.f, rN * 1000.f,
            -dot(face, axis));
}

// The live version of the same measurement: one line a second while the oral act plays, so a report from the game
// itself can be read against the harness instead of guessed at.
void Game::oralLiveLog() {
    if (harness || posIndex_ < 0 || posIndex_ >= (int)positions_.size()) return;
    const PositionInfo& p = positions_[(size_t)posIndex_];
    if (p.id != "oral" || time_ - oralLogT_ < 1.f) return;
    oralLogT_ = time_;
    Character& v = scene.vivian;
    Character& m = scene.marcus;
    float miss = -1.f, radial = -1.f, tip = -1.f, bend = -1.f;
    V3 root, chord, end, axis, av;
    if (v.loaded() && m.loaded() && oralShaft(m, root, chord, end, axis) && v.touch.mouthApertureWorld(v.anim, av)) {
        float s = dot(av - root, chord);
        miss = length(av - v.oralTarget) * 1000.f;
        radial = length(av - (root + chord * s)) * 1000.f;
        tip = length(av - end) * 1000.f;
        bend = std::acos(clampf(dot(chord, axis), -1.f, 1.f)) * 57.29578f;
    }
    AG_INFO("AG: ORAL-LIVE pos=%s depth=%.2f oralW=%.2f apTip=%.1fmm apRadial=%.1fmm apMiss=%.1fmm bend=%.0fdeg "
            "grip=%d/%d rub=%.2f embrace=%.2f pending=%d fade=%.2f aroused=%.2f len=%.2f girth=%.2f her=%.2f him=%.2f conn=%.2f",
            p.id.c_str(), depth_, v.oralW, tip, radial, miss, bend, (int)v.touchGoals.left.active, (int)v.touchGoals.right.active,
            rub_, embrace_, pendingPos_, posFade_, m.loaded() ? m.morph("aroused") : -1.f,
            m.loaded() ? m.morph("length_up") : -1.f, m.loaded() ? m.morph("girth_up") : -1.f, night_.her, night_.him,
            night_.connection);
}

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
