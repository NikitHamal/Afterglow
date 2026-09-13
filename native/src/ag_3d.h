// Afterglow native — ag_3d.h
// 3D-mode surface (1:1 with js3d/*). Bodies are TODO stubs; this header locks
// the architecture so the 2D port never boxes 3D in:
//   - shared sim lives in Game (js/core|input|mechanics reused unchanged,
//     exactly like 3d.html reuses js/*.js)
//   - rendering is GLES2-level (works on Windows via SDL_GL + on Android via
//     the same SDL activity): PBR-lite matching three.js r150 params,
//     glTF via cgltf, CPU skinning (models are small), fixed camera rig
//     mirroring js3d/engine3d.js (orbit/side/top/fpv-look + RST).
#pragma once
#include "ag_core.h"

namespace ag::d3 {

struct Camera {
  F64 yaw = 0, pitch = 0, dist = 6, tx = 0, ty = 1.2, tz = 0;
  F64 fov = 45;
  void reset();
};

struct Engine {
  bool ready = false;
  Camera cam;
  bool init(int w, int h); // GL context owned by platform
  void resize(int w, int h);
  void shutdown();
  void beginFrame();
  void endFrame();
};

// js3d/glbModel.js — cgltf load + CPU skinning into a dynamic VBO.
struct Model {
  bool loaded = false;
  std::string path;
  bool load(const char* glbPath);
  void unload();
  void pose(const Game& g, double dt); // js3d/poses3d.js + anim3d.js curves
  void draw(Engine& e, const Game& g);
};

// js3d/chars3d.js — procedural Yuki/Him assembly on top of Model/glTF.
struct Cast {
  Model her, him;
  bool loadAll(const char* assetDir);
  void pose(const Game& g, double dt);
  void draw(Engine& e, const Game& g);
};

// js3d/fx3d.js — GPU-light particles (hearts/dust/bloom sprites).
struct Fx {
  void update(const Game& g, double dt);
  void draw(Engine& e, const Game& g);
};

// js3d/app3d.js — frame entry: shared sim step + 3D render.
struct App3d {
  Engine eng;
  Cast cast;
  Fx fx;
  bool lookMode = false;
  bool init(const char* assetDir, int w, int h);
  void frame(Game& g, Rng& rng, double dt); // step + render
  void shutdown();
};

} // namespace ag::d3
