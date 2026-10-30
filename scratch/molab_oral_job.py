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
status('Checking oral alignment', 'Full depth, entrance and close head angles; looking for the nose-versus-mouth error.', 'oral-r1')
out = r / 'board/shots/oral-r1'
out.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, LIBGL_ALWAYS_SOFTWARE='1', GALLIUM_DRIVER='llvmpipe')
with (r / 'oral-r1.log').open('w') as log:
    result = subprocess.run(['xvfb-run', '-a', '-s', '-screen 0 1920x1080x24', './afterglow', '--shotlist', str(r / 'Afterglow/tools/qa/oral_shots.txt'), '--outdir', str(out), '--res', '1100x760'], cwd=r / 'build/bin', env=env, stdout=log, stderr=subprocess.STDOUT)
status('Oral baseline ready', f'Profile, front, top and five head angles rendered; harness exit {result.returncode}.', 'oral-r1')
'''
path = pathlib.Path('/root/oral_r1_job.py')
path.write_text(job)
p = subprocess.Popen(['python3', str(path)], start_new_session=True)
print('oral r1 pid', p.pid)
