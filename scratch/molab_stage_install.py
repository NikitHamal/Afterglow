import marimo._code_mode as cm

SETUP_CELL = r'''# AFTERGLOW_STAGE_SETUP
import marimo as mo
import json
import time
from pathlib import Path
import shutil
import base64
import anywidget
import traitlets

refresh = mo.ui.refresh(options=["5s", "15s", "off"], default_interval="5s", label="Live visual pass")
_stage_public = Path("/marimo/public/afterglow")
_stage_public.mkdir(parents=True, exist_ok=True)
_stage_assets = [
    (Path("/root/Afterglow/native/assets/room/room.glb"), _stage_public / "room.glb"),
    (Path("/root/Afterglow/native/assets/characters/vivian.glb"), _stage_public / "vivian.glb"),
    (Path("/root/Afterglow/native/assets/characters/marcus.glb"), _stage_public / "marcus.glb"),
]
for _src, _dst in _stage_assets:
    if _src.exists() and (not _dst.exists() or _dst.stat().st_size != _src.stat().st_size):
        shutil.copyfile(_src, _dst)
_vendor = Path("/root/afterglow_stage/vendor")
if _vendor.is_dir():
    shutil.copytree(_vendor, _stage_public / "vendor", dirs_exist_ok=True)

from urllib.parse import quote
_notebook_id = quote((Path(mo.notebook_dir()) / "notebook.py").as_posix(), safe="")

class AfterglowStage(anywidget.AnyWidget):
    _esm = Path("/root/afterglow_stage/stage_bundle.js")
    public_base = traitlets.Unicode("/public/afterglow/").tag(sync=True)
    notebook_id = traitlets.Unicode(_notebook_id).tag(sync=True)
    clip = traitlets.Unicode("oral.in").tag(sync=True)
    capture = traitlets.Unicode("").tag(sync=True)

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.observe(self._save_capture, names="capture")

    def _save_capture(self, change):
        data = change["new"]
        if isinstance(data, str) and data.startswith("data:image/"):
            raw = base64.b64decode(data.split(",", 1)[1])
            Path("/root/board/stage_capture.jpg").write_bytes(raw)
'''

STAGE_CELL = r'''# AFTERGLOW_STAGE_VIEW
refresh.value
_stage_root = Path("/root/board")
try:
    _stage_status = json.loads((_stage_root / "status.json").read_text())
except Exception:
    _stage_status = {"phase": "Starting", "detail": "Waiting for the renderer.", "updated": time.time()}
_stage_age = max(0, int(time.time() - _stage_status.get("updated", time.time())))
_stage_shots = len(list((_stage_root / "shots").glob("**/*.png")))
_phase = _stage_status.get("phase", "Working")
_detail = _stage_status.get("detail", "")
_pass = _stage_status.get("pass", "baseline")
_commit = _stage_status.get("commit", "1500eb0")
stage = mo.ui.anywidget(AfterglowStage(clip="oral.in"))
mo.vstack([
    mo.md(f"# Afterglow — native visual polish\n"
          f"<span style='color:#e8b274;letter-spacing:.14em;font-size:11px;font-weight:700'>LIVE PASS · {_pass.upper()}</span>\n\n"
          f"## {_phase}\n{_detail}\n\n"
          f"<span style='color:#8f8179'>Updated {_stage_age}s ago · {_stage_shots} renders on the box · commit {_commit}</span>"),
    stage,
    mo.md("<span style='color:#8f8179;font-size:12px'>The stage reads the shipped GLBs and all 21 baked clips. Use Profile / Front / Top with Her face or Shaft tip to check contact; drag to orbit and scrub the clip below the viewport. Room can be hidden for a clean rig view.</span>"),
])
'''

GALLERY_CELL = r'''# AFTERGLOW_GALLERY
refresh.value
_gallery_root = Path("/root/board/shots")
_gallery_passes = sorted([p for p in _gallery_root.iterdir() if p.is_dir()], key=lambda p: p.stat().st_mtime)
_gallery_pass = _gallery_passes[-1] if _gallery_passes else None
_gallery_shots = sorted(_gallery_pass.glob("*.png"), key=lambda p: p.name) if _gallery_pass else []
_rows = []
for _i in range(0, len(_gallery_shots), 2):
    _pair = _gallery_shots[_i:_i + 2]
    _cards = []
    for _p in _pair:
        _cards.append(mo.vstack([
            mo.md(f"<div style='padding:4px 2px 0;color:#8f8179;font-size:10px;letter-spacing:.12em;text-transform:uppercase'>{_p.parent.name} · {_p.stem.replace('_', ' ')}</div>"),
            mo.image(src=_p.read_bytes(), width="100%"),
        ]))
    if len(_cards) == 1:
        _rows.append(mo.hstack([_cards[0]], widths=[1]))
    else:
        _rows.append(mo.hstack(_cards, widths=[1, 1]))
_archive = " · ".join(f"{p.name} ({len(list(p.glob('*.png')))})" for p in reversed(_gallery_passes[:-1][-6:])) or "No earlier passes yet."
mo.vstack([
    mo.md(f"## Latest renders · {_gallery_pass.name if _gallery_pass else 'waiting'}\n"
          f"{len(_gallery_shots)} frames in this pass. Each card is the full 1100×760 render, not a thumbnail strip."),
    mo.vstack(_rows) if _rows else mo.md("The next pass is rendering."),
    mo.md(f"<span style='color:#6f635c;font-size:11px'>Earlier: {_archive}</span>"),
])
'''

async with cm.get_context() as ctx:
    cells = list(ctx.cells.values())
    setup = next((c for c in cells if 'AFTERGLOW_BOARD_SETUP' in c.code or 'AFTERGLOW_STAGE_SETUP' in c.code), None)
    board = next((c for c in cells if 'AFTERGLOW_BOARD_DISPLAY' in c.code or 'AFTERGLOW_STAGE_VIEW' in c.code), None)
    gallery = next((c for c in cells if 'AFTERGLOW_GALLERY' in c.code), None)
    setup_id = ctx.edit_cell(setup.id, SETUP_CELL) if setup else ctx.create_cell(SETUP_CELL)
    ctx.run_cell(setup_id)
    stage_id = ctx.edit_cell(board.id, STAGE_CELL) if board else ctx.create_cell(STAGE_CELL)
    ctx.run_cell(stage_id)
    gallery_id = ctx.edit_cell(gallery.id, GALLERY_CELL) if gallery else ctx.create_cell(GALLERY_CELL)
    ctx.run_cell(gallery_id)
    print('live cells', setup_id, stage_id, gallery_id)
