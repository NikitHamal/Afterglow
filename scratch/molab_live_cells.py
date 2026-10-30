import marimo._code_mode as cm

SETUP_CELL = r'''# AFTERGLOW_BOARD_SETUP
import marimo as mo
import json
import time
from pathlib import Path
refresh = mo.ui.refresh(options=["5s", "15s", "off"], default_interval="5s", label="Live visual pass")
refresh
'''

BOARD_CELL = r'''# AFTERGLOW_BOARD_DISPLAY
refresh.value
root = Path("/root/board")
try:
    status = json.loads((root / "status.json").read_text())
except Exception:
    status = {"phase": "Starting", "detail": "Waiting for the renderer.", "updated": time.time()}
age = max(0, int(time.time() - status.get("updated", time.time())))
shots = sorted((root / "shots").glob("**/*.png"), key=lambda p: p.stat().st_mtime, reverse=True)
models = sorted((root / "models").glob("*.glb"))
shot_cards = [
    mo.vstack([mo.md(f"**{p.parent.name} / {p.stem.replace('_', ' ')}**"), mo.image(src=p.read_bytes(), width=560)])
    for p in shots[:18]
]
model_cards = [
    mo.hstack([
        mo.md(f"**{p.name}** · {p.stat().st_size / 1048576:.1f} MB"),
        mo.download(data=p.read_bytes(), filename=p.name, mimetype="model/gltf-binary"),
    ])
    for p in models
]
mo.vstack([
    mo.md("# Afterglow — native visual polish"),
    mo.md(f"## {status.get('phase', 'Working')}\n{status.get('detail', '')}\n\n**Updated:** {age}s ago · auto-refresh every 5s"),
    mo.hstack([
        mo.stat(label="Pass", value=str(status.get("pass", "baseline"))),
        mo.stat(label="Rendered shots", value=str(len(shots))),
        mo.stat(label="Commit", value=str(status.get("commit", "8ba3173"))),
    ]),
    mo.md("## Live shots"),
    mo.hstack(shot_cards, widths="equal", wrap=True) if shot_cards else mo.md("Baseline renders are being prepared."),
    mo.md("## Current 3D models"),
    mo.vstack(model_cards) if model_cards else mo.md("Model artifacts will appear here after the first verified pose batch."),
])
'''

async with cm.get_context() as ctx:
    cells = list(ctx.cells.values())
    setup = next((c for c in cells if 'AFTERGLOW_BOARD_SETUP' in c.code), None)
    board = next((c for c in cells if 'AFTERGLOW_BOARD_DISPLAY' in c.code), None)
    setup_id = ctx.edit_cell(setup.id, SETUP_CELL) if setup else ctx.edit_cell(next(c for c in cells if not c.code.strip() or 'refresh = mo.ui.refresh' in c.code).id, SETUP_CELL)
    ctx.run_cell(setup_id)
    board_id = ctx.edit_cell(board.id, BOARD_CELL) if board else ctx.create_cell(BOARD_CELL)
    ctx.run_cell(board_id)
    print('live cells', setup_id, board_id)
