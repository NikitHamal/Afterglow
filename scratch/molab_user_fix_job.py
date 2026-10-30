import pathlib, subprocess
job=r'''import pathlib, subprocess, json, time, os
r=pathlib.Path('/root'); sp=r/'board/status.json'
def status(p,d,n='user-fix'):
 x=json.loads(sp.read_text());x.update(phase=p,detail=d,updated=time.time(),commit='local');x['pass']=n;sp.write_text(json.dumps(x))
status('Fixing the max-size mouth target','Measuring the length-morphed glans tip against Amara/Daniel and rendering the user profile.')
with (r/'user-fix-build.log').open('w') as log: b=subprocess.run(['cmake','--build',str(r/'build'),'-j','12'],cwd=r/'Afterglow',stdout=log,stderr=subprocess.STDOUT)
if b.returncode: status('User-profile build failed','See /root/user-fix-build.log.','user-fix');raise SystemExit(1)
out=r/'board/_experiments/user-fix-r1';out.mkdir(parents=True,exist_ok=True);env=dict(os.environ,LIBGL_ALWAYS_SOFTWARE='1',GALLIUM_DRIVER='llvmpipe')
with (r/'user-fix.log').open('w') as log: res=subprocess.run(['xvfb-run','-a','-s','-screen 0 1920x1080x24','./afterglow','--shotlist',str(r/'Afterglow/tools/qa/user_profile_shots.txt'),'--outdir',str(out),'--res','1100x760'],cwd=r/'build/bin',env=env,stdout=log,stderr=subprocess.STDOUT)
status('Max-size mouth comparison ready',f'Build {b.returncode}, render {res.returncode}.','user-fix')
'''
p=pathlib.Path('/root/user_fix_job.py');p.write_text(job);p=subprocess.Popen(['python3','/root/user_fix_job.py'],start_new_session=True);print('pid',p.pid)
