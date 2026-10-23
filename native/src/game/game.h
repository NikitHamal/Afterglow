// Afterglow — the game. Screens: loading, title, main menu, customisation (live 3D preview), settings,
// play (the night), pause, results. Everything is drawn with the custom UI kit over the live 3D scene.
// The harness drives it with specs: screen=title|menu|customize|settings|play|pause|results plus
// gameplay keys (pos, depth, her, him, desire, ...), so CI renders every screen.
#pragma once
#include "app/app.h"
#include "audio/audio.h"
#include "game/night.h"
#include "game/settings.h"
#include "gfx/renderer.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "ui/ui.h"
#include "core/json.h"
#include <functional>
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

    enum class Screen { Title, Menu, Customize, Settings, Play, Pause, Results };

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
    std::vector<PositionInfo> positions_;
    int posIndex_ = 0, pendingPos_ = -1;
    float posFade_ = 0.f;                 // position change dip (0..1..0)
    Night night_;
    float depth_ = 0.f, depthTarget_ = 0.f, dragDepth_ = 0.f;
    bool rhythm_ = false, rhythmToggle_ = false;
    float rhythmPhase_ = 0.f, rhythmTempo_ = 0.9f, rhythmAmp_ = 0.55f, rhythmCenter_ = 0.5f;
    float rub_ = 0.f, rubTarget_ = 0.f, embrace_ = 0.f, embraceTarget_ = 0.f, rubPhase_ = 0.f, idlePhase_ = 0.f;
    float wheelImpulse_ = 0.f;
    bool holdDepth_ = false;              // harness stills: keep the requested depth
    int camMode_ = 0;                     // 0 side, 1 front, 2 top, 3 first person
    bool camUser_ = false;                // player orbited: stop cinematic drift until next cut
    float cinematicT_ = 0.f, nextCut_ = 14.f;
    bool hudHidden_ = false, barOpen_ = false;
    float barT_ = 0.f, hintsT_ = 0.f, hudT_ = 0.f;
    // presentation state
    float blinkT_ = 2.f, blinkV_ = 0.f, mBlinkT_ = 3.f, mBlinkV_ = 0.f;
    float moanPulse_ = 0.f, smilePulse_ = 0.f, climaxGlow_ = 0.f, flash_ = 0.f, sweat_ = 0.f, slowmo_ = 1.f;
    float herClimaxT_ = 99.f, hisClimaxT_ = 99.f, afterT_ = 0.f;
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
    void heroLogo(float cx, float y, float size, float a);
    int custTab_ = 0, setTab_ = 0;
    bool confirmQuit_ = false, confirmNew_ = false;
    bool debug_ = false;

    // ---- audio ----
    void initAudio();
    void shutdownAudio();
    void updateAudio(float dt);
    void voice(const std::string& name, float vol = 1.f, float pitch = 1.f);
    void sfx(const std::string& prefix, float vol, float pitchJitter = 0.06f);
    float ambLevel_ = 0.f, musicLevel_ = 0.f;

    // ---- core objects ----
    Scene scene;
    Renderer renderer;
    CameraRig camera;
    Audio audio;
    ui::Ui ui;
    CameraState cam_;
    Json clipsMeta_;
    float time_ = 0.f, frameDt_ = 1.f / 60.f;
    int fbW_ = 1280, fbH_ = 720;
    std::string settingsPath_, profilePath_;
    float saveTimer_ = 0.f;
    bool sceneReady_ = false;
};

}  // namespace ag
