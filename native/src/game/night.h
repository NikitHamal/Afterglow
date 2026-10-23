// Afterglow game — the night: a small simulation of two people. The player sets the rhythm (depth over
// time); strokes are detected from that signal, giving tempo, stroke length and energy. Her arousal
// responds to rhythm match, stroke length, the position (and how fresh it still feels), touch and kisses,
// all scaled by the connection between them. His arousal climbs with energy and her pleasure, so the
// player has to read her desires, pace him (edging pays) and aim for a shared climax.
#pragma once
#include "game/settings.h"
#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace ag {

enum class Desire { None = 0, Faster, Slower, Deeper, Gentler, Touch, Kiss, Change };

struct PositionInfo {
    std::string id, name, group;     // group: Foreplay | Oral | Positions | Solo
    std::string baseClip;            // "<id>.in" (depth-driven) or a loop ("<id>.idle", "<id>.loop")
    bool depthDriven = true;         // the played depth scrubs the clip; loops run at the played tempo
    float her = 1.f, him = 1.f;      // how much each partner gets from it
    bool penetrative = true;
    bool canTouch = false, canKiss = false, solo = false;
};

struct NightEvent {
    enum Type { Stroke, DesireNew, DesireMet, DesireMissed, HerClimax, HisClimax, Together, EdgeStart, Favorite };
    Type type;
    float value = 0.f;               // stroke: impact speed; climax: intensity
    Desire desire = Desire::None;
};

class Night {
public:
    void begin(unsigned seed);
    void restore(const SavedNight& s);
    SavedNight snapshot() const;

    struct Input {
        const PositionInfo* pos = nullptr;
        float depth = 0.f;           // 0..1 (rub intensity in solo)
        float rub = 0.f;             // her touch layer weight 0..1
        float embrace = 0.f;         // embrace / kiss layer weight 0..1
    };
    void update(float dt, const Input& in);
    // still in rhythm: the gap since the last stroke is within the current stroke period (slow strokes count)
    bool stroking() const { return sinceStroke < std::max(1.4f, 1.35f / std::max(tempo, 0.2f) + 0.3f); }
    std::vector<NightEvent> events;  // produced by the last update

    // what she says for a desire (and when it is met)
    static const char* desireLine(Desire d, bool stroking);
    static const char* metLine(unsigned k);

    // ---- state (read by the presentation) ----
    float her = 0.f, him = 0.f;      // arousal 0..1
    float connection = 0.5f;         // 0..1
    float score = 0.f, elapsed = 0.f;
    float tempo = 0.f, amplitude = 0.f, energy = 0.f, sync = 0.f;
    float wantTempo = 0.8f, wantAmp = 0.4f;
    float herRate = 0.f, himRate = 0.f;
    Desire desire = Desire::None;
    float desireAge = 0.f, desireTimeout = 10.f, desireHold = 0.f, nextDesire = 9.f;
    bool edging = false;
    float edgeTime = 0.f;
    int herClimaxes = 0, hisClimaxes = 0, desiresMet = 0, desiresMissed = 0, streak = 0, bestStreak = 0;
    bool together = false, finished = false;
    float herClimaxAge = 1e6f, hisClimaxAge = 1e6f, sinceStroke = 1e6f;
    float posTime = 0.f;
    std::string pos;
    std::map<std::string, float> novelty, like;
    std::string favorite;
    unsigned seed = 1;

private:
    float rnd();
    void detectStrokes(float dt, float depth);
    void pickDesire(const Input& in);
    bool desireSatisfied(const Input& in) const;
    void climaxes();
    unsigned rng_ = 1;
    float lastDepth_ = 0.f, extreme_ = 0.f, lastTrough_ = 0.f, lastPeakT_ = -10.f, maxVel_ = 0.f;
    bool goingIn_ = true;
    std::string desirePos_;
    bool favoriteSaid_ = false;
};

}  // namespace ag
