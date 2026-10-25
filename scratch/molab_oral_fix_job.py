import pathlib
import subprocess

job = r'''import pathlib, subprocess, json, time, os
r = pathlib.Path('/root')
status_path = r / 'board/status.json'
def status(phase, detail, pass_name):
    d = json.loads(status_path.read_text())
    d.update(phase=phase, detail=detail, updated=time.time(), commit='1500eb0')
    d['pass'] = pass_name
    d['shots'] = len(list((r / 'board/shots').glob('**/*.png')))
    status_path.write_text(json.dumps(d))
status('Correcting oral contact', 'Mouth on the shaft axis after the procedural head pass; checking side, FPV, entry, face and tip.', 'oral-r2')
with (r / 'oral-r2-build.log').open('w') as log:
    build = subprocess.run(['cmake', '--build', str(r / 'build'), '-j', '12'], cwd=r / 'Afterglow', stdout=log, stderr=subprocess.STDOUT)
if build.returncode != 0:
    status('Oral build failed', 'See /root/oral-r2-build.log.', 'oral-r2')
    raise SystemExit(1)
out = r / 'board/shots/oral-r2'
out.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, LIBGL_ALWAYS_SOFTWARE='1', GALLIUM_DRIVER='llvmpipe')
with (r / 'oral-r2.log').open('w') as log:
    result = subprocess.run(['xvfb-run', '-a', '-s', '-screen 0 1920x1080x24', './afterglow', '--shotlist', str(r / 'Afterglow/tools/qa/oral_fix_shots.txt'), '--outdir', str(out), '--res', '1100x760'], cwd=r / 'build/bin', env=env, stdout=log, stderr=subprocess.STDOUT)
status('Oral contact pass ready', f'Build {build.returncode}, render {result.returncode}; side and FPV measured.', 'oral-r2')
'''
path = pathlib.Path('/root/oral_fix_job.py')
path.write_text(job)
p = subprocess.Popen(['python3', str(path)], start_new_session=True)
print('oral fix pid', p.pid)
