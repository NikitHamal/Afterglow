// Afterglow — story voices: the Kokoro takes in assets/story/voice (manifest.json + <key>.ogg, see docs/VOICE.md),
// streamed one line at a time, with what lip sync needs: the loudness envelope, word timings and a viseme track
// derived from the line's own words (open / spread / round / lips closed / teeth on lip).
#pragma once
#include "raylib.h"
#include "scene/actor.h"
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace ag {

class VoiceBank {
public:
    struct Take {
        std::string who, delivery;
        float dur = 0.f;
        std::vector<unsigned char> env;              // loudness at envFps, 0..255
        std::vector<std::pair<float, float>> words;  // start / end seconds
    };
    bool load(const std::string& voiceDir);          // reads manifest.json; false when there are no voices
    bool ready() const { return !files_.empty(); }
    int lineCount() const { return (int)files_.size(); }
    // the take for a story line (nullptr: silent line, or not voiced yet)
    const Take* take(const std::string& story, const std::string& node, int line, std::string* key = nullptr) const;
    std::string file(const std::string& key) const { return dir_ + "/" + key + ".ogg"; }

    // ---- lip sync helpers ----
    float envelope(const Take& t, float time) const;            // 0..1 (linear between frames)
    int wordAt(const Take& t, float time, float pad = 0.035f) const;   // index of the word being spoken, -1 between words
    // what the mouth does at `time` for this take of `text` (words aligned in order; letters spread over each word)
    Viseme viseme(const Take& t, const std::string& text, float time) const;
    // words of `text` that are out by `time` (karaoke subtitles); all of them once the take has ended
    int wordsSpoken(const Take& t, float time) const;

private:
    std::string dir_;
    float envFps_ = 30.f;
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::string>>> lines_;   // story -> node -> keys
    std::unordered_map<std::string, Take> files_;
};

// one line at a time, streamed (Music) so a story's hundreds of takes never sit in memory
class VoicePlayer {
public:
    bool play(const std::string& file, float volume);
    void stop();
    void update(float dt, bool paused);
    void setVolume(float v);
    bool playing() const { return active_; }
    float time() const { return t_; }          // seconds since the line started (what the listener hears)
private:
    Music music_{};
    bool loaded_ = false, active_ = false, paused_ = false;
    float t_ = 0.f, len_ = 0.f;
};

}  // namespace ag
