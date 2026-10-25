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
status('Fitting the oral aperture', 'The glans is 42 mm across; the plain oral morph opens the front teeth only 12 mm. Comparing wider openings with the baked grip.', 'mouth-r2')
with (r / 'mouth-r2-build.log').open('w') as log:
    build = subprocess.run(['cmake', '--build', str(r / 'build'), '-j', '12'], cwd=r / 'Afterglow', stdout=log, stderr=subprocess.STDOUT)
if build.returncode != 0:
    status('Mouth build failed', 'See /root/mouth-r2-build.log.', 'mouth-r2')
    raise SystemExit(1)
out = r / 'board/shots/mouth-r2'
out.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, LIBGL_ALWAYS_SOFTWARE='1', GALLIUM_DRIVER='llvmpipe')
with (r / 'mouth-r2.log').open('w') as log:
    result = subprocess.run(['xvfb-run', '-a', '-s', '-screen 0 1920x1080x24', './afterglow', '--shotlist', str(r / 'Afterglow/tools/qa/mouth_close_shots.txt'), '--outdir', str(out), '--res', '1100x760'], cwd=r / 'build/bin', env=env, stdout=log, stderr=subprocess.STDOUT)
status('Oral aperture comparison ready', f'Open 1.0 / 1.8 / 2.2 / 2.7 rendered; build {build.returncode}, render {result.returncode}.', 'mouth-r2')
'''
path = pathlib.Path('/root/mouth_job.py')
path.write_text(job)
p = subprocess.Popen(['python3', str(path)], start_new_session=True)
print('mouth r1 pid', p.pid)
