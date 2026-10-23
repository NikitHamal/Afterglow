#!/usr/bin/env python3
"""Story voice acting with Kokoro-82M (Apache-2.0): every spoken line of native/assets/story/*.json becomes a
small mono OGG, plus a manifest with durations, lip-sync envelopes and word timings.

    python tools/voice/gen_voices.py [--stories DIR] [--out DIR] [--only STORY] [--limit N] [--dry]

Files are content-addressed (hash of speaker, delivery, voice mix, speed, text), so re-running after a story
edit only synthesises the lines that changed; files no longer referenced are deleted.
Runs on GitHub Actions (.github/workflows/voices.yml) — see docs/VOICE.md.
"""
import argparse, base64, hashlib, json, os, re, subprocess, sys, tempfile, time

import numpy as np

SR = 24000
ENV_FPS = 30
MODEL_TAG = "kokoro-82M-v1.0"

# Casting. Weights are mixed voice-pack tensors (Kokoro style vectors blend linearly).
CAST = {
    # Vivian, early forties: warm, low, unhurried
    "vivian":   {"lang": "a", "mix": {"af_heart": 0.62, "af_bella": 0.38}, "speed": 0.95},
    # Marcus, about forty: warm baritone
    "marcus":   {"lang": "a", "mix": {"am_michael": 0.58, "am_fenrir": 0.42}, "speed": 0.94},
    # Narrator: an intimate, literary British voice
    "narrator": {"lang": "b", "mix": {"bf_emma": 0.72, "bf_isabella": 0.28}, "speed": 0.93},
}
# Delivery tags (story line field "delivery"); "blend" adds packs on top of the cast mix before normalising.
DELIVERY = {
    "normal":     {},
    "soft":       {"blend": {"vivian": {"af_nicole": 0.30}, "marcus": {"am_onyx": 0.35}, "narrator": {"bf_isabella": 0.2}}, "speed": 0.93},
    "whisper":    {"blend": {"vivian": {"af_nicole": 0.75}, "marcus": {"am_onyx": 0.55}, "narrator": {"bf_isabella": 0.4}},
                   "speed": 0.88, "gain_db": -2.5, "fx": "highpass=f=150,treble=g=2.5"},
    "playful":    {"speed": 1.04},
    "urgent":     {"speed": 1.08},
    "breathless": {"blend": {"vivian": {"af_nicole": 0.45}, "marcus": {"am_onyx": 0.3}}, "speed": 0.9},
    "tender":     {"blend": {"vivian": {"af_nicole": 0.2}, "marcus": {"am_onyx": 0.25}}, "speed": 0.9},
}
WHISPER_CUES = re.compile(r"\b(whisper|whispers|whispered|murmur|murmurs|murmured|breathes|breathed|against (his|her) (ear|mouth|lips|neck))\b", re.I)
SOFT_CUES = re.compile(r"\b(softly|quietly|barely|gently|low voice|under (his|her) breath)\b", re.I)


def clean_text(t):
    t = t.replace("…", "...").replace("—", ", ").replace("–", ", ")
    t = re.sub(r"\s+-\s+", ", ", t)            # spaced hyphen used as a dash
    t = re.sub(r"[*_\"“”]", "", t)
    t = re.sub(r"\bMm+\b", "Mmm", t)
    t = re.sub(r"\s+", " ", t).strip()
    return t


def infer_delivery(node, idx, lines):
    ln = lines[idx]
    if ln.get("delivery"):
        return ln["delivery"]
    prev = lines[idx - 1]["text"] if idx > 0 and (lines[idx - 1].get("who") or "narrator") == "narrator" else ""
    if WHISPER_CUES.search(prev):
        return "whisper"
    intimate = bool(node.get("act")) or any(v in ("partial", "nude") for v in (node.get("outfit") or {}).values())
    if SOFT_CUES.search(prev):
        return "soft"
    if intimate and (ln.get("who") in ("vivian", "marcus")):
        return "tender"
    return "normal"


