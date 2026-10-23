#include "game/night.h"
#include <algorithm>
#include <cmath>

namespace ag {

namespace {
float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
float gauss(float x, float w) { return std::exp(-(x * x) / (w * w)); }
float approach(float v, float target, float rate) { return v + (target - v) * (1.f - std::exp(-rate)); }
}  // namespace

float Night::rnd() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
    return (float)(rng_ & 0xffffff) / 16777215.f;
}

void Night::begin(unsigned s) {
    *this = Night{};
    seed = s ? s : 1u;
    rng_ = seed * 2654435761u + 7u;
    const char* ids[] = {"missionary", "legsup", "doggy", "prone", "cowgirl", "revcowgirl", "spoon", "oral", "lick", "solo"};
    for (const char* id : ids) { novelty[id] = 1.f; like[id] = 0.85f + 0.3f * rnd(); }
    // tonight she has a favourite (never solo / oral: those are for variety)
    const char* fav[] = {"missionary", "legsup", "doggy", "prone", "cowgirl", "revcowgirl", "spoon", "lick"};
    favorite = fav[(int)(rnd() * 7.999f)];
    like[favorite] = 1.3f;
    nextDesire = 7.f + 4.f * rnd();
}

void Night::restore(const SavedNight& s) {
    begin(s.seed);
    her = s.her; him = s.him; connection = s.connection; score = s.score; elapsed = s.elapsed;
    herClimaxes = s.herClimaxes; desiresMet = s.desiresMet; desiresMissed = s.desiresMissed;
    for (auto& kv : s.novelty) novelty[kv.first] = kv.second;
    pos = s.pos;
}

SavedNight Night::snapshot() const {
    SavedNight s;
    s.valid = !finished;
    s.pos = pos; s.her = std::min(her, 0.95f); s.him = std::min(him, 0.9f); s.connection = connection; s.score = score;
    s.elapsed = elapsed; s.herClimaxes = herClimaxes; s.desiresMet = desiresMet; s.desiresMissed = desiresMissed;
    s.seed = seed; s.novelty = novelty;
    return s;
}

const char* Night::desireLine(Desire d, bool stroking) {
    switch (d) {
        case Desire::Faster: return stroking ? "Faster\xe2\x80\xa6 don't stop." : "Don't make me wait\xe2\x80\xa6";
        case Desire::Slower: return "Slow down\xe2\x80\xa6 make it last.";
        case Desire::Deeper: return "Deeper\xe2\x80\xa6 all of you.";
        case Desire::Gentler: return "Gently\xe2\x80\xa6 just like that, softer.";
        case Desire::Touch: return "Let me touch myself for you\xe2\x80\xa6";
        case Desire::Kiss: return "Come here. Kiss me.";
        case Desire::Change: return "Let's try something new\xe2\x80\xa6";
        default: return "";
    }
}

const char* Night::metLine(unsigned k) {
    static const char* lines[] = {"Yes\xe2\x80\xa6 like that.", "Mmm, right there.", "You know me so well.", "Oh\xe2\x80\xa6 yes.",
                                  "Don't stop.", "Just like that\xe2\x80\xa6"};
    return lines[k % 6];
}

void Night::detectStrokes(float dt, float d) {
    float v = (d - lastDepth_) / std::max(dt, 1e-4f);
    lastDepth_ = d;
    sinceStroke += dt;
    if (goingIn_) {
        maxVel_ = std::max(maxVel_, v);
        if (d > extreme_) extreme_ = d;
        // turned around after going in at least 6 % of the range: one stroke completed at the peak
        if (d < extreme_ - 0.04f && extreme_ - lastTrough_ > 0.06f) {
            float amp = extreme_ - lastTrough_;
            float period = elapsed - lastPeakT_;
            if (period > 0.18f && period < 4.f) tempo = approach(tempo, 1.f / period, 0.55f);
            else if (period >= 4.f) tempo = approach(tempo, 0.25f, 0.6f);
            amplitude = approach(amplitude, amp, 0.5f);
            lastPeakT_ = elapsed;
            sinceStroke = 0.f;
            events.push_back({NightEvent::Stroke, std::max(0.f, maxVel_), Desire::None});
            goingIn_ = false;
            extreme_ = d;
            maxVel_ = 0.f;
        } else if (d < extreme_ - 0.04f) {
            goingIn_ = false; extreme_ = d;
        }
    } else {
        if (d < extreme_) extreme_ = d;
        if (d > extreme_ + 0.04f) { lastTrough_ = extreme_; goingIn_ = true; extreme_ = d; maxVel_ = 0.f; }
    }
    // no stroke for longer than the rhythm explains: tempo and amplitude relax toward stillness
    if (!stroking()) { tempo = approach(tempo, 0.f, dt * 1.5f); amplitude = approach(amplitude, 0.f, dt * 0.8f); }
}

