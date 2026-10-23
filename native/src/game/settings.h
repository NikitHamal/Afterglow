// Afterglow game — persistent player settings and profile (customisation, stats, the night to continue).
// Both live as JSON in the per-user data folder and are written atomically.
#pragma once
#include <map>
#include <string>

namespace ag {

struct Settings {
    // display
    int windowMode = 0;            // 0 windowed, 1 borderless fullscreen, 2 exclusive fullscreen
    int winW = 1600, winH = 900;
    bool vsync = true;
    int fpsCap = 1;                // index into kFpsCaps
    float renderScale = 1.f;       // 0.5 .. 1
    // graphics
    int quality = 2;               // 0 low, 1 medium, 2 high
    bool bloom = true, ssao = true, filmGrain = true, hairPhysics = true;
    // lighting (live): mood 0 warm evening, 1 candlelit night, 2 bright daylight; offsets -1..1
    int lightMood = 0;
    float brightness = 0.f, warmth = 0.f;
    // audio (0..1)
    float master = 0.85f, voice = 1.f, effects = 0.9f, ambience = 0.65f, music = 0.45f;
    // controls
    float mouseSens = 1.f;
    bool invertY = false;
    bool holdRhythm = true;        // hold Space for the auto rhythm (else toggle)
    // game
    int hints = 2;                 // 0 off, 1 subtle (voice + face only), 2 clear
    bool cinematicCamera = true;
    bool storyAuto = true;         // story lines go on by themselves after the voice
    bool depthOfField = true;      // shallow focus in story close-ups
    bool checkUpdates = true;
    bool pauseWhenUnfocused = true;
    std::string skippedVersion;

    static const int kFpsCaps[5];  // 30, 60, 120, 144, 0 (unlimited)
    int fpsLimit() const;
    bool load(const std::string& path);
    bool save(const std::string& path) const;
};

// skin tones, lightest to darkest: Porcelain, Fair, Honey, Olive, Caramel, Bronze, Deep, Ebony
const char* skinToneName(int i);
int skinToneIndex(const std::string& name);   // -1 when unknown
constexpr int kSkinToneCount = 8;
constexpr float kMinAge = 21.f, kMaxAge = 60.f;   // adults only: nothing below 21, ever

struct HerLook {
    float bust = 0.f, hips = 0.f, waist = 0.f, butt = 0.f, weight = 0.f, muscle = 0.f, belly = 0.f;   // -1..1 (muscle/belly 0..1)
    int tone = 4;                  // skin tone index (caramel)
    int hair = 0;                  // index into the hair colour table
    float age = 42.f;              // years, 21..60
};
struct HisLook {
    float muscle = 0.f, weight = 0.f, belly = 0.f, length = 0.f, girth = 0.f;
    int tone = 4;
    float age = 40.f;
};

struct SavedNight {
    bool valid = false;
    std::string pos;
    float her = 0.f, him = 0.f, connection = 0.5f, score = 0.f, elapsed = 0.f, sweat = 0.f;
    int herClimaxes = 0, desiresMet = 0, desiresMissed = 0;
    unsigned seed = 1;
    std::map<std::string, float> novelty;
};

struct Stats {
    int nights = 0, together = 0, herClimaxes = 0, bestScore = 0;
    float bestConnection = 0.f, totalMinutes = 0.f;
};

struct Profile {
    HerLook her;
    HisLook him;
    Stats stats;
    SavedNight night;
    bool load(const std::string& path);
    bool save(const std::string& path) const;
};

}  // namespace ag
