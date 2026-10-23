// Afterglow engine — sandbox app: loads the scene and exposes every engine feature through an F1
// debug overlay (clips, depth, layers, morphs, body sliders, tones, cameras, quality, toggles).
// Also the reference implementation of App for the screenshot harness.
#pragma once
#include "app/app.h"
#include "audio/audio.h"
#include "gfx/renderer.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include <map>
#include <string>
#include <vector>

namespace ag {

class Sandbox : public App {
public:
    bool init() override;
    void shutdown() override;
    void reset() override;
    void applySpec(const Spec& spec) override;
    void update(float dt) override;
    void render(int fbWidth, int fbHeight) override;
    bool wantsQuit() const override { return quit_; }

    void cameraPreset(const std::string& name, float blend = 0.8f);

    Scene scene;
    Renderer renderer;
    CameraRig camera;
    Audio audio;
    bool enableAudio = true;
    bool overlay = true;

private:
    void applyPose();
    void drawOverlay(int w, int h);
    std::string currentPos() const { return positions_.empty() ? std::string() : positions_[(size_t)posIndex_]; }
    V3 focusPoint() const;
    const Json& clipMeta(const std::string& clip) const;

    std::vector<std::string> positions_;
    int posIndex_ = 0;
    float depth_ = 0.55f, rub_ = 0.f, embrace_ = 0.f, aroused_ = 1.f, flex_ = 0.f;
    bool autoThrust_ = false;
    float thrustRate_ = 1.1f, thrustPhase_ = 0.f, rubPhase_ = 0.f, idlePhase_ = 0.f;
    std::map<std::string, float> face_, mface_, body_, mbody_;
    int tone_ = 1, mtone_ = 1, quality_ = 2;
    bool hairSim_ = true, breathing_ = true, lookAt_ = true;
    float time_ = 0.f;
    CameraState cam_;
    Json clipsMeta_;
    std::string camPreset_ = "front";
    bool quit_ = false;
    bool dropdownEdit_ = false;
    bool forceChars_ = false;
    int fbW_ = 1280, fbH_ = 720;
};

}  // namespace ag
