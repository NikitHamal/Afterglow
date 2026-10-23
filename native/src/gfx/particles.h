// Afterglow engine — CPU particles drawn as camera-facing soft sprites (additive or alpha-blended),
// optionally stretched along velocity. Used for dust motes, sweat drops, fluids, sparkles.
#pragma once
#include "gfx/shader.h"
#include "core/mathx.h"
#include <vector>

namespace ag {

struct CameraState;

struct EmitParams {
    V3 pos{0, 0, 0}, posJitter{0, 0, 0};
    V3 vel{0, 0, 0}, velJitter{0, 0, 0};
    float gravity = 0.f;          // m/s^2 downward
    float drag = 0.f;             // per second
    float life = 1.f, lifeJitter = 0.2f;
    float size0 = 0.01f, size1 = 0.01f;   // metres (radius), start -> end
    V4 color0{1, 1, 1, 1}, color1{1, 1, 1, 0};   // linear HDR rgb + alpha, start -> end
    bool additive = true;
    bool stretch = false;         // elongate along velocity (droplets, streaks)
    bool collideFloor = false;    // stop on the floor plane (stick)
    int count = 1;
};

class Particles {
public:
    bool init();
    void destroy();
    void emit(const EmitParams& e);
    void update(float dt, float floorY);
    void render(const CameraState& cam);
    void clear() { ps_.clear(); }
    int count() const { return (int)ps_.size(); }
    int maxParticles = 6000;

private:
    struct P { V3 p, v; float age, life, s0, s1, drag, grav; V4 c0, c1; bool add, stretch, floor, stuck; };
    std::vector<P> ps_;
    std::vector<float> verts_;
    GLuint vao_ = 0, vbo_ = 0;
    Program prog_;
    unsigned seed_ = 12345u;
    float rnd();
};

}  // namespace ag
