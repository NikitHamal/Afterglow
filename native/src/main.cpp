// Afterglow — native game entry point.
//   afterglow                      the game
//   afterglow --sandbox            engine sandbox (F1 overlay with every engine feature)
//   afterglow --shot/--shotlist    screenshot harness (game screens; add --sandbox for engine shots)
//   afterglow --verbose            echo the log to the console
#include "app/harness.h"
#include "app/platform.h"
#include "app/sandbox.h"
#include "app/updater.h"
#include "core/log.h"
#include "core/paths.h"
#include "game/game.h"
#include "raylib.h"
#include <algorithm>
#include <cstring>
#include <memory>

namespace {

void applyDisplay(const ag::Settings& s, bool modeChanged) {
    if (s.vsync) SetWindowState(FLAG_VSYNC_HINT);
    else ClearWindowState(FLAG_VSYNC_HINT);
    SetTargetFPS(s.fpsLimit());
    if (!modeChanged) return;
    if (IsWindowFullscreen()) ToggleFullscreen();
    if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) ToggleBorderlessWindowed();
    if (s.windowMode == 1) ToggleBorderlessWindowed();
    else if (s.windowMode == 2) {
        int mon = GetCurrentMonitor();
        SetWindowSize(GetMonitorWidth(mon), GetMonitorHeight(mon));
        ToggleFullscreen();
    } else {
        int mon = GetCurrentMonitor();
        int w = std::min(s.winW, GetMonitorWidth(mon) - 80), h = std::min(s.winH, GetMonitorHeight(mon) - 120);
        SetWindowSize(std::max(960, w), std::max(540, h));
        SetWindowPosition((GetMonitorWidth(mon) - GetScreenWidth()) / 2, (GetMonitorHeight(mon) - GetScreenHeight()) / 2);
    }
}

int runSandbox() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(1600, 900, "Afterglow sandbox");
    SetExitKey(KEY_NULL);
    SetTargetFPS(0);
    auto app = std::make_unique<ag::Sandbox>();
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

}  // namespace

int main(int argc, char** argv) {
    ag::HarnessArgs ha;
    bool harness = ag::parseHarnessArgs(argc, argv, ha);
    bool verbose = false, sandbox = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--verbose") == 0) verbose = true;
        if (std::strcmp(argv[i], "--sandbox") == 0) sandbox = true;
    }

    if (harness) {
        SetTraceLogLevel(LOG_INFO);
        return ag::runHarness(ha, [sandbox]() -> std::unique_ptr<ag::App> {
            if (sandbox) {
                auto s = std::make_unique<ag::Sandbox>();
                s->enableAudio = false;
                return s;
            }
            auto g = std::make_unique<ag::Game>();
            g->enableAudio = false;
            g->harness = true;
            return g;
        });
    }

    ag::platform::openLog(verbose);
    ag::platform::installCrashHandler();
    SetTraceLogLevel(verbose ? LOG_INFO : LOG_INFO);
    AG_INFO("AG: Afterglow %s starting", ag::Updater::current().c_str());
    if (!ag::platform::acquireSingleInstance()) {
        AG_INFO("AG: already running, brought the other window forward");
        ag::platform::closeLog();
        return 0;
    }
    if (sandbox) {
        int rc = runSandbox();
        ag::platform::releaseSingleInstance();
        ag::platform::closeLog();
        return rc;
    }

    // window from the saved display settings
    ag::Settings pre;
    pre.load(ag::paths::join(ag::paths::userDir(), "settings.json"));
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI | (pre.vsync ? FLAG_VSYNC_HINT : 0));
    InitWindow(pre.winW, pre.winH, "Afterglow");
    if (!IsWindowReady()) { AG_ERROR("AG: no OpenGL 3.3 window"); ag::platform::releaseSingleInstance(); ag::platform::closeLog(); return 1; }
    SetWindowMinSize(960, 540);
    SetExitKey(KEY_NULL);
    if (pre.windowMode != 0) applyDisplay(pre, true);
    else applyDisplay(pre, false);

    ag::Updater updater;
    ag::Game game;
    game.updater = &updater;
    game.applyDisplay = applyDisplay;
    if (!game.init()) {
        AG_ERROR("AG: game init failed");
        CloseWindow();
        ag::platform::releaseSingleInstance();
        ag::platform::closeLog();
        return 1;
    }

    bool wasFocused = true;
    int lastCap = -2;
    while (!game.wantsQuit()) {
        if (WindowShouldClose()) break;
        bool minimized = IsWindowMinimized();
        bool focused = IsWindowFocused();
        if (wasFocused && !focused) game.onFocusLost();
        wasFocused = focused;
        if (minimized) {               // nothing to show: sleep, keep the event queue alive
            PollInputEvents();
            WaitTime(0.12);
            continue;
        }
        int cap = focused ? game.settings.fpsLimit() : 20;   // unfocused: a slow, cheap frame rate
        if (cap != lastCap) { SetTargetFPS(cap); lastCap = cap; }
        if (IsKeyPressed(KEY_F11) || ((IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) && IsKeyPressed(KEY_ENTER))) {
            game.settings.windowMode = game.settings.windowMode == 0 ? 1 : 0;
            applyDisplay(game.settings, true);
            game.saveAll();
        }
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;
        game.update(dt);
        BeginDrawing();
        game.render(GetRenderWidth(), GetRenderHeight());
        EndDrawing();
        if (game.settings.windowMode == 0 && !IsWindowMaximized() && !IsWindowFullscreen()) {
            game.settings.winW = GetScreenWidth();
            game.settings.winH = GetScreenHeight();
        }
    }
    AG_INFO("AG: shutting down (peak RSS %.0f MB)", (double)ag::platform::peakResidentBytes() / 1048576.0);
    game.shutdown();
    updater.shutdown();
    CloseWindow();
    ag::platform::releaseSingleInstance();
    ag::platform::closeLog();
    return 0;
}
