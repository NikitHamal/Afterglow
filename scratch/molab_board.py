from pathlib import Path
import json, time

board = Path('/root/board')
(board / 'shots').mkdir(parents=True, exist_ok=True)
(board / 'models').mkdir(parents=True, exist_ok=True)
status = board / 'status.json'
if not status.exists():
    status.write_text(json.dumps({'phase': 'Provisioning native renderer', 'detail': 'Molab connected; build and baseline kiss shots starting.', 'updated': time.time()}, indent=2))

Path('/marimo/notebook.py').write_text('''import marimo

app = marimo.App(width="full")

@app.cell
def __():
    import marimo as mo
    import json
    import time
    from pathlib import Path
    return mo, json, time, Path

@app.cell
def __(mo):
    refresh = mo.ui.refresh(interval="5s")
    mo.vstack([mo.md("# Afterglow — native visual polish"), refresh])
    return refresh,

@app.cell
def __(mo, json, time, Path, refresh):
    refresh
    root = Path("/root/board")
    status_path = root / "status.json"
    try:
        status = json.loads(status_path.read_text())
    except Exception:
        status = {"phase": "Starting", "detail": "Waiting for the renderer.", "updated": time.time()}
    age = max(0, int(time.time() - status.get("updated", time.time())))
    cards = [
        mo.md(f"## {status.get('phase', 'Working')}\\n{status.get('detail', '')}\\n\\n**Updated:** {age}s ago"),
        mo.hstack([
            mo.stat(label="Pass", value=str(status.get("pass", "baseline"))),
            mo.stat(label="Rendered shots", value=str(status.get("shots", 0))),
            mo.stat(label="Commit", value=str(status.get("commit", "8ba3173"))),
        ]),
    ]
    mo.vstack(cards)
    return root,

@app.cell
def __(mo, Path, root, refresh):
    refresh
    shots = sorted((root / "shots").glob("**/*.png"), key=lambda p: p.stat().st_mtime, reverse=True)
    cards = []
    for shot in shots[:24]:
        cards.append(mo.vstack([
            mo.md(f"**{shot.stem.replace('_', ' ')}**"),
            mo.image(src=shot.read_bytes(), width=560),
        ]))
    mo.vstack([mo.md("## Live shots"), mo.hstack(cards, widths="equal", wrap=True) if cards else mo.md("Baseline renders are being prepared.")])
    return shots,

@app.cell
def __(mo, Path, root, refresh):
    refresh
    models = sorted((root / "models").glob("*.glb"))
    items = [mo.hstack([mo.md(f"**{p.name}** · {p.stat().st_size / 1048576:.1f} MB"), mo.download(data=p.read_bytes(), filename=p.name, mimetype="model/gltf-binary")]) for p in models]
    mo.vstack([mo.md("## Current 3D models"), mo.vstack(items) if items else mo.md("Model artifacts will appear here after the first verified pose batch.")])
    return models,
''')

def heartbeat():
    import subprocess
    code = "import json,time,pathlib\np=pathlib.Path('/root/board/heartbeat')\nwhile True:\n p.write_text(str(time.time()))\n time.sleep(20)\n"
    return subprocess.Popen(['python3', '-c', code], start_new_session=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

heartbeat()
print('Live board installed; heartbeat running.')
