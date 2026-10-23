#include "gfx/texture.h"
#include "core/log.h"
#include "core/paths.h"
#include <vector>

// Private stb_image instance (static linkage avoids clashing with raylib's internal copy).
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "external/stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace ag {

static float maxAniso() {
    static float a = -1.f;
    if (a < 0) {
        a = 1.f;
        GLfloat v = 0; glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &v);
        if (glGetError() == GL_NO_ERROR && v > 1.f) a = v;
    }
    return a;
}

static GLuint upload(const unsigned char* rgba, int w, int h, bool srgb) {
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    float a = maxAniso();
    if (a > 1.f) glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, a < 8.f ? a : 8.f);
    glBindTexture(GL_TEXTURE_2D, 0);
    return t;
}

GLuint textureFromMemory(const unsigned char* data, int size, bool srgb, int* outW, int* outH) {
    int w = 0, h = 0, n = 0;
    unsigned char* px = stbi_load_from_memory(data, size, &w, &h, &n, 4);
    if (!px) { AG_WARN("AG: texture decode failed: %s", stbi_failure_reason()); return 0; }
    GLuint t = upload(px, w, h, srgb);
    stbi_image_free(px);
    if (outW) *outW = w;
    if (outH) *outH = h;
    return t;
}

GLuint textureFromFile(const std::string& path, bool srgb, int* outW, int* outH) {
    std::string bytes = paths::readText(path);
    if (bytes.empty()) { AG_WARN("AG: missing texture %s", path.c_str()); return 0; }
    return textureFromMemory((const unsigned char*)bytes.data(), (int)bytes.size(), srgb, outW, outH);
}

GLuint textureSolid(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    unsigned char px[4 * 4];
    for (int i = 0; i < 4; ++i) { px[i * 4] = r; px[i * 4 + 1] = g; px[i * 4 + 2] = b; px[i * 4 + 3] = a; }
    return upload(px, 2, 2, false);
}

GLuint textureFromRGBA(const unsigned char* rgba, int w, int h, bool srgb) { return upload(rgba, w, h, srgb); }

void textureDestroy(GLuint& t) { if (t) glDeleteTextures(1, &t); t = 0; }

static GLenum baseFormatFor(GLenum internal) {
    switch (internal) {
        case GL_R8: case GL_R16F: case GL_R32F: return GL_RED;
        case GL_RG16F: return GL_RG;
        default: return GL_RGBA;
    }
}
static GLenum typeFor(GLenum internal) {
    switch (internal) {
        case GL_RGBA16F: case GL_R16F: case GL_RG16F: case GL_R11F_G11F_B10F: return GL_HALF_FLOAT;
        case GL_RGBA32F: case GL_R32F: return GL_FLOAT;
        default: return GL_UNSIGNED_BYTE;
    }
}

bool RenderTarget::create(int width, int height, GLenum internalFormat, bool withDepth, bool depthIsTexture, GLenum filter) {
    destroy();
    w = width; h = height; colorFormat = internalFormat;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenTextures(1, &color);
    glBindTexture(GL_TEXTURE_2D, color);
    GLenum fmt = baseFormatFor(internalFormat);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, fmt == GL_RGBA && internalFormat == GL_R11F_G11F_B10F ? GL_RGB : fmt, typeFor(internalFormat), nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
    if (withDepth) {
        if (depthIsTexture) {
            glGenTextures(1, &depth);
            glBindTexture(GL_TEXTURE_2D, depth);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
        } else {
            glGenRenderbuffers(1, &depth);
            glBindRenderbuffer(GL_RENDERBUFFER, depth);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
        }
    }
    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (st != GL_FRAMEBUFFER_COMPLETE) { AG_ERROR("AG: framebuffer incomplete (0x%x) %dx%d", st, w, h); return false; }
    return true;
}

void RenderTarget::destroy() {
    if (fbo) glDeleteFramebuffers(1, &fbo);
    if (color) glDeleteTextures(1, &color);
    if (depth) {
        if (glIsTexture(depth)) glDeleteTextures(1, &depth);
        else glDeleteRenderbuffers(1, &depth);
    }
    fbo = color = depth = 0; w = h = 0;
}

void RenderTarget::bind() const { glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, w, h); }

bool ShadowMap::create(int sz) {
    destroy();
    size = sz;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenTextures(1, &depth);
    glBindTexture(GL_TEXTURE_2D, depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, sz, sz, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[4] = {1, 1, 1, 1};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (st != GL_FRAMEBUFFER_COMPLETE) { AG_ERROR("AG: shadow framebuffer incomplete (0x%x)", st); return false; }
    return true;
}

void ShadowMap::destroy() {
    if (fbo) glDeleteFramebuffers(1, &fbo);
    if (depth) glDeleteTextures(1, &depth);
    fbo = depth = 0; size = 0;
}

}  // namespace ag
