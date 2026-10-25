import pathlib, subprocess
job=r'''import pathlib, subprocess, json, time, os
r=pathlib.Path('/root');sp=r/'board/status.json'
def status(p,d,n='user-grip'):
 x=json.loads(sp.read_text());x.update(phase=p,detail=d,updated=time.time(),commit='local');x['pass']=n;sp.write_text(json.dumps(x))
status('Solving both oral hands onto the shaft','Testing a two-hand grip at shaft fractions below the lips on the user profile.')
with (r/'user-grip-build.log').open('w') as log: b=subprocess.run(['cmake','--build',str(r/'build'),'-j','12'],cwd=r/'Afterglow',stdout=log,stderr=subprocess.STDOUT)
if b.returncode: status('Grip build failed','See /root/user-grip-build.log.','user-grip');raise SystemExit(1)
out=r/'board/_experiments/user-grip-r1';out.mkdir(parents=True,exist_ok=True);env=dict(os.environ,LIBGL_ALWAYS_SOFTWARE='1',GALLIUM_DRIVER='llvmpipe')
with (r/'user-grip.log').open('w') as log: res=subprocess.run(['xvfb-run','-a','-s','-screen 0 1920x1080x24','./afterglow','--shotlist',str(r/'Afterglow/tools/qa/user_grip_shots.txt'),'--outdir',str(out),'--res','1100x760'],cwd=r/'build/bin',env=env,stdout=log,stderr=subprocess.STDOUT)
status('Two-hand grip comparison ready',f'Build {b.returncode}, render {res.returncode}.','user-grip')
'''
p=pathlib.Path('/root/user_grip_job.py');p.write_text(job);p=subprocess.Popen(['python3','/root/user_grip_job.py'],start_new_session=True);print('pid',p.pid)
