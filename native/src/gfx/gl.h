// Afterglow engine — direct OpenGL 3.3 access through raylib's statically linked glad loader.
// raylib (built with GRAPHICS_API_OPENGL_33) defines the glad_gl* function pointers; this header only
// declares them, so the engine can use full GL (32-bit indices, sRGB textures, FBO control) while
// raylib keeps owning the context, window, input, audio and 2D drawing.
#pragma once
#include "external/glad.h"

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

namespace ag::gl {
// Put GL state back to what raylib's 2D batch expects after custom 3D rendering.
void restoreRaylibState(int fbWidth, int fbHeight);
}
