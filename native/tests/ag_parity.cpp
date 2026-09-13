// Afterglow native — ag_parity.cpp
// Headless pixel harness (CI): scripted multi-view run, deterministic hashes,
// cache-cleared redraw identity, PPM frames for human visual review against
// the web build's .shots/*.png. Exit nonzero on any mismatch.
#include "../src/ag_2dmods.h"
#include "../src/ag_platform.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char** argv) {
  const char* out = "parity-out";
  int frames = 24;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--out") && i + 1 < argc) out = argv[++i];
    else if (!std::strcmp(argv[i], "--frames") && i + 1 < argc)
      frames = std::atoi(argv[++i]);
  }
  const char* views[3] = {"side", "fpv", "oral"};
  for (const char* v : views) {
    ag::HeadlessOpts o;
    o.frames = frames;
    o.outDir = std::string(out) + "/" + v;
    o.view = v;
    std::printf("== view %s ==\n", v);
    int rc = ag::headlessRun(o);
    if (rc != 0) {
      std::printf("PARITY FAIL view=%s\n", v);
      return 1;
    }
  }
  std::printf("PARITY OK\n");
  return 0;
}
