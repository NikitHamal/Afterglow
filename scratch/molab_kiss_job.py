import pathlib
import subprocess

root = pathlib.Path('/root')
job = r'''import pathlib, subprocess, json, time, os
r = pathlib.Path('/root')
status_path = r / 'board/status.json'
def status(phase, detail, pass_name):
    d = json.loads(status_path.read_text())
    d.update(phase=phase, detail=detail, updated=time.time())
    d['pass'] = pass_name
    d['shots'] = len(list((r / 'board/shots').glob('**/*.png')))
    status_path.write_text(json.dumps(d))
status('Rendering every story kiss and embrace', 'All five stories, six orbit angles and the approach, using the stable revision 13 solver.', 'kiss-r15')
with (r / 'rebuild.log').open('w') as log:
    subprocess.run(['cmake', '--build', str(r / 'build'), '-j', '12'], cwd=r / 'Afterglow', stdout=log, stderr=subprocess.STDOUT, check=True)
out = r / 'board/shots/kiss-r15'
out.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, LIBGL_ALWAYS_SOFTWARE='1', GALLIUM_DRIVER='llvmpipe')
with (r / 'kiss-r15.log').open('w') as log:
    result = subprocess.run(['xvfb-run', '-a', '-s', '-screen 0 1920x1080x24', './afterglow', '--shotlist', str(r / 'Afterglow/tools/qa/kiss_shots.txt'), '--outdir', str(out), '--res', '1100x760'], cwd=r / 'build/bin', env=env, stdout=log, stderr=subprocess.STDOUT)
status('Full kiss and embrace set ready', f'All-story orbit comparison rendered; harness exit {result.returncode}.', 'kiss-r15')
'''
path = root / 'kiss_r1.py'
path.write_text(job)
p = subprocess.Popen(['python3', str(path)], start_new_session=True)
print('kiss r1 pid', p.pid)
