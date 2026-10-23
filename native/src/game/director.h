// Afterglow — story camera direction. Shots are composed every frame from where the actors' heads are:
// establishing views of the place, wide shots, two-shots, over-the-shoulder for dialogue, singles and close-ups
// of the speaker (shallow focus), with slow push-ins while a shot holds. One side of the line between the two is
// kept for a whole scene (the 180-degree rule), so cutting between speakers never flips screen direction.
#pragma once
#include "core/mathx.h"
#include <string>

namespace ag {

struct ShotFrame {
    V3 eye{0, 1.5f, 3.f}, target{0, 1.2f, 0};
    float fov = 36.f;           // vertical, degrees
    float focus = 2.f;          // metres to the subject
    float aperture = 0.f;       // depth-of-field amount (0 = everything sharp)
};

struct ActorView {
    bool shown = false;
    V3 head{0, 1.5f, 0};        // skull centre
    V3 face{0, 0, 1};           // face forward
    V3 hips{0, 0.9f, 0};
};

class Director {
public:
    // kinds: establish | wide | two | ots | close | xclose | medium; subject 0 her, 1 him, -1 both
    void shot(const std::string& kind, int subject, bool cut, float holdSeconds);
    void newScene() { sideSet_ = false; }
    // the frame for now (call once per frame); room centre = where the open space is (the camera side)
    ShotFrame frame(float dt, const ActorView& her, const ActorView& him, const V3& roomCenter);
    bool takeCut() { bool c = cut_; cut_ = false; return c; }
    const std::string& kind() const { return kind_; }
    int subject() const { return subject_; }
    float held() const { return t_; }

private:
    std::string kind_ = "two";
    int subject_ = -1;
    float t_ = 0.f, hold_ = 6.f, side_ = 1.f, vary_ = 0.f;
    bool sideSet_ = false, cut_ = true, snap_ = true;
    V3 sEye_{0, 1.5f, 3.f}, sTarget_{0, 1.2f, 0};
};

}  // namespace ag
