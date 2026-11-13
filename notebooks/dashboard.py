import marimo

__generated_with = "0.9.0"
app = marimo.App(width="full", app_title="Afterglow — Kill The Doll Motion Audit")

@app.cell
def __():
    import os, glob, json, subprocess
    import marimo as mo
    refresh = mo.ui.refresh(options=["3s", "5s", "10s"], default_interval="3s", label="Live Auto-Refresh")
    scenes = ["the_massage", "the_storm", "old_flames", "neighbours", "salt_and_sun", "baseline"]
    scorecard_path = "audit/scorecard.json"
    scorecard = json.load(open(scorecard_path)) if os.path.exists(scorecard_path) else {"baseline": {}, "current": {}}
    clips = sorted(set(list(scorecard.get("current", {}).keys()) + ["stand.idle", "stand.embrace", "kiss.stand", "sit.bed", "kiss.sit"]))
    scene_sel = mo.ui.dropdown(options=scenes, value="the_massage", label="Scene")
    clip_sel = mo.ui.dropdown(options=clips, value="stand.idle", label="Clip")
    work_files = sorted(glob.glob("/work/**/*.glb", recursive=True) + glob.glob("native/assets/characters/*.glb") + glob.glob("/work/**/*.blend", recursive=True))
    file_sel = mo.ui.dropdown(options=work_files or ["native/assets/characters/vivian.glb"], value=(work_files[0] if work_files else "native/assets/characters/vivian.glb"), label="Load /work Asset")
    return clips, clip_sel, file_sel, glob, json, mo, os, refresh, scene_sel, scenes, scorecard, subprocess, work_files

@app.cell
def __(clip_sel, file_sel, json, mo, os, refresh, scene_sel, scorecard):
    _ = refresh.value
    sc = scene_sel.value
    cl = clip_sel.value
    b_dir = os.path.join("audit", "baseline", cl)
    c_dir = os.path.join("audit", sc, cl) if os.path.exists(os.path.join("audit", sc, cl)) else b_dir
    b_met = scorecard.get("baseline", {}).get(cl, {})
    c_met = scorecard.get("current", {}).get(cl, b_met)
    b_score = b_met.get("doll_score", 0.0)
    c_score = c_met.get("doll_score", b_score)
    delta = round(c_score - b_score, 1)
    arrow = f"▲ +{delta}" if delta > 0 else (f"▼ {delta}" if delta < 0 else "● 0.0")

    def _img(folder, name):
        p = os.path.join(folder, name)
        return mo.image(src=p, width=340) if os.path.exists(p) else mo.md(f"*Missing `{p}`*")

    card1 = mo.vstack([
        mo.md(f"## Card 1 — LIVE REVIEW PLAYER (`{sc}` → `{cl}`) | Score: **{c_score}** ({arrow} vs baseline {b_score})"),
        mo.hstack([refresh, scene_sel, clip_sel]),
        mo.hstack([
            mo.vstack([mo.md("**Baseline 3/4 & Side**"), _img(b_dir, "sheet_3q.png"), _img(b_dir, "sheet_side.png"), _img(b_dir, "onion.png"), _img(b_dir, "trace.png"), _img(b_dir, "velocity.png")]),
            mo.vstack([mo.md("**Current 3/4 & Side**"), _img(c_dir, "sheet_3q.png"), _img(c_dir, "sheet_side.png"), _img(c_dir, "onion.png"), _img(c_dir, "trace.png"), _img(c_dir, "velocity.png")]),
        ]),
    ])

    rows = []
    for k, cur in scorecard.get("current", {}).items():
        base = scorecard.get("baseline", {}).get(k, cur)
        d = round(cur.get("doll_score", 0.0) - base.get("doll_score", 0.0), 1)
        row = {"clip": k, "doll_score": cur.get("doll_score", 0.0), "delta": d, "fails": cur.get("fail_count", 0)}
        row.update(cur.get("fields", {}))
        rows.append(row)
    rows.sort(key=lambda r: r["delta"])
    card2 = mo.vstack([mo.md("## Card 2 — DOLL-FACTOR SCORECARD"), mo.ui.table(rows)])

    issues_md = open("audit/ISSUES.md").read() if os.path.exists("audit/ISSUES.md") else "No issues."
    card3 = mo.vstack([mo.md("## Card 3 — ISSUE BOARD"), mo.md(issues_md)])

    scene_chips = []
    for s_id in ["the_massage", "the_storm", "old_flames", "neighbours", "salt_and_sun"]:
        s_clips = glob.glob(f"audit/{s_id}/*/metrics.json") if "glob" in dir() else []
        scene_chips.append(f"- **`{s_id}`**: {len(s_clips)} clips audited")
    card4 = mo.vstack([mo.md("## Card 4 — SCENE BROWSER"), mo.md("\n".join(scene_chips))])

    card5 = mo.vstack([
        mo.md("## Card 5 — LIVE PLAYGROUND (`/work/` & Repo Asset Loader)"),
        file_sel,
        mo.md(f"Selected asset: `{file_sel.value}` | Active clip scorecard: `{json.dumps(c_met.get('fields', {}))}`"),
    ])

    card6 = mo.vstack([
        mo.md("## Card 6 — BEFORE / AFTER SCENES"),
        mo.md(open("ANIMATION_STATUS.md").read() if os.path.exists("ANIMATION_STATUS.md") else ""),
    ])
    return mo.vstack([card1, card2, card3, card4, card5, card6])

if __name__ == "__main__":
    app.run()
