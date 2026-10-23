"""Authoring helpers for Afterglow story scripts (emitted as native/assets/story/<id>.json).

Lines: N(...) narrator, V(...) Vivian, M(...) Marcus. Keyword staging (all optional):
  shot     establish | wide | two | ots | close | xclose | kiss | medium | (act poses) side | front | top | face | pov
  gesture  open | explain | point | shrug | reach | wave | chest | hips | none
  look     partner | away | down | camera        head  nod | shake | tilt | down | up
  react    the listener: smile | laugh | nod | gasp | shy | sigh | frown
  expr     {"smile": .6, "her.shy": .5, "him.pleasure": .3}   delivery  normal | soft | whisper | playful | urgent | breathless | tender
  action   kiss | embrace | step_in | part | apart | undress:<vivian|marcus|both>[:outer|under|all] | dress:<who> | to_bed | sit | stand
  vox / sfx   samples riding on the line
"""
import json, os, sys

def L(who, text, **kw):
    d = {"who": who, "text": text}
    for k, v in kw.items():
        if v is not None:
            d[k] = v
    return d

def N(text, **kw): return L("narrator", text, **kw)
def V(text, **kw): return L("vivian", text, **kw)
def M(text, **kw): return L("marcus", text, **kw)

def node(chapter, lines, **kw):
    d = {"chapter": chapter, "lines": lines}
    for k, v in kw.items():
        if v is not None:
            d[k] = v
    return d

def beat(lines):
    return {"beat": True, "lines": lines}

def C(text, nxt, cond=None, **sets):
    d = {"text": text, "next": nxt}
    if cond:
        d["if"] = cond
    if sets:
        d["set"] = {k: v for k, v in sets.items()}
    return d

def validate(s):
    errs = []
    nodes = s["nodes"]
    for c in s["chapters"]:
        if c["start"] not in nodes:
            errs.append(f"chapter {c['id']} start {c['start']} missing")
    endings = 0
    chapters = {c["id"] for c in s["chapters"]}
    for nid, n in nodes.items():
        if n.get("beat"):
            continue
        if n.get("chapter") not in chapters:
            errs.append(f"{nid}: chapter {n.get('chapter')} unknown")
        if n.get("next") and n["next"] not in nodes:
            errs.append(f"{nid}.next -> {n['next']}")
        if "play" in n:
            if n["play"]["next"] not in nodes:
                errs.append(f"{nid}.play -> {n['play']['next']}")
            for t, b in n["play"].get("beats", {}).items():
                if b not in nodes or not nodes[b].get("beat"):
                    errs.append(f"{nid}.beats.{t} -> {b} (missing or not a beat)")
        for ch in n.get("choices", []):
            if ch["next"] not in nodes:
                errs.append(f"{nid} choice -> {ch['next']}")
        if "end" in n:
            endings += 1
        if not n.get("next") and not n.get("choices") and "play" not in n and "end" not in n:
            errs.append(f"{nid} is a dead end")
        if n.get("choices") and all(c.get("if") for c in n["choices"]):
            errs.append(f"{nid}: every choice is conditional")
        for i, l in enumerate(n.get("lines", [])):
            if l.get("who") not in ("narrator", "vivian", "marcus"):
                errs.append(f"{nid}[{i}] speaker {l.get('who')}")
    # reachability from chapter 1
    seen, todo = set(), [s["chapters"][0]["start"]]
    while todo:
        k = todo.pop()
        if k in seen or k not in nodes:
            continue
        seen.add(k)
        n = nodes[k]
        for t in [n.get("next")] + [c["next"] for c in n.get("choices", [])] + ([n["play"]["next"]] if "play" in n else []):
            if t:
                todo.append(t)
        if "play" in n:
            for b in n["play"].get("beats", {}).values():
                seen.add(b)
    unreached = [k for k in nodes if k not in seen]
    if unreached:
        errs.append(f"unreachable: {unreached}")
    if endings < 3:
        errs.append(f"only {endings} endings")
    words = sum(len(l["text"].split()) for n in nodes.values() for l in n.get("lines", []))
    lines = sum(len(n.get("lines", [])) for n in nodes.values())
    return errs, endings, words, lines

def emit(story, out_dir):
    errs, endings, words, lines = validate(story)
    print(f"{story['id']}: {len(story['chapters'])} chapters, {len(story['nodes'])} nodes, {endings} endings, {lines} lines, {words} words",
          "OK" if not errs else errs)
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, story["id"] + ".json"), "w", encoding="utf-8") as f:
        json.dump(story, f, ensure_ascii=False, indent=1)
    return not errs
