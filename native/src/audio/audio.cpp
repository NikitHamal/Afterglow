#include "audio/audio.h"
#include "core/paths.h"
#include "core/log.h"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace ag {

static std::string keyFor(const std::string& root, const std::string& file) {
    std::string rel = file.substr(root.size());
    while (!rel.empty() && (rel[0] == '/' || rel[0] == '\\')) rel.erase(0, 1);
    size_t dot = rel.find_last_of('.');
    if (dot != std::string::npos) rel = rel.substr(0, dot);
    for (auto& c : rel) { c = (char)std::tolower((unsigned char)c); if (c == '\\') c = '/'; }
    return rel;
}

bool Audio::init() {
    InitAudioDevice();
    ready_ = IsAudioDeviceReady();
    if (!ready_) AG_WARN("AG: no audio device; sound disabled");
    return ready_;
}

void Audio::shutdown() {
    for (auto& kv : voices_) for (Sound& s : kv.second.aliases) UnloadSoundAlias(s);
    for (auto& kv : sounds_) UnloadSound(kv.second);
    for (auto& kv : loops_) UnloadMusicStream(kv.second);
    voices_.clear(); sounds_.clear(); loops_.clear();
    if (ready_) CloseAudioDevice();
    ready_ = false;
}

int Audio::loadAll() {
    if (!ready_) return 0;
    std::vector<std::pair<std::string, std::string>> roots;   // (root dir, name prefix)
    std::string native = paths::join(paths::assetRoot(), "audio");
    if (paths::isDir(native)) roots.push_back({native, ""});
    else if (!paths::repoRoot().empty()) {
        roots.push_back({paths::join(paths::repoRoot(), "assets/Shinlalala's Lewd Sound Pack"), ""});
        roots.push_back({paths::join(paths::repoRoot(), "assets/vox"), "vox/"});
    }
    int n = 0;
    for (auto& r : roots) {
        for (const std::string& f : paths::listFilesRecursive(r.first, {".ogg", ".wav", ".mp3"})) {
            std::string key = r.second + keyFor(r.first, f);
            files_[key] = f;
            if (key.rfind("loops/", 0) == 0 || key.rfind("ambience/", 0) == 0) continue;   // streamed on demand
            Sound s = LoadSound(f.c_str());
            if (s.frameCount == 0) continue;
            sounds_[key] = s;
            Voice v;
            for (int i = 0; i < 3; ++i) v.aliases.push_back(LoadSoundAlias(s));
            voices_[key] = std::move(v);
            ++n;
        }
    }
    AG_INFO("AG: audio: %d samples", n);
    return n;
}

std::vector<std::string> Audio::names(const std::string& prefix) const {
    std::vector<std::string> out;
    for (auto& kv : sounds_) if (kv.first.rfind(prefix, 0) == 0) out.push_back(kv.first);
    std::sort(out.begin(), out.end());
    return out;
}

bool Audio::play(const std::string& name, float volume, float pitch, float pan) {
    if (!ready_) return false;
    auto it = voices_.find(name);
    if (it == voices_.end() || it->second.aliases.empty()) return false;
    Voice& v = it->second;
    Sound& s = v.aliases[v.next];
    v.next = (v.next + 1) % v.aliases.size();
    SetSoundVolume(s, std::max(0.f, volume));
    SetSoundPitch(s, std::max(0.05f, pitch));
    SetSoundPan(s, std::min(1.f, std::max(0.f, pan)));
    PlaySound(s);
    return true;
}

bool Audio::playRandom(const std::string& prefix, float volume, float jitter, float pan) {
    std::vector<std::string> cands = names(prefix);
    if (cands.empty()) return false;
    std::string& last = lastRandom_[prefix];
    std::string pick = cands[(size_t)GetRandomValue(0, (int)cands.size() - 1)];
    if (cands.size() > 1 && pick == last) pick = cands[(size_t)GetRandomValue(0, (int)cands.size() - 1)];
    last = pick;
    float pitch = 1.f + jitter * ((float)GetRandomValue(-1000, 1000) / 1000.f);
    float vol = volume * (1.f + 0.6f * jitter * ((float)GetRandomValue(-1000, 1000) / 1000.f));
    return play(pick, vol, pitch, pan);
}

bool Audio::addProcedural(const std::string& name, int frames, const std::function<void(float*, int)>& gen) {
    if (!ready_ || frames <= 0) return false;
    std::vector<float> data((size_t)frames, 0.f);
    gen(data.data(), frames);
    Wave w{};
    w.frameCount = (unsigned)frames; w.sampleRate = 44100; w.sampleSize = 32; w.channels = 1;
    w.data = data.data();
    Sound s = LoadSoundFromWave(w);   // copies the data
    if (s.frameCount == 0) return false;
    auto old = sounds_.find(name);
    if (old != sounds_.end()) {
        for (Sound& a : voices_[name].aliases) UnloadSoundAlias(a);
        UnloadSound(old->second);
    }
    sounds_[name] = s;
    Voice v;
    for (int i = 0; i < 3; ++i) v.aliases.push_back(LoadSoundAlias(s));
    voices_[name] = std::move(v);
    return true;
}

bool Audio::loop(const std::string& name, float volume) {
    if (!ready_) return false;
    auto it = loops_.find(name);
    if (it == loops_.end()) {
        auto f = files_.find(name);
        if (f == files_.end()) return false;
        Music m = LoadMusicStream(f->second.c_str());
        if (m.frameCount == 0) return false;
        m.looping = true;
        it = loops_.emplace(name, m).first;
    }
    if (volume <= 0.f) { StopMusicStream(it->second); return true; }
    SetMusicVolume(it->second, volume);
    if (!IsMusicStreamPlaying(it->second)) PlayMusicStream(it->second);
    return true;
}

void Audio::setLoopVolume(const std::string& name, float volume) {
    auto it = loops_.find(name);
    if (it != loops_.end()) SetMusicVolume(it->second, std::max(0.f, volume));
}

void Audio::update() {
    if (!ready_) return;
    for (auto& kv : loops_) if (IsMusicStreamPlaying(kv.second)) UpdateMusicStream(kv.second);
}

void Audio::setMasterVolume(float v) {
    master_ = std::min(1.f, std::max(0.f, v));
    if (ready_) SetMasterVolume(master_);
}

}  // namespace ag
