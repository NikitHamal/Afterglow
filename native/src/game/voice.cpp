#include "game/voice.h"
#include "core/json.h"
#include "core/log.h"
#include "core/paths.h"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace ag {

namespace {
float clampUnit(float x) { return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }
float smoothUnit(float x) { x = clampUnit(x); return x * x * (3.f - 2.f * x); }
int b64val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62;
    if (c == '/' || c == '_') return 63;
    return -1;
}
std::vector<unsigned char> b64decode(const std::string& s) {
    std::vector<unsigned char> out;
    out.reserve(s.size() * 3 / 4);
    int buf = 0, bits = 0;
    for (char c : s) {
        int v = b64val(c);
        if (v < 0) continue;
        buf = (buf << 6) | v; bits += 6;
        if (bits >= 8) { bits -= 8; out.push_back((unsigned char)((buf >> bits) & 0xff)); }
    }
    return out;
}
// the line's words as the synthesiser counts them (tokens with a letter or digit), lower-case ASCII letters only
std::vector<std::string> wordsOf(const std::string& text) {
    std::vector<std::string> out;
    std::string cur;
    bool any = false;
    auto flush = [&]() { if (any) out.push_back(cur); cur.clear(); any = false; };
    for (unsigned char c : text) {
        if (std::isspace(c) || c == '-') { flush(); continue; }
        if (std::isalnum(c)) { cur += (char)std::tolower(c); any = true; }   // accents etc. ride along silently
    }
    flush();
    return out;
}
Viseme letterViseme(char c) {
    Viseme v;
    switch (c) {
        case 'a': v.open = 0.85f; break;
        case 'e': v.open = 0.45f; v.wide = 0.55f; break;
        case 'i': case 'y': v.open = 0.3f; v.wide = 0.75f; break;
        case 'o': v.open = 0.6f; v.round = 0.6f; break;
        case 'u': case 'w': case 'q': v.open = 0.25f; v.round = 0.9f; break;
        case 'm': case 'b': case 'p': v.closed = 1.f; break;
        case 'f': case 'v': v.bite = 0.85f; v.open = 0.1f; break;
        case 's': case 'z': case 'c': case 'x': v.open = 0.15f; v.wide = 0.35f; break;
        case 'h': v.open = 0.4f; break;
        case 'r': case 'j': v.open = 0.25f; v.round = 0.3f; break;
        case 'l': case 'n': case 't': case 'd': case 'k': case 'g': v.open = 0.3f; break;
        default: v.open = 0.3f; break;   // digits and the rest
    }
    return v;
}
Viseme mixV(const Viseme& a, const Viseme& b, float t) {
    Viseme r;
    r.open = a.open + (b.open - a.open) * t; r.wide = a.wide + (b.wide - a.wide) * t; r.round = a.round + (b.round - a.round) * t;
    r.bite = a.bite + (b.bite - a.bite) * t; r.closed = a.closed + (b.closed - a.closed) * t;
    return r;
}
}  // namespace

bool VoiceBank::load(const std::string& dir) {
    dir_ = dir;
    lines_.clear(); files_.clear();
    std::string text = paths::readText(paths::join(dir, "manifest.json"));
    if (text.empty()) return false;
    std::string err;
    Json j = Json::parse(text, &err);
    if (!j.isObject()) { AG_WARN("AG: voice manifest: %s", err.c_str()); return false; }
    envFps_ = j["envFps"].f(30.f);
    for (auto& st : j["lines"].obj)
        for (auto& nd : st.second.obj) {
            std::vector<std::string>& row = lines_[st.first][nd.first];
            for (size_t i = 0; i < nd.second.size(); ++i) row.push_back(nd.second[i].str());
        }
    for (auto& f : j["files"].obj) {
        Take t;
        t.who = f.second["who"].str();
        t.delivery = f.second["delivery"].str();
        t.dur = f.second["dur"].f(0.f);
        t.env = b64decode(f.second["env"].str());
        const Json& w = f.second["words"];
        for (size_t i = 0; i < w.size(); ++i) if (w[i].size() >= 2) t.words.push_back({w[i][0].f(), w[i][1].f()});
        files_[f.first] = std::move(t);
    }
    AG_INFO("AG: voices: %d takes for %d stories", (int)files_.size(), (int)lines_.size());
    return !files_.empty();
}

