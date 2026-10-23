#include "gfx/particles.h"
#include "scene/camera.h"
#include <algorithm>
#include <cmath>

namespace ag {

// per vertex: center(3) corner(2) size(1) color(4) dir(3) stretch(1) = 14 floats
static const int kStride = 14;

bool Particles::init() {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, 16, nullptr, GL_DYNAMIC_DRAW);
    const GLsizei st = kStride * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, st, (void*)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, st, (void*)(3 * sizeof(float)));
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, st, (void*)(5 * sizeof(float)));
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, st, (void*)(6 * sizeof(float)));
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, st, (void*)(10 * sizeof(float)));
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, st, (void*)(13 * sizeof(float)));
    for (GLuint i = 0; i < 6; ++i) glEnableVertexAttribArray(i);
    glBindVertexArray(0);
    return prog_.build("particles", shaderSource("particle.vert"), shaderSource("particle.frag"));
}

void Particles::destroy() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    vbo_ = vao_ = 0;
    prog_.destroy();
    ps_.clear();
}

float Particles::rnd() {   // xorshift, -1..1
    seed_ ^= seed_ << 13; seed_ ^= seed_ >> 17; seed_ ^= seed_ << 5;
    return (float)(seed_ & 0xffffff) / 8388607.5f - 1.f;
}

void Particles::emit(const EmitParams& e) {
    for (int i = 0; i < e.count && (int)ps_.size() < maxParticles; ++i) {
        P p;
        p.p = e.pos + V3{e.posJitter.x * rnd(), e.posJitter.y * rnd(), e.posJitter.z * rnd()};
        p.v = e.vel + V3{e.velJitter.x * rnd(), e.velJitter.y * rnd(), e.velJitter.z * rnd()};
        p.age = 0.f;
        p.life = std::max(0.05f, e.life * (1.f + e.lifeJitter * rnd()));
        p.s0 = e.size0; p.s1 = e.size1; p.drag = e.drag; p.grav = e.gravity;
        p.c0 = e.color0; p.c1 = e.color1;
        p.add = e.additive; p.stretch = e.stretch; p.floor = e.collideFloor; p.stuck = false;
        ps_.push_back(p);
    }
}

void Particles::update(float dt, float floorY) {
    if (dt <= 0.f) return;
    for (P& p : ps_) {
        p.age += dt;
        if (p.stuck) continue;
        p.v.y -= p.grav * dt;
        p.v *= std::max(0.f, 1.f - p.drag * dt);
        p.p += p.v * dt;
        if (p.floor && p.p.y < floorY) { p.p.y = floorY + 0.001f; p.v = {0, 0, 0}; p.stuck = true; }
    }
    ps_.erase(std::remove_if(ps_.begin(), ps_.end(), [](const P& p) { return p.age >= p.life; }), ps_.end());
}

void Particles::render(const CameraState& cam) {
    if (ps_.empty()) return;
    V3 right{cam.view.m[0], cam.view.m[4], cam.view.m[8]};
    V3 up{cam.view.m[1], cam.view.m[5], cam.view.m[9]};
    prog_.use();
    prog_.set("u_viewProj", cam.viewProj);
    prog_.set("u_right", right);
    prog_.set("u_up", up);
    prog_.set("u_camPos", cam.eye);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    static const float corners[6][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, -1}, {1, 1}, {-1, 1}};
    for (int pass = 0; pass < 2; ++pass) {        // 0 alpha-blended, 1 additive
        verts_.clear();
        for (const P& p : ps_) {
            if (p.add != (pass == 1)) continue;
            float t = std::min(1.f, p.age / p.life);
            float size = p.s0 + (p.s1 - p.s0) * t;
            V4 c{p.c0.x + (p.c1.x - p.c0.x) * t, p.c0.y + (p.c1.y - p.c0.y) * t, p.c0.z + (p.c1.z - p.c0.z) * t, p.c0.w + (p.c1.w - p.c0.w) * t};
            c.w *= smoothstep(0.f, 0.08f, t);   // no pop-in
            float sp = length(p.v);
            V3 dir = sp > 1e-4f ? p.v / sp : V3{0, 1, 0};
            float st = (p.stretch && !p.stuck) ? std::min(6.f, 1.f + sp * 0.06f / std::max(size, 1e-4f)) : 1.f;
            for (auto& cr : corners) {
                float v[kStride] = {p.p.x, p.p.y, p.p.z, cr[0], cr[1], size, c.x, c.y, c.z, c.w, dir.x, dir.y, dir.z, st};
                verts_.insert(verts_.end(), v, v + kStride);
            }
        }
        if (verts_.empty()) continue;
        if (pass == 1) glBlendFunc(GL_ONE, GL_ONE);
        else glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        prog_.set("u_additive", pass);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts_.size() * sizeof(float)), verts_.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(verts_.size() / kStride));
    }
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glBindVertexArray(0);
}

}  // namespace ag
