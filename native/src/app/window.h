// Afterglow — the game window: one coordinate space on every platform, sizes that fit the monitor it is on.
//
// Windows / Linux run WITHOUT FLAG_WINDOW_HIGHDPI. raylib 5.5 handles that flag only until the first resize: the
// window opens in logical units with a 2D scale matrix and a scaled mouse, then any resize (fitting the window to a
// laptop screen, maximising, borderless, dragging the border) sets the screen size to physical pixels while the
// scale matrix and the mouse scale stay at the display scale — the UI is drawn 1.25-2x too big, the panels' SDF
// shapes slide away from their text, and clicks land beside the buttons. Without the flag the screen, the
// framebuffer and the mouse are the same physical pixels, the window is still crisp (GLFW is per-monitor DPI
// aware), and the UI scales from the real framebuffer.
// macOS keeps the flag (Retina): points for layout and input, pixels for the framebuffer.
#pragma once

namespace ag { struct Settings; }

namespace ag::window {

// SetConfigFlags() for the game window (resizable, vsync, Retina on macOS only)
unsigned int flags(bool vsync);

// after InitWindow: a saved size that does not fit this monitor is fitted, a tiny default on a big HiDPI monitor
// grows, and the window is centred on the monitor it is on (multi-monitor aware)
void placeInitial(Settings& s);

// window mode / vsync from the settings; modeChanged = windowed / borderless / fullscreen was switched
void applyDisplay(const Settings& s, bool modeChanged);

// right after BeginDrawing(): nothing scales the 2D layer behind the UI's back (raylib's DPI matrix, mouse scale)
void beginFrame();

// the size to remember for windowed mode (0 when the window is maximised / borderless / fullscreen)
bool windowedSize(int* w, int* h);

constexpr int kMinW = 960, kMinH = 540;

}  // namespace ag::window
