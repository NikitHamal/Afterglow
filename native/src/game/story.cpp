#include "game/story.h"
#include "core/paths.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <ctime>

namespace ag {

namespace {
std::vector<std::pair<std::string, std::string>> readSet(const Json& j) {
    std::vector<std::pair<std::string, std::string>> out;
    for (auto& kv : j.obj) out.emplace_back(kv.first, kv.second.isNumber() ? ("=" + std::to_string(kv.second.num())) : kv.second.str());
    return out;
}
// UTF-8 aware character count for the typewriter
int u8len(const std::string& s) {
    int n = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n;
    return n;
}
}  // namespace

bool Story::load(const std::string& path, std::string& err) {
    std::string text = paths::readText(path);
    if (text.empty()) { err = "missing " + path; return false; }
    Json j = Json::parse(text, &err);
    if (!j.isObject()) { if (err.empty()) err = "not a story object"; return false; }
    id = j["id"].str(paths::dirname(path));
    title = j["title"].str(id);
    tagline = j["tagline"].str();
    setting = j["setting"].str();
    mood = j["mood"].str("evening");
    for (auto& kv : j["vars"].obj) varsInit[kv.first] = kv.second.f(0.f);
    for (size_t i = 0; i < j["chapters"].size(); ++i) {
        const Json& c = j["chapters"][i];
        chapters.push_back({c["id"].str(), c["title"].str(), c["start"].str()});
    }
    for (auto& kv : j["gallery"].obj) gallery.emplace_back(kv.first, kv.second.str());
    std::string currentChapter;
    for (auto& kv : j["nodes"].obj) {
        const Json& n = kv.second;
        StoryNode nd;
        nd.id = kv.first;
        nd.chapter = n["chapter"].str();
        nd.location = n["location"].str();
        nd.mood = n["mood"].str();
        nd.cam = n["camera"].str(n["cam"].str());
        for (auto& o : n["outfit"].obj) nd.outfit[o.first] = o.second.str();
        if (n["show"].isArray()) {
            nd.showSet = true;
            for (size_t i = 0; i < n["show"].size(); ++i) nd.show.push_back(n["show"][i].str());
        }
        nd.act = n["act"].str();
        nd.actT = n["actT"].f(0.4f);
        for (size_t i = 0; i < n["lines"].size(); ++i) {
            const Json& l = n["lines"][i];
            StoryLine ln;
            if (l.isString()) { ln.text = l.s; }
            else if (l.isArray() && l.size() >= 2) { ln.who = l[0].str(); ln.text = l[1].str(); }
            else {
                ln.who = l["who"].str(); ln.text = l["text"].str();
                ln.voice = l["voice"].str(); ln.sfx = l["sfx"].str(); ln.cam = l["camera"].str(l["cam"].str());
                ln.wait = l["wait"].f(0.f);
                for (auto& e : l["expr"].obj) ln.expr[e.first] = e.second.f(0.f);
            }
            nd.lines.push_back(ln);
        }
        for (size_t i = 0; i < n["choices"].size(); ++i) {
            const Json& c = n["choices"][i];
            StoryChoice ch;
            ch.text = c["text"].str(); ch.cond = c["if"].str(); ch.next = c["next"].str();
            ch.set = readSet(c["set"]);
            nd.choices.push_back(ch);
        }
        nd.next = n["next"].str();
        nd.set = readSet(n["set"]);
        if (n["play"].isObject()) { nd.playGoal = n["play"]["goal"].str("together"); nd.playNext = n["play"]["next"].str(); nd.act = n["play"]["act"].str(nd.act); }
        if (n["end"].isObject()) {
            nd.ending = true;
            nd.endTitle = n["end"]["title"].str("The end");
            nd.endText = n["end"]["text"].str();
            endings.emplace_back(nd.id, nd.endTitle);
        }
        nd.gallery = n["gallery"].str();
        nodes[nd.id] = nd;
    }
    if (chapters.empty() || nodes.empty()) { err = "no chapters or nodes"; return false; }
    for (auto& c : chapters) if (!node(c.start)) { err = "chapter " + c.id + " starts at a missing node " + c.start; return false; }
    for (auto& kv : nodes) {
        const StoryNode& n = kv.second;
        auto check = [&](const std::string& to) { if (!to.empty() && !node(to)) err += "node " + n.id + " -> missing " + to + "; "; };
        check(n.next); check(n.playNext);
        for (auto& c : n.choices) check(c.next);
    }
    return err.empty();
}

int Story::chapterIndex(const std::string& cid) const {
    for (size_t i = 0; i < chapters.size(); ++i) if (chapters[i].id == cid) return (int)i;
    return -1;
}

// ------------------------------------------------------------------ progress + saves
bool StoryProgress::add(std::vector<std::string>& v, const std::string& s) {
    if (s.empty() || has(v, s)) return false;
    v.push_back(s);
    return true;
}
bool StoryProgress::has(const std::vector<std::string>& v, const std::string& s) { return std::find(v.begin(), v.end(), s) != v.end(); }

bool StoryProgress::load(const std::string& path) {
    Json j = Json::parse(paths::readText(path));
    if (!j.isObject()) return false;
    auto rd = [&](const char* key, std::map<std::string, std::vector<std::string>>& m) {
        for (auto& kv : j[key].obj) for (size_t i = 0; i < kv.second.size(); ++i) m[kv.first].push_back(kv.second[i].str());
    };
    rd("chapters", chapters); rd("endings", endings); rd("gallery", gallery); rd("seen", seen);
    return true;
}

bool StoryProgress::save(const std::string& path) const {
    Json j = Json::object();
    auto wr = [&](const char* key, const std::map<std::string, std::vector<std::string>>& m) {
        Json o = Json::object();
        for (auto& kv : m) { Json a = Json::array(); for (auto& s : kv.second) a.push(Json::of(s)); o.set(kv.first, a); }
        j.set(key, o);
    };
    wr("chapters", chapters); wr("endings", endings); wr("gallery", gallery); wr("seen", seen);
    return paths::writeAtomic(path, j.dump());
}

std::vector<StorySave> loadStorySaves(const std::string& path, int slots) {
    std::vector<StorySave> out((size_t)slots);
    Json j = Json::parse(paths::readText(path));
    for (int i = 0; i < slots && i < (int)j["slots"].size(); ++i) {
        const Json& s = j["slots"][(size_t)i];
        if (!s["used"].boolean(false)) continue;
        StorySave& sv = out[(size_t)i];
        sv.used = true; sv.story = s["story"].str(); sv.node = s["node"].str(); sv.line = (int)s["line"].num(0);
        sv.chapterTitle = s["chapter"].str(); sv.when = s["when"].str();
        for (auto& kv : s["vars"].obj) sv.vars[kv.first] = kv.second.f(0.f);
    }
    return out;
}

bool saveStorySaves(const std::string& path, const std::vector<StorySave>& saves) {
    Json j = Json::object(), arr = Json::array();
    for (const StorySave& s : saves) {
        Json o = Json::object();
        o.set("used", Json::ofBool(s.used));
        if (s.used) {
            o.set("story", Json::of(s.story)); o.set("node", Json::of(s.node)); o.set("line", Json::of(s.line));
            o.set("chapter", Json::of(s.chapterTitle)); o.set("when", Json::of(s.when));
            Json v = Json::object();
            for (auto& kv : s.vars) v.set(kv.first, Json::of(kv.second));
            o.set("vars", v);
        }
        arr.push(o);
    }
    j.set("slots", arr);
    return paths::writeAtomic(path, j.dump());
}

// ------------------------------------------------------------------ player
void StoryPlayer::start(const Story* s, const std::string& nodeId, const std::map<std::string, float>* v, int lineIx) {
    story_ = s;
    vars = v ? *v : s->varsInit;
    history.clear();
    autoMode = skipMode = false;
    enter(nodeId);
    line_ = std::max(0, std::min(lineIx, node_ ? (int)node_->lines.size() : 0));
    reveal_ = 0.f;
    // resuming mid-scene (a save, a gallery replay): the lines already read belong to the log and the choice prompt
    for (int i = 0; i < line_; ++i) history.push_back({node_->lines[(size_t)i].who, node_->lines[(size_t)i].text});
}

void StoryPlayer::enter(const std::string& id) {
    node_ = story_ ? story_->node(id) : nullptr;
    line_ = 0;
    reveal_ = 0.f;
    autoT_ = 0.f;
    nodeChanged = lineChanged = true;
    if (node_) applySet(node_->set);
}

void StoryPlayer::jump(const std::string& id) { if (story_ && story_->node(id)) enter(id); }

void StoryPlayer::applySet(const std::vector<std::pair<std::string, std::string>>& set) {
    for (auto& kv : set) {
        const std::string& e = kv.second;
        float& v = vars[kv.first];
        if (e.empty()) continue;
        float x = (float)std::atof(e.c_str() + ((e[0] == '+' || e[0] == '-' || e[0] == '=') ? 1 : 0));
        if (e[0] == '+') v += x; else if (e[0] == '-') v -= x; else v = x;
    }
}

bool StoryPlayer::evalCond(const std::string& cond) const {
    // "var >= 2", "trust > desire", "honest == 1", joined by "&&" / "||" (|| binds looser); empty = true
    if (cond.empty()) return true;
    size_t bar = cond.find("||");
    if (bar != std::string::npos) return evalCond(cond.substr(0, bar)) || evalCond(cond.substr(bar + 2));
    size_t amp = cond.find("&&");
    if (amp != std::string::npos) return evalCond(cond.substr(0, amp)) && evalCond(cond.substr(amp + 2));
    const char* ops[] = {">=", "<=", "==", "!=", ">", "<"};
    for (const char* op : ops) {
        size_t at = cond.find(op);
        if (at == std::string::npos) continue;
        auto trim = [](std::string s) { size_t a = s.find_first_not_of(" "), b = s.find_last_not_of(" "); return a == std::string::npos ? std::string() : s.substr(a, b - a + 1); };
        std::string l = trim(cond.substr(0, at)), r = trim(cond.substr(at + std::string(op).size()));
        auto val = [&](const std::string& t) -> float {
            if (!t.empty() && (std::isdigit((unsigned char)t[0]) || t[0] == '-' || t[0] == '.')) return (float)std::atof(t.c_str());
            auto it = vars.find(t);
            return it == vars.end() ? 0.f : it->second;
        };
        float a = val(l), b = val(r);
        std::string o = op;
        if (o == ">=") return a >= b;
        if (o == "<=") return a <= b;
        if (o == "==") return std::fabs(a - b) < 1e-4f;
        if (o == "!=") return std::fabs(a - b) >= 1e-4f;
        if (o == ">") return a > b;
        return a < b;
    }
    auto it = vars.find(cond);
    return it != vars.end() && it->second != 0.f;
}

std::vector<int> StoryPlayer::visibleChoices() const {
    std::vector<int> out;
    if (!node_) return out;
    for (size_t i = 0; i < node_->choices.size(); ++i) if (evalCond(node_->choices[i].cond)) out.push_back((int)i);
    return out;
}

void StoryPlayer::choose(int i) {
    if (!node_ || i < 0 || i >= (int)node_->choices.size()) return;
    const StoryChoice& c = node_->choices[(size_t)i];
    history.push_back({"you", c.text});
    applySet(c.set);
    enter(c.next);
}

bool StoryPlayer::lineDone() const {
    const StoryLine* l = line();
    return !l || reveal_ >= (float)u8len(l->text);
}

bool StoryPlayer::advance() {
    if (!node_) return false;
    const StoryLine* l = line();
    if (l && !lineDone()) { reveal_ = (float)u8len(l->text) + 1.f; return false; }   // first press: show it all
    if (l) {
        history.push_back({l->who, l->text});
        if (history.size() > 400) history.erase(history.begin());
        ++line_;
        reveal_ = 0.f;
        autoT_ = 0.f;
        lineChanged = true;
    }
    if (line_ >= (int)node_->lines.size() && node_->choices.empty() && node_->playGoal.empty() && !node_->ending && !node_->next.empty()) {
        enter(node_->next);
        return true;
    }
    return false;
}

void StoryPlayer::update(float dt) {
    const StoryLine* l = line();
    if (!l) return;
    reveal_ += dt * (skipMode ? 2000.f : textSpeed);
    if (lineDone()) {
        autoT_ += dt;
        float hold = l->wait > 0.f ? l->wait : (autoMode ? 1.2f + 0.035f * (float)u8len(l->text) : -1.f);
        if (skipMode) hold = 0.06f;
        if (hold >= 0.f && autoT_ >= hold) advance();
    }
}

}  // namespace ag
