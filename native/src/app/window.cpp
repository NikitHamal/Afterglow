#include "app/window.h"
#include "game/settings.h"
#include "core/log.h"
#include "raylib.h"
#include "rlgl.h"
#include <algorithm>

namespace ag::window {

namespace {
struct Monitor { int x = 0, y = 0, w = 1920, h = 1080; };

// the monitor the window is on, with its position on the desktop (secondary monitors are not at 0,0)
Monitor current() {
    Monitor m;
    int mon = GetCurrentMonitor();
    if (mon >= 0 && mon < GetMonitorCount()) {
        Vector2 p = GetMonitorPosition(mon);
        m.x = (int)p.x; m.y = (int)p.y;
        m.w = GetMonitorWidth(mon); m.h = GetMonitorHeight(mon);
    }
    if (m.w <= 0 || m.h <= 0) { m.w = 1920; m.h = 1080; }
    return m;
}

// a windowed size that fits the monitor with its taskbar and title bar, never below the minimum the monitor allows
void fit(int w, int h, const Monitor& m, int* ow, int* oh) {
    int maxW = std::max(640, m.w - 80), maxH = std::max(360, m.h - 120);
    *ow = std::max(std::min(kMinW, maxW), std::min(w, maxW));
    *oh = std::max(std::min(kMinH, maxH), std::min(h, maxH));
}

void centre(int w, int h, const Monitor& m) {
    // the client area's top-left: keep room for the title bar above it
    SetWindowPosition(m.x + std::max(0, (m.w - w) / 2), m.y + std::max(32, (m.h - h) / 2 - 20));
}
}  // namespace

unsigned int flags(bool vsync) {
    unsigned int f = (unsigned)FLAG_WINDOW_RESIZABLE | (vsync ? (unsigned)FLAG_VSYNC_HINT : 0u);
#if defined(__APPLE__)
    f |= FLAG_WINDOW_HIGHDPI;
#endif
    return f;
}

void placeInitial(Settings& s) {
    if (s.windowMode != 0) { applyDisplay(s, true); return; }
    Monitor m = current();
    int w0 = GetScreenWidth(), h0 = GetScreenHeight(), w = w0, h = h0;
#if !defined(__APPLE__)
    // physical pixels: the 1600x900 default is a postage stamp on a 4K laptop at 200 %; grow it with the display scale
    float scale = GetWindowScaleDPI().x;
    if (scale > 1.2f && (float)w < (float)m.w * 0.55f) {
        float k = std::min(scale, (float)m.w * 0.8f / (float)std::max(1, w));
        w = (int)((float)w * k); h = (int)((float)h * k);
    }
#endif
    fit(w, h, m, &w, &h);
    if (w != w0 || h != h0) {
        SetWindowSize(w, h);
        centre(w, h, m);
    }
    s.winW = w; s.winH = h;
    AG_INFO("AG: window %dx%d (framebuffer %dx%d, display scale %.2f) on a %dx%d monitor", GetScreenWidth(), GetScreenHeight(),
            GetRenderWidth(), GetRenderHeight(), (double)GetWindowScaleDPI().x, m.w, m.h);
}

void applyDisplay(const Settings& s, bool modeChanged) {
    if (s.vsync) SetWindowState(FLAG_VSYNC_HINT);
    else ClearWindowState(FLAG_VSYNC_HINT);
    SetTargetFPS(0);   // the main loop paces frames itself (sleeping, never spinning)
    if (!modeChanged) return;
    if (IsWindowFullscreen()) ToggleFullscreen();
    if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) ToggleBorderlessWindowed();
    if (s.windowMode == 1) {
        ToggleBorderlessWindowed();
    } else if (s.windowMode == 2) {
        int mon = GetCurrentMonitor();
        SetWindowSize(GetMonitorWidth(mon), GetMonitorHeight(mon));
        ToggleFullscreen();
    } else {
        if (IsWindowMaximized()) RestoreWindow();
        Monitor m = current();
        int w = 0, h = 0;
        fit(s.winW, s.winH, m, &w, &h);
        SetWindowSize(w, h);
        centre(w, h, m);
    }
    AG_INFO("AG: display mode %d: window %dx%d, framebuffer %dx%d", s.windowMode, GetScreenWidth(), GetScreenHeight(), GetRenderWidth(),
            GetRenderHeight());
}

void beginFrame() {
#if !defined(__APPLE__)
    // screen = framebuffer = mouse pixels: no DPI matrix on the 2D layer, no mouse scale (see window.h)
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    SetMouseScale(1.f, 1.f);
#endif
}

bool windowedSize(int* w, int* h) {
    if (IsWindowFullscreen() || IsWindowMaximized() || IsWindowMinimized() || IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) return false;
    *w = GetScreenWidth(); *h = GetScreenHeight();
    return *w >= 320 && *h >= 200;
}

}  // namespace ag::window
