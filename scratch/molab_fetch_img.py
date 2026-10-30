import base64, subprocess, sys, tempfile
from pathlib import Path

def fetch(remote, local):
    code = f"""
import base64, subprocess, pathlib, tempfile
src = pathlib.Path({remote!r})
tmp = pathlib.Path('/tmp/_fetch.jpg')
subprocess.run(['ffmpeg','-y','-loglevel','error','-i',str(src),'-vf','scale=1100:-2','-q:v','3',str(tmp)], check=True)
print(base64.b64encode(tmp.read_bytes()).decode())
"""
    with tempfile.NamedTemporaryFile('w', suffix='.py', delete=False, encoding='utf-8') as f:
        f.write(code); p = Path(f.name)
    r = subprocess.run(['uvx','marimo@latest','pair','execute','--url','https://sb-6ea0024e3b1d87b6.sb.molab.run/','--session','sess-8rak9k1tkenb63vy','--token-file',str(Path.home()/'.molab_token'),'--code-file',str(p)],capture_output=True,text=True,check=True)
    p.unlink()
    out = r.stdout
    # find the JSON, take stdout, base64-decode
    import json
    try:
        j = json.loads(out[out.index('{'):out.rindex('}')+1])
        b64 = j.get('stdout','').strip()
        if not b64:
            print('FETCH FAIL', out[:500]); return False
        Path(local).write_bytes(base64.b64decode(b64))
        print(local, Path(local).stat().st_size)
        return True
    except Exception as e:
        print('ERR', e, out[:500]); return False

if __name__ == '__main__':
    fetch(sys.argv[1], sys.argv[2])
