import pathlib
import subprocess

job = r'''import pathlib, subprocess, json, time, os
r = pathlib.Path('/root')
status_path = r / 'board/status.json'
def status(phase, detail, pass_name):
    d = json.loads(status_path.read_text())
    d.update(phase=phase, detail=detail, updated=time.time(), commit='ba4b339')
    d['pass'] = pass_name
    d['shots'] = len(list((r / 'board/shots').glob('**/*.png')))
    status_path.write_text(json.dumps(d))
status('Reviewing hands, titjob and spoon', 'Multi-angle renders of the next reported contacts; no code change yet.', 'contact-r1')
out = r / 'board/shots/contact-r1'
out.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, LIBGL_ALWAYS_SOFTWARE='1', GALLIUM_DRIVER='llvmpipe')
with (r / 'contact-r1.log').open('w') as log:
    result = subprocess.run(['xvfb-run', '-a', '-s', '-screen 0 1920x1080x24', './afterglow', '--shotlist', str(r / 'Afterglow/tools/qa/contact_shots.txt'), '--outdir', str(out), '--res', '1100x760'], cwd=r / 'build/bin', env=env, stdout=log, stderr=subprocess.STDOUT)
status('Contact review ready', f'Titjob, spoon and hand views rendered; harness exit {result.returncode}.', 'contact-r1')
'''
path = pathlib.Path('/root/contact_job.py')
path.write_text(job)
p = subprocess.Popen(['python3', str(path)], start_new_session=True)
print('contact r1 pid', p.pid)
