import pathlib

# ---- gl.h: GLES 3 on Android, glad on desktop ----
p = pathlib.Path('native/src/gfx/gl.h')
s = p.read_text(encoding='utf-8')
s = s.replace(
"""#pragma once
#include "external/glad.h"
""",
"""#pragma once
#if defined(__ANDROID__)
#include <GLES3/gl3.h>
#else
#include "external/glad.h"
#endif
""")
p.write_text(s, encoding='utf-8')

# ---- platform.h: wheel() ----
p = pathlib.Path('native/src/app/platform.h')
s = p.read_text(encoding='utf-8')
s = s.replace(
"""// hand freed heap pages back to the OS (after loading: decode buffers, file images)
void trimHeap();""",
"""// mouse wheel this frame; on touch screens a two-finger pinch maps onto it
float wheel();

// hand freed heap pages back to the OS (after loading: decode buffers, file images)
void trimHeap();""")
p.write_text(s, encoding='utf-8')

# ---- platform.cpp: wheel on desktop, Android no-ops ----
p = pathlib.Path('native/src/app/platform.cpp')
s = p.read_text(encoding='utf-8')
s = s.replace(
"""bool acquireSingleInstance() {
#if defined(_WIN32)""",
"""float wheel() {
#if defined(__ANDROID__)
    return 0.f;   // platform_android.cpp maps the pinch onto it
#else
    return GetMouseWheelMove();
#endif
}

bool acquireSingleInstance() {
#if defined(_WIN32)""")
s = s.replace(
"""    return true;
#else
    std::string p = paths::join(paths::userDir(), ".instance.lock");""",
"""    return true;
#elif defined(__ANDROID__)
    return true;   // one activity per process already
#else
    std::string p = paths::join(paths::userDir(), ".instance.lock");""")
s = s.replace(
"""void trimHeap() {
#if defined(_WIN32)
    HeapCompact(GetProcessHeap(), 0);
#elif defined(__APPLE__)""",
"""void trimHeap() {
#if defined(__ANDROID__)
    // bionic has no malloc_trim; the allocator returns pages on its own
#elif defined(_WIN32)
    HeapCompact(GetProcessHeap(), 0);
#elif defined(__APPLE__)""")
p.write_text(s, encoding='utf-8')

# ---- ui.cpp: wheel through the platform shim ----
p = pathlib.Path('native/src/ui/ui.cpp')
s = p.read_text(encoding='utf-8')
s = s.replace('#include "core/log.h"\n', '#include "core/log.h"\n#include "app/platform.h"\n')
s = s.replace('in.wheel = GetMouseWheelMove();', 'in.wheel = ag::platform::wheel();')
p.write_text(s, encoding='utf-8')

# ---- game.cpp: wheel through the platform shim ----
p = pathlib.Path('native/src/game/game.cpp')
s = p.read_text(encoding='utf-8')
s = s.replace('GetMouseWheelMove()', 'ag::platform::wheel()')
p.write_text(s, encoding='utf-8')

# ---- window.cpp: Android has no desktop window management ----
p = pathlib.Path('native/src/app/window.cpp')
s = p.read_text(encoding='utf-8')
s = s.replace(
"""unsigned int flags(bool vsync) {
    unsigned int f = (unsigned)FLAG_WINDOW_RESIZABLE | (vsync ? (unsigned)FLAG_VSYNC_HINT : 0u);""",
"""unsigned int flags(bool vsync) {
#if defined(__ANDROID__)
    (void)vsync;
    return (unsigned)FLAG_VSYNC_HINT;   // the surface is always fullscreen; there is nothing to resize
#else
    unsigned int f = (unsigned)FLAG_WINDOW_RESIZABLE | (vsync ? (unsigned)FLAG_VSYNC_HINT : 0u);""")
s = s.replace(
"""void placeInitial(Settings& s) {
    if (s.windowMode != 0) { applyDisplay(s, true); return; }""",
"""void placeInitial(Settings& s) {
#if defined(__ANDROID__)
    s.winW = GetScreenWidth(); s.winH = GetScreenHeight();
    AG_INFO("AG: window %dx%d (framebuffer %dx%d, touch)", GetScreenWidth(), GetScreenHeight(), GetRenderWidth(), GetRenderHeight());
    return;
#else
    if (s.windowMode != 0) { applyDisplay(s, true); return; }""")
