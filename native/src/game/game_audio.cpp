#include "game/game.h"
#include "core/log.h"
#include "raylib.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>

namespace ag {

namespace {
float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
float frand() { return (float)GetRandomValue(0, 10000) / 10000.f; }

// ---- generated room ambience + a slow pad, streamed on the audio thread ----
// a state-variable filter (band / low pass), f = 2 sin(pi fc / sr), q = 1 / Q
struct Svf { float lp = 0.f, bp = 0.f; float run(float x, float f, float q) { lp += f * bp; float hp = x - lp - q * bp; bp += f * hp; return bp; } };

// places: 0 the bedroom at night, 1 party (murmur, glasses), 2 sea (waves, breeze), 3 storm (rain, wind, thunder),
// 4 fire (crackling hearth), 5 city (distant traffic), 6 spa (trickling water, chimes)
enum AmbScene { kAmbNight = 0, kAmbParty, kAmbSea, kAmbStorm, kAmbFire, kAmbCity, kAmbSpa, kAmbCount };

struct AmbState {
    std::atomic<float> amb{0.f}, music{0.f}, duck{0.f};
    std::atomic<int> scene{0};                 // bit mask of AmbScene layers (rain + fire in the cabin...)
    float sine[2048];
    unsigned rng = 0x1234567u;
    float brown = 0.f, lp1 = 0.f, lp2 = 0.f, dropEnv = 0.f, dropLp = 0.f, hp = 0.f;
    double t = 0.0;
    float ph[3][6] = {};
    float padLp[2] = {0.f, 0.f};
    float ambS = 0.f, musS = 0.f, duckS = 0.f;
    float lay[kAmbCount] = {1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};   // smoothed layer gains
    // party: murmuring voices (formant band-pass on noise, syllable + phrase gates), glasses now and then
    Svf voiceF[6]; float vForm[6] = {420, 560, 380, 700, 500, 640}, vSyl[6] = {}, vSylRate[6] = {4.1f, 3.6f, 5.2f, 4.6f, 3.9f, 4.8f};
    float vPhrase[6] = {}, vGate[6] = {}, vPan[6] = {0.2f, 0.8f, 0.4f, 0.65f, 0.3f, 0.75f};
    float clinkEnv = 0.f, clinkPh = 0.f, clinkF = 3200.f;
    // sea: swell + foam
    Svf seaF, foamF; float swellPh = 0.f, swellLen = 8.f, seaLp = 0.f;
    // storm: rain hiss, roof patter, gusts, thunder
    Svf rainF, gustF; float rainLp = 0.f, patterEnv = 0.f, gustPh = 0.f, thunderEnv = 0.f, thunderLp = 0.f, thunderLp2 = 0.f, thunderT = 20.f;
    // fire: roar + crackles
    float fireLp = 0.f, crackEnv = 0.f, crackLp = 0.f;
    // city: traffic hum + passing cars
    Svf carF; float cityLp = 0.f, cityLp2 = 0.f, carEnv = 0.f, carT = 6.f;
    // spa: trickle + chimes
    Svf waterF; float waterEnv = 0.f, chimeEnv = 0.f, chimePh[3] = {}, chimeF[3] = {1318.5f, 1760.f, 2217.f};
};
AmbState gAmb;
AudioStream gAmbStream{};
bool gAmbOn = false;

float wn() { gAmb.rng = gAmb.rng * 1664525u + 1013904223u; return (float)(gAmb.rng >> 9) / 4194304.f - 1.f; }

void ambCallback(void* buffer, unsigned int frames) {
    float* out = (float*)buffer;
    const float sr = 44100.f;
    // chords (Hz): Dmaj9, Bm7, Gmaj7, A6sus - warm, unresolved, slow
    static const float chords[4][5] = {{146.83f, 185.0f, 220.0f, 277.18f, 329.63f}, {123.47f, 146.83f, 185.0f, 220.0f, 277.18f},
                                       {98.0f, 123.47f, 146.83f, 185.0f, 246.94f}, {110.0f, 138.59f, 164.81f, 185.0f, 246.94f}};
    const float chordLen = 11.f;
    for (unsigned i = 0; i < frames; ++i) {
        gAmb.ambS += (gAmb.amb.load(std::memory_order_relaxed) - gAmb.ambS) * 0.0005f;
        gAmb.musS += (gAmb.music.load(std::memory_order_relaxed) - gAmb.musS) * 0.0005f;
        // room tone: leaky brown noise, double low-passed, breathing slowly
        gAmb.brown = gAmb.brown * 0.996f + wn() * 0.04f;
        gAmb.lp1 += (gAmb.brown - gAmb.lp1) * 0.05f;
        gAmb.lp2 += (gAmb.lp1 - gAmb.lp2) * 0.05f;
        float swell = 0.75f + 0.25f * std::sin((float)gAmb.t * 0.11f);
        float room = gAmb.lp2 * 1.6f * swell;
        // rain on the window: sparse soft ticks through a band-pass
        if ((gAmb.rng & 0xffff) < 26) gAmb.dropEnv = 0.4f + 0.6f * (float)((gAmb.rng >> 16) & 255) / 255.f;
        float drop = wn() * gAmb.dropEnv;
        gAmb.dropEnv *= 0.9975f;
        gAmb.dropLp += (drop - gAmb.dropLp) * 0.35f;
        float hpIn = gAmb.dropLp;
        float rain = hpIn - gAmb.hp;
        gAmb.hp += (hpIn - gAmb.hp) * 0.06f;
        float ambient = (room * 0.9f + rain * 0.12f) * gAmb.ambS * 0.085f;   // ~ -36 dBFS: felt more than heard
        // ---- the place (story scenes): layers cross-fade over ~0.6 s ----
        int sc = gAmb.scene.load(std::memory_order_relaxed);
        float placeL = 0.f, placeR = 0.f;
        for (int k = 1; k < kAmbCount; ++k) gAmb.lay[k] += ((((sc >> k) & 1) ? 1.f : 0.f) - gAmb.lay[k]) * 0.00004f;
        float dt1 = 1.f / sr;
        if (gAmb.lay[kAmbParty] > 0.001f) {
            float sumL = 0.f, sumR = 0.f;
            for (int v = 0; v < 6; ++v) {
                // phrases come and go; inside a phrase the syllables pulse at 3.5-5 Hz
                gAmb.vPhrase[v] -= dt1;
                if (gAmb.vPhrase[v] <= 0.f) {
                    bool talking = gAmb.vGate[v] < 0.5f;
                    gAmb.vGate[v] = talking ? 1.f : 0.f;
                    gAmb.vPhrase[v] = talking ? 1.2f + 2.6f * (wn() * 0.5f + 0.5f) : 0.4f + 1.6f * (wn() * 0.5f + 0.5f);
                    gAmb.vForm[v] = 320.f + 520.f * (wn() * 0.5f + 0.5f);
                }
                gAmb.vSyl[v] += gAmb.vSylRate[v] * dt1;
                if (gAmb.vSyl[v] >= 1.f) gAmb.vSyl[v] -= 1.f;
                float syl = gAmb.sine[(int)(gAmb.vSyl[v] * 2048.f) & 2047] * 0.5f + 0.5f;
                float f = 2.f * std::sin(3.14159f * gAmb.vForm[v] / sr);
                float x = gAmb.voiceF[v].run(wn(), f, 0.22f) * syl * syl * gAmb.vGate[v];
                sumL += x * (1.f - gAmb.vPan[v]); sumR += x * gAmb.vPan[v];
            }
            if ((gAmb.rng & 0x3ffff) < 2) { gAmb.clinkEnv = 1.f; gAmb.clinkF = 2600.f + 1800.f * (wn() * 0.5f + 0.5f); }
            gAmb.clinkPh += gAmb.clinkF / sr; if (gAmb.clinkPh >= 1.f) gAmb.clinkPh -= 1.f;
            float clink = gAmb.sine[(int)(gAmb.clinkPh * 2048.f) & 2047] * gAmb.clinkEnv * 0.05f;
            gAmb.clinkEnv *= 0.99965f;
            float g = gAmb.lay[kAmbParty] * 0.11f;
            placeL += (sumL + clink) * g; placeR += (sumR + clink * 0.8f) * g;
        }
        if (gAmb.lay[kAmbSea] > 0.001f) {
            // a wave every 7-11 s: the swell rises over ~3 s, breaks, foam hisses out
            gAmb.swellPh += dt1 / gAmb.swellLen;
            if (gAmb.swellPh >= 1.f) { gAmb.swellPh -= 1.f; gAmb.swellLen = 7.f + 4.f * (wn() * 0.5f + 0.5f); }
            float p = gAmb.swellPh;
            float swell = p < 0.35f ? p / 0.35f : std::exp(-(p - 0.35f) * 4.5f);
            float foam = p < 0.33f ? 0.f : std::exp(-(p - 0.33f) * 7.f);
            gAmb.seaLp += (wn() - gAmb.seaLp) * 0.02f;
            float body = gAmb.seaF.run(gAmb.seaLp, 0.03f, 0.9f) * 2.2f;
            float hiss = gAmb.foamF.run(wn(), 0.6f, 1.2f);
            float s = (body * (0.35f + 0.65f * swell) + hiss * foam * 0.35f) * gAmb.lay[kAmbSea] * 0.12f;
            placeL += s; placeR += s * 0.94f;
        }
        if (gAmb.lay[kAmbStorm] > 0.001f) {
            gAmb.rainLp += (wn() - gAmb.rainLp) * 0.45f;
            float hiss = gAmb.rainF.run(gAmb.rainLp, 0.5f, 1.4f);
            if ((gAmb.rng & 0xfff) < 6) gAmb.patterEnv = 0.6f + 0.4f * (wn() * 0.5f + 0.5f);
            float patter = wn() * gAmb.patterEnv; gAmb.patterEnv *= 0.993f;
            gAmb.gustPh += dt1 * 0.07f; if (gAmb.gustPh >= 1.f) gAmb.gustPh -= 1.f;
            float gust = std::pow(std::max(0.f, gAmb.sine[(int)(gAmb.gustPh * 2048.f) & 2047]), 2.f);
            float wind = gAmb.gustF.run(wn(), 0.035f + 0.02f * gust, 0.6f) * (0.2f + gust);
            gAmb.thunderT -= dt1;
            if (gAmb.thunderT <= 0.f) { gAmb.thunderEnv = 1.f; gAmb.thunderT = 22.f + 30.f * (wn() * 0.5f + 0.5f); }
            gAmb.thunderLp += (wn() - gAmb.thunderLp) * 0.004f;
            gAmb.thunderLp2 += (gAmb.thunderLp - gAmb.thunderLp2) * 0.01f;
            float thunder = gAmb.thunderLp2 * gAmb.thunderEnv * 14.f;
            gAmb.thunderEnv *= 0.99996f;
            float s = (hiss * 0.5f + patter * 0.05f + wind * 0.5f + thunder) * gAmb.lay[kAmbStorm] * 0.14f;
            placeL += s; placeR += s * 0.97f + patter * 0.004f;
        }
        if (gAmb.lay[kAmbFire] > 0.001f) {
            gAmb.fireLp += (wn() - gAmb.fireLp) * 0.012f;
            if ((gAmb.rng & 0xffff) < 9) gAmb.crackEnv = 0.4f + 0.6f * (wn() * 0.5f + 0.5f);
            float crack = wn() * gAmb.crackEnv; gAmb.crackEnv *= 0.985f;
            gAmb.crackLp += (crack - gAmb.crackLp) * 0.5f;
            float s = (gAmb.fireLp * 1.4f + gAmb.crackLp * 0.25f) * gAmb.lay[kAmbFire] * 0.12f;
            placeL += s; placeR += s * 0.9f;
        }
        if (gAmb.lay[kAmbCity] > 0.001f) {
            gAmb.cityLp += (wn() - gAmb.cityLp) * 0.01f;
            gAmb.cityLp2 += (gAmb.cityLp - gAmb.cityLp2) * 0.01f;
            gAmb.carT -= dt1;
            if (gAmb.carT <= 0.f) { gAmb.carEnv = 0.f; gAmb.carT = 5.f + 9.f * (wn() * 0.5f + 0.5f); }
            gAmb.carEnv += dt1 * 0.35f;
            float car = gAmb.carF.run(wn(), 0.05f, 0.7f) * std::max(0.f, std::sin(std::min(gAmb.carEnv, 1.f) * 3.14159f));
            float s = (gAmb.cityLp2 * 3.f + car * 0.3f) * gAmb.lay[kAmbCity] * 0.1f;
            placeL += s; placeR += s * 0.9f;
        }
        if (gAmb.lay[kAmbSpa] > 0.001f) {
            if ((gAmb.rng & 0x7ff) < 5) gAmb.waterEnv = 0.4f + 0.6f * (wn() * 0.5f + 0.5f);
            float water = gAmb.waterF.run(wn(), 0.32f, 0.35f) * gAmb.waterEnv; gAmb.waterEnv *= 0.997f;
            if ((gAmb.rng & 0x3fffff) < 3) gAmb.chimeEnv = 1.f;
            float chime = 0.f;
            for (int c = 0; c < 3; ++c) {
                gAmb.chimePh[c] += gAmb.chimeF[c] / sr; if (gAmb.chimePh[c] >= 1.f) gAmb.chimePh[c] -= 1.f;
                chime += gAmb.sine[(int)(gAmb.chimePh[c] * 2048.f) & 2047] * (0.5f - 0.12f * (float)c);
            }
            chime *= gAmb.chimeEnv * 0.03f; gAmb.chimeEnv *= 0.99993f;
            float s = (water * 0.3f + chime) * gAmb.lay[kAmbSpa] * 0.12f;
            placeL += s; placeR += s * 0.92f;
        }
        gAmb.duckS += (gAmb.duck.load(std::memory_order_relaxed) - gAmb.duckS) * 0.0008f;
        float duckA = 1.f - 0.45f * gAmb.duckS;
        float placeSum = 0.f;
        for (int k = 1; k < kAmbCount; ++k) placeSum += gAmb.lay[k];
        ambient *= duckA * (1.f - 0.5f * std::min(1.f, placeSum));   // the room tone sits under a place
        float placeGain = gAmb.ambS * duckA;
        placeL *= placeGain; placeR *= placeGain;
        // pad: two neighbouring chords cross-fade
        float pos = (float)(gAmb.t / chordLen);
        int c0 = (int)pos % 4, c1 = (c0 + 1) % 4;
        float f = pos - std::floor(pos);
        float x = clamp01((f - 0.7f) / 0.3f);
        float w1 = x * x * (3.f - 2.f * x), w0 = 1.f - w1;
        float pad = 0.f;
        if (gAmb.musS > 0.0005f) {
            for (int k = 0; k < 2; ++k) {
                int c = k == 0 ? c0 : c1;
                float wgt = k == 0 ? w0 : w1;
                if (wgt < 0.001f) continue;
                for (int n = 0; n < 5; ++n) {
                    float fr = chords[c][n] * (1.f + 0.0015f * std::sin((float)gAmb.t * (0.3f + 0.07f * (float)n)));
                    float& p = gAmb.ph[k][n];
                    p += fr / sr;
                    if (p >= 1.f) p -= 1.f;
                    float s1 = gAmb.sine[(int)(p * 2048.f) & 2047];
                    float s2 = gAmb.sine[(int)(p * 4096.f) & 2047];
                    pad += (s1 + 0.22f * s2) * wgt * (n == 0 ? 0.9f : 0.55f);
                }
            }
            // keep the oscillators of the outgoing slot in phase with the next chord
            if (f < 0.0001f) for (int n = 0; n < 6; ++n) gAmb.ph[0][n] = gAmb.ph[1][n];
            pad *= 0.085f * gAmb.musS * (0.85f + 0.15f * std::sin((float)gAmb.t * 0.21f));
        }
        gAmb.padLp[0] += (pad - gAmb.padLp[0]) * 0.12f;
        float padD = gAmb.padLp[0] * (1.f - 0.5f * gAmb.duckS);
        float l = ambient + padD + placeL;
        float r = ambient * 0.96f + padD * 0.98f + placeR;
        out[i * 2] = l;
        out[i * 2 + 1] = r;
        gAmb.t += 1.0 / sr;
    }
}
}  // namespace

void Game::initAudio() {
    if (!audio.init()) return;
    int n = audio.loadAll();
    audio.setMasterVolume(settings.master);
    // UI: soft plucks and ticks, synthesized
    auto pluck = [](float f0, float f1, float decay, float amp) {
        return [=](float* d, int nf) {
            for (int i = 0; i < nf; ++i) {
                float t = (float)i / 44100.f;
                float e = std::exp(-t / decay) * std::min(1.f, t * 900.f);
                d[i] = amp * e * (std::sin(6.2831853f * f0 * t) * 0.7f + std::sin(6.2831853f * f1 * t) * 0.3f);
            }
        };
    };
    audio.addProcedural("ui/hover", 2200, pluck(2100.f, 3150.f, 0.012f, 0.25f));
    audio.addProcedural("ui/press", 9000, pluck(660.f, 990.f, 0.07f, 0.4f));
    audio.addProcedural("ui/back", 9000, pluck(440.f, 660.f, 0.07f, 0.35f));
    audio.addProcedural("ui/tick", 900, pluck(3000.f, 4500.f, 0.004f, 0.18f));
    // breaths: band-limited noise with a soft swell (in) or a sigh (out)
    for (int k = 0; k < 6; ++k) {
        bool in = k < 3;
        float len = in ? 0.65f + 0.1f * (float)k : 0.8f + 0.12f * (float)(k - 3);
        int nf = (int)(len * 44100.f);
        unsigned seed = 977u * (unsigned)(k + 1);
        audio.addProcedural(std::string("breath/") + (in ? "in" : "out") + std::to_string(k % 3), nf, [=](float* d, int cnt) {
            unsigned s = seed;
            float lp = 0.f, lp2 = 0.f, bp = 0.f;
            for (int i = 0; i < cnt; ++i) {
                s = s * 1664525u + 1013904223u;
                float w = (float)(s >> 9) / 4194304.f - 1.f;
                float u = (float)i / (float)cnt;
                float env = in ? std::sin(3.14159f * std::pow(u, 0.8f)) : std::pow(std::sin(3.14159f * std::pow(u, 0.5f)), 1.5f);
                float cut = in ? 0.16f + 0.06f * u : 0.12f - 0.05f * u;
                lp += (w - lp) * cut;
                lp2 += (lp - lp2) * 0.03f;
                bp = lp - lp2;
                d[i] = bp * env * (in ? 0.55f : 0.7f);
            }
        });
    }
    // story foley: fabric sliding off (a few soft swishes), a kiss
    for (int k = 0; k < 3; ++k) {
        unsigned seed = 7919u * (unsigned)(k + 3);
        audio.addProcedural("fx/rustle" + std::to_string(k), (int)(0.95f * 44100.f), [=](float* d, int cnt) {
            unsigned sd = seed;
            Svf f1;
            float sw[4], at[4];
            for (int j = 0; j < 4; ++j) { sd = sd * 1664525u + 1013904223u; at[j] = 0.08f + 0.2f * (float)j + 0.05f * (float)(sd >> 24) / 255.f; sw[j] = 0.5f + 0.5f * (float)((sd >> 16) & 255) / 255.f; }
            for (int i = 0; i < cnt; ++i) {
                sd = sd * 1664525u + 1013904223u;
                float w = (float)(sd >> 9) / 4194304.f - 1.f;
                float t = (float)i / 44100.f, env = 0.f;
                for (int j = 0; j < 4; ++j) { float u = (t - at[j]) / 0.16f; if (u > 0.f && u < 1.f) env += sw[j] * std::sin(3.14159f * u) * std::sin(3.14159f * u); }
                d[i] = f1.run(w, 0.55f + 0.15f * std::sin(t * 9.f), 0.9f) * env * 0.35f;
            }
        });
    }
    audio.addProcedural("fx/kiss", (int)(0.3f * 44100.f), [](float* d, int cnt) {
        unsigned sd = 4242u;
        Svf f1;
        for (int i = 0; i < cnt; ++i) {
            sd = sd * 1664525u + 1013904223u;
            float w = (float)(sd >> 9) / 4194304.f - 1.f;
            float t = (float)i / 44100.f;
            float smack = f1.run(w, 0.22f, 0.5f) * std::exp(-std::max(0.f, t - 0.05f) * 60.f) * std::min(1.f, t * 400.f);
            float pop = std::sin(6.2831853f * 190.f * t) * std::exp(-t * 45.f) * 0.4f;
            d[i] = (smack * 0.5f + pop * 0.5f) * 0.6f;
        }
    });
    for (int i = 0; i < 2048; ++i) gAmb.sine[i] = std::sin(6.2831853f * (float)i / 2048.f);
    SetAudioStreamBufferSizeDefault(4096);
    gAmbStream = LoadAudioStream(44100, 32, 2);
    if (IsAudioStreamValid(gAmbStream)) {
        SetAudioStreamCallback(gAmbStream, ambCallback);
        PlayAudioStream(gAmbStream);
        gAmbOn = true;
    }
    AG_INFO("AG: audio: %d samples + generated ambience", n);
}

void Game::shutdownAudio() {
    if (gAmbOn) {
        gAmb.amb.store(0.f); gAmb.music.store(0.f);
        StopAudioStream(gAmbStream);
        UnloadAudioStream(gAmbStream);
        gAmbOn = false;
    }
    audio.shutdown();
}

// the place's sound: party | sea | rain (storm) | fire | city | spa | night, combined with '+'
void Game::setAmbience(const std::string& places) {
    int mask = 0;
    size_t i = 0;
    while (i <= places.size()) {
        size_t j = places.find('+', i);
        if (j == std::string::npos) j = places.size();
        std::string p = places.substr(i, j - i);
        if (p == "party") mask |= 1 << kAmbParty;
        else if (p == "sea" || p == "beach") mask |= 1 << kAmbSea;
        else if (p == "rain" || p == "storm") mask |= 1 << kAmbStorm;
        else if (p == "fire" || p == "hearth") mask |= 1 << kAmbFire;
        else if (p == "city") mask |= 1 << kAmbCity;
        else if (p == "spa" || p == "water") mask |= 1 << kAmbSpa;
        i = j + 1;
    }
    if (mask != gAmb.scene.load(std::memory_order_relaxed) && harness) AG_INFO("AG: audio ambience %s", places.empty() ? "night" : places.c_str());
    gAmb.scene.store(mask, std::memory_order_relaxed);
}

// voices speaking: the room and the music step back
void Game::duckAmbience(float amount) { gAmb.duck.store(clamp01(amount), std::memory_order_relaxed); }

void Game::voice(const std::string& name, float vol, float pitch) {
    // per-sample gains measured offline (RMS/peak): the moan is mastered hot, the whispered lines quiet
    static const struct { const char* n; float g; } kGain[] = {{"moan02", 0.4f}, {"loveyou", 0.8f}, {"takeme", 3.f}, {"suck05", 4.f}, {"swallow04", 1.5f}};
    float g = 1.f;
    for (const auto& k : kGain) if (name == k.n) g = k.g;
    if (harness) AG_INFO("AG: audio t=%.2f voice %s vol %.2f pitch %.2f", (double)night_.elapsed, name.c_str(), (double)vol, (double)pitch);
    if (audio.ready()) audio.play("vox/" + name, vol * g * settings.voice, pitch);
    // simple lip sync: open her mouth for the length of the line (moans longer, words shorter)
    if (name == "moan02" || name == "loveyou" || name == "takeme") {
        mouthDur_ = (name == "moan02" ? 1.9f : 0.9f) / std::max(0.5f, pitch);
        mouthT_ = mouthDur_;
    }
}

void Game::sfx(const std::string& prefix, float vol, float jitter) {
    if (harness) AG_INFO("AG: audio t=%.2f sfx %s vol %.2f", (double)night_.elapsed, prefix.c_str(), (double)vol);
    if (audio.ready()) audio.playRandom("sfx/" + prefix + "/", vol * settings.effects, jitter);
}

void Game::breath(bool in, float vol) {
    if (harness) AG_INFO("AG: audio t=%.2f breath %s vol %.2f", (double)night_.elapsed, in ? "in" : "out", (double)vol);
    if (audio.ready()) audio.playRandom(in ? "breath/in" : "breath/out", vol * settings.voice, 0.06f);
}

void Game::playAudioEvents() {
    if ((!audio.ready() && !harness) || posIndex_ < 0) return;
    const PositionInfo& p = positions_[(size_t)posIndex_];
    float her = night_.her;
    for (const NightEvent& e : night_.events) {
        switch (e.type) {
            case NightEvent::Stroke: {
                float impact = clamp01(0.2f + 0.22f * e.value) * (0.55f + 0.45f * clamp01(night_.amplitude * 1.6f));
                if (p.solo) { if (frand() < 0.6f) sfx("strokes", 0.35f * impact); }
                else if (p.id == "oral") {
                    if (frand() < 0.35f) voice("suck05", 0.35f + 0.3f * impact, 0.95f + 0.1f * frand());
                    else sfx("wet", 0.3f * impact);
                } else if (p.id == "lick") sfx("wet", 0.28f * impact);
                else sfx(her > 0.35f ? "wetplaps" : "dryplaps", 0.25f + 0.65f * impact);
                break;
            }
            case NightEvent::DesireNew: breath(true, 0.4f); break;
            case NightEvent::DesireMet: voice("moan02", 0.35f + 0.3f * her, 1.12f + 0.06f * frand()); moanT_ = std::max(moanT_, 2.5f); break;
            case NightEvent::HerClimax: voice("moan02", 1.f, 1.05f); moanT_ = 3.f; break;
            case NightEvent::HisClimax: sfx("cum", 0.75f, 0.03f); break;
            case NightEvent::Together: break;
            default: break;
        }
    }
}

void Game::updateAudio(float dt) {
    if (!audio.ready() && !harness) return;
    if (audio.ready()) { audio.setMasterVolume(settings.master); audio.update(); }
    bool playing = screen_ == Screen::Play;
    bool inNight = playing || screen_ == Screen::Pause || screen_ == Screen::Results;
    // the room is quieter in the menus, the pad a little more present
    float ambT = settings.ambience * (inNight ? 0.85f : 1.f);
    float musT = settings.music * (inNight ? 0.55f : 1.f) * (screen_ == Screen::Pause ? 0.6f : 1.f);
    gAmb.amb.store(ambT, std::memory_order_relaxed);
    gAmb.music.store(musT, std::memory_order_relaxed);
    if (!playing || posIndex_ < 0) return;
    const PositionInfo& p = positions_[(size_t)posIndex_];
    float her = night_.her;
    // her voice: breaths always, moans with arousal
    breathT_ -= dt;
    if (breathT_ <= 0.f) {
        static bool inhale = true;
        breath(inhale, 0.16f + 0.35f * her + 0.15f * touchAct_);
        inhale = !inhale;
        breathT_ = (3.0f - 2.0f * her) * (0.85f + 0.3f * frand());
    }
    // her voice is always there: soft sighs as soon as you move together, moans that grow with her
    moanT_ -= dt;
    if (moanT_ <= 0.f && p.id != "oral") {
        bool active = night_.stroking() || p.solo || rub_ > 0.5f || touchAct_ > 0.2f;
        if (active) {
            if (her < 0.15f && frand() < 0.5f) breath(false, 0.45f);                              // a sigh
            else voice("moan02", 0.18f + 0.7f * her, 0.88f + 0.18f * frand() + 0.06f * her);   // soft early, full later
        }
        if (her > 0.6f && (night_.desire == Desire::Deeper || night_.desire == Desire::Faster) && frand() < 0.35f) voice("takeme", 0.7f, 1.f);
        moanT_ = (7.5f - 5.2f * her) * (0.75f + 0.5f * frand());
    }
    // afterglow
    if (night_.finished && afterT_ > 2.2f && afterT_ - dt <= 2.2f && together_) voice("loveyou", 0.85f, 1.f);
    wetT_ -= dt;
    if (wetT_ <= 0.f && her > 0.5f && night_.sinceStroke < 0.8f && !p.solo && p.penetrative) {
        sfx("wet", 0.12f + 0.2f * her);
        wetT_ = 0.9f + 1.5f * frand();
    }
}

}  // namespace ag
