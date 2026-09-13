// Afterglow native — ag_port_todo.cpp
// 2D is fully ported end-to-end. This TU keeps the frame compositor
// (app::draw/step — the js/app.js port) so routing lives in exactly one
// place. 3D leftovers live in ag_3d_todo.cpp (Camera::reset only).
//
// PORT_STATUS (final):
//   DONE: core, gfx, skin (+handS/footS/breastS/vulvaS/neckS/tressS/
//         hairMassS), mech, input, poses (7), side, fpv, oral (side+FPV),
//         chars (6 presets + getSkin ramps), charui (drawer), audio
//         (decode+mixer+scheduling), hud, app compositor.
//   DEDUPE (visual no-ops, file-local copies to unify later): hair/tress
//         (poses/ag_poses, side/ag_side, fpv/ag_fpv, oral/ag_oral),
//         herExpression (side + fpv + oral copies), poseBreast/poseGlute
//         (poses + oral copies), drawFPVRoom/drawFPVFluids (fpv + oral).
#include "ag_2dmods.h"

namespace ag {

namespace app {
void draw(Canvas& cv, Game& g) {
  // js/app.js: setTransform fit + route side/fpv/oral/poses + light + fluids.
  cv.setTransform((F64)cv.w() / VW, 0, 0, (F64)cv.h() / VH, 0, 0);
  if (g.view == "fpv") fpv::draw(cv, g);
  else if (g.oralT > 0 || g.oral > 0.03) oral::draw(cv, g);
  else if (g.pos != 0) {
    drawRoom(cv, g);
    drawBed(cv, g);
    poses::drawBodies(cv, g);
  } else {
    side::draw(cv, g);
  }
  drawLight(cv, g);
  drawFluids(cv, g);
  if (charui::visible(g)) charui::draw(cv, g); // DOM overlay equivalent
}
void step(Game& g, Rng& rng, F64 dt) {
  input::update(g, dt);
  mech::update(g, rng, dt);
  audio::update(g, dt);
}
} // namespace app

} // namespace ag
