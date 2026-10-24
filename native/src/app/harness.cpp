#include "app/harness.h"
#include "app/window.h"
#include "core/paths.h"
#include "core/log.h"
#include "raylib.h"
#include "rlgl.h"
#include <algorithm>
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
        else if (k == "--resize" && i + 1 < argc) {
            std::string r = argv[++i];
            size_t x = r.find('x');
            if (x != std::string::npos) { a.resizeW = std::atoi(r.substr(0, x).c_str()); a.resizeH = std::atoi(r.substr(x + 1).c_str()); }
        }
        else if (k == "--hidpi") a.hidpi = true;
        else if (k == "--legacy-dpi") a.legacyDpi = true;
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

    // the window the game opens (window::flags), or the old HIGHDPI-everywhere model to reproduce its bugs
    unsigned int flags = a.legacyDpi ? (unsigned)FLAG_WINDOW_HIGHDPI : (a.hidpi ? window::flags(false) : 0u);
    SetConfigFlags(flags);
    InitWindow(a.width, a.height, "Afterglow QA");
    SetTargetFPS(0);
    if (a.resizeW > 0 && a.resizeH > 0) {
        SetWindowSize(a.resizeW, a.resizeH);
        for (int k = 0; k < 3; ++k) { BeginDrawing(); EndDrawing(); }   // let the resize events arrive
    }
    AG_INFO("AG: harness window: screen %dx%d, render %dx%d, dpi scale %.2f", GetScreenWidth(), GetScreenHeight(), GetRenderWidth(),
            GetRenderHeight(), (double)GetWindowScaleDPI().x);
    std::unique_ptr<App> app = makeApp();
    if (!app || !app->init()) { AG_ERROR("AG: harness: app init failed"); CloseWindow(); return 3; }
    int failures = 0, uiIssues = 0;
    for (const Shot& s : shots) {
        app->reset();
        Spec spec = Spec::parse(s.spec);
        app->applySpec(spec);
        int frames = (int)spec.num("frames", (float)a.frames);
        int renderFrom = std::max(0, std::min(frames - 1, (int)spec.num("render_from", 0.f)));   // long sims: draw the end only
        std::string out = a.shotlist.empty() ? s.name : paths::join(a.outdir, s.name + ".png");
        if (!a.shotlist.empty()) paths::writeText(paths::join(a.outdir, ".keep"), "");
        for (int f = 0; f < frames; ++f) {
            app->update(1.f / 60.f);
            if (f < renderFrom) { PollInputEvents(); continue; }
            BeginDrawing();
            if (!a.legacyDpi) window::beginFrame();
            app->render(GetRenderWidth(), GetRenderHeight());
            if (f == frames - 1) {
                rlDrawRenderBatchActive();   // flush raylib's 2D batch (UI) before reading pixels
                // the framebuffer as it is (LoadImageFromScreen assumes screen x DPI, which is wrong after a resize)
                Image img{};
                img.width = GetRenderWidth(); img.height = GetRenderHeight();
                img.mipmaps = 1; img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
                img.data = rlReadScreenPixels(img.width, img.height);
                if (!ExportImage(img, out.c_str())) { AG_ERROR("AG: harness: cannot write %s", out.c_str()); ++failures; }
                else AG_INFO("AG: harness: wrote %s", out.c_str());
                UnloadImage(img);
                // the UI guard's findings for this shot's final frame
                std::vector<std::string> found = app->qaFindings();
                for (const std::string& l : found) AG_INFO("AG: %s: %s", s.name.c_str(), l.c_str());
                AG_INFO("AG: harness: %s ui-issues %d", s.name.c_str(), (int)found.size());
                uiIssues += (int)found.size();
            }
            EndDrawing();
        }
    }
    AG_INFO("AG: harness: %d shots at %dx%d, ui-issues total %d", (int)shots.size(), GetScreenWidth(), GetScreenHeight(), uiIssues);
    app->shutdown();
    app.reset();
    CloseWindow();
    return failures ? 1 : 0;
}

}  // namespace ag
