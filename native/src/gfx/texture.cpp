#include "gfx/texture.h"
#include "core/log.h"
#include "core/paths.h"
#include <algorithm>
#include <unordered_map>
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

static int gMaxTex = 1 << 14;
static long long gTexBytes = 0;
static std::unordered_map<GLuint, long long> gTexSizes;

void setTextureMaxSize(int px) { gMaxTex = px > 0 ? px : (1 << 14); }
int textureMaxSize() { return gMaxTex; }
long long textureBytes() { return gTexBytes; }

// 2x2 box downsample (sRGB data averaged in gamma space: fine for albedo detail at half size)
static std::vector<unsigned char> halve(const unsigned char* src, int w, int h, int& ow, int& oh) {
    ow = std::max(1, w / 2); oh = std::max(1, h / 2);
    std::vector<unsigned char> out((size_t)ow * (size_t)oh * 4);
    for (int y = 0; y < oh; ++y)
        for (int x = 0; x < ow; ++x)
            for (int c = 0; c < 4; ++c) {
                int x0 = std::min(w - 1, x * 2), x1 = std::min(w - 1, x * 2 + 1), y0 = std::min(h - 1, y * 2), y1 = std::min(h - 1, y * 2 + 1);
                int sum = src[((size_t)y0 * w + x0) * 4 + c] + src[((size_t)y0 * w + x1) * 4 + c] +
                          src[((size_t)y1 * w + x0) * 4 + c] + src[((size_t)y1 * w + x1) * 4 + c];
                out[((size_t)y * ow + x) * 4 + c] = (unsigned char)((sum + 2) / 4);
            }
    return out;
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
    long long bytes = (long long)w * h * 4 * 4 / 3;   // + mip chain
    gTexBytes += bytes;
    gTexSizes[t] = bytes;
    return t;
}

GLuint textureFromMemory(const unsigned char* data, int size, bool srgb, int* outW, int* outH) {
    int w = 0, h = 0, n = 0;
    unsigned char* px = stbi_load_from_memory(data, size, &w, &h, &n, 4);
    if (!px) { AG_WARN("AG: texture decode failed: %s", stbi_failure_reason()); return 0; }
    GLuint t = 0;
    if (std::max(w, h) > gMaxTex) {
        // quality policy: large maps are halved until they fit (Low 1K, Medium 2K, High full size)
        std::vector<unsigned char> cur;
        const unsigned char* src = px;
        int cw = w, ch = h;
        while (std::max(cw, ch) > gMaxTex) {
            int nw, nh;
            std::vector<unsigned char> next = halve(src, cw, ch, nw, nh);
            cur.swap(next);
            src = cur.data(); cw = nw; ch = nh;
        }
        stbi_image_free(px);
        px = nullptr;
        t = upload(cur.data(), cw, ch, srgb);
        w = cw; h = ch;
    } else {
        t = upload(px, w, h, srgb);
        stbi_image_free(px);
    }
    if (outW) *outW = w;
    if (outH) *outH = h;
    return t;
}

bool decodeImage(const unsigned char* data, int size, DecodedImage& out) {
    int w = 0, h = 0, n = 0;
    unsigned char* px = stbi_load_from_memory(data, size, &w, &h, &n, 4);
    if (!px) return false;
    if (std::max(w, h) > gMaxTex) {
        std::vector<unsigned char> cur(px, px + (size_t)w * h * 4);
        stbi_image_free(px);
        while (std::max(w, h) > gMaxTex) {
            int nw, nh;
            std::vector<unsigned char> next = halve(cur.data(), w, h, nw, nh);
            cur.swap(next);
            w = nw; h = nh;
        }
        out.px.swap(cur);
    } else {
        out.px.assign(px, px + (size_t)w * h * 4);
        stbi_image_free(px);
    }
    out.w = w; out.h = h;
    return true;
}

GLuint textureFromDecoded(const DecodedImage& img, bool srgb) {
    if (img.w <= 0 || img.px.empty()) return 0;
    return upload(img.px.data(), img.w, img.h, srgb);
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

void textureDestroy(GLuint& t) {
    if (!t) return;
    auto it = gTexSizes.find(t);
    if (it != gTexSizes.end()) { gTexBytes -= it->second; gTexSizes.erase(it); }
    glDeleteTextures(1, &t);
    t = 0;
}

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
