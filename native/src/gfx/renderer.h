// Afterglow engine — frame renderer: shadow map, forward HDR pass (PBR / skin / hair / eye / room),
// soft hair edges, bloom, filmic composite and FXAA.
#pragma once
#include "gfx/shader.h"
#include "gfx/texture.h"
#include "scene/scene.h"
#include "scene/camera.h"

namespace ag {

enum class Quality { Low = 0, Medium = 1, High = 2 };

struct RenderSettings {
    Quality quality = Quality::High;
    int shadowRes = 2048;
    bool shadows = true;
    float shadowSoft = 2.4f;          // PCF radius in texels
    bool bloom = true;
    float bloomStrength = 0.07f, bloomThreshold = 1.0f, bloomKnee = 0.7f, bloomRadius = 1.0f;
    int bloomLevels = 6;
    bool fxaa = true;
    bool hairSoft = true;
    float renderScale = 1.f;          // internal resolution multiplier
    float exposure = 1.1f, warmth = 0.06f, saturation = 1.0f, contrast = 1.04f, vignette = 0.26f, grain = 0.024f;
    float flash = 0.f;                // climax flash 0..1
};
void applyQuality(RenderSettings& s, Quality q);

class Renderer {
public:
    bool init();
    void shutdown();
    // Renders the scene to the default framebuffer (fbW x fbH pixels), leaving GL ready for raylib 2D.
    void render(Scene& scene, const CameraState& cam, int fbW, int fbH, float time);
    bool reloadShaders();

    RenderSettings settings;
    int drawCalls = 0;
    long triangles = 0;

private:
    void ensureTargets(int w, int h);
    void setFrame(Program& p, Scene& s, const CameraState& cam, float time);
    void drawModel(Program& p, Model& m, Animator& a, const Character* ch, int pass);
    void bindMaterial(Program& p, const Model& m, int matIndex, const Character* ch, bool shadowPass);
    void fullscreen(Program& p);
    M4 computeShadowVP(const Scene& s) const;

    Program pbr_, depth_, prefilter_, down_, up_, composite_, fxaa_;
    RenderTarget hdr_, ldr_;
    RenderTarget bloom_[8];
    int bloomCount_ = 0;
    ShadowMap shadow_;
    GLuint emptyVao_ = 0, white_ = 0, flatNormal_ = 0, black_ = 0;
    int w_ = 0, h_ = 0;
    M4 shadowVP_ = M4::identity();
};

}  // namespace ag
