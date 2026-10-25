import base64
import os
from pathlib import Path
import subprocess
import sys
import tempfile

local = Path(sys.argv[1])
remote = sys.argv[2]
payload = base64.b64encode(local.read_bytes()).decode()
code = f"import base64,pathlib; p=pathlib.Path({remote!r}); p.parent.mkdir(parents=True,exist_ok=True); p.write_bytes(base64.b64decode({payload!r})); print(p, p.stat().st_size)"
cmd = [
    "uvx", "marimo@latest", "pair", "execute",
    "--url", "https://sb-6ea0024e3b1d87b6.sb.molab.run/",
    "--session", "sess-8rak9k1tkenb63vy",
    "--token-file", str(Path.home() / ".molab_token"),
]
with tempfile.NamedTemporaryFile("w", suffix=".py", delete=False, encoding="utf-8") as f:
    f.write(code)
    code_file = Path(f.name)
try:
    subprocess.run(cmd + ["--code-file", str(code_file)], check=True)
finally:
    code_file.unlink()
