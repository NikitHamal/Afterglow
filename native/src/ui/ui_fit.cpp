// Afterglow UI kit — fitting (scale scopes, measured panel fits, text that fits its box) and the QA guard
// that proves it: every text and widget is checked against the screen and its container, texts against each other.
#include "ui/ui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace ag::ui {

namespace {
bool inside(Rectangle in, Rectangle out, float tol) {
    return in.x >= out.x - tol && in.y >= out.y - tol && in.x + in.width <= out.x + out.width + tol &&
           in.y + in.height <= out.y + out.height + tol;
}
std::string clipName(const std::string& s) { return s.size() > 48 ? s.substr(0, 45) + "..." : s; }
}  // namespace

// ---------------------------------------------------------------- scale scopes
void Ui::pushScale(float k) {
    scaleStack_.push_back(k_);
    k_ *= std::max(0.3f, std::min(1.f, k));
}

void Ui::popScale() {
    if (scaleStack_.empty()) return;
    k_ = scaleStack_.back();
    scaleStack_.pop_back();
}

float Ui::fitScale(float natW, float natH, float margin) const {
    float sc = scale();
    float aw = W - 2.f * margin * sc, ah = H - 2.f * margin * sc;
    float k = std::min(aw / std::max(1.f, natW * sc), ah / std::max(1.f, natH * sc));
    return std::max(0.3f, std::min(1.f, k));
}

float Ui::fitGet(const std::string& id) {
    auto it = fits_.find(id);
    return it == fits_.end() ? 1.f : it->second;
}

void Ui::fitReport(const std::string& id, float top, float contentBottom, float availBottom) {
    float& k = fits_.try_emplace(id, 1.f).first->second;
    float used = contentBottom - top, avail = availBottom - top;
    if (used <= 1.f || avail <= 1.f) return;
    // the content scales with k: the k that just fits; shrink at once, grow back only with clear room (settles)
    float want = std::max(0.4f, std::min(1.f, k * avail / used));
    if (want < k - 0.002f || want > k + 0.03f) k = want;
}

// ---------------------------------------------------------------- text that fits
float Ui::fitPx(Face f, float px, const std::string& t, float maxW, float minFrac, float spacing) {
    if (t.empty() || maxW <= 0.f) return px;
    float w = measure(f, px, t, spacing).x;
    if (w <= maxW) return px;
    return std::max(px * minFrac, px * maxW / w);
}

std::string Ui::ellipsize(Face f, float px, const std::string& t, float maxW, float spacing) {
    if (t.empty() || measure(f, px, t, spacing).x <= maxW) return t;
    static const std::string kEll = "\xe2\x80\xa6";
    std::vector<size_t> cuts;   // byte offsets that start a UTF-8 character
    for (size_t i = 1; i < t.size(); ++i)
        if (((unsigned char)t[i] & 0xC0) != 0x80) cuts.push_back(i);
    auto cand = [&](size_t n) {
        std::string c = t.substr(0, n);
        while (!c.empty() && (c.back() == ' ' || c.back() == ',' || c.back() == '.')) c.pop_back();
        return c + kEll;
    };
    int lo = -1, hi = (int)cuts.size() - 1;   // largest i whose cut fits
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (measure(f, px, cand(cuts[(size_t)mid]), spacing).x <= maxW) lo = mid;
        else hi = mid - 1;
    }
    return lo >= 0 ? cand(cuts[(size_t)lo]) : kEll;
}

float Ui::textIn(Face f, float px, const std::string& t, Rectangle box, Color c, int align, float spacing, float minFrac) {
    float p = fitPx(f, px, t, box.width, minFrac, spacing);
    std::string shown = ellipsize(f, p, t, box.width, spacing);
    float x = align == Center ? box.x + box.width * 0.5f : (align == Right ? box.x + box.width : box.x);
    text(f, p, shown, x, box.y + (box.height - p) * 0.5f, c, align, spacing);
    return p;
}