def voice_spec(who, delivery):
    c = CAST[who]; d = DELIVERY.get(delivery, {})
    mix = dict(c["mix"])
    for k, w in (d.get("blend", {}).get(who, {}) or {}).items():
        mix[k] = mix.get(k, 0.0) + w
    s = sum(mix.values())
    mix = {k: round(v / s, 4) for k, v in sorted(mix.items())}
    return {"lang": c["lang"], "mix": mix, "speed": round(c["speed"] * d.get("speed", 1.0), 3),
            "gain_db": d.get("gain_db", 0.0), "fx": d.get("fx", "")}


def key_for(who, delivery, spec, text):
    blob = json.dumps({"m": MODEL_TAG, "who": who, "d": delivery, "s": spec, "t": text}, sort_keys=True)
    return hashlib.sha1(blob.encode()).hexdigest()[:16]


def trim(audio, thresh_db=-46.0, pad=0.04):
    a = np.abs(audio); thr = 10 ** (thresh_db / 20) * max(1e-6, a.max())
    idx = np.where(a > thr)[0]
    if not len(idx):
        return audio, 0.0
    p = int(pad * SR); s = max(0, idx[0] - p); e = min(len(audio), idx[-1] + p)
    return audio[s:e], s / SR


def normalise(audio, gain_db=0.0, target_dbfs=-20.0):
    rms = np.sqrt(np.mean(audio.astype(np.float64) ** 2)) + 1e-9
    g = 10 ** ((target_dbfs + gain_db) / 20) / rms
    y = audio * g
    peak = np.abs(y).max()
    if peak > 0.89:                     # soft-knee limiter instead of clipping
        y = np.tanh(y / 0.89 * 1.2) * 0.89 / np.tanh(1.2)
    return y.astype(np.float32)


