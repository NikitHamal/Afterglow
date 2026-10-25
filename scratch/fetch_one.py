import base64, json, subprocess, sys, tempfile, time
from pathlib import Path
remote, local = sys.argv[1], sys.argv[2]
code = f"""
import base64, subprocess, pathlib
src = pathlib.Path({remote!r})
tmp = pathlib.Path('/tmp/_fetch.jpg')
subprocess.run(['ffmpeg','-y','-loglevel','error','-i',str(src),'-vf','scale=1100:-2','-q:v','3',str(tmp)], check=True)
print('@@'+base64.b64encode(tmp.read_bytes()).decode()+'@@')
"""
for attempt in range(3):
    with tempfile.NamedTemporaryFile('w', suffix='.py', delete=False, encoding='utf-8') as f:
        f.write(code); p=Path(f.name)
    r=subprocess.run(['uvx','marimo@latest','pair','execute','--url','https://sb-6ea0024e3b1d87b6.sb.molab.run/','--session','sess-8rak9k1tkenb63vy','--token-file',str(Path.home()/'.molab_token'),'--code-file',str(p)],capture_output=True,text=True)
    p.unlink()
    if r.returncode==0:
        try:
            j=json.loads(r.stdout); s=j.get('stdout') or ''
            b=s.split('@@',2)[1]
        except Exception as e:
            print('parse fail', str(e), r.stdout[:200]); time.sleep(2); continue
        Path(local).write_bytes(base64.b64decode(b))
        print(local, Path(local).stat().st_size, 'ok')
        break
    else:
        print('attempt', attempt, 'rc', r.returncode, 'err', r.stderr[:200], 'out', r.stdout[:200]); time.sleep(2)
else:
    raise SystemExit(1)
