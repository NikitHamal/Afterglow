"""Helpers for the Afterglow review dashboard (notebooks/dashboard.py). Runs locally and on the MoLab box."""
import glob, os, shutil, subprocess, threading, time, urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent


def find_root():
    for c in (os.environ.get("AG_ROOT"), "/work/repo", str(HERE.parent)):
        if c and (Path(c) / "native" / "assets").is_dir():
            return Path(c)
    return HERE.parent


ROOT = find_root()
_last = {"t": 0.0, "msg": ""}


def git(*args, timeout=90):
    r = subprocess.run(["git", "-C", str(ROOT), *args], capture_output=True, text=True, timeout=timeout)
    return (r.stdout + r.stderr).strip()


def sync(min_interval=30):
    """git pull --ff-only, throttled; returns (short sha, subject)."""
    if time.time() - _last["t"] > min_interval:
        _last["t"] = time.time()
        _last["msg"] = git("pull", "-q", "--ff-only", "origin", "main") if (ROOT / ".git").exists() else "no git"
    return git("log", "-1", "--format=%h|%s").split("|", 1)


def keepalive(port_url=None, every=60):
    """Daemon thread: pings the notebook server and pulls the repo once a minute (idempotent)."""
    if any(t.name == "ag-keepalive" for t in threading.enumerate()):
        return "running"

    def loop():
        while True:
            try:
                sync(every - 1)
                if port_url:
                    urllib.request.urlopen(port_url, timeout=10).read(64)
            except Exception:
                pass
            time.sleep(every)

    threading.Thread(target=loop, name="ag-keepalive", daemon=True).start()
    return "started"


def install_stage(public_dir):
    """Copy the shipped room + character GLBs next to the notebook so the 3D widget can fetch them."""
    pub = Path(public_dir) / "afterglow"
    pub.mkdir(parents=True, exist_ok=True)
    n = 0
    for rel, name in (("room/room.glb", "room.glb"), ("characters/vivian.glb", "vivian.glb"), ("characters/marcus.glb", "marcus.glb")):
        src, dst = ROOT / "native" / "assets" / rel, pub / name
        if src.exists() and (not dst.exists() or dst.stat().st_size != src.stat().st_size):
            shutil.copyfile(src, dst); n += 1
    return pub, n


def newest_shots(n=8, exts=("png", "jpg")):
    files = []
    for d in ("audit", "shots", "renders"):
        for e in exts:
            files += glob.glob(str(ROOT / d / "**" / f"*.{e}"), recursive=True)
    files.sort(key=os.path.getmtime, reverse=True)
    return files[:n]


def rig_rows():
    import importlib.util
    spec = importlib.util.spec_from_file_location("rename_rig", ROOT / "tools" / "model" / "rename_rig.py")
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    return [{"legacy (MMD / Japanese)": k, "English rig name": v} for k, v in m.MAP.items()]
