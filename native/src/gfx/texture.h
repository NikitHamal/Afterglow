// Afterglow engine — GPU textures (sRGB-aware, mipmapped, anisotropic) and render targets.
#pragma once
#include "gfx/gl.h"
#include <string>
#include <vector>

namespace ag {

// Decode a PNG/JPEG blob and upload it. srgb=true for colour data (base colour, emissive, lightmaps).
GLuint textureFromMemory(const unsigned char* data, int size, bool srgb, int* outW = nullptr, int* outH = nullptr);
GLuint textureFromFile(const std::string& path, bool srgb, int* outW = nullptr, int* outH = nullptr);
GLuint textureSolid(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
GLuint textureFromRGBA(const unsigned char* rgba, int w, int h, bool srgb);   // procedural textures
void textureDestroy(GLuint& tex);
// CPU decode (thread-safe; applies the size policy) and GPU upload split, for parallel loading
struct DecodedImage { std::vector<unsigned char> px; int w = 0, h = 0; };
bool decodeImage(const unsigned char* data, int size, DecodedImage& out);
GLuint textureFromDecoded(const DecodedImage& img, bool srgb);
// texture detail policy: decoded maps larger than this are halved before upload (0 = no limit)
void setTextureMaxSize(int px);
int textureMaxSize();
long long textureBytes();       // GPU bytes held by textures created through this module (incl. mips)

// Colour (+ optional depth) framebuffer.
struct RenderTarget {
    GLuint fbo = 0, color = 0, depth = 0;
    int w = 0, h = 0;
    GLenum colorFormat = 0;
    bool create(int width, int height, GLenum internalFormat, bool withDepth, bool depthIsTexture = false, GLenum filter = GL_LINEAR);
    void destroy();
    void bind() const;
};

// Depth-only shadow map (depth texture sampled with manual PCF).
struct ShadowMap {
    GLuint fbo = 0, depth = 0;
    int size = 0;
    bool create(int sz);
    void destroy();
};

}  // namespace ag