const VoiceBank::Take* VoiceBank::take(const std::string& story, const std::string& node, int line, std::string* key) const {
    auto s = lines_.find(story);
    if (s == lines_.end()) return nullptr;
    auto n = s->second.find(node);
    if (n == s->second.end() || line < 0 || line >= (int)n->second.size()) return nullptr;
    const std::string& k = n->second[(size_t)line];
    if (k.empty()) return nullptr;
    auto f = files_.find(k);
    if (f == files_.end()) return nullptr;
    if (key) *key = k;
    return &f->second;
}

float VoiceBank::envelope(const Take& t, float time) const {
    if (t.env.empty() || time < 0.f) return 0.f;
    float f = time * envFps_;
    int i = (int)f;
    if (i >= (int)t.env.size() - 1) return i < (int)t.env.size() ? t.env[(size_t)i] / 255.f : 0.f;
    float u = f - (float)i;
    return (t.env[(size_t)i] * (1.f - u) + t.env[(size_t)i + 1] * u) / 255.f;
}

int VoiceBank::wordAt(const Take& t, float time, float pad) const {
    for (size_t i = 0; i < t.words.size(); ++i)
        if (time >= t.words[i].first - pad && time < t.words[i].second + pad) return (int)i;
    return -1;
}

int VoiceBank::wordsSpoken(const Take& t, float time) const {
    if (time >= t.dur) return (int)t.words.size();
    int n = 0;
    for (auto& w : t.words) if (w.first <= time + 0.02f) ++n;
    return n;
}

Viseme VoiceBank::viseme(const Take& t, const std::string& text, float time) const {
    Viseme none;
    int wi = wordAt(t, time, 0.f);
    if (wi < 0) return none;
    std::vector<std::string> ws = wordsOf(text);
    if (ws.empty()) { Viseme v; v.open = 0.5f; return v; }
    // the synthesiser and the text rarely disagree on the word count; when they do, map proportionally
    size_t ti = ws.size() == t.words.size() ? (size_t)wi : std::min(ws.size() - 1, (size_t)((float)wi * (float)ws.size() / (float)std::max<size_t>(1, t.words.size())));
    const std::string& w = ws[ti];
    if (w.empty()) { Viseme v; v.open = 0.4f; return v; }
    float s = t.words[(size_t)wi].first, e = t.words[(size_t)wi].second;
    float u = clampUnit((time - s) / std::max(0.04f, e - s));
    float lf = u * (float)w.size();
    size_t li = std::min(w.size() - 1, (size_t)lf);
    size_t ln = std::min(w.size() - 1, li + 1);
    return mixV(letterViseme(w[li]), letterViseme(w[ln]), smoothUnit(lf - (float)li));
}

// ------------------------------------------------------------------ player
bool VoicePlayer::play(const std::string& file, float volume) {
    stop();
    if (!IsAudioDeviceReady() || !paths::exists(file)) return false;
    music_ = LoadMusicStream(file.c_str());
    if (music_.frameCount == 0) { UnloadMusicStream(music_); music_ = Music{}; return false; }
    loaded_ = true;
    music_.looping = false;
    len_ = GetMusicTimeLength(music_);
    SetMusicVolume(music_, std::max(0.f, volume));
    PlayMusicStream(music_);
    active_ = true;
    t_ = 0.f;
    return true;
}

void VoicePlayer::stop() {
    if (loaded_) { StopMusicStream(music_); UnloadMusicStream(music_); }
    music_ = Music{};
    loaded_ = active_ = paused_ = false;
}

void VoicePlayer::setVolume(float v) { if (loaded_) SetMusicVolume(music_, std::max(0.f, v)); }

void VoicePlayer::update(float dt, bool paused) {
    if (!loaded_) return;
    if (paused) { if (!paused_) { PauseMusicStream(music_); paused_ = true; } return; }
    if (paused_) { ResumeMusicStream(music_); paused_ = false; }
    UpdateMusicStream(music_);
    t_ += dt;
    if (t_ > len_ + 0.15f) { stop(); t_ = len_ + 0.15f; }
}

}  // namespace ag
