#include "app/harness.h"
#include "core/paths.h"
#include "core/log.h"
#include "raylib.h"
#include "rlgl.h"
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <vector>

namespace ag {

bool parseHarnessArgs(int argc, char** argv, HarnessArgs& a) {
    bool harness = false;
    for (int i = 1; i < argc; ++i) {
        std::string k = argv[i];
        auto next = [&](std::string& dst) { if (i + 1 < argc) dst = argv[++i]; };
        if (k == "--shot") { next(a.shot); harness = true; }
        else if (k == "--shotlist") { next(a.shotlist); harness = true; }
        else if (k == "--scene") next(a.scene);
        else if (k == "--outdir") next(a.outdir);
        else if (k == "--frames" && i + 1 < argc) a.frames = std::atoi(argv[++i]);
        else if (k == "--res" && i + 1 < argc) {
            std::string r = argv[++i];
            size_t x = r.find('x');
            if (x != std::string::npos) { a.width = std::atoi(r.substr(0, x).c_str()); a.height = std::atoi(r.substr(x + 1).c_str()); }
        }
    }
    return harness;
}

int runHarness(const HarnessArgs& a, const std::function<std::unique_ptr<App>()>& makeApp) {
    struct Shot { std::string name, spec; };
    std::vector<Shot> shots;
    if (!a.shotlist.empty()) {
        std::istringstream in(paths::readText(a.shotlist));
        std::string line;
        while (std::getline(in, line)) {
            line = Spec::trim(line);
            if (line.empty() || line[0] == '#') continue;
            size_t bar = line.find('|');
            if (bar == std::string::npos) continue;
            shots.push_back({Spec::trim(line.substr(0, bar)), Spec::trim(line.substr(bar + 1))});
        }
    } else {
        shots.push_back({a.shot, a.scene});
    }
    if (shots.empty()) { AG_ERROR("AG: harness: no shots"); return 2; }

    SetConfigFlags(0);
    InitWindow(a.width, a.height, "Afterglow QA");
    SetTargetFPS(0);
    std::unique_ptr<App> app = makeApp();
    if (!app || !app->init()) { AG_ERROR("AG: harness: app init failed"); CloseWindow(); return 3; }
    int failures = 0;
    for (const Shot& s : shots) {
        app->reset();
        Spec spec = Spec::parse(s.spec);
        app->applySpec(spec);
        int frames = (int)spec.num("frames", (float)a.frames);
        std::string out = a.shotlist.empty() ? s.name : paths::join(a.outdir, s.name + ".png");
        if (!a.shotlist.empty()) paths::writeText(paths::join(a.outdir, ".keep"), "");
        for (int f = 0; f < frames; ++f) {
            app->update(1.f / 60.f);
            BeginDrawing();
            app->render(GetRenderWidth(), GetRenderHeight());
            if (f == frames - 1) {
                rlDrawRenderBatchActive();   // flush raylib's 2D batch (UI) before reading pixels
                Image img = LoadImageFromScreen();
                if (!ExportImage(img, out.c_str())) { AG_ERROR("AG: harness: cannot write %s", out.c_str()); ++failures; }
                else AG_INFO("AG: harness: wrote %s", out.c_str());
                UnloadImage(img);
            }
            EndDrawing();
        }
    }
    app->shutdown();
    app.reset();
    CloseWindow();
    return failures ? 1 : 0;
}

}  // namespace ag
