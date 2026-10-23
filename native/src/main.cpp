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
#include "embedded_icon.h"
#include <algorithm>
#include <chrono>
#include <thread>
#include <cstring>
#include <memory>

namespace {

void applyDisplay(const ag::Settings& s, bool modeChanged) {
    if (s.vsync) SetWindowState(FLAG_VSYNC_HINT);
    else ClearWindowState(FLAG_VSYNC_HINT);
    SetTargetFPS(0);   // the main loop paces frames itself (sleeping, never spinning)
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
    ag::platform::installQuitSignals();
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
#if !defined(_WIN32)
    {
        Image icon = LoadImageFromMemory(".png", kAppIconPng, kAppIconPngSize);
        if (icon.data) { SetWindowIcon(icon); UnloadImage(icon); }
    }
#endif
    if (pre.windowMode != 0) applyDisplay(pre, true);
    else {
        applyDisplay(pre, false);
        // small screens (1366x768 laptops): fit the window and centre it
        int mon = GetCurrentMonitor(), mw = GetMonitorWidth(mon), mh = GetMonitorHeight(mon);
        if (mw > 0 && mh > 0 && (GetScreenWidth() > mw - 60 || GetScreenHeight() > mh - 100)) {
            int w = std::max(960, std::min(GetScreenWidth(), mw - 60)), h = std::max(540, std::min(GetScreenHeight(), mh - 100));
            SetWindowSize(w, h);
            SetWindowPosition((mw - w) / 2, std::max(0, (mh - h) / 2 - 20));
        }
    }

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
    double nextFrame = GetTime();
    while (!game.wantsQuit()) {
        if (WindowShouldClose() || ag::platform::quitRequested()) break;
        bool minimized = IsWindowMinimized();
        bool focused = IsWindowFocused();
        if (wasFocused && !focused) game.onFocusLost();
        wasFocused = focused;
        if (minimized) {               // nothing to show: sleep, keep the event queue alive
            PollInputEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(120));   // raylib WaitTime spins its last 5 %
            continue;
        }
        int cap = focused ? game.settings.fpsLimit() : 8;    // unfocused: paused game, a few cheap frames a second
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
        // frame cap by sleeping (raylib's limiter busy-waits the tail of every frame): the CPU idles between frames
        if (cap > 0) {
            double period = 1.0 / (double)cap, now = GetTime();
            nextFrame += period;
            if (nextFrame < now - period) nextFrame = now;                  // fell behind: don't try to catch up
            double wait = nextFrame - now;
            if (wait > 0.0005) std::this_thread::sleep_for(std::chrono::duration<double>(wait));
        } else nextFrame = GetTime();
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
