#include "gfx/hairoit.h"
#include "core/log.h"

namespace ag {

bool HairOit::build() {
    bool ok = prog_.build("hair_oit", shaderSource("fullscreen.vert"), shaderSource("hair_oit.frag"));
    if (!ok) AG_WARN("AG: hair OIT composite unavailable, soft hair falls back to plain blending");
    return ok;
}

void HairOit::destroy() {
    if (fbo_) glDeleteFramebuffers(1, &fbo_);
    if (accum_) glDeleteTextures(1, &accum_);
    if (weight_) glDeleteTextures(1, &weight_);
    fbo_ = accum_ = weight_ = 0; depth_ = 0; w_ = h_ = 0;
    prog_.destroy();
}

void HairOit::ensure(int w, int h, GLuint depthTex) {
    if (w == w_ && h == h_ && depthTex == depth_ && fbo_) return;
    if (fbo_) glDeleteFramebuffers(1, &fbo_);
    if (accum_) glDeleteTextures(1, &accum_);
    if (weight_) glDeleteTextures(1, &weight_);
    w_ = w; h_ = h; depth_ = depthTex;
    auto tex = [&](GLenum fmt, GLenum base) {
        GLuint t = 0;
        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_2D, t);
        glTexImage2D(GL_TEXTURE_2D, 0, (GLint)fmt, w, h, 0, base, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return t;
    };
    accum_ = tex(GL_RGBA16F, GL_RGBA);    // rgb = sum(colour * a * w), a = prod(1 - a) (revealage)
    weight_ = tex(GL_R16F, GL_RED);       // r = sum(a * w)
    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, accum_, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, weight_, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);
    const GLenum bufs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, bufs);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        AG_WARN("AG: hair OIT framebuffer incomplete, soft hair falls back to plain blending");
        glDeleteFramebuffers(1, &fbo_); fbo_ = 0;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void HairOit::begin() {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, w_, h_);
    const float a0[4] = {0.f, 0.f, 0.f, 1.f}, w0[4] = {0.f, 0.f, 0.f, 0.f};
    glClearBufferfv(GL_COLOR, 0, a0);
    glClearBufferfv(GL_COLOR, 1, w0);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_FALSE);   // behind the opaque frame: hidden
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
}

void HairOit::end(GLuint hdrFbo, int w, int h, GLuint emptyVao) {
    glDepthMask(GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFbo);
    glViewport(0, 0, w, h);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ZERO, GL_ONE);   // dst * reveal + avg * (1 - reveal)
    prog_.use();
    prog_.sampler("u_accum", 0, accum_);
    prog_.sampler("u_weight", 1, weight_);
    glBindVertexArray(emptyVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

}  // namespace ag
