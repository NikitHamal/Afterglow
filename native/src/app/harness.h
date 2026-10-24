// Afterglow engine — screenshot harness (CI visual QA):
//   afterglow --shot out.png --scene "pos=missionary;depth=0.6;cam=side" [--frames 45] [--res 1280x720]
//   afterglow --shotlist tools/qa/shots.txt [--outdir qa-out]     (lines: name|spec, '#' comments)
//   --hidpi        open the window like the game does on macOS (FLAG_WINDOW_HIGHDPI) — with Xft.dpi set under Xvfb
//                  this reproduces display scaling (125 % / 150 % / 200 %) on Linux
//   --legacy-dpi   the old window model (FLAG_WINDOW_HIGHDPI on every platform), to reproduce its bugs
//   --resize WxH   resize the window after it opened (what a user, a maximise or a mode switch does)
#pragma once
#include "app/app.h"
#include <functional>
#include <memory>
#include <string>

namespace ag {

struct HarnessArgs {
    std::string shot, scene, shotlist, outdir = "qa-out";
    int frames = 45, width = 1280, height = 720;
    bool hidpi = false, legacyDpi = false;
    int resizeW = 0, resizeH = 0;
};

// Parses harness flags; returns true when the process should run in harness mode.
bool parseHarnessArgs(int argc, char** argv, HarnessArgs& out);

// Creates the window, runs every shot through `makeApp()` and writes PNGs. Returns a process exit code.
int runHarness(const HarnessArgs& args, const std::function<std::unique_ptr<App>()>& makeApp);

}  // namespace ag
