// Afterglow engine — facial life shared by the story actors and the night: natural blinks and lids that follow the
// gaze. A blink is a quick close (~70 ms), a held instant, a slower open (~150 ms); intervals are uneven (mostly short,
// now and then a long stare), sometimes a double blink, and a blink is likely when the gaze jumps or a line starts.
#pragma once
#include <cmath>
#include <cstdint>

namespace ag {

class Blinker {
public:
    explicit Blinker(uint32_t seed = 1u) : seed_(seed * 2654435761u + 1u) { next_ = 0.6f + 2.f * rnd(); }
    // a blink now (unless one is running); chance 0..1
    void trigger(float chance = 1.f) { if (phase_ < 0.f && rnd() < chance) { phase_ = 0.f; wait_ = 0.f; } }
    // weight 0..1 this frame; rate > 1 blinks more often (talking, aroused), < 1 less (rapt attention)
    float update(float dt, float rate = 1.f) {
        if (phase_ < 0.f) {
            wait_ += dt * rate;
            if (wait_ >= next_) { phase_ = 0.f; wait_ = 0.f; }
            else return 0.f;
        }
        phase_ += dt / 0.24f;
        float p = phase_, w;
        if (p < 0.29f) { float t = p / 0.29f; w = t * t * (3.f - 2.f * t); }
        else if (p < 0.4f) w = 1.f;
        else if (p < 1.f) { float t = (p - 0.4f) / 0.6f; w = 1.f - (1.f - (1.f - t) * (1.f - t)); }
        else {
            w = 0.f;
            phase_ = -1.f;
            if (double_) { double_ = false; wait_ = 0.f; next_ = 0.12f; }           // the second of a double blink
            else { double_ = rnd() < 0.12f; float r = rnd(); next_ = 1.1f + 4.2f * r * r + (rnd() < 0.1f ? 4.f : 0.f); }
        }
        return w;
    }

private:
    float rnd() { seed_ = seed_ * 1664525u + 1013904223u; return (float)((seed_ >> 8) & 0xFFFFFFu) / 16777216.f; }
    uint32_t seed_;
    float phase_ = -1.f, wait_ = 0.f, next_ = 2.f;
    bool double_ = false;
};

// the upper lids follow the eyes: down when looking down (they cover the iris' top), up a little when looking up
// pitch = the gaze elevation in the head's frame (radians, + up)
inline void lidsFollow(float pitch, float& down, float& up) {
    auto c01 = [](float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); };
    down = c01(-pitch * 1.6f) * 0.6f;
    up = c01(pitch * 1.4f) * 0.5f;
}

}  // namespace ag
