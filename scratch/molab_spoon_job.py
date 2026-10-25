import pathlib
import subprocess
job = r'''import pathlib, subprocess, json, time, os
r = pathlib.Path('/root')
status_path = r / 'board/status.json'
def status(phase, detail, pass_name):
    d = json.loads(status_path.read_text())
    d.update(phase=phase, detail=detail, updated=time.time(), commit='pending')
    d['pass'] = pass_name
    d['shots'] = len(list((r / 'board/shots').glob('**/*.png')))
    status_path.write_text(json.dumps(d))
status('Correcting the spoon hands', 'Both of his hands are being solved onto her waist surfaces; checking the wrap and bed contact.', 'spoon-r1')
with (r / 'spoon-build.log').open('w') as log:
    build = subprocess.run(['cmake', '--build', str(r / 'build'), '-j', '12'], cwd=r / 'Afterglow', stdout=log, stderr=subprocess.STDOUT)
if build.returncode != 0:
    status('Spoon build failed', 'See /root/spoon-build.log.', 'spoon-r1'); raise SystemExit(1)
out = r / 'board/shots/spoon-r1'; out.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, LIBGL_ALWAYS_SOFTWARE='1', GALLIUM_DRIVER='llvmpipe')
with (r / 'spoon.log').open('w') as log:
    result = subprocess.run(['xvfb-run', '-a', '-s', '-screen 0 1920x1080x24', './afterglow', '--shotlist', str(r / 'Afterglow/tools/qa/spoon_shots.txt'), '--outdir', str(out), '--res', '1100x760'], cwd=r / 'build/bin', env=env, stdout=log, stderr=subprocess.STDOUT)
status('Spoon hand pass ready', f'Build {build.returncode}, render {result.returncode}.', 'spoon-r1')
'''
path = pathlib.Path('/root/spoon_job.py'); path.write_text(job)
p = subprocess.Popen(['python3', str(path)], start_new_session=True)
print('spoon pid', p.pid)
