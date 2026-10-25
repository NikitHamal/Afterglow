import pathlib, subprocess
job=r'''import pathlib, subprocess, json, time, os
r=pathlib.Path('/root');sp=r/'board/status.json'
def status(p,d,n='user-fix2'):
 x=json.loads(sp.read_text());x.update(phase=p,detail=d,updated=time.time(),commit='local');x['pass']=n;sp.write_text(json.dumps(x))
status('Deepening the lip target and clearing the grip','Testing 20 mm base insertion, wider max-girth mouth and hands placed off the shaft surface.')
with (r/'user-fix2-build.log').open('w') as log: b=subprocess.run(['cmake','--build',str(r/'build'),'-j','12'],cwd=r/'Afterglow',stdout=log,stderr=subprocess.STDOUT)
if b.returncode: status('Fix2 build failed','See /root/user-fix2-build.log.','user-fix2');raise SystemExit(1)
out=r/'board/_experiments/user-fix2-r1';out.mkdir(parents=True,exist_ok=True);env=dict(os.environ,LIBGL_ALWAYS_SOFTWARE='1',GALLIUM_DRIVER='llvmpipe')
with (r/'user-fix2.log').open('w') as log: res=subprocess.run(['xvfb-run','-a','-s','-screen 0 1920x1080x24','./afterglow','--shotlist',str(r/'Afterglow/tools/qa/user_fix2_shots.txt'),'--outdir',str(out),'--res','1100x760'],cwd=r/'build/bin',env=env,stdout=log,stderr=subprocess.STDOUT)
status('Fix2 comparison ready',f'Build {b.returncode}, render {res.returncode}.','user-fix2')
'''
p=pathlib.Path('/root/user_fix2_job.py');p.write_text(job);p=subprocess.Popen(['python3','/root/user_fix2_job.py'],start_new_session=True);print('pid',p.pid)
