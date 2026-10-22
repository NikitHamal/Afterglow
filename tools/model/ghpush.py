#!/app/py/.venv/bin/python
"""Push the working-tree diff of a local clone to a GitHub branch through the platform's GitHub connector (Git Data API).
usage: ghpush.py <repo_dir> <owner/repo> <branch> <message-file>
Calls the connector endpoint in-process (no argv size limits), so large binaries (GLB) work."""
import base64, json, os, subprocess, sys
from actl.api import ApiClient, config_from_env
repo_dir, slug, branch, msgfile = sys.argv[1:5]
API = f"https://api.github.com/repos/{slug}"
client = ApiClient(config_from_env(os.environ))
def call(method, url, body=None):
    payload = {"app": "github", "url": url, "method": method}
    if body is not None: payload["body"] = json.dumps(body)
    resp = client.request("POST", "/api/agent/connector", json_body=payload, timeout=600)
    env = json.loads(resp.body.decode("utf-8"))
    if not env.get("ok"): sys.exit(f"{method} {url} -> {env.get('status')}: {str(env.get('body'))[:400]}")
    b = env["body"]; return json.loads(b) if isinstance(b, str) else b
def git(*a): return subprocess.run(["git", "-C", repo_dir, *a], capture_output=True, text=True, check=True).stdout
head = call("GET", f"{API}/git/ref/heads/{branch}")["object"]["sha"]
local_base = git("rev-parse", "HEAD").strip()
if head != local_base:
    sys.exit(f"remote {branch} is at {head}, local base {local_base}: fetch/rebase first")
base_tree = call("GET", f"{API}/git/commits/{head}")["tree"]["sha"]
git("add", "-A")
changes = [l.split("\t") for l in git("diff", "--cached", "--name-status", "--no-renames", "HEAD").splitlines()]
tree = []
for st, path in changes:
    if st == "D":
        tree.append({"path": path, "mode": "100644", "type": "blob", "sha": None}); continue
    data = open(f"{repo_dir}/{path}", "rb").read()
    blob = call("POST", f"{API}/git/blobs", {"content": base64.b64encode(data).decode(), "encoding": "base64"})
    mode = "100755" if os.access(f"{repo_dir}/{path}", os.X_OK) else "100644"
    tree.append({"path": path, "mode": mode, "type": "blob", "sha": blob["sha"]})
    print("blob", st, path, f"{len(data)/1e6:.2f} MB" if len(data) > 1e6 else "", flush=True)
new_tree = call("POST", f"{API}/git/trees", {"base_tree": base_tree, "tree": tree})["sha"]
msg = open(msgfile).read()
commit = call("POST", f"{API}/git/commits", {"message": msg, "tree": new_tree, "parents": [head]})["sha"]
call("PATCH", f"{API}/git/refs/heads/{branch}", {"sha": commit, "force": False})
git("fetch", "-q", "origin", branch)
git("reset", "-q", "--soft", f"origin/{branch}")
print("pushed", commit, "clean" if not git("status", "--porcelain").strip() else "local differs from pushed tree!")
