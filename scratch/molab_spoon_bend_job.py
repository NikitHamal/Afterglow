import pathlib, subprocess
job = r'''import pathlib, subprocess, json, time, os
r = pathlib.Path('/root'); status_path = r / 'board/status.json'
def status(phase, detail, pass_name):
    d = json.loads(status_path.read_text()); d.update(phase=phase, detail=detail, updated=time.time(), commit='pending'); d['pass']=pass_name; d['shots']=len(list((r/'board/shots').glob('**/*.png'))); status_path.write_text(json.dumps(d))
status('Curling the spoon upper body', 'Testing 0 / 0.18 / 0.34 rad forward curl while his hands solve onto her waist.', 'spoon-bend-r1')
with (r/'spoon-bend-build.log').open('w') as log:
    build=subprocess.run(['cmake','--build',str(r/'build'),'-j','12'],cwd=r/'Afterglow',stdout=log,stderr=subprocess.STDOUT)
if build.returncode!=0:
    status('Spoon bend build failed','See /root/spoon-bend-build.log.','spoon-bend-r1'); raise SystemExit(1)
out=r/'board/shots/spoon-bend-r1'; out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,LIBGL_ALWAYS_SOFTWARE='1',GALLIUM_DRIVER='llvmpipe')
with (r/'spoon-bend.log').open('w') as log:
    result=subprocess.run(['xvfb-run','-a','-s','-screen 0 1920x1080x24','./afterglow','--shotlist',str(r/'Afterglow/tools/qa/spoon_bend_shots.txt'),'--outdir',str(out),'--res','1100x760'],cwd=r/'build/bin',env=env,stdout=log,stderr=subprocess.STDOUT)
status('Spoon curl comparison ready',f'Build {build.returncode}, render {result.returncode}.','spoon-bend-r1')
'''
path=pathlib.Path('/root/spoon_bend_job.py'); path.write_text(job)
p=subprocess.Popen(['python3',str(path)],start_new_session=True); print('spoon bend pid',p.pid)