float Ui::measureParagraph(Face f, float px, const std::string& t, float width, float lineGap, int* lines) {
    std::istringstream words(t);
    std::string w, line;
    int n = 0;
    while (words >> w) {
        std::string cand = line.empty() ? w : line + " " + w;
        if (!line.empty() && measure(f, px, cand).x > width) { ++n; line = w; }
        else line = cand;
    }
    if (!line.empty()) ++n;
    if (lines) *lines = n;
    return (float)n * px * lineGap;
}

float Ui::keyHintWidth(const std::string& key, const std::string& label, float px) {
    float kw = std::max(measure(Face::SansBold, u(px * 0.82f), key).x + u(14.f), u(26.f));
    return kw + u(9.f) + measure(Face::SansMedium, u(px), label).x;
}

// ---------------------------------------------------------------- QA guard
void Ui::pushClip(Rectangle r, const std::string& id) { clips_.push_back({r, id}); }

void Ui::popClip() {
    if (!clips_.empty()) clips_.pop_back();
}

void Ui::guardRect(const char* kind, const std::string& id, Rectangle r) {
    if (!guard || alpha < 0.05f) return;
    float tol = std::max(1.5f, s * 2.f);
    char b[320];
    if (!inside(r, Rectangle{0.f, 0.f, W, H}, tol)) {
        std::snprintf(b, sizeof b, "UI-OVERFLOW %s \"%s\" at %.0f,%.0f size %.0fx%.0f leaves the %.0fx%.0f screen", kind,
                      clipName(id).c_str(), (double)r.x, (double)r.y, (double)r.width, (double)r.height, (double)W, (double)H);
        guardPending_.push_back(b);
    } else if (!clips_.empty() && !inside(r, clips_.back().first, tol)) {
        const Rectangle& c = clips_.back().first;
        std::snprintf(b, sizeof b, "UI-OVERFLOW %s \"%s\" at %.0f,%.0f size %.0fx%.0f leaves \"%s\" (%.0f,%.0f %.0fx%.0f)", kind,
                      clipName(id).c_str(), (double)r.x, (double)r.y, (double)r.width, (double)r.height, clips_.back().second.c_str(),
                      (double)c.x, (double)c.y, (double)c.width, (double)c.height);
        guardPending_.push_back(b);
    }
}

void Ui::guardText(const std::string& t, Rectangle r) {
    if (!guard || alpha < 0.05f) return;
    guardRect("text", t, r);
    guardTexts_.push_back({t, r});
}

void Ui::guardFrame() {
    if (!guard) return;
    // texts on top of each other (a text's own drop shadow is the same string a few pixels away: not an overlap)
    for (size_t i = 0; i < guardTexts_.size(); ++i) {
        for (size_t j = i + 1; j < guardTexts_.size(); ++j) {
            const GuardText& a = guardTexts_[i];
            const GuardText& b = guardTexts_[j];
            if (a.t == b.t && std::fabs(a.r.x - b.r.x) + std::fabs(a.r.y - b.r.y) < 4.f + 6.f * s) continue;
            float ox = std::min(a.r.x + a.r.width, b.r.x + b.r.width) - std::max(a.r.x, b.r.x);
            float oy = std::min(a.r.y + a.r.height, b.r.y + b.r.height) - std::max(a.r.y, b.r.y);
            if (ox > 2.f && oy > 0.3f * std::min(a.r.height, b.r.height)) {
                char m[320];
                std::snprintf(m, sizeof m, "UI-OVERLAP \"%s\" and \"%s\" (%.0fx%.0f px)", clipName(a.t).c_str(), clipName(b.t).c_str(),
                              (double)ox, (double)oy);
                guardPending_.push_back(m);
            }
        }
    }
    guardLog_.swap(guardPending_);
    guardPending_.clear();
    guardTexts_.clear();
    clips_.clear();
}

}  // namespace ag::ui
