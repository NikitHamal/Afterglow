// Afterglow — native game entry point.
//   afterglow                      interactive (sandbox until the game app lands; F1 overlay)
//   afterglow --sandbox            engine sandbox
//   afterglow --shot/--shotlist    screenshot harness (see app/harness.h)
#include "app/harness.h"
#include "app/sandbox.h"
#include "core/log.h"
#include "raylib.h"
#include <cstring>
#include <memory>

int main(int argc, char** argv) {
    ag::HarnessArgs ha;
    bool harness = ag::parseHarnessArgs(argc, argv, ha);
    bool verbose = false;
    for (int i = 1; i < argc; ++i) if (std::strcmp(argv[i], "--verbose") == 0) verbose = true;
    SetTraceLogLevel(verbose || harness ? LOG_INFO : LOG_WARNING);

    if (harness) {
        return ag::runHarness(ha, [] {
            auto s = std::make_unique<ag::Sandbox>();
            s->enableAudio = false;
            return std::unique_ptr<ag::App>(std::move(s));
        });
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(1600, 900, "Afterglow");
    SetExitKey(KEY_NULL);
    SetTargetFPS(0);
    std::unique_ptr<ag::App> app = std::make_unique<ag::Sandbox>();
    if (!app->init()) { CloseWindow(); return 1; }
    while (!WindowShouldClose() && !app->wantsQuit()) {
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;
        app->update(dt);
        BeginDrawing();
        app->render(GetRenderWidth(), GetRenderHeight());
        EndDrawing();
    }
    app->shutdown();
    CloseWindow();
    return 0;
}
