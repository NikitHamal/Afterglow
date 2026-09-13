// Afterglow native — ag_platform.h (entry + headless runner).
#pragma once
#include <string>

namespace ag {

struct HeadlessOpts {
  int frames = 24;
  std::string outDir; // empty = no PPM output
  std::string view = "side";
  double t0 = 25, pleasure = 62, depth = 0.55;
};
// Deterministic scripted run: prints per-frame hashes, verifies
// cache-cleared redraw identity. Returns 0 on success.
int headlessRun(const HeadlessOpts& o);
// Interactive (SDL) or headless fallback when built AG_WITH_SDL=OFF.
int runGame(int argc, char** argv);

} // namespace ag
