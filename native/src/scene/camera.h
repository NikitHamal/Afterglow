// Afterglow engine — camera rig: orbit with smooth preset transitions, first-person bone attach, sway.
#pragma once
#include "core/mathx.h"
#include <string>

namespace ag {

class Character;

struct CameraState {
    V3 eye{0, 1.4f, 3.f}, target{0, 0.8f, 0}, up{0, 1, 0};
    float fovy = 0.66f, znear = 0.03f, zfar = 40.f;
    M4 view = M4::identity(), proj = M4::identity(), viewProj = M4::identity();
};

class CameraRig {
public:
    enum class Mode { Orbit, FirstPerson };

    // Orbit around target: yaw 0 looks from +Z toward -Z, pitch up positive (radians).
    void setOrbit(const V3& target, float yaw, float pitch, float dist, float blendSeconds = 0.8f);
    void setFirstPerson(Character* c, const std::string& bone, const V3& offset, float blendSeconds = 0.6f);
    void setFov(float degrees) { fovDeg_ = degrees; }
    Mode mode() const { return mode_; }

    // Mouse: RMB drag orbits / looks, wheel zooms, MMB drag pans.
    void handleInput(float dt);
    CameraState update(float dt, float aspect);

    float sway = 0.f;            // handheld sway amount (0 off)
    bool inputEnabled = true;
    // live orbit parameters (readable)
    V3 target{0, 0.8f, 0};
    float yaw = 0.f, pitch = 0.3f, dist = 2.6f;

private:
    Mode mode_ = Mode::Orbit;
    // transition
    V3 fromTarget_{0, 0.8f, 0}, toTarget_{0, 0.8f, 0};
    float fromYaw_ = 0, toYaw_ = 0, fromPitch_ = 0.3f, toPitch_ = 0.3f, fromDist_ = 2.6f, toDist_ = 2.6f;
    float blendT_ = 1.f, blendDur_ = 0.f;
    // first person
    Character* fpChar_ = nullptr;
    std::string fpBone_;
    V3 fpOffset_{0, 0.08f, 0.08f};
    float fpYaw_ = 0.f, fpPitch_ = -0.35f, fpBlend_ = 0.f;
    V3 lastEye_{0, 1.4f, 3.f}, lastTarget_{0, 0.8f, 0};
    float fovDeg_ = 38.f, time_ = 0.f;
};

}  // namespace ag
