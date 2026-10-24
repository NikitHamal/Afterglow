#include "gfx/renderer.h"
#include "core/log.h"
#include <algorithm>
#include <cmath>

namespace ag {

// hair: alpha above this is a dense strand core (opaque, depth); below it the strand sheet, tips and wisps go soft
constexpr float kHairCore = 0.86f;

void applyQuality(RenderSettings& s, Quality q) {
    s.quality = q;
    switch (q) {
        case Quality::Low: s.shadowRes = 1024; s.shadowSoft = 1.6f; s.bloomLevels = 4; s.hairSoft = false; s.renderScale = 0.8f; s.fxaa = true; s.ssao = false; break;
        // the shadow frustum only spans the bed (about 2 x 2 m): 2048 texels are ~1 mm, and the PCF radius is kept
        // in world units, so High looks the same as the old 4096 map at a quarter of the fill cost
        case Quality::Medium: s.shadowRes = 1536; s.shadowSoft = 1.65f; s.bloomLevels = 5; s.hairSoft = true; s.renderScale = 1.f; s.fxaa = true; s.ssao = true; s.ssaoSamples = 10; break;
        case Quality::High: s.shadowRes = 2048; s.shadowSoft = 1.5f; s.bloomLevels = 6; s.hairSoft = true; s.renderScale = 1.f; s.fxaa = true; s.ssao = true; s.ssaoSamples = 12; break;
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
    ok = dof_.build("dof", fsv, shaderSource("dof.frag")) && ok;
    oit_.build();   // optional: without it soft hair blends in draw order
    if (!ok) AG_ERROR("AG: shader build failed");
    return ok;
}

void Renderer::shutdown() {
    for (Program* p : {&pbr_, &depth_, &prefilter_, &down_, &up_, &composite_, &fxaa_, &ssao_, &ssaoBlur_, &dof_}) p->destroy();
    hdr_.destroy(); ldr_.destroy(); aoRaw_.destroy(); ao_.destroy(); dofOut_.destroy();
    oit_.destroy();
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
    oit_.ensure(w, h, hdr_.depth);                             // soft hair layers, depth-tested against the frame
    ldr_.create(w, h, GL_RGBA8, false, false, GL_LINEAR);
    dofOut_.destroy();   // made on first use (story close-ups only)
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
    p.set("u_bakeMix", s.bakeMix);
    p.set("u_bakeSat", s.bakeSat);
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
    p.set("u_dissolve", 1.f);   // garments set their own (undressing)
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
    bool skinLike = mat.shading == Shading::Skin || mat.name.find("genital") != std::string::npos;
    p.set("u_tint", (ch && hair) ? ch->hairTint : ((ch && skinLike) ? ch->skinTint : V3{1, 1, 1}));
    p.set("u_hairShape", (ch && hair) ? 1 : 0);
    if (ch && hair) p.set("u_hairCenter", ch->hairCenter());
    if (ch && mat.shading == Shading::Eye) {
        V3 hc, fwd, up;
        ch->headAxes(hc, fwd, up);
        p.set("u_headUp", up);
        p.set("u_headFwd", fwd);
    }
    bool skin = mat.shading == Shading::Skin;
    p.set("u_flush", (ch && skin) ? ch->flush : 0.f);
    p.set("u_sweat", (ch && skin) ? ch->sweat : 0.f);
    p.set("u_wet", (ch && skin) ? ch->wet : 0.f);
    GLuint mask = (ch && skin) ? ch->skinMask() : 0, detail = (ch && skin) ? ch->skinDetail() : 0;
    p.set("u_hasSkinMask", mask ? 1 : 0);
    p.set("u_hasDetailN", detail ? 1 : 0);
    p.sampler("u_skinMask", 9, mask ? mask : white_);       // 7 is the SSAO buffer, 8 kept free
    p.sampler("u_detailN", 10, detail ? detail : flatNormal_);
    p.set("u_baked", (mat.shading == Shading::Room && mat.baked && mat.texEmis) ? 1 : 0);
}

void Renderer::drawCharacter(Program& p, Character& c, int pass) {
    drawModel(p, c.model, c.anim, &c, pass);
    if (Outfit* o = c.outfit()) if (o->loaded() && pass != 2) drawModel(p, o->model, c.anim, &c, pass, o);
    // a hair style replaces the GLB's own hair (skipped in drawModel), skinned by the character's bones
    if (HairStyle* h = c.hairStyle()) drawModel(p, h->model, c.anim, &c, pass);
}

void Renderer::drawModel(Program& p, Model& m, Animator& a, const Character* ch, int pass, const Outfit* outfit) {
    // pass: 0 shadow, 1 opaque + alpha-test, 2 soft hair edges, 3 depth prepass
    bool bonesBound = false;
    for (size_t ni = 0; ni < m.nodes.size(); ++ni) {
        const Node& n = m.nodes[ni];
        if (n.mesh < 0) continue;
        Mesh& mesh = m.meshes[(size_t)n.mesh];
        float vis = 1.f;
        if (outfit) {
            int layer = (size_t)n.mesh < outfit->meshLayer.size() ? outfit->meshLayer[(size_t)n.mesh] : 0;
            vis = outfit->visibility(layer);
            if (vis <= 0.002f) continue;   // taken off
            // underwear stays under the clothes: while the outer layer is fully worn nothing of it is drawn (not
            // even where it reaches past the outer layer, like a bra's band above a strapless dress)
            if (layer == 1 && outfit->visibility(0) > 0.999f) continue;
        }
        for (size_t pi = 0; pi < mesh.prims.size(); ++pi) {
            Primitive& prim = mesh.prims[pi];
            const Material& mat = m.materials[(size_t)std::max(0, prim.material)];
            // his genitals stay out of sight while he still wears something
            if (ch && ch->hideGenitals && !outfit && mat.shading == Shading::Skin && mat.name.find("genital") != std::string::npos) continue;
            // the character's own hair gives way to a hair style
            if (ch && &m == &ch->model && mat.shading == Shading::Hair && ch->hairStyle()) continue;
            if (pass == 0 && (!mat.castShadow || mat.alpha == AlphaMode::Blend)) continue;
            if (pass == 3 && mat.alpha == AlphaMode::Blend) continue;
            if (pass == 2 && !(mat.alpha == AlphaMode::Mask && mat.shading == Shading::Hair)) continue;
            bool skinned = prim.skinned && n.skin >= 0 && a.boneTexture();
            if (skinned) {
                if (!bonesBound) {
                    p.sampler("u_bones", 6, a.boneTexture()); bonesBound = true;
                    // soft tissue: finger contacts dent the skin (the touching hands themselves excluded)
                    int nc = ch && pass != 0 ? std::min(8, (int)ch->touch.contacts.size()) : 0;
                    p.set("u_numContacts", nc);
                    if (nc > 0) {
                        V4 cs[8];
                        for (int i = 0; i < nc; ++i) cs[i] = V4{ch->touch.contacts[(size_t)i].c.x, ch->touch.contacts[(size_t)i].c.y, ch->touch.contacts[(size_t)i].c.z, ch->touch.contacts[(size_t)i].r};
                        p.setArray("u_contacts", cs, nc);
                        p.set("u_noDent", V4{(float)ch->touch.handJointLo[0], (float)ch->touch.handJointHi[0], (float)ch->touch.handJointLo[1], (float)ch->touch.handJointHi[1]});
                    }
                }
                p.set("u_model", M4::identity());
                p.set("u_skinned", 1);
                p.set("u_skinRow", n.skin);
            } else {
                p.set("u_model", a.global((int)ni));
                p.set("u_skinned", 0);
            }
            // garments are their own materials: the character's tone swaps / skin masks / tints are not theirs
            bindMaterial(p, m, prim.material, outfit ? nullptr : ch, pass == 0 || pass == 3);
            if (settings.hairSoft && pass != 0 && mat.shading == Shading::Hair && mat.alpha == AlphaMode::Mask)
                p.set("u_cutoff", kHairCore);
            if (outfit) {
                size_t mi = (size_t)std::max(0, prim.material);
                p.set("u_dissolve", vis);
                V2 hr = mi < outfit->heightRange.size() ? outfit->heightRange[mi] : V2{0.f, 1.8f};
                p.set("u_wipe", V4{hr.x, hr.y, mi < outfit->wipeDown.size() ? (float)outfit->wipeDown[mi] : 1.f, 0.f});
                if (pass != 0 && pass != 3) {
                    p.set("u_fabric", mi < outfit->fabric.size() ? outfit->fabric[mi] : 0);
                    p.set("u_fabric2", mi < outfit->pattern.size() ? outfit->pattern[mi] : V3{0, 0, 0});
                }
            }
            // garment fit: skin under a worn garment is not drawn (it can never show through); garments trim along
            // their smoothed border, an under layer hides where the outer one covers it
            const Outfit* worn = !outfit && ch && &m == &ch->model ? ch->outfit() : nullptr;   // fit data is per body mesh
            GLuint fitT = outfit ? outfit->fitTexture(n.mesh, pi) : (worn && worn->loaded() ? worn->bodyFitTexture(n.mesh, pi) : 0);
            p.set("u_fitMode", fitT ? (outfit ? 2 : 1) : 0);
            if (fitT) {
                p.sampler("u_fit", 11, fitT);
                if (outfit) {
                    p.set("u_fitHide", V2{0.0005f, outfit->visibility(0) > 0.999f ? 1.f : 0.f});
                    // a little clearance off the skin (reads as the fabric's thickness): more for the outer layer
                    bool under = (size_t)n.mesh < outfit->meshLayer.size() && outfit->meshLayer[(size_t)n.mesh] == 1;
                    p.set("u_fitInflate", under ? 0.0015f : 0.003f);
                }
                else {
                    // skin hides under the outer layer when it is fully worn, under the underwear when that is drawn
                    bool outerOn = worn->visibility(0) > 0.999f;
                    p.set("u_fitHide", V2{outerOn ? 1.f : 0.f, !outerOn && worn->visibility(1) > 0.999f ? 1.f : 0.f});
                }
            }
            p.set("u_pass", pass == 2 ? 1 : 0);
            // cards (not a tiled shell / cap) feather their long sides in every pass, depth passes included
            p.set("u_hairCards", mat.shading == Shading::Hair && !prim.uvTiled ? 1 : 0);
            if (pass == 1 || pass == 2) p.set("u_hairDebug", settings.hairDebug && mat.shading == Shading::Hair ? 1 : 0);
            if (mat.shading == Shading::Hair && pass != 0 && pass != 3) {
                p.set("u_hairUnder", mat.name.find("cap") != std::string::npos ? 1 : 0);
            }
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
        for (auto& c : chars) if (c.second && c.first->loaded()) drawCharacter(depth_, *c.first, 0);
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
        for (auto& c : chars) if (c.second && c.first->loaded()) drawCharacter(depth_, *c.first, 3);
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
    pbr_.set("u_skinDebug", settings.skinDebug);
    pbr_.set("u_screenSize", V2{(float)hdr_.w, (float)hdr_.h});
    pbr_.sampler("u_ssao", 7, ssaoOn ? ao_.color : white_);
    drawModel(pbr_, s.room, s.roomAnim, nullptr, 1);
    for (auto& c : chars) if (c.second && c.first->loaded()) drawCharacter(pbr_, *c.first, 1);
    if (settings.hairSoft) {
        // everything of the hair under its dense strand cores: accumulated order-independently, then composited
        bool oit = oit_.ready();
        if (oit) oit_.begin();
        else {
            glEnable(GL_BLEND);
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
            glDepthMask(GL_FALSE);
        }
        pbr_.set("u_oit", oit ? 1 : 0);
        for (auto& c : chars) if (c.second && c.first->loaded()) drawCharacter(pbr_, *c.first, 2);
        pbr_.set("u_oit", 0);
        if (oit) { oit_.end(hdr_.fbo, hdr_.w, hdr_.h, emptyVao_); glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); }
        else { glDepthMask(GL_TRUE); glDisable(GL_BLEND); }
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

    // ---- blurred copy for frosted UI panels (menus) ----
    bool dofOn = settings.dofAmount > 0.01f;
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

    // ---- depth of field ----
    const RenderTarget* finalSrc = &ldr_;
    if (dofOn) {
        if (dofOut_.w != ldr_.w || dofOut_.h != ldr_.h || !dofOut_.fbo) dofOut_.create(ldr_.w, ldr_.h, GL_RGBA8, false, false, GL_LINEAR);
        dofOut_.bind();
        dof_.use();
        dof_.sampler("u_sharp", 0, ldr_.color);
        dof_.sampler("u_depth", 2, hdr_.depth);
        dof_.set("u_texel", V2{1.f / (float)ldr_.w, 1.f / (float)ldr_.h});
        dof_.set("u_maxPx", 0.016f * (float)ldr_.h);   // ~17 px at 1080p, the same look at any size
        dof_.set("u_near", cam.znear);
        dof_.set("u_far", cam.zfar);
        dof_.set("u_focus", settings.dofFocus);
        dof_.set("u_amount", settings.dofAmount);
        fullscreen(dof_);
        finalSrc = &dofOut_;
    }

    // ---- FXAA -> backbuffer ----
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, fbW, fbH);
    fxaa_.use();
    fxaa_.sampler("u_src", 0, finalSrc->color);
    fxaa_.set("u_texel", V2{1.f / (float)finalSrc->w, 1.f / (float)finalSrc->h});
    fxaa_.set("u_enabled", settings.fxaa ? 1 : 0);
    fullscreen(fxaa_);

    gl::restoreRaylibState(fbW, fbH);
}

}  // namespace ag
