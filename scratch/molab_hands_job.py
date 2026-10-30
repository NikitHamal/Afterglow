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
status('Correcting the oral hand pose', 'The far hand is off her face and resting on the bed; checking the near grip in profile.', 'oral-hands-r1')
with (r / 'oral-hands-build.log').open('w') as log:
    build = subprocess.run(['cmake', '--build', str(r / 'build'), '-j', '12'], cwd=r / 'Afterglow', stdout=log, stderr=subprocess.STDOUT)
if build.returncode != 0:
    status('Oral hands build failed', 'See /root/oral-hands-build.log.', 'oral-hands-r1')
    raise SystemExit(1)
out = r / 'board/shots/oral-hands-r1'
out.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, LIBGL_ALWAYS_SOFTWARE='1', GALLIUM_DRIVER='llvmpipe')
with (r / 'oral-hands.log').open('w') as log:
    result = subprocess.run(['xvfb-run', '-a', '-s', '-screen 0 1920x1080x24', './afterglow', '--shotlist', str(r / 'Afterglow/tools/qa/oral_hand_shots.txt'), '--outdir', str(out), '--res', '1100x760'], cwd=r / 'build/bin', env=env, stdout=log, stderr=subprocess.STDOUT)
status('Oral hand pass ready', f'Build {build.returncode}, render {result.returncode}.', 'oral-hands-r1')
'''
path = pathlib.Path('/root/oral_hands_job.py')
path.write_text(job)
p = subprocess.Popen(['python3', str(path)], start_new_session=True)
print('oral hands pid', p.pid)