bool Night::desireSatisfied(const Input& in) const {
    switch (desire) {
        case Desire::Faster: return tempo > wantTempo - 0.15f && stroking();
        case Desire::Slower: return tempo < wantTempo + 0.15f && sinceStroke < 3.5f;
        case Desire::Deeper: return amplitude > wantAmp - 0.08f && stroking();
        case Desire::Gentler: return amplitude < wantAmp + 0.1f && sinceStroke < 3.5f;
        case Desire::Touch: return in.rub > 0.5f;
        case Desire::Kiss: return in.embrace > 0.5f;
        case Desire::Change: return pos != desirePos_;
        default: return false;
    }
}

void Night::pickDesire(const Input& in) {
    const PositionInfo& p = *in.pos;
    std::vector<std::pair<Desire, float>> c;
    bool isStroking = stroking();
    if (!p.solo) {
        float dt = tempo - wantTempo, da = amplitude - wantAmp;
        if (p.penetrative || p.id == "oral" || p.id == "lick") {
            if (dt < -0.25f || !isStroking) c.push_back({Desire::Faster, 2.f + std::max(0.f, -dt) * 3.f});
            if (dt > 0.3f) c.push_back({Desire::Slower, 2.f + dt * 3.f});
            if (da < -0.15f) c.push_back({Desire::Deeper, 1.5f + std::max(0.f, -da) * 4.f});
            if (da > 0.2f) c.push_back({Desire::Gentler, 1.2f + da * 3.f});
        }
        if (p.canTouch && in.rub < 0.3f) c.push_back({Desire::Touch, 0.9f});
        if (p.canKiss && in.embrace < 0.3f) c.push_back({Desire::Kiss, 1.1f});
    }
    if (posTime > 55.f) c.push_back({Desire::Change, 0.6f + (1.f - novelty[pos]) * 2.5f});
    if (c.empty()) { nextDesire = 5.f + 4.f * rnd(); return; }
    float total = 0.f;
    for (auto& e : c) total += e.second;
    float r = rnd() * total;
    desire = c.back().first;
    for (auto& e : c) { if ((r -= e.second) <= 0.f) { desire = e.first; break; } }
    desireAge = 0.f; desireHold = 0.f;
    desireTimeout = 9.f + 4.f * rnd();
    desirePos_ = pos;
    events.push_back({NightEvent::DesireNew, 0.f, desire});
}

void Night::climaxes() {
    herClimaxAge += 0.f;
    if (her >= 1.f) {
        ++herClimaxes;
        herClimaxAge = 0.f;
        float intensity = 0.6f + 0.4f * connection;
        events.push_back({NightEvent::HerClimax, intensity, Desire::None});
        score += 600.f * (0.5f + connection) * (herClimaxes == 1 ? 1.f : 0.7f);
        connection = clamp01(connection + 0.12f);
        her = 0.5f;                        // she can come again: the glow stays
        desire = Desire::None; nextDesire = 9.f;
        if (him > 0.88f) { him = 1.f; }    // carried over the edge with her
    }
    if (him >= 1.f) {
        ++hisClimaxes;
        hisClimaxAge = 0.f;
        together = herClimaxAge < 3.5f || her > 0.9f;
        if (together && herClimaxAge >= 3.5f) {   // she is right there: the moment takes her too
            ++herClimaxes; herClimaxAge = 0.f;
            events.push_back({NightEvent::HerClimax, 0.9f + 0.1f * connection, Desire::None});
        }
        events.push_back({NightEvent::HisClimax, 0.7f + 0.3f * connection, Desire::None});
        if (together) {
            events.push_back({NightEvent::Together, 1.f, Desire::None});
            score += 2000.f * (0.5f + connection);
        } else if (herClimaxes == 0) {
            score *= 0.85f;                // too soon
        }
        him = 1.f;
        finished = true;
    }
}

