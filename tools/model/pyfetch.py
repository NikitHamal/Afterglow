#!/usr/bin/env python3
"""Fast molab file fetch: talks to the marimo kernel API directly (POST /api/kernel/execute, SSE), md5-verified.
usage: pyfetch.py <remote_path> <local_path> [<remote> <local> ...]   (env MOLAB_URL, MOLAB_TOKEN_FILE)"""
import base64, hashlib, json, os, sys, urllib.request
URL = os.environ.get("MOLAB_URL", "").rstrip("/")
TOK = open(os.environ["MOLAB_TOKEN_FILE"]).read().strip()
H = {"Authorization": f"Bearer {TOK}", "User-Agent": "curl/8.5.0", "Accept": "*/*"}
def req(path, data=None, headers=None):
    h = dict(H); h.update(headers or {})
    r = urllib.request.Request(URL + path, data=data, headers=h, method="POST" if data is not None else "GET")
    return urllib.request.urlopen(r, timeout=600)
sessions = json.loads(req("/api/sessions").read())
SID = next(iter(sessions))
def run(code):
    body = json.dumps({"code": code}).encode()
    out, ev = [], None
    with req("/api/kernel/execute", body, {"Content-Type": "application/json", "Marimo-Session-Id": SID}) as r:
        for raw in r:
            line = raw.decode("utf-8", "replace").rstrip("\r\n")
            if line.startswith("event:"): ev = line[6:].strip()
            elif line.startswith("data:"):
                p = json.loads(line[5:].strip())
                if ev == "stdout": out.append(p.get("data", ""))
                elif ev == "done": break
    return "".join(out)
CH = 740000
args = sys.argv[1:]
for rem, loc in zip(args[0::2], args[1::2]):
    info = run(f"import os,hashlib;print(os.path.getsize({rem!r}), hashlib.md5(open({rem!r},'rb').read()).hexdigest())").split()
    size, md5 = int(info[0]), info[1]
    buf = bytearray()
    for off in range(0, size, CH):
        s = run(f"import base64;f=open({rem!r},'rb');f.seek({off});print('B64:'+base64.b64encode(f.read({CH})).decode())")
        buf += base64.b64decode(s.strip().split("B64:", 1)[1])
    ok = hashlib.md5(buf).hexdigest() == md5 and len(buf) == size
    if ok:
        os.makedirs(os.path.dirname(os.path.abspath(loc)), exist_ok=True); open(loc, "wb").write(buf)
    print(loc, len(buf), "/", size, "MD5-OK" if ok else "MD5-MISMATCH", flush=True)
