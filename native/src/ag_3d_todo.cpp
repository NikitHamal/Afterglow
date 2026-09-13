// Afterglow native — ag_3d_todo.cpp (stubs; see ag_3d.h + PORT_STATUS).
#include "ag_3d.h"
#include "ag_2dmods.h"

namespace ag::d3 {

void Camera::reset() { *this = Camera(); }

bool Engine::init(int, int) { return false; } // TODO js3d/engine3d.js
void Engine::resize(int, int) {}
void Engine::shutdown() {}
void Engine::beginFrame() {}
void Engine::endFrame() {}

bool Model::load(const char* p) {
  path = p ? p : "";
  return false;
} // TODO js3d/glbModel.js (cgltf, CPU skin)
void Model::unload() { loaded = false; }
void Model::pose(const Game&, double) {} // TODO poses3d+anim3d curves
void Model::draw(Engine&, const Game&) {}

bool Cast::loadAll(const char*) { return false; } // TODO js3d/chars3d.js
void Cast::pose(const Game&, double) {}
void Cast::draw(Engine&, const Game&) {}

void Fx::update(const Game&, double) {} // TODO js3d/fx3d.js
void Fx::draw(Engine&, const Game&) {}

bool App3d::init(const char*, int, int) { return false; } // TODO app3d.js
void App3d::frame(Game& g, Rng& rng, double dt) {
  app::step(g, rng, dt); // shared sim already real
}
void App3d::shutdown() {}

} // namespace ag::d3