s = s.replace(
"""    s.winW = w; s.winH = h;
    AG_INFO("AG: window %dx%d (framebuffer %dx%d, display scale %.2f) on a %dx%d monitor", GetScreenWidth(), GetScreenHeight(),
            GetRenderWidth(), GetRenderHeight(), (double)GetWindowScaleDPI().x, m.w, m.h);
}""",
"""    s.winW = w; s.winH = h;
    AG_INFO("AG: window %dx%d (framebuffer %dx%d, display scale %.2f) on a %dx%d monitor", GetScreenWidth(), GetScreenHeight(),
            GetRenderWidth(), GetRenderHeight(), (double)GetWindowScaleDPI().x, m.w, m.h);
#endif
}""")
s = s.replace(
"""void applyDisplay(const Settings& s, bool modeChanged) {
    if (s.vsync) SetWindowState(FLAG_VSYNC_HINT);""",
"""void applyDisplay(const Settings& s, bool modeChanged) {
#if defined(__ANDROID__)
    (void)s; (void)modeChanged;
    SetTargetFPS(0);
    return;
#endif
    if (s.vsync) SetWindowState(FLAG_VSYNC_HINT);""")
p.write_text(s, encoding='utf-8')

# ---- main.cpp: no desktop icon / F11 fullscreen on Android ----
p = pathlib.Path('native/src/main.cpp')
s = p.read_text(encoding='utf-8')
s = s.replace(
"""#if !defined(_WIN32)
    {
        Image icon = LoadImageFromMemory(".png", kAppIconPng, kAppIconPngSize);""",
"""#if !defined(_WIN32) && !defined(__ANDROID__)
    {
        Image icon = LoadImageFromMemory(".png", kAppIconPng, kAppIconPngSize);""")
s = s.replace(
"""        if (IsKeyPressed(KEY_F11) || ((IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) && IsKeyPressed(KEY_ENTER))) {""",
"""#if !defined(__ANDROID__)
        if (IsKeyPressed(KEY_F11) || ((IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) && IsKeyPressed(KEY_ENTER))) {""")
s = s.replace(
"""            game.saveAll();
        }
        float dt = GetFrameTime();""",
"""            game.saveAll();
        }
#endif
        float dt = GetFrameTime();""")
p.write_text(s, encoding='utf-8')

# ---- texture.cpp: ES-safe shadow map + float-target fallback ----
p = pathlib.Path('native/src/gfx/texture.cpp')
s = p.read_text(encoding='utf-8')
s = s.replace(
"""    if (st != GL_FRAMEBUFFER_COMPLETE) { AG_ERROR("AG: framebuffer incomplete (0x%x) %dx%d", st, w, h); return false; }
    return true;
}""",
"""    if (st != GL_FRAMEBUFFER_COMPLETE) {
#if defined(__ANDROID__)
        // some ES3 GPUs do not render to float targets: fall back to 8-bit and keep the frame
        GLenum alt = 0;
        if (internalFormat == GL_RGBA16F || internalFormat == GL_RGBA32F || internalFormat == GL_R11F_G11F_B10F) alt = GL_RGBA8;
        else if (internalFormat == GL_R16F || internalFormat == GL_R32F) alt = GL_R8;
        if (alt) {
            AG_WARN("AG: %s target unsupported, using 0x%x", internalFormat == GL_RGBA16F ? "HDR" : "float", alt);
            destroy();
            return create(width, height, alt, withDepth, depthIsTexture, filter);
        }
#endif
        AG_ERROR("AG: framebuffer incomplete (0x%x) %dx%d", st, w, h); return false;
    }
    return true;
}""")
s = s.replace(
"""    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[4] = {1, 1, 1, 1};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);""",
"""#if defined(__ANDROID__)
    // GLES has no border clamp / border colour; the shadow shader treats outside-frustum taps as lit
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
    glDrawBuffers(0, nullptr);
#else
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[4] = {1, 1, 1, 1};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
#endif""")
p.write_text(s, encoding='utf-8')
print('core patches applied')
