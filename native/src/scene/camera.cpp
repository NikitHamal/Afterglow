#include "scene/camera.h"
#include "scene/character.h"
#include "raylib.h"
#include <cmath>

namespace ag {

static float wrapAngle(float a) { while (a > kPi) a -= 2.f * kPi; while (a < -kPi) a += 2.f * kPi; return a; }

void CameraRig::setOrbit(const V3& t, float y, float p, float d, float blendSeconds) {
    mode_ = Mode::Orbit;
    fromTarget_ = target; fromYaw_ = yaw; fromPitch_ = pitch; fromDist_ = dist;
    toTarget_ = t; toYaw_ = fromYaw_ + wrapAngle(y - fromYaw_); toPitch_ = p; toDist_ = d;
    blendDur_ = blendSeconds; blendT_ = blendSeconds > 0.f ? 0.f : 1.f;
    if (blendSeconds <= 0.f) { target = t; yaw = toYaw_; pitch = p; dist = d; }
}

void CameraRig::setFirstPerson(Character* c, const std::string& bone, const V3& offset, float blendSeconds) {
    mode_ = Mode::FirstPerson;
    fpChar_ = c; fpBone_ = bone; fpOffset_ = offset; fpLocal_ = false;
    fpYaw_ = 0.f; fpPitch_ = -0.35f;
    fpBlend_ = blendSeconds > 0.f ? 0.f : 1.f;
    blendDur_ = blendSeconds;
}

void CameraRig::setFirstPersonLocal(Character* c, const std::string& bone, const V3& offLocal, const V3& fwdLocal, float blendSeconds) {
    setFirstPerson(c, bone, offLocal, blendSeconds);
    fpLocal_ = true;
    fpForwardLocal_ = normalize(fwdLocal);
    fpPitch_ = -0.15f;
}

void CameraRig::handleInput(float dt) {
    (void)dt;
    if (!inputEnabled) return;
    Vector2 md = GetMouseDelta();
    float wheel = GetMouseWheelMove();
    if (mode_ == Mode::Orbit) {
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            toYaw_ -= md.x * 0.0055f; toPitch_ = clampf(toPitch_ + md.y * 0.0045f, -0.25f, 1.45f);
            blendT_ = blendDur_ = 0.f;
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
            V3 right{std::cos(yaw), 0.f, -std::sin(yaw)};
            toTarget_ = toTarget_ + right * (-md.x * 0.0018f * dist) + V3{0, md.y * 0.0018f * dist, 0};
            blendT_ = blendDur_ = 0.f;
        }
        if (wheel != 0.f) { toDist_ = clampf(toDist_ * std::pow(0.9f, wheel), 0.35f, 8.f); blendT_ = blendDur_ = 0.f; }
    } else {
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            fpYaw_ = clampf(fpYaw_ - md.x * 0.004f, -1.2f, 1.2f);
            fpPitch_ = clampf(fpPitch_ - md.y * 0.004f, -1.3f, 0.6f);
        }
    }
}

CameraState CameraRig::update(float dt, float aspect) {
    time_ += dt;
    CameraState cs;
    cs.fovy = fovDeg_ * kPi / 180.f;
    // orbit parameters: timed blend for presets, critically damped follow for user input
    if (blendDur_ > 0.f && blendT_ < 1.f) {
        blendT_ = std::min(1.f, blendT_ + dt / blendDur_);
        float u = smoothstep(0.f, 1.f, blendT_);
        target = lerp(fromTarget_, toTarget_, u);
        yaw = lerpf(fromYaw_, toYaw_, u); pitch = lerpf(fromPitch_, toPitch_, u); dist = lerpf(fromDist_, toDist_, u);
    } else {
        float k = dt > 0.f ? damp(14.f, dt) : 1.f;
        target = lerp(target, toTarget_, k);
        yaw = lerpf(yaw, toYaw_, k); pitch = lerpf(pitch, toPitch_, k); dist = lerpf(dist, toDist_, k);
    }
    V3 off{std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw)};
    V3 eye = target + off * dist, tgt = target;

    if (mode_ == Mode::FirstPerson && fpChar_ && fpChar_->loaded()) {
        fpBlend_ = blendDur_ > 0.f ? std::min(1.f, fpBlend_ + dt / blendDur_) : 1.f;
        M4 hb = fpChar_->boneWorld(fpBone_);
        V3 head = hb.translation();
        V3 fwd = fpChar_->forward();
        V3 fEye;
        if (fpLocal_) {
            fEye = xformPoint(hb, fpOffset_);
            fwd = normalize(xformDir(hb, fpForwardLocal_));
        } else {
            V3 r0 = normalize(cross(fwd, V3{0, 1, 0}));
            fEye = head + V3{0, fpOffset_.y, 0} + fwd * fpOffset_.z + r0 * fpOffset_.x;
        }
        V3 right = normalize(cross(fwd, V3{0, 1, 0}));
        if (length(right) < 0.5f) right = V3{1, 0, 0};
        Quat q = qmul(qaxis({0, 1, 0}, fpYaw_), qaxis(right, fpPitch_));
        V3 dir = normalize(qrotate(q, fwd));
        float u = smoothstep(0.f, 1.f, fpBlend_);
        eye = lerp(eye, fEye, u);
        tgt = lerp(tgt, fEye + dir, u);
    }
    if (sway > 0.f) {
        V3 s{vnoise1(time_ * 0.37f), vnoise1(time_ * 0.29f + 5.f), vnoise1(time_ * 0.31f + 9.f)};
        eye = eye + s * (0.012f * sway);
        tgt = tgt + V3{vnoise1(time_ * 0.23f + 2.f), vnoise1(time_ * 0.19f + 7.f), 0.f} * (0.006f * sway);
    }
    lastEye_ = eye; lastTarget_ = tgt;
    cs.eye = eye; cs.target = tgt;
    cs.view = lookAt(eye, tgt, V3{0, 1, 0});
    cs.proj = perspective(cs.fovy, aspect > 0.f ? aspect : 1.7778f, cs.znear, cs.zfar);
    cs.viewProj = cs.proj * cs.view;
    return cs;
}

}  // namespace ag
