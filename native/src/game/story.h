// Afterglow — story mode: data-driven visual novels over the live 3D scene.
//   native/assets/story/<id>.json  — see docs/STORY_ASSETS.md for the format and the assets each story wants
// A story is a graph of nodes. A node stages the scene (location, lighting mood, who is shown, outfits, a pose
// or act, a camera shot), plays lines (narrator / Vivian / Marcus, voice + sfx cues, expressions, timed
// auto-advance), then offers choices (conditions on variables, effects on them), jumps on, hands the night
// over to free play with a goal ("together", "her", "him", "time:90"), or ends (an ending, recorded).
#pragma once
#include "core/json.h"
#include <map>
#include <string>
#include <vector>

namespace ag {

struct StoryLine {
    std::string who;            // "", "narrator", "vivian", "marcus"
    std::string text;
    std::string voice, sfx;     // voice: "none" keeps a line silent (else the TTS voice from voice/manifest.json)
    std::string vox;            // a vocal sample under audio/vox (moan, sigh...) played with the line
    std::string delivery;       // normal | soft | whisper | playful | urgent | breathless | tender (TTS + acting)
    std::map<std::string, float> expr;   // face morphs for the speaker ("her.smile" / "him.smile" for either)
    float wait = 0.f;           // auto-advance after this many seconds (0 = wait for the player)
    std::string cam;            // camera shot: establish | wide | two | ots | close | xclose | pov | side | front | top | face
    // acting
    std::string gesture;        // open | explain | point | shrug | chest | reach | touch_face | hips | wave | none
    std::string look;           // partner (default) | away | down | camera
    std::string head;           // nod | shake | tilt | down | up
    std::string react;          // the listener: smile | laugh | nod | gasp | shy | sigh | frown
    std::string action;         // staged: kiss | embrace | part | step_in | undress:<who>[:outer|under] | dress:<who> | to_bed | sit | stand
};

struct StoryChoice {
    std::string text, cond, next;
    std::vector<std::pair<std::string, std::string>> set;   // var -> "+1" | "-1" | "=3"
};

struct StoryNode {
    std::string id, chapter, location, mood, cam;
    std::string place;          // location asset wanted (hotel_room, cabin_bedroom, villa_terrace...; the engine
                                // falls back to the closest one that exists)
    std::string ambience;       // party | sea | rain | fire | city | spa | night ("" = from the place)
    std::string pose;           // staging clip: stand | sit | lie | a clip name ("" = keep, or the act's pose)
    std::map<std::string, std::string> wardrobe;  // character -> outfit style (overrides the story's)
    std::map<std::string, std::string> outfit;   // character -> dressed | partial | nude
    std::vector<std::string> show;                // characters on stage (empty = keep)
    bool showSet = false;
    std::string act;            // free-play act id or clip family to pose with ("" = keep)
    float actT = 0.4f;          // depth / phase for the pose
    std::vector<StoryLine> lines;
    std::vector<StoryChoice> choices;
    std::string next;           // after the lines when there are no choices
    std::vector<std::pair<std::string, std::string>> set;   // applied on entering the node
    // hand over to free play: the night runs until the goal, then the story continues at playNext
    std::string playGoal, playNext;
    std::vector<std::string> playActs;           // the acts offered while playing ("" = the node's act family)
    std::map<std::string, std::string> beats;    // play moments -> beat node (start | desire | edge | her_peak | end)
    bool beat = false;          // a beat node: lines spoken over the play, never entered as a scene
    // endings
    bool ending = false;
    std::string endTitle, endText;
    std::string gallery;        // unlocks this gallery entry when reached
};

struct StoryChapter { std::string id, title, start; };

struct Story {
    std::string id, title, tagline, setting, mood;
    std::map<std::string, std::string> wardrobe;  // character -> default outfit style ("cast")
    std::vector<StoryChapter> chapters;
    std::map<std::string, StoryNode> nodes;
    std::map<std::string, float> varsInit;
    std::vector<std::pair<std::string, std::string>> gallery;   // id -> title
    std::vector<std::pair<std::string, std::string>> endings;   // id (node) -> title
    bool load(const std::string& path, std::string& err);
    const StoryNode* node(const std::string& id) const { auto it = nodes.find(id); return it == nodes.end() ? nullptr : &it->second; }
    int chapterIndex(const std::string& id) const;
};

// what the player has seen and reached across playthroughs (persisted with the profile folder)
struct StoryProgress {
    std::map<std::string, std::vector<std::string>> chapters;   // story -> chapters reached
    std::map<std::string, std::vector<std::string>> endings;    // story -> endings found
    std::map<std::string, std::vector<std::string>> gallery;    // story -> gallery entries unlocked
    std::map<std::string, std::vector<std::string>> seen;       // story -> node ids whose lines were read (skip)
    bool load(const std::string& path);
    bool save(const std::string& path) const;
    static bool add(std::vector<std::string>& v, const std::string& s);   // true when new
    static bool has(const std::vector<std::string>& v, const std::string& s);
};

struct StorySave {
    bool used = false;
    std::string story, node, chapterTitle, when;
    int line = 0;
    std::map<std::string, float> vars;
};

// The running story: node / line cursor, variables, typewriter, auto / skip, history.
class StoryPlayer {
public:
    void start(const Story* s, const std::string& node, const std::map<std::string, float>* vars = nullptr, int line = 0);
    bool active() const { return story_ != nullptr; }
    void stop() { story_ = nullptr; }
    const Story* story() const { return story_; }
    const StoryNode* node() const { return node_; }
    int lineIndex() const { return line_; }
    const StoryLine* line() const { return node_ && line_ < (int)node_->lines.size() ? &node_->lines[(size_t)line_] : nullptr; }
    bool atChoices() const { return node_ && line_ >= (int)node_->lines.size() && !node_->choices.empty(); }
    bool atPlay() const { return node_ && line_ >= (int)node_->lines.size() && !node_->playGoal.empty() && node_->choices.empty(); }
    bool atEnding() const { return node_ && line_ >= (int)node_->lines.size() && node_->ending; }
    std::vector<int> visibleChoices() const;
    bool evalCond(const std::string& cond) const;
    void choose(int choiceIndex);
    // advance: finish the typewriter first, then go to the next line / node; returns true when the node changed
    bool advance();
    void update(float dt);      // typewriter + auto advance
    float revealed() const { return reveal_; }          // characters revealed of the current line
    bool lineDone() const;
    void jump(const std::string& node);
    std::map<std::string, float> vars;
    bool autoMode = false, skipMode = false;
    float textSpeed = 42.f;     // characters per second
    float voicedHold = -1.f;    // auto mode: seconds after a voiced line before going on (-1: by text length)
    struct Past { std::string who, text; };
    std::vector<Past> history;
    bool nodeChanged = true;    // consumer resets after staging
    bool lineChanged = true;    // consumer resets after cues

private:
    void applySet(const std::vector<std::pair<std::string, std::string>>& set);
    void enter(const std::string& id);
    const Story* story_ = nullptr;
    const StoryNode* node_ = nullptr;
    int line_ = 0;
    float reveal_ = 0.f, autoT_ = 0.f;
};

std::vector<StorySave> loadStorySaves(const std::string& path, int slots);
bool saveStorySaves(const std::string& path, const std::vector<StorySave>& saves);

}  // namespace ag