def envelope(audio):
    hop = SR // ENV_FPS
    n = max(1, len(audio) // hop)
    fr = audio[: n * hop].reshape(n, hop)
    rms = np.sqrt((fr.astype(np.float64) ** 2).mean(1))
    ref = np.percentile(rms[rms > 1e-4], 95) if (rms > 1e-4).any() else 1.0
    v = np.clip(rms / max(ref, 1e-6), 0, 1) ** 0.75
    return base64.b64encode((v * 255 + 0.5).astype(np.uint8).tobytes()).decode()


def encode_ogg(audio, path, fx):
    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as tf:
        tmp = tf.name
    import soundfile as sf
    sf.write(tmp, audio, SR, subtype="PCM_16")
    cmd = ["ffmpeg", "-y", "-loglevel", "error", "-i", tmp]
    if fx:
        cmd += ["-af", fx]
    cmd += ["-ac", "1", "-ar", str(SR), "-c:a", "libvorbis", "-q:a", "4", path]
    subprocess.run(cmd, check=True)
    os.remove(tmp)


class Synth:
    def __init__(self):
        import torch
        from kokoro import KModel, KPipeline
        torch.set_num_threads(max(1, os.cpu_count() or 1))
        self.torch = torch
        self.model = KModel(repo_id="hexgrad/Kokoro-82M").eval()
        self.pipes = {l: KPipeline(lang_code=l, repo_id="hexgrad/Kokoro-82M", model=self.model) for l in ("a", "b")}
        self.packs = {}

    def pack(self, lang, name):
        k = (lang, name)
        if k not in self.packs:
            self.packs[k] = self.pipes[lang].load_voice(name)
        return self.packs[k]

    def say(self, text, spec):
        lang = spec["lang"]
        voice = sum(self.pack(lang, n) * w for n, w in spec["mix"].items())
        chunks, words, t = [], [], 0.0
        for r in self.pipes[lang](text, voice=voice, speed=spec["speed"]):
            a = r.audio.detach().cpu().numpy() if hasattr(r.audio, "detach") else np.asarray(r.audio)
            for tok in (getattr(r, "tokens", None) or []):
                st, en = getattr(tok, "start_ts", None), getattr(tok, "end_ts", None)
                if st is not None and en is not None and re.search(r"\w", tok.text or ""):
                    words.append([round(t + st, 3), round(t + en, 3)])
            chunks.append(a); t += len(a) / SR
            chunks.append(np.zeros(int(0.12 * SR), np.float32)); t += 0.12
        audio = np.concatenate(chunks) if chunks else np.zeros(1, np.float32)
        return audio.astype(np.float32), words


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--stories", default="native/assets/story")
    ap.add_argument("--out", default="native/assets/story/voice")
    ap.add_argument("--only", default="")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--dry", action="store_true")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    man_path = os.path.join(a.out, "manifest.json")
    old = json.load(open(man_path)) if os.path.exists(man_path) else {"files": {}}
    manifest = {"version": 1, "model": MODEL_TAG, "rate": SR, "envFps": ENV_FPS, "cast": CAST,
                "lines": {}, "files": {}}
    todo = []
    for path in sorted(os.listdir(a.stories)):
        if not path.endswith(".json") or path == "manifest.json":
            continue
        story = json.load(open(os.path.join(a.stories, path)))
        sid = story.get("id") or path[:-5]
        if a.only and sid != a.only:
            continue
        lines_out = manifest["lines"].setdefault(sid, {})
        for nid, node in story.get("nodes", {}).items():
            lines = node.get("lines", [])
            row = []
            for i, ln in enumerate(lines):
                who = ln.get("who") or "narrator"
                text = clean_text(ln.get("text", ""))
                if who not in CAST or not re.search(r"\w", text) or ln.get("voice") == "none":
                    row.append(None); continue
                delivery = infer_delivery(node, i, lines)
                spec = voice_spec(who, delivery)
                k = key_for(who, delivery, spec, text)
                row.append(k)
                if k in old.get("files", {}) and os.path.exists(os.path.join(a.out, k + ".ogg")):
                    manifest["files"][k] = old["files"][k]
                elif k not in [t[0] for t in todo]:
                    todo.append((k, who, delivery, spec, text))
            lines_out[nid] = row
    if a.limit:
        todo = todo[: a.limit]
    print(f"{len(manifest['files'])} lines reused, {len(todo)} to synthesise", flush=True)
    if todo and not a.dry:
        syn = Synth()
        t0 = time.time(); total = 0.0
        for n, (k, who, delivery, spec, text) in enumerate(todo, 1):
            audio, words = syn.say(text, spec)
            audio, off = trim(audio)
            words = [[round(max(0.0, s - off), 3), round(max(0.0, e - off), 3)] for s, e in words]
            audio = normalise(audio, spec["gain_db"], -20.0 if who != "narrator" else -21.0)
            encode_ogg(audio, os.path.join(a.out, k + ".ogg"), spec["fx"])
            dur = round(len(audio) / SR, 3); total += dur
            manifest["files"][k] = {"who": who, "delivery": delivery, "dur": dur, "env": envelope(audio), "words": words,
                                    "text": text[:120]}
            if n % 10 == 0 or n == len(todo):
                el = time.time() - t0
                print(f"  {n}/{len(todo)}  {total:.0f}s audio in {el:.0f}s  ({total / max(el, 1e-3):.1f}x realtime)", flush=True)
    # drop files nobody references any more
    keep = set(manifest["files"])
    for f in os.listdir(a.out):
        if f.endswith(".ogg") and f[:-4] not in keep:
            os.remove(os.path.join(a.out, f))
    if not a.dry:
        tmp = man_path + ".tmp"
        json.dump(manifest, open(tmp, "w"), separators=(",", ":"))
        os.replace(tmp, man_path)
    print("manifest:", sum(len(r) for s in manifest["lines"].values() for r in s.values()), "line slots,",
          len(manifest["files"]), "voice files", flush=True)


if __name__ == "__main__":
    main()
