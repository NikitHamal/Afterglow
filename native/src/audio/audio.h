// Afterglow engine — audio hooks on raylib/miniaudio: named samples with polyphony, random variants,
// pitch/pan/volume, ambient loops, and procedural buffers.
#pragma once
#include "raylib.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ag {

class Audio {
public:
    bool init();                         // opens the device (safe to call without hardware: becomes a no-op)
    void shutdown();
    // Loads every .ogg/.wav/.mp3 under assets/audio (falls back to the repo's legacy sound folders).
    // Names: lower-case relative path without extension, e.g. "strokes/stroke03".
    int loadAll();
    bool has(const std::string& name) const { return sounds_.count(name) > 0; }
    std::vector<std::string> names(const std::string& prefix) const;

    bool play(const std::string& name, float volume = 1.f, float pitch = 1.f, float pan = 0.5f);
    // plays a random sample whose name starts with prefix (never the same one twice in a row)
    bool playRandom(const std::string& prefix, float volume = 1.f, float pitchJitter = 0.05f, float pan = 0.5f);
    // length in seconds of a loaded sample (0 when absent) - the lip sync reads it instead of hard-coding durations
    float length(const std::string& name) const;
    // first prefix (in order) that has samples, then a random name from it (never the same one twice in a row):
    // how a per-model voice bank falls back to the role's, and the role's to the shared one. "" when none.
    std::string randomFrom(const std::vector<std::string>& prefixes);
    // procedural one-shot: generator writes `frames` mono float samples at 44.1 kHz
    bool addProcedural(const std::string& name, int frames, const std::function<void(float*, int)>& gen);

    bool loop(const std::string& name, float volume);   // ambient loop (streamed); volume 0 stops
    void setLoopVolume(const std::string& name, float volume);
    void update();                                      // feeds music streams; call once per frame

    void setMasterVolume(float v);
    float masterVolume() const { return master_; }
    bool ready() const { return ready_; }

private:
    struct Voice { std::vector<Sound> aliases; size_t next = 0; };
    std::unordered_map<std::string, Sound> sounds_;
    std::unordered_map<std::string, Voice> voices_;
    std::unordered_map<std::string, std::string> files_;
    std::unordered_map<std::string, Music> loops_;
    std::unordered_map<std::string, std::string> lastRandom_;
    bool ready_ = false;
    float master_ = 1.f;
};

}  // namespace ag
