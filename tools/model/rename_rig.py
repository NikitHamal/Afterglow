#!/usr/bin/env python3
"""One-shot migration of the MMD (Japanese) rig bone names to English.

Idempotent: running it again on migrated files changes nothing.
  python3 tools/model/rename_rig.py [--check]     (--check lists leftovers, writes nothing)
Text sources are rewritten in place; GLBs get their glTF JSON chunk rewritten (binary chunk untouched).
"""
import json, os, re, struct, sys

# Longest key first is enforced below. Full-width digits become ASCII.
MAP = {
    "全ての親": "root", "センター": "center", "グルーブ": "groove", "腰": "waist",
    "下半身": "pelvis", "上半身2": "chest", "上半身": "spine", "首": "neck", "頭": "head",
    "肩": "shoulder", "腕": "upper_arm", "ひじ": "elbow", "手首": "wrist",
    "足首": "ankle", "足": "thigh", "ひざ": "knee", "つま先": "toe", "両目": "eyes", "目": "eye",
    "髪": "hair", "親指": "thumb", "人指": "index", "中指": "middle", "薬指": "ring", "小指": "pinky",
    "乳親": "breast", "乳": "breast_tip",
    "竿先": "shaft_tip", "竿根": "shaft_base", "竿端": "shaft_end", "竿中": "shaft_mid", "竿": "shaft",
    "玉袋": "scrotum",
}
FW = {ord(c): str(i) for i, c in enumerate("０１２３４５６７８９")}
RX = re.compile("|".join(re.escape(k) for k in sorted(MAP, key=len, reverse=True)))
CJK = re.compile(r"[\u3040-\u30ff\u4e00-\u9fff\uff10-\uff19]")
SKIP_DIRS = {"Shinlalala's Lewd Sound Pack", ".git", "node_modules", "__pycache__", "build", "dist"}
SKIP_FILES = {"rename_rig.py"}
TEXT_EXT = {".py", ".cpp", ".h", ".hpp", ".c", ".js", ".mjs", ".json", ".md", ".txt", ".yml", ".yaml", ".cmake",
            ".sh", ".ts", ".tsx", ".html", ".gradle", ".kt", ".java", ".glsl", ".vert", ".frag", ".fs", ".vs", ""}


def tr(s):
    return RX.sub(lambda m: MAP[m.group(0)], s.translate(FW))


def patch_glb(p, check):
    b = open(p, "rb").read()
    if b[:4] != b"glTF":
        return False
    ln, typ = struct.unpack_from("<II", b, 12)
    j = json.loads(b[20:20 + ln].decode("utf-8"))
    if not CJK.search(json.dumps(j, ensure_ascii=False)):
        return False
    if check:
        return True
    j = json.loads(tr(json.dumps(j, ensure_ascii=False)))
    new = json.dumps(j, ensure_ascii=False, separators=(",", ":"))
    names = [n.get("name") for n in j.get("nodes", []) if n.get("name")]
    assert len(names) == len(set(names)), f"{p}: node names collide after rename"
    raw = new.encode("utf-8")
    raw += b" " * ((4 - len(raw) % 4) % 4)
    rest = b[20 + ln:]
    total = 12 + 8 + len(raw) + len(rest)
    out = b[:4] + struct.pack("<II", b[4:8] and 2, total) + struct.pack("<II", len(raw), typ) + raw + rest
    open(p, "wb").write(out)
    return True


def main():
    check = "--check" in sys.argv
    root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    hits = []
    for dp, dn, fn in os.walk(root):
        dn[:] = [d for d in dn if d not in SKIP_DIRS]
        for f in fn:
            p = os.path.join(dp, f)
            ext = os.path.splitext(f)[1].lower()
            if f in SKIP_FILES:
                continue
            if ext == ".glb":
                if patch_glb(p, check):
                    hits.append(p)
                continue
            if ext not in TEXT_EXT:
                continue
            try:
                s = open(p, encoding="utf-8").read()
            except Exception:
                continue
            if CJK.search(s):
                hits.append(p)
                if not check:
                    open(p, "w", encoding="utf-8", newline="").write(tr(s))
    for h in hits:
        print(("LEFT " if check else "patched ") + os.path.relpath(h, root))


if __name__ == "__main__":
    main()
