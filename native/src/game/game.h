// Afterglow — the game. Screens: loading, title, main menu, customisation (live 3D preview), settings,
// play (the night), pause, results. Everything is drawn with the custom UI kit over the live 3D scene.
// The harness drives it with specs: screen=title|menu|customize|settings|play|pause|results plus
// gameplay keys (pos, depth, her, him, desire, ...), so CI renders every screen.
#pragma once
#include "app/app.h"
#include "audio/audio.h"
#include "game/director.h"
#include "game/night.h"
#include "game/story.h"
#include "game/voice.h"
#include "game/settings.h"
#include "gfx/renderer.h"
#include "scene/camera.h"
#include "scene/actor.h"
#include "scene/collision.h"
#include "scene/scene.h"
#include "ui/ui.h"
#include "core/json.h"
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace ag {

class Updater;

class Game : public App {
public:
    bool init() override;
    void shutdown() override;
    void reset() override;
    void applySpec(const Spec& spec) override;
    void update(float dt) override;
    void render(int fbWidth, int fbHeight) override;
    bool wantsQuit() const override { return quit_; }

    // set by main before init()
    bool enableAudio = true;
    bool harness = false;
    Updater* updater = nullptr;
    std::function<void(const Settings&, bool modeChanged)> applyDisplay;   // window mode / vsync / fps
    std::function<void(float, const char*)> loadingProgress;                // draws the loading screen

    Settings settings;
    Profile profile;
    bool paused() const { return screen_ == Screen::Pause; }
    // focus lost while playing: pause (when the setting asks for it)
    void onFocusLost();
    void saveAll();

    enum class Screen { Title, Menu, Customize, Settings, Play, Pause, Results, Stories, Story };

private:
    // ---- flow ----
    void go(Screen s, float fadeTime = 0.35f);
    void enterScreen(Screen s);
    Screen screen_ = Screen::Title, pending_ = Screen::Title;
    bool switching_ = false;
    float fade_ = 0.f, fadeDir_ = 0.f, fadeTime_ = 0.35f, screenTime_ = 0.f, introT_ = 0.f;
    Screen settingsReturn_ = Screen::Menu;
    bool quit_ = false;

    // ---- scene staging ----
    enum class Stage { Bedroom, Showcase, Night };
    void stage(Stage st);
    void applyLooks();                    // profile -> characters (morphs, tones, hair colour)
    void standPose(Character& c);         // relaxed standing pose (customisation preview)
    void applySkin(Character& c, int tone);
    static float ageToMorph(float years, float base);
    void updateMenuCamera(float dt);
    Stage stage_ = Stage::Bedroom;
    float turntable_ = 0.f, turnVel_ = 0.f, showcaseZoom_ = 0.f;
    int showcaseWho_ = 0;                 // 0 her, 1 him

    // ---- play ----
    void startNight(bool resume);
    void endNight(bool natural);
    void updatePlay(float dt);
    void presentPlay(float dt);           // expressions, gaze, sweat, audio, camera, post
    void setPosition(int index, bool instant = false);
    void playAudioEvents();
    void cameraForPosition(float blend);
    void drawHud();
    void drawPositionBar(float a);
    void drawActionBar();
    void actById(const std::string& id);
    int nextCamMode(int cur, bool solo) const;
    int lastAct_ = -1;
    Rectangle actionBarRect_{0, 0, 0, 0};
    int lastTip_ = -1;           // action bar tooltip (kept while it fades out)
    Rectangle lastTipR_{0, 0, 0, 0};
    std::vector<PositionInfo> positions_;
    int posIndex_ = 0, pendingPos_ = -1;
    float posFade_ = 0.f;                 // position change dip (0..1..0)
    Night night_;
    float depth_ = 0.f, depthTarget_ = 0.f, dragDepth_ = 0.f;
    bool rhythm_ = false, rhythmToggle_ = false;
    float rhythmPhase_ = 0.f, rhythmTempo_ = 0.9f, rhythmAmp_ = 0.55f, rhythmCenter_ = 0.5f;
    float rub_ = 0.f, rubTarget_ = 0.f, embrace_ = 0.f, embraceTarget_ = 0.f, rubPhase_ = 0.f, idlePhase_ = 0.f, loopPhase_ = 0.f;
    float wheelImpulse_ = 0.f;
    bool holdDepth_ = false;              // harness stills: keep the requested depth
    bool autoplay_ = false;               // harness: a simple player plays the night (end-to-end test)
    float autoLog_ = 0.f;
    void autoplayStep(float dt);
    int camMode_ = 0;                     // 0 side, 1 front, 2 top, 3 first person, 4 her face
    bool camUser_ = false;                // player orbited: stop cinematic drift until next cut
    float cinematicT_ = 0.f, nextCut_ = 14.f;
    bool hudHidden_ = false, barOpen_ = false;
    float barT_ = 0.f, hintsT_ = 0.f, hudT_ = 0.f;
    // presentation state
    float blinkT_ = 2.f, blinkV_ = 0.f, mBlinkT_ = 3.f, mBlinkV_ = 0.f;
    float moanPulse_ = 0.f, smilePulse_ = 0.f, climaxGlow_ = 0.f, flash_ = 0.f, sweat_ = 0.f, slowmo_ = 1.f;
    float herClimaxT_ = 99.f, hisClimaxT_ = 99.f, afterT_ = 0.f;
    // the night's effect on the grade, applied on top of the lighting mood
    float postBloomAdd_ = 0.f, postSatAdd_ = 0.f, postVigAdd_ = 0.f, postWarmAdd_ = 0.f, postExpMul_ = 1.f;
    std::string subtitle_, subtitle2_;
    float subtitleT_ = 0.f, subtitleDur_ = 0.f;
    Desire shownDesire_ = Desire::None;
    float desireShow_ = 0.f;
    float breathT_ = 0.f, moanT_ = 3.f, wetT_ = 0.f;
    bool together_ = false;
    std::vector<std::pair<float, std::string>> toasts_;   // floating feedback (age, text)
    float introCard_ = 0.f;               // first night: how-to card (seconds left)
    void drawIntroCard(float a);

    // ---- results ----
    struct Results { int score = 0; float minutes = 0.f, connection = 0.f; int herClimaxes = 0; bool together = false;
                     int desiresMet = 0, desiresMissed = 0, bestStreak = 0; std::string rating, line; bool soon = false; } results_;

    // ---- screens (UI) ----
    void drawTitle();
    void drawMenu();
    void drawCustomize();
    void drawSettings();
    void drawPause();
    void drawResults();
    void drawUpdateCard(float x, float y, float w);
    void drawDebug();
    void drawLoading(float f, const char* what);
    float loadF_ = 0.f;
    std::string loadWhat_;
    void heroLogo(float cx, float y, float size, float a);
    int custTab_ = 0, setTab_ = 0;
    bool confirmQuit_ = false, confirmNew_ = false;
    bool debug_ = false;
    bool shotPending_ = false;            // F12: capture after this frame's UI
    std::string shotToast_;
    float shotToastT_ = 0.f;

    // ---- story mode ----
    void loadStories();
    const Story* storyById(const std::string& id) const;
    void beginStory(const Story* s, const std::string& node, const std::map<std::string, float>* vars = nullptr, int line = 0);
    void storyAutosave();
    void stageStoryNode();
    void storyCamera(const std::string& cam, float blend);
    bool storyCharVisible(int who) const;
    void updateStory(float dt);
    void presentStory(float dt);
    void drawStories();
    void drawStory();
    void drawStoryLog();
    void drawStorySlots(bool saving);
    void drawStoryMenu();
    void drawGallery();
    std::vector<Story> stories_;
    StoryProgress storyProgress_;
    StoryPlayer story_;
    std::vector<StorySave> storySaves_;
    std::string storyProgressPath_, storySavesPath_;
    bool storyPlaying_ = false, storySkipToggle_ = false;
    int storyOverlay_ = 0;                // 0 none, 1 log, 2 save, 3 load, 4 gallery, 5 menu
    float storyCardT_ = 99.f, storyLogScroll_ = 0.f;
    int storyCardKind_ = 0, storyMood_ = -1, storySpeak_ = -1, gallerySel_ = 0;
    std::string storyLastChapter_, storyLocation_;
    bool storyShow_[2] = {true, true};
    std::string storyOutfit_[2];
    std::map<std::string, float> storyExpr_;
    Rectangle storyControlsRect_{0, 0, 0, 0};
    Screen pauseReturn_ = Screen::Play;

    // ---- the story as a film (game_cinema.cpp): staging, voices + lip sync, acting, direction, intimacy ----
    void storyStage(const StoryNode* n);
    std::string resolvePlace(const StoryNode* n) const;
    void storyLineStarted();
    void storyAction(const std::string& action);
    void storyShot(const StoryLine* l, bool newNode);
    void applyStoryPose(float dt);
    void directStory(float dt);
    void storyStartPlay();
    void storyBeats(float dt);
    void startBeat(const std::string& trigger);
    void leaveStoryWorld();
    void drawSubtitle(const std::string& who, const std::string& text, int wordsShown, float alpha, float baseY);
    float storyVoiceTime() const { return voiceHold_ >= 0.f ? voiceHold_ : voiceT_; }
    VoiceBank voices_;
    VoicePlayer voicePlayer_;
    Director director_;
    Actor actors_[2];
    const VoiceBank::Take* take_ = nullptr;
    std::string takeKey_;
    float voiceT_ = 0.f, voiceHold_ = -1.f;   // harness: hold the voice clock (lip-sync stills)
    int speaker_ = -1, prevSpeaker_ = -2;
    float lastEnv_ = 0.f;
    std::string pose_, place_;
    float black_ = 0.f;
    int blackDir_ = 0;                    // 1 going dark, -1 coming back
    bool stagePending_ = false, playPending_ = false;
    std::string posePending_;
    float letterbox_ = 0.f;
    float kiss_ = 0.f, kissTarget_ = 0.f, near_ = 0.f, nearTarget_ = 0.f;
    std::string beatNode_, beatWho_, beatText_;
    int beatLine_ = -1;
    float beatT_ = 0.f, beatCool_ = 0.f, beatMouth_ = 0.f;
    bool beatEdge_ = false, beatPeak_ = false;
    const VoiceBank::Take* beatTake_ = nullptr;
    int shotCount_ = 0;
    std::string storyStyle_[2], storyAmb_;
    void setStoryPose(const std::string& pose, float actT);
    int actIndex(const std::string& id) const;
    void enterStoryWorld();

    // ---- audio ----
    void initAudio();
    void shutdownAudio();
    void updateAudio(float dt);
    void voice(const std::string& name, float vol = 1.f, float pitch = 1.f);
    void sfx(const std::string& prefix, float vol, float pitchJitter = 0.06f);
    void breath(bool in, float vol);
    void setAmbience(const std::string& places);   // party | sea | rain | fire | city | spa ("" = the night room)
    void duckAmbience(float amount);
    float ambLevel_ = 0.f, musicLevel_ = 0.f;
    float mouthT_ = 0.f, mouthDur_ = 1.f;   // her mouth follows her voice lines (open over the sample)
    // touch reactions: how much she is being touched (zone-weighted), the flinch when it starts, her arch
    float touchAct_ = 0.f, flinch_ = 0.f, arch_ = 0.f;
    void archBody(Animator& a) const;

    // ---- core objects ----
    Scene scene;
    Renderer renderer;
    CameraRig camera;
    BodyProxies proxies_;
    CameraSafety camSafety_;
    Audio audio;
    ui::Ui ui;
    CameraState cam_;
    Json clipsMeta_;
    float time_ = 0.f, frameDt_ = 1.f / 60.f;
    int fbW_ = 1280, fbH_ = 720;
    int lastRenderW_ = 0, lastRenderH_ = 0;
    int blurFresh_ = 0;                   // frames rendered with the UI blur since entering this screen
    std::string settingsPath_, profilePath_;
    float saveTimer_ = 0.f;
    bool sceneReady_ = false;
};

}  // namespace ag
