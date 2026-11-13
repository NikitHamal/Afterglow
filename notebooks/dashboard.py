import marimo

__generated_with = "0.25.2"
app = marimo.App(width="full", app_title="Afterglow — Motion Review Board")


@app.cell
def _():
    import json, os, sys, glob, time
    from pathlib import Path
    import marimo as mo
    for _p in (str(Path(__file__).resolve().parent) if "__file__" in globals() else "", "/work/repo/notebooks", "notebooks"):
        if _p and os.path.exists(os.path.join(_p, "dash_lib.py")) and _p not in sys.path:
            sys.path.insert(0, _p)
    import dash_lib as D
    refresh = mo.ui.refresh(options=["3s", "5s", "10s"], default_interval="3s", label="Live auto-refresh")
    ka = D.keepalive("http://127.0.0.1:2718/api/status" if os.path.isdir("/marimo") else None)
    return D, Path, glob, json, ka, mo, os, refresh, time


@app.cell
def _(D, glob, mo, os, refresh, time, ka):
    refresh.value
    sha, subject = D.sync()
    shots = D.newest_shots(8)
    strip = mo.hstack([mo.vstack([mo.image(src=p, width=230), mo.md(f"`{os.path.relpath(p, D.ROOT)}` · {time.strftime('%H:%M:%S', time.localtime(os.path.getmtime(p)))}")]) for p in shots], wrap=True) if shots else mo.md("_no shots yet_")
    n_clips = len(glob.glob(str(D.ROOT / "audit" / "**" / "metrics.json"), recursive=True))
    mo.vstack([
        mo.md(f"# Afterglow — Motion Review Board\n**main @ `{sha}`** — {subject} · keep-alive: `{ka}` · {n_clips} audited clips · refreshed {time.strftime('%H:%M:%S')}"),
        refresh,
        mo.md("### Latest shots (newest first)"),
        strip,
    ])
    return


@app.cell
def _(D, Path, mo, os):
    import anywidget, traitlets
    _pub = Path("/marimo/public") if os.path.isdir("/marimo") else D.HERE / "public"
    _pubdir, _copied = D.install_stage(_pub)

    class AfterglowStage(anywidget.AnyWidget):
        _esm = D.ROOT / "tools" / "qa" / "molab_stage" / "stage_bundle.js"
        public_base = traitlets.Unicode("/public/afterglow/" if os.path.isdir("/marimo") else "./public/afterglow/").tag(sync=True)
        notebook_id = traitlets.Unicode("").tag(sync=True)
        clip = traitlets.Unicode("stand.idle").tag(sync=True)
        capture = traitlets.Unicode("").tag(sync=True)

    stage = mo.ui.anywidget(AfterglowStage())
    mo.vstack([
        mo.md("## Interactive 3D stage — shipped room, both rigs, every baked clip\nOrbit / zoom / scrub, switch clip + speed + camera, toggle **Rig bones** to see the English skeleton (`pelvis`, `spine`, `chest`, `upper_arm.L` …)."),
        stage,
    ])
    return


@app.cell
def _(D, json, mo, os):
    sc_path = D.ROOT / "audit" / "scorecard.json"
    scorecard = json.load(open(sc_path)) if sc_path.exists() else {"baseline": {}, "current": {}}
    scenes = sorted(d.name for d in (D.ROOT / "audit").iterdir() if d.is_dir() and any(d.glob("*/sheet_3q.png")))
    clips = sorted(scorecard.get("current", {}).keys()) or ["stand.idle"]
    scene_sel = mo.ui.dropdown(options=sorted(set(scenes + ["baseline"])), value=(scenes[0] if scenes else "baseline"), label="Scene")
    clip_sel = mo.ui.dropdown(options=clips, value=clips[0], label="Clip")
    return clip_sel, clips, scene_sel, scenes, scorecard


@app.cell
def _(D, clip_sel, mo, os, scene_sel, scorecard):
    def _img(folder, name, w=330):
        p = folder / name
        return mo.image(src=str(p), width=w) if p.exists() else mo.md(f"_missing `{name}`_")

    cl, sc = clip_sel.value, scene_sel.value
    b_dir = D.ROOT / "audit" / "baseline" / cl
    c_dir = D.ROOT / "audit" / sc / cl
    if not c_dir.exists():
        c_dir = b_dir
    b, c = scorecard.get("baseline", {}).get(cl, {}), scorecard.get("current", {}).get(cl, {})
    names = ["sheet_3q.png", "sheet_side.png", "onion.png", "trace.png", "velocity.png", "compare_3q.png"]
    mo.vstack([
        mo.md(f"## Before / after review — `{sc}` → `{cl}`  ·  doll score **{b.get('doll_score', '–')} → {c.get('doll_score', '–')}**"),
        mo.hstack([scene_sel, clip_sel]),
        mo.hstack([mo.vstack([mo.md("**Baseline**")] + [_img(b_dir, n) for n in names[:5]]),
                   mo.vstack([mo.md("**Current**")] + [_img(c_dir, n) for n in names[:5]])], gap=2),
        _img(c_dir, "compare_3q.png", 700),
    ])
    return


@app.cell
def _(D, mo, os, scorecard):
    rows = []
    for k, cur in scorecard.get("current", {}).items():
        base = scorecard.get("baseline", {}).get(k, cur)
        rows.append({"clip": k, "baseline": base.get("doll_score", 0), "current": cur.get("doll_score", 0),
                     "fails": cur.get("fail_count", 0), **cur.get("fields", {})})
    rows.sort(key=lambda r: r["current"])
    rd = lambda n: (D.ROOT / n).read_text() if (D.ROOT / n).exists() else ""
    mo.ui.tabs({
        "Scorecard (weakest first)": mo.ui.table(rows, page_size=25),
        "Issue board": mo.md(rd("audit/ISSUES.md")),
        "Changelog": mo.md(rd("audit/CHANGELOG.md")),
        "Rig names (legacy → English)": mo.ui.table(D.rig_rows(), selection=None, page_size=40),
        "ANIMATION_STATUS": mo.md(rd("ANIMATION_STATUS.md")),
    })
    return


if __name__ == "__main__":
    app.run()
