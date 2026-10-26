"""Talk to the live Molab sandbox: push a local file, run code, or fetch a remote file.

The sandbox dies after ~12h or ~10min idle, so the URL/session pair changes between runs.
Update URL/SESSION here (or pass --url/--session) whenever `pair notebook list` reports a new one.
Token lives in ~/.molab_token and is never printed.
"""
import argparse
import base64
import json
import subprocess
import sys
import tempfile
from pathlib import Path

URL = "https://sb-d8a897d382a3c6a6.sb.molab.run/"
SESSION = "sess-f30t0v90z095tfyj"


def execute(code: str, url: str = URL, session: str = SESSION, timeout: int = 600) -> dict:
    with tempfile.NamedTemporaryFile("w", suffix=".py", delete=False, encoding="utf-8") as f:
        f.write(code)
        path = Path(f.name)
    try:
        r = subprocess.run(
            ["uvx", "marimo@latest", "pair", "execute", "--url", url, "--session", session,
             "--token-file", str(Path.home() / ".molab_token"), "--code-file", str(path)],
            capture_output=True, text=True, timeout=timeout)
    finally:
        path.unlink()
    try:
        return json.loads(r.stdout)
    except Exception:
        return {"success": False, "stdout": r.stdout, "stderr": r.stderr, "rc": r.returncode}


def _remote_path(remote: str) -> str:
    # Git Bash rewrites a leading "/root/..." argv into "<msys-prefix>/root/..."; undo that so a forgotten
    # MSYS_NO_PATHCONV cannot scatter junk trees on the box.
    if not remote.startswith("/") and "/root/" in remote:
        return "/root/" + remote.split("/root/", 1)[1].lstrip("/")
    return remote


def push(local: str, remote: str, url: str = URL, session: str = SESSION) -> bool:
    remote = _remote_path(remote)
    payload = base64.b64encode(Path(local).read_bytes()).decode()
    code = (f"import base64,pathlib; p=pathlib.Path({remote!r}); p.parent.mkdir(parents=True,exist_ok=True); "
            f"p.write_bytes(base64.b64decode({payload!r})); print(p, p.stat().st_size)")
    out = execute(code, url, session)
    ok = bool(out.get("success"))
    print(("pushed  " if ok else "FAILED  ") + local + " -> " + remote, (out.get("stdout") or out.get("stderr") or "")[:200])
    return ok


def fetch(remote: str, local: str, url: str = URL, session: str = SESSION, chunk: int = 600_000) -> bool:
    # pair execute caps what comes back, so pull the file in byte ranges and stitch them here
    remote = _remote_path(remote)
    size = execute(f"import pathlib; print(pathlib.Path({remote!r}).stat().st_size)", url, session).get("stdout", "").strip()
    if not size.isdigit():
        print("FAILED fetch", remote, "(missing?)")
        return False
    total = int(size)
    buf = bytearray()
    for off in range(0, total, chunk):
        code = (f"import base64,pathlib; d=pathlib.Path({remote!r}).read_bytes()[{off}:{off + chunk}]; "
                f"print('@@'+base64.b64encode(d).decode()+'@@')")
        s = execute(code, url, session).get("stdout") or ""
        if "@@" not in s:
            print("FAILED fetch", remote, "at", off)
            return False
        buf += base64.b64decode(s.split("@@", 2)[1])
    Path(local).parent.mkdir(parents=True, exist_ok=True)
    Path(local).write_bytes(bytes(buf))
    print("fetched ", remote, "->", local, len(buf), "/", total)
    return len(buf) == total


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=["run", "push", "fetch", "ping"])
    ap.add_argument("a", nargs="?")
    ap.add_argument("b", nargs="?")
    ap.add_argument("--url", default=URL)
    ap.add_argument("--session", default=SESSION)
    args = ap.parse_args()
    if args.mode == "ping":
        print(json.dumps(execute("import socket; print(socket.gethostname())", args.url, args.session), indent=1)[:600])
    elif args.mode == "run":
        print(json.dumps(execute(Path(args.a).read_text(), args.url, args.session), indent=1)[:4000])
    elif args.mode == "push":
        sys.exit(0 if push(args.a, args.b, args.url, args.session) else 1)
    else:
        sys.exit(0 if fetch(args.a, args.b, args.url, args.session) else 1)
