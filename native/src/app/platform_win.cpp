// Windows-only window activation, kept apart from raylib's headers (winuser.h clashes with them).
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace ag::platform {

void winActivateExisting() {
    HWND w = FindWindowW(nullptr, L"Afterglow");
    if (!w) return;
    if (IsIconic(w)) ShowWindow(w, SW_RESTORE);
    SetForegroundWindow(w);
}

}  // namespace ag::platform
#endif
