// Afterglow engine — acting: what keeps a character alive while talking or listening, layered over any clip.
// Gaze toward the partner (or away / down / at the camera), nods, tilts and small beats on stressed words, talk
// gestures as additive rotations in the body's own frame, a lean while speaking, listener reactions, blinking,
// and facial expressions blended with the speech visemes into the character's face morphs.
#pragma once
#include "core/mathx.h"
#include "scene/facelife.h"
#include <map>
#include <string>

namespace ag {

class Animator;
class Character;

// the mouth shape of speech at one moment (from the voice take: see game/voice.h)
struct Viseme {
    float open = 0.f;     // jaw drop (a, o)
    float wide = 0.f;     // lips spread (e, i)
    float round = 0.f;    // lips rounded / pushed (o, u, w)
    float bite = 0.f;     // upper teeth on the lower lip (f, v)
    float closed = 0.f;   // lips pressed (m, b, p)
};

// what a line asks of one character
struct ActingCue {
    bool speaking = false;
    std::string gesture;      // open | explain | point | shrug | reach | wave | chest | none
    std::string head;         // nod | shake | tilt | down | up
    std::string look;         // partner | away | down | camera
    std::string react;        // when listening: smile | laugh | nod | gasp | shy | sigh | frown
    float energy = 0.5f;      // delivery: whisper 0.2 .. urgent 0.9
};

class Actor {
public:
    void bind(Character* c);
    void cue(const ActingCue& cue);                 // a new line starts
    // speech this frame (only while this character's line plays)
    void speech(float loudness, const Viseme& v, bool beat);
    void silence() { speaking_ = false; }
    void update(float dt, const V3& partnerHead, bool hasPartner, const V3& camEye);
    void pose(Animator& a);                         // install as the character's poseHook (body, arms)
    void postPose(Animator& a);                     // ... and postHook (head motions, after the look-at)
    void reset();
    void releaseFace();                             // hand the face morphs back (leaving the story)
    float gestureWeight(const char* g) const { return gesture_ == g ? gestW_ * acting : 0.f; }

    std::map<std::string, float> expr;              // expression targets (morph -> weight), blended
    float acting = 1.f;                             // gestures / head / lean (0: an act owns the body)
    float lean = 0.f;                               // leaning in toward the partner (kiss, embrace) 0..1
    float leanMax = 0.12f;                          // ... at most this far (radians)
    float turn = 0.f;                               // 0..1 the upper body turns toward the partner (seated side by side)
    float sit = 0.f;                                // seated: gestures stay in the forearms
    bool eyesClosed = false;                        // kissing
    // the kiss: the lip front goes to kissTarget (world) with weight kissW (neck + head two-bone IK), the head rolled
    // by kissRoll (radians, toward the character's own right); mouthRest = the lip front before that, this frame
    float kissW = 0.f, kissRoll = 0.f;
    V3 kissTarget{0, 0, 0}, mouthRest{0, 0, 0};

private:
    struct Arm { float flex = 0, abduct = 0, elbow = 0, supin = 0, shrug = 0; };
    void gestureTargets(Arm& l, Arm& r) const;
    void face(float dt);
    void kissIK(Animator& a);

    Character* c_ = nullptr;
    bool female_ = true;
    float time_ = 0.f;
    // line state
    std::string gesture_, head_, look_, react_;
    float energy_ = 0.5f, lineT_ = 0.f, reactT_ = 99.f, headT_ = 99.f, beatT_ = 99.f, tiltSide_ = 1.f;
    bool speaking_ = false;
    float speakW_ = 0.f, gestW_ = 0.f, loud_ = 0.f;
    Viseme vis_, visS_;
    // gaze
    V3 partner_{0, 1.5f, 0}, cam_{0, 1.5f, 3.f};
    bool hasPartner_ = false;
    // face
    std::map<std::string, float> cur_;
    Blinker blinker_;
    float closed_ = 0.f, pitch_ = 0.f;              // pitch_: gaze elevation in the head's frame (lids follow it)
    V3 lastLook_{0, 0, 0};
    int head = -1, neck = -1, upper = -1, chest = -1, sh[2] = {-1, -1}, arm[2] = {-1, -1}, elbow[2] = {-1, -1}, wrist[2] = {-1, -1};
};

}  // namespace ag