void Night::update(float dt, const Input& in) {
    events.clear();
    if (finished || !in.pos || dt <= 0.f) return;
    const PositionInfo& p = *in.pos;
    if (p.id != pos) { pos = p.id; posTime = 0.f; }
    elapsed += dt; posTime += dt;
    herClimaxAge += dt; hisClimaxAge += dt;
    detectStrokes(dt, in.depth);

    // her wishes drift with her arousal: patient and slow early, urgent and deep near the end
    float wobble = 0.12f * std::sin(elapsed * 0.071f + (float)seed) + 0.08f * std::sin(elapsed * 0.13f);
    wantTempo = 0.55f + 1.35f * her * her + wobble;
    wantAmp = 0.28f + 0.5f * her + 0.06f * std::sin(elapsed * 0.09f);
    if (p.id == "lick") { wantTempo = 0.9f + 1.1f * her; wantAmp = 0.45f + 0.3f * her; }
    if (p.id == "oral") { wantTempo = 0.6f + 0.9f * him; wantAmp = 0.35f + 0.45f * him; }

    bool stroking = this->stroking();
    float match = stroking ? gauss(tempo - wantTempo, 0.42f) * gauss(amplitude - wantAmp, 0.3f) : 0.f;
    sync = approach(sync, match, dt * 2.f);
    energy = approach(energy, stroking ? tempo * (0.25f + amplitude) : 0.f, dt * 2.5f);

    // novelty: a position feels fresh again after a break
    for (auto& kv : novelty) {
        if (kv.first == pos) kv.second = std::max(0.25f, kv.second - dt / 150.f);
        else kv.second = std::min(1.f, kv.second + dt / 200.f);
    }
    float fresh = 0.6f + 0.4f * novelty[pos];
    float liked = like[pos];

    // ---- her ----
    float stim = 0.f;
    if (p.solo) {
        stim = 0.55f * clamp01(in.depth * 1.2f) + 0.35f * in.rub + 0.3f * sync;
    } else if (p.id == "oral") {
        stim = 0.12f + 0.25f * connection;          // pleasing him pleases her, a little
    } else {
        float drive = std::min(1.f, energy / 1.1f);
        stim = (0.25f * drive + 0.75f * sync * (0.35f + 0.65f * drive)) * p.her;
    }
    stim += 0.45f * in.rub * (p.canTouch ? 1.f : 0.f) * (p.solo ? 0.f : 1.f);
    stim += 0.18f * in.embrace;
    // early on every good stroke shows on her (faster start), the last stretch takes patience
    herRate = stim * fresh * liked * (0.65f + 0.7f * connection) * 0.0062f * (1.45f - 0.6f * her);
    if (stim < 0.05f) herRate = -0.004f;
    if (herClimaxAge < 4.f) herRate *= 0.2f;     // afterglow of her climax
    her = clamp01(her + herRate * dt);

    // ---- him ----
    float push = std::pow(std::min(1.2f, energy / 1.1f), 1.35f);
    if (p.solo) himRate = -0.01f;
    else if (p.id == "lick") himRate = 0.0012f * (0.5f + her) - 0.002f;
    else if (p.id == "oral") himRate = (0.0022f + 0.005f * sync) * (0.4f + push) * p.him;
    else himRate = (0.0009f + 0.0031f * push) * p.him * (0.75f + 0.5f * her) * (0.85f + 0.3f * connection);
    if (energy < 0.25f && !p.solo) himRate -= 0.006f;   // slowing down lets him cool off
    if (herClimaxAge < 2.5f) himRate += 0.012f;          // her climax is hard to resist
    him = clamp01(him + himRate * dt);

    // ---- edging: close, then slowing down on purpose ----
    bool close = him > 0.82f;
    if (close && himRate < 0.f) {
        if (!edging) { edging = true; edgeTime = 0.f; events.push_back({NightEvent::EdgeStart, him, Desire::None}); }
        edgeTime += dt;
        score += dt * 10.f;
        connection = clamp01(connection + dt * 0.004f);
    } else if (!close) edging = false;

    // ---- connection ----
    if (stroking || p.solo || in.embrace > 0.3f) connection = approach(connection, 0.25f + 0.75f * sync, dt * 0.06f);
    else connection = clamp01(connection - dt * 0.004f);
    if (in.embrace > 0.5f) connection = clamp01(connection + dt * 0.01f);

    // ---- desires ----
    if (desire == Desire::None) {
        nextDesire -= dt;
        if (nextDesire <= 0.f && herClimaxAge > 5.f) pickDesire(in);
    } else {
        desireAge += dt;
        desireHold = desireSatisfied(in) ? desireHold + dt : std::max(0.f, desireHold - dt * 2.f);
        if (desireHold > 1.1f || (desire == Desire::Change && desireHold > 0.f)) {
            ++desiresMet; ++streak; bestStreak = std::max(bestStreak, streak);
            score += 100.f * (1.f + 0.25f * (float)std::min(streak, 8));
            connection = clamp01(connection + 0.05f);
            her = clamp01(her + 0.025f);
            events.push_back({NightEvent::DesireMet, (float)streak, desire});
            desire = Desire::None;
            nextDesire = 8.f + 7.f * rnd();
        } else if (desireAge > desireTimeout) {
            ++desiresMissed; streak = 0;
            connection = clamp01(connection - 0.035f);
            events.push_back({NightEvent::DesireMissed, 0.f, desire});
            desire = Desire::None;
            nextDesire = 6.f + 6.f * rnd();
        }
    }
    if (!favoriteSaid_ && pos == favorite && sync > 0.6f && posTime > 6.f) {
        favoriteSaid_ = true;
        events.push_back({NightEvent::Favorite, 0.f, Desire::None});
    }

    score += dt * (0.5f + 6.f * sync * (0.5f + connection)) * (stroking || p.solo ? 1.f : 0.2f);
    climaxes();
}

}  // namespace ag
