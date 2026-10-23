#include "gfx/renderer.h"
#include "core/log.h"
#include <algorithm>
#include <cmath>

namespace ag {

void applyQuality(RenderSettings& s, Quality q) {
    s.quality = q;
    switch (q) {
        case Quality::Low: s.shadowRes = 1024; s.shadowSoft = 1.6f; s.bloomLevels = 4; s.hairSoft = false; s.renderScale = 0.8f; s.fxaa = true; s.ssao = false; break;
        case Quality::Medium: s.shadowRes = 2048; s.shadowSoft = 2.2f; s.bloomLevels = 5; s.hairSoft = true; s.renderScale = 1.f; s.fxaa = true; s.ssao = true; s.ssaoSamples = 10; break;
        case Quality::High: s.shadowRes = 4096; s.shadowSoft = 3.0f; s.bloomLevels = 6; s.hairSoft = true; s.renderScale = 1.f; s.fxaa = true; s.ssao = true; s.ssaoSamples = 16; break;
    }
}

bool Renderer::init() {
    glGenVertexArrays(1, &emptyVao_);
    white_ = textureSolid(255, 255, 255, 255);
    flatNormal_ = textureSolid(128, 128, 255, 255);
    black_ = textureSolid(0, 0, 0, 255);
    if (!reloadShaders()) return false;
    return true;
}

bool Renderer::reloadShaders() {
    std::string vs = shaderSource("mesh.vert"), fsv = shaderSource("fullscreen.vert");
    bool ok = pbr_.build("pbr", vs, shaderSource("pbr.frag"));
    ok = depth_.build("depth", vs, shaderSource("depth.frag")) && ok;
    ok = prefilter_.build("bloom_prefilter", fsv, shaderSource("bloom_prefilter.frag")) && ok;
    ok = down_.build("bloom_down", fsv, shaderSource("bloom_down.frag")) && ok;
    ok = up_.build("bloom_up", fsv, shaderSource("bloom_up.frag")) && ok;
    ok = composite_.build("composite", fsv, shaderSource("composite.frag")) && ok;
    ok = fxaa_.build("fxaa", fsv, shaderSource("fxaa.frag")) && ok;
    ok = ssao_.build("ssao", fsv, shaderSource("ssao.frag")) && ok;
    ok = ssaoBlur_.build("ssao_blur", fsv, shaderSource("ssao_blur.frag")) && ok;
    if (!ok) AG_ERROR("AG: shader build failed");
    return ok;
}

void Renderer::shutdown() {
    for (Program* p : {&pbr_, &depth_, &prefilter_, &down_, &up_, &composite_, &fxaa_, &ssao_, &ssaoBlur_}) p->destroy();
    hdr_.destroy(); ldr_.destroy(); aoRaw_.destroy(); ao_.destroy();
    for (auto& b : bloom_) b.destroy();
    for (auto& b : blur_) b.destroy();
    shadow_.destroy();
    if (emptyVao_) glDeleteVertexArrays(1, &emptyVao_);
    emptyVao_ = 0;
    textureDestroy(white_); textureDestroy(flatNormal_); textureDestroy(black_);
}

void Renderer::ensureTargets(int w, int h) {
    if (w == w_ && h == h_ && hdr_.fbo) return;
    w_ = w; h_ = h;
    hdr_.create(w, h, GL_RGBA16F, true, true, GL_LINEAR);      // depth as texture (SSAO)
    ldr_.create(w, h, GL_RGBA8, false, false, GL_LINEAR);
    aoRaw_.create(std::max(1, w / 2), std::max(1, h / 2), GL_R8, false, false, GL_LINEAR);
    ao_.create(std::max(1, w / 2), std::max(1, h / 2), GL_R8, false, false, GL_LINEAR);
    for (auto& b : bloom_) b.destroy();
    for (auto& b : blur_) b.destroy();
    bloomCount_ = 0;
    int bw = std::max(1, w / 2), bh = std::max(1, h / 2);
    for (int i = 0; i < 8 && i < settings.bloomLevels && bw >= 8 && bh >= 8; ++i) {
        bloom_[i].create(bw, bh, GL_RGBA16F, false, false, GL_LINEAR);
        ++bloomCount_;
        bw = std::max(1, bw / 2); bh = std::max(1, bh / 2);
    }
}

void Renderer::fullscreen(Program& p) {
    (void)p;
    glBindVertexArray(emptyVao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

M4 Renderer::computeShadowVP(const Scene& s) const {
    const Lights& L = s.lights;
    V3 dir = normalize(L.keyDir);
    V3 up = std::fabs(dir.y) > 0.95f ? V3{0, 0, 1} : V3{0, 1, 0};
    M4 view = lookAt(L.keyPos, L.keyPos + dir, up);
    // fit the frustum around the bed volume (characters live there) for maximum texel density
    const RoomInfo& ri = s.info;
    float maxAng = 0.2f;
    for (int i = 0; i < 8; ++i) {
        V3 c = ri.bedCenter + V3{(i & 1 ? 1.f : -1.f) * (ri.bedHalf.x + 0.35f), (i & 2) ? 1.25f : -0.45f, (i & 4 ? 1.f : -1.f) * (ri.bedHalf.z + 0.35f)};
        float a = std::acos(clampf(dot(normalize(c - L.keyPos), dir), -1.f, 1.f));
        maxAng = std::max(maxAng, a);
    }
    float fov = std::min(2.f * maxAng + 0.06f, 2.6f);
    return perspective(fov, 1.f, 0.15f, std::max(L.keyRange, 6.f)) * view;
}

void Renderer::setFrame(Program& p, Scene& s, const CameraState& cam, float time) {
    const Lights& L = s.lights;
    p.set("u_viewProj", cam.viewProj);
    p.set("u_camPos", cam.eye);
    p.set("u_time", time);
    p.set("u_keyPos", L.keyPos);
    p.set("u_keyDir", normalize(L.keyDir));
    p.set("u_keyColor", L.keyColor);
    p.set("u_keyIntensity", L.keyIntensity);
    p.set("u_keyCosInner", std::cos(L.keyInner));
    p.set("u_keyCosOuter", std::cos(L.keyOuter));
    p.set("u_keyRange", L.keyRange);
    p.set("u_moonDir", normalize(L.moonDir));
    p.set("u_moonColor", L.moonColor);
    p.set("u_moonIntensity", L.moonIntensity);
    p.set("u_fillDir", normalize(L.fillDir));
    p.set("u_fillColor", L.fillColor);
    p.set("u_fillIntensity", L.fillIntensity);
    p.set("u_skyColor", L.sky);
    p.set("u_groundColor", L.ground);
    p.set("u_ambient", L.ambient);
    p.set("u_bakeScale", s.bakeTint);
    V3 pp[6], pc[6]; float pi[6], pr[6];
    for (int i = 0; i < 6; ++i) { pp[i] = L.points[i].pos; pc[i] = L.points[i].color; pi[i] = L.points[i].intensity; pr[i] = L.points[i].range; }
    p.set("u_numPoints", L.numPoints);
    p.setArray("u_pointPos", pp, 6);
    p.setArray("u_pointColor", pc, 6);
    p.setArray("u_pointIntensity", pi, 6);
    p.setArray("u_pointRange", pr, 6);
    bool sh = settings.shadows && L.keyShadow && shadow_.depth;
    p.set("u_shadowOn", sh ? 1 : 0);
    p.set("u_shadowVP", shadowVP_);
    p.set("u_shadowTexel", shadow_.size > 0 ? 1.f / (float)shadow_.size : 0.f);
    p.set("u_shadowSoft", settings.shadowSoft);
    p.sampler("u_shadowMap", 5, sh ? shadow_.depth : white_);
}

void Renderer::bindMaterial(Program& p, const Model& m, int mi, const Character* ch, bool shadowPass) {
    const Material& mat = m.materials[(size_t)std::max(0, mi)];
    GLuint base = mat.texBase;
    if (ch) { GLuint o = ch->baseOverride(mi); if (o) base = o; }
    p.set("u_baseColor", mat.baseColor);
    p.set("u_alphaMode", (int)mat.alpha);
    p.set("u_cutoff", mat.cutoff);
    p.set("u_hasBase", base ? 1 : 0);
    p.sampler("u_texBase", 0, base ? base : white_);
    if (shadowPass) return;
    p.set("u_uvBase", mat.uvBase);
    p.set("u_mode", (int)mat.shading);
    p.set("u_metallic", mat.metallic);
    p.set("u_roughness", mat.roughness);
    p.set("u_normalScale", mat.normalScale);
    p.set("u_occStrength", mat.occlusion);
    p.set("u_emissive", mat.emissive);
    p.set("u_doubleSided", mat.doubleSided ? 1 : 0);
    p.set("u_hasNormal", mat.texNormal ? 1 : 0);
    p.set("u_hasMR", mat.texMR ? 1 : 0);
    p.set("u_hasOcc", mat.texOcc ? 1 : 0);
    p.set("u_hasEmis", mat.texEmis ? 1 : 0);
    p.set("u_uvNormal", mat.uvNormal);
    p.set("u_uvMR", mat.uvMR);
    p.set("u_uvOcc", mat.uvOcc);
    p.set("u_uvEmis", mat.uvEmis);
    p.sampler("u_texNormal", 1, mat.texNormal ? mat.texNormal : flatNormal_);
    p.sampler("u_texMR", 2, mat.texMR ? mat.texMR : white_);
    p.sampler("u_texOcc", 3, mat.texOcc ? mat.texOcc : white_);
    p.sampler("u_texEmis", 4, mat.texEmis ? mat.texEmis : white_);
    bool hair = mat.shading == Shading::Hair;
    p.set("u_tint", (ch && hair) ? ch->hairTint : V3{1, 1, 1});
    p.set("u_hairShape", (ch && hair) ? 1 : 0);
    if (ch && hair) p.set("u_hairCenter", ch->hairCenter());
    bool skin = mat.shading == Shading::Skin;
    p.set("u_flush", (ch && skin) ? ch->flush : 0.f);
    p.set("u_sweat", (ch && skin) ? ch->sweat : 0.f);
    GLuint mask = (ch && skin) ? ch->skinMask() : 0, detail = (ch && skin) ? ch->skinDetail() : 0;
    p.set("u_hasSkinMask", mask ? 1 : 0);
    p.set("u_hasDetailN", detail ? 1 : 0);
    p.sampler("u_skinMask", 7, mask ? mask : white_);
    p.sampler("u_detailN", 8, detail ? detail : flatNormal_);
    p.set("u_baked", (mat.shading == Shading::Room && mat.texEmis && mat.uvEmis == 1) ? 1 : 0);
}

void Renderer::drawModel(Program& p, Model& m, Animator& a, const Character* ch, int pass) {
    // pass: 0 shadow, 1 opaque + alpha-test, 2 soft hair edges
    bool bonesBound = false;
    for (size_t ni = 0; ni < m.nodes.size(); ++ni) {
        const Node& n = m.nodes[ni];
        if (n.mesh < 0) continue;
        Mesh& mesh = m.meshes[(size_t)n.mesh];
        for (Primitive& prim : mesh.prims) {
            const Material& mat = m.materials[(size_t)std::max(0, prim.material)];
            if (pass == 0 && (!mat.castShadow || mat.alpha == AlphaMode::Blend)) continue;
            if (pass == 3 && mat.alpha == AlphaMode::Blend) continue;
            if (pass == 2 && !(mat.alpha == AlphaMode::Mask && mat.shading == Shading::Hair)) continue;
            bool skinned = prim.skinned && n.skin >= 0 && a.boneTexture();
            if (skinned) {
                if (!bonesBound) { p.sampler("u_bones", 6, a.boneTexture()); bonesBound = true; }
                p.set("u_model", M4::identity());
                p.set("u_skinned", 1);
                p.set("u_skinRow", n.skin);
            } else {
                p.set("u_model", a.global((int)ni));
                p.set("u_skinned", 0);
            }
            bindMaterial(p, m, prim.material, ch, pass == 0 || pass == 3);
            p.set("u_pass", pass == 2 ? 1 : 0);
            if (mat.doubleSided || pass == 0) glDisable(GL_CULL_FACE);
            else { glEnable(GL_CULL_FACE); glCullFace(GL_BACK); }
            glBindVertexArray(prim.vao);
            glDrawElements(GL_TRIANGLES, prim.indexCount, GL_UNSIGNED_INT, nullptr);
            ++drawCalls;
            triangles += prim.indexCount / 3;
        }
    }
}

void Renderer::render(Scene& s, const CameraState& cam, int fbW, int fbH, float time) {
    drawCalls = 0; triangles = 0;
    int iw = std::max(16, (int)std::lround(fbW * settings.renderScale));
    int ih = std::max(16, (int)std::lround(fbH * settings.renderScale));
    ensureTargets(iw, ih);
    if (settings.shadows && shadow_.size != settings.shadowRes) shadow_.create(settings.shadowRes);

    std::vector<std::pair<Character*, bool>> chars = {{&s.vivian, s.showVivian}, {&s.marcus, s.showMarcus}};

    // ---- shadow pass ----
    if (settings.shadows && shadow_.fbo) {
        shadowVP_ = computeShadowVP(s);
        glBindFramebuffer(GL_FRAMEBUFFER, shadow_.fbo);
        glViewport(0, 0, shadow_.size, shadow_.size);
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(1.5f, 2.0f);
        depth_.use();
        depth_.set("u_viewProj", shadowVP_);
        if (!s.roomBaked) drawModel(depth_, s.room, s.roomAnim, nullptr, 0);
        for (auto& c : chars) if (c.second && c.first->loaded()) drawModel(depth_, c.first->model, c.first->anim, c.first, 0);
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    // ---- depth prepass + SSAO ----
    bool ssaoOn = settings.ssao && ao_.fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, hdr_.fbo);
    glViewport(0, 0, hdr_.w, hdr_.h);
    glClearColor(0.008f, 0.006f, 0.008f, 1.f);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (ssaoOn) {
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        depth_.use();
        depth_.set("u_viewProj", cam.viewProj);
        drawModel(depth_, s.room, s.roomAnim, nullptr, 3);
        for (auto& c : chars) if (c.second && c.first->loaded()) drawModel(depth_, c.first->model, c.first->anim, c.first, 3);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        aoRaw_.bind();
        ssao_.use();
        ssao_.sampler("u_depth", 0, hdr_.depth);
        ssao_.set("u_proj", cam.proj);
        ssao_.set("u_invProj", inverse(cam.proj));
        ssao_.set("u_texel", V2{1.f / (float)hdr_.w, 1.f / (float)hdr_.h});
        ssao_.set("u_radius", settings.ssaoRadius);
        ssao_.set("u_intensity", settings.ssaoIntensity);
        ssao_.set("u_samples", settings.ssaoSamples);
        fullscreen(ssao_);
        ao_.bind();
        ssaoBlur_.use();
        ssaoBlur_.sampler("u_ao", 0, aoRaw_.color);
        ssaoBlur_.sampler("u_depth", 1, hdr_.depth);
        ssaoBlur_.set("u_texel", V2{1.f / (float)aoRaw_.w, 1.f / (float)aoRaw_.h});
        ssaoBlur_.set("u_near_far", V2{cam.znear, cam.zfar});
        fullscreen(ssaoBlur_);
        glBindFramebuffer(GL_FRAMEBUFFER, hdr_.fbo);
        glViewport(0, 0, hdr_.w, hdr_.h);
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_TRUE);
    }

    // ---- main HDR pass ----
    pbr_.use();
    setFrame(pbr_, s, cam, time);
    pbr_.set("u_ssaoOn", ssaoOn ? 1 : 0);
    pbr_.set("u_screenSize", V2{(float)hdr_.w, (float)hdr_.h});
    pbr_.sampler("u_ssao", 7, ssaoOn ? ao_.color : white_);
    drawModel(pbr_, s.room, s.roomAnim, nullptr, 1);
    for (auto& c : chars) if (c.second && c.first->loaded()) drawModel(pbr_, c.first->model, c.first->anim, c.first, 1);
    if (settings.hairSoft) {
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
        glDepthMask(GL_FALSE);
        for (auto& c : chars) if (c.second && c.first->loaded()) drawModel(pbr_, c.first->model, c.first->anim, c.first, 2);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    s.particles.render(cam);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    // ---- bloom ----
    bool bloomOn = settings.bloom && bloomCount_ > 0;
    if (bloomOn) {
        bloom_[0].bind();
        prefilter_.use();
        prefilter_.sampler("u_src", 0, hdr_.color);
        prefilter_.set("u_texel", V2{1.f / (float)hdr_.w, 1.f / (float)hdr_.h});
        prefilter_.set("u_threshold", settings.bloomThreshold);
        prefilter_.set("u_knee", settings.bloomKnee);
        fullscreen(prefilter_);
        down_.use();
        for (int i = 1; i < bloomCount_; ++i) {
            bloom_[i].bind();
            down_.sampler("u_src", 0, bloom_[i - 1].color);
            down_.set("u_texel", V2{1.f / (float)bloom_[i - 1].w, 1.f / (float)bloom_[i - 1].h});
            fullscreen(down_);
        }
        up_.use();
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        for (int i = bloomCount_ - 1; i > 0; --i) {
            bloom_[i - 1].bind();
            up_.sampler("u_src", 0, bloom_[i].color);
            up_.set("u_texel", V2{1.f / (float)bloom_[i].w, 1.f / (float)bloom_[i].h});
            up_.set("u_radius", settings.bloomRadius);
            fullscreen(up_);
        }
        glDisable(GL_BLEND);
    }

    // ---- composite -> LDR ----
    ldr_.bind();
    composite_.use();
    composite_.sampler("u_hdr", 0, hdr_.color);
    composite_.sampler("u_bloom", 1, bloomOn ? bloom_[0].color : black_);
    composite_.set("u_hasBloom", bloomOn ? 1 : 0);
    composite_.set("u_bloomStrength", settings.bloomStrength);
    composite_.set("u_exposure", settings.exposure);
    composite_.set("u_warmth", settings.warmth);
    composite_.set("u_saturation", settings.saturation);
    composite_.set("u_contrast", settings.contrast);
    composite_.set("u_vignette", settings.vignette);
    composite_.set("u_grain", settings.grain);
    composite_.set("u_time", time);
    composite_.set("u_flash", settings.flash);
    composite_.set("u_resolution", V2{(float)ldr_.w, (float)ldr_.h});
    fullscreen(composite_);

    // ---- blurred copy for frosted UI panels (menus only) ----
    if (uiBlur) {
        int bw = std::max(2, ldr_.w / 2), bh = std::max(2, ldr_.h / 2);
        if (blur_[0].w != bw || blur_[0].h != bh || !blur_[0].fbo) {
            for (int i = 0; i < 3; ++i) { blur_[i].create(bw, bh, GL_RGBA8, false, false, GL_LINEAR); bw = std::max(1, bw / 2); bh = std::max(1, bh / 2); }
            blur_[3].create(blur_[1].w, blur_[1].h, GL_RGBA8, false, false, GL_LINEAR);
        }
        down_.use();
        const RenderTarget* src = &ldr_;
        for (int i = 0; i < 3; ++i) {
            blur_[i].bind();
            down_.sampler("u_src", 0, src->color);
            down_.set("u_texel", V2{1.f / (float)src->w, 1.f / (float)src->h});
            fullscreen(down_);
            src = &blur_[i];
        }
        blur_[3].bind();
        up_.use();
        up_.sampler("u_src", 0, blur_[2].color);
        up_.set("u_texel", V2{1.f / (float)blur_[2].w, 1.f / (float)blur_[2].h});
        up_.set("u_radius", 1.f);
        fullscreen(up_);
    }

    // ---- FXAA -> backbuffer ----
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, fbW, fbH);
    fxaa_.use();
    fxaa_.sampler("u_src", 0, ldr_.color);
    fxaa_.set("u_texel", V2{1.f / (float)ldr_.w, 1.f / (float)ldr_.h});
    fxaa_.set("u_enabled", settings.fxaa ? 1 : 0);
    fullscreen(fxaa_);

    gl::restoreRaylibState(fbW, fbH);
}

}  // namespace ag
