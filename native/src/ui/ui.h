// Afterglow UI kit — custom-drawn immediate-mode interface: embedded fonts at exact pixel sizes, frosted
// glass panels (blurred scene), SDF rounded shapes with soft shadows, animated widgets with hover/press
// feedback, and keyboard focus navigation (arrows / Enter / Esc) alongside the mouse.
//
// Nothing may overflow, at any window size: layouts are written in 1920x1080 design units and u() maps them
// linearly onto the window (a 16:9 window is exactly the design; other shapes only add room). Panels that would
// not fit a small window shrink as a whole inside a scale scope (fitScale / fitGet); text that would not fit its
// box shrinks a little, then ends in an ellipsis (textIn / fitPx / ellipsize); widgets measure their labels.
// In harness runs a guard checks every text and widget against the screen and its container (UI-OVERFLOW) and
// texts against each other (UI-OVERLAP); see ui_fit.cpp.
#pragma once
#include "raylib.h"
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ag::ui {

enum class Face { Serif = 0, SerifBold, SerifItalic, Sans, SansMedium, SansBold, Count };
enum Align { Left = 0, Center = 1, Right = 2 };

struct Theme {
    Color ink{246, 238, 231, 255};
    Color inkDim{208, 193, 185, 255};
    Color inkFaint{156, 141, 137, 255};
    Color accent{240, 178, 132, 255};     // candle amber
    Color rose{233, 126, 150, 255};       // her
    Color gold{246, 198, 122, 255};       // him
    Color glass{22, 15, 19, 255};         // frosted panel tint
    Color hairline{255, 232, 214, 40};
};

// one SDF shape: fill (+ gradient), optional frosted-glass base, border, soft shadow/glow
struct Box {
    float radius = 0.f;
    Color fill{0, 0, 0, 0}, fill2{0, 0, 0, 0};
    int grad = 0;                          // 0 none, 1 horizontal, 2 vertical
    bool glass = false;
    Color glassTint{22, 15, 19, 255};
    float glassAmt = 0.55f;
    float border = 0.f;
    Color borderCol{0, 0, 0, 0};
    float shadow = 0.f;
    Color shadowCol{0, 0, 0, 0};
};

struct Input {
    Vector2 mouse{0, 0};
    bool mouseDown = false, clicked = false, released = false, mouseMoved = false, rightClicked = false;
    float wheel = 0.f;
    bool up = false, down = false, left = false, right = false, accept = false, back = false;
    bool tabNext = false, tabPrev = false;
};

class Ui {
public:
    bool init();
    void shutdown();
    void begin(float dt, bool readInput = true);   // screen metrics + input + focus navigation
    void end();

    // ---- metrics: layouts are written in 1920x1080 design units, u() converts to screen units ----
    float W = 1280.f, H = 720.f, s = 1.f, dpi = 1.f;
    float u(float v) const { return v * s * k_; }
    float scale() const { return s * k_; }   // screen units per design unit right now (inside scale scopes too)

    // ---- fitting (ui_fit.cpp) ----
    // scale scope: u() inside is multiplied by k (k <= 1 shrinks a panel and everything in it together)
    void pushScale(float k);
    void popScale();
    // the k that fits a natW x natH design-unit panel on screen with `margin` design units around it
    float fitScale(float natW, float natH, float margin = 40.f) const;
    // measured fit for panels whose content height varies: the k to use this frame (push it), then report the
    // content's top / bottom and the bottom it may use (screen units, measured inside the scope)
    float fitGet(const std::string& id);
    void fitReport(const std::string& id, float top, float contentBottom, float availBottom);
    // text that fits a width: the px to draw at (down to minFrac of px), an ellipsised string
    float fitPx(Face f, float px, const std::string& t, float maxW, float minFrac = 0.78f, float spacing = 0.f);
    std::string ellipsize(Face f, float px, const std::string& t, float maxW, float spacing = 0.f);
    // `t` inside `box`: shrinks (down to minFrac), then ellipsises; vertically centred; returns the px used
    float textIn(Face f, float px, const std::string& t, Rectangle box, Color c, int align = Left, float spacing = 0.f,
                 float minFrac = 0.78f);

    // ---- QA guard (harness): containers + the frame's findings ----
    bool guard = false;
    void pushClip(Rectangle r, const std::string& id);   // a container: what is drawn inside must stay inside
    void popClip();
    void guardRect(const char* kind, const std::string& id, Rectangle r);   // a widget's box (text is checked itself)
    int guardIssues() const { return (int)guardLog_.size(); }
    const std::vector<std::string>& guardLog() const { return guardLog_; }
    Theme th;
    float alpha = 1.f;       // global fade (screen transitions)
    float time = 0.f;

    // ---- text ----
    Font& font(Face f, float px);
    Vector2 measure(Face f, float px, const std::string& t, float spacing = 0.f);
    void text(Face f, float px, const std::string& t, float x, float y, Color c, int align = Left, float spacing = 0.f);
    // word-wrapped paragraph, returns the height used; measureParagraph = the same without drawing (lines in *lines)
    float paragraph(Face f, float px, const std::string& t, float x, float y, float width, Color c, float lineGap = 1.35f, int align = Left);
    float measureParagraph(Face f, float px, const std::string& t, float width, float lineGap = 1.35f, int* lines = nullptr);

    // ---- shapes ----
    void box(Rectangle r, const Box& b);
    void panel(Rectangle r, float radius = 22.f, float tint = 0.58f);           // standard frosted panel
    void backdrop(float amount);                                               // full-screen blur + darken
    void fillRect(Rectangle r, Color c, float radius = 0.f);
    void glow(float x, float y, float radius, Color c);
    void line(float x0, float y0, float x1, float y1, float w, Color c);
    void heart(float x, float y, float size, Color c);
    void diamond(float x, float y, float size, Color c);
    Color fade(Color c, float a = 1.f) const;       // applies the global alpha

    // blurred copy of the 3D frame (from the renderer) for glass; 0 = flat tint
    unsigned int blurTex = 0;
    int blurW = 0, blurH = 0, fbW = 1280, fbH = 720;

    // ---- widgets (return true when activated / changed) ----
    Input in;
    bool keyboardNav = false;
    bool menuItem(const std::string& id, const std::string& label, float x, float y, float w, bool enabled = true);
    bool button(const std::string& id, const std::string& label, Rectangle r, bool primary = false, bool enabled = true);
    bool slider(const std::string& id, const std::string& label, float* v, float lo, float hi, Rectangle r,
                bool bipolar = false, const std::string& valueText = "");
    bool toggle(const std::string& id, const std::string& label, bool* v, Rectangle r);
    bool choice(const std::string& id, const std::string& label, int* idx, const std::vector<std::string>& opts, Rectangle r);
    bool swatches(const std::string& id, const std::string& label, int* idx, const std::vector<Color>& colors,
                  const std::vector<std::string>& names, Rectangle r);
    bool tabs(const std::string& id, int* idx, const std::vector<std::string>& labels, Rectangle r);
    bool chip(const std::string& id, const std::string& label, Rectangle r, bool active);
    // "[Space] Rhythm" key hint; returns its width (keyHintWidth: the same, measured only)
    float keyHint(float x, float y, const std::string& key, const std::string& label, float px = 17.f);
    float keyHintWidth(const std::string& key, const std::string& label, float px = 17.f);
    void sectionLabel(const std::string& t, float x, float y, float w);

    // focus / animation helpers
    void resetFocus(int index = 0) { focus_ = index; keyboardNav = false; active_.clear(); }
    int focusIndex() const { return focus_; }
    bool anyActive() const { return !active_.empty(); }
    float anim(const std::string& id, float target, float speed);    // eased per-key value
    std::function<void(int)> onSound;   // 0 hover, 1 press, 2 back, 3 slider tick

private:
    struct WState { float hover = 0.f, press = 0.f, value = 0.f, focusT = 0.f; bool wasHot = false; bool init = false; };
    WState& st(const std::string& id) { return states_[id]; }
    int takeFocus() { return focusCount_++; }
    bool focused(int idx) const { return focus_ == idx; }
    bool hit(Rectangle r) const { return CheckCollisionPointRec(in.mouse, r); }
    // common interaction: hover/focus/press bookkeeping, returns true when activated
    bool interact(const std::string& id, Rectangle r, int fidx, bool enabled, WState& w);
    void sound(int k) { if (onSound) onSound(k); }

    struct GuardText { std::string t; Rectangle r; };
    void guardText(const std::string& t, Rectangle r);
    void guardFrame();   // end(): overlaps between texts, the frame's log
    std::vector<float> scaleStack_;
    float k_ = 1.f;
    std::unordered_map<std::string, float> fits_;
    std::vector<std::pair<Rectangle, std::string>> clips_;
    std::vector<GuardText> guardTexts_;
    std::vector<std::string> guardPending_, guardLog_;

    std::unordered_map<std::string, WState> states_;
    std::unordered_map<uint64_t, Font> fonts_;
    Shader shader_{};
    int locRect_ = -1, locRadius_ = -1, locScreenH_ = -1, locFill_ = -1, locFill2_ = -1, locGrad_ = -1, locGlass_ = -1,
        locGlassTint_ = -1, locBorder_ = -1, locBorderCol_ = -1, locShadow_ = -1, locShadowCol_ = -1, locTime_ = -1;
    int focus_ = 0, focusCount_ = 0, lastFocusCount_ = 0;
    std::string active_;
    float dt_ = 1.f / 60.f;
    bool inited_ = false;
};

// easing
inline float easeOut(float t) { t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t); return 1.f - (1.f - t) * (1.f - t) * (1.f - t); }
inline float easeInOut(float t) { t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t); return t * t * (3.f - 2.f * t); }
inline Color mixc(Color a, Color b, float t) {
    t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
    return Color{(unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
                 (unsigned char)(a.b + (b.b - a.b) * t), (unsigned char)(a.a + (b.a - a.a) * t)};
}
inline Color withA(Color c, float a) { c.a = (unsigned char)(a < 0.f ? 0 : (a > 1.f ? 255 : a * 255.f)); return c; }

}  // namespace ag::ui
