import pathlib, subprocess
job=r'''import pathlib, subprocess, json, time, os
r=pathlib.Path('/root'); sp=r/'board/status.json'
def status(p,d,n='deepen'):
 x=json.loads(sp.read_text()) if sp.exists() else {}
 x.update(phase=p,detail=d,updated=time.time(),commit='local'); x['pass']=n; sp.write_text(json.dumps(x))
status('Rendering the deeper shallow end','Lip ring 50-90 mm from the tip so the whole glans stays inside the mouth.')
with (r/'deepen-build.log').open('w') as log:
 b=subprocess.run(['cmake','--build',str(r/'build'),'-j','12'],stdout=log,stderr=subprocess.STDOUT)
if b.returncode:
 status('Deepen build failed','See /root/deepen-build.log.','deepen'); raise SystemExit(1)
out=r/'board/_experiments/deepen-r1'; out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,LIBGL_ALWAYS_SOFTWARE='1',GALLIUM_DRIVER='llvmpipe')
with (r/'deepen.log').open('w') as log:
 res=subprocess.run(['xvfb-run','-a','-s','-screen 0 1920x1080x24','./afterglow','--shotlist',str(r/'Afterglow/tools/qa/lowdepth_shots.txt'),'--outdir',str(out),'--res','1100x760'],cwd=r/'build/bin',env=env,stdout=log,stderr=subprocess.STDOUT)
status('Deepened renders ready',f'Build {b.returncode}, render {res.returncode}.','deepen')
'''
pathlib.Path('/root/deepen_job.py').write_text(job)
p=subprocess.Popen(['python3','/root/deepen_job.py'],start_new_session=True)
print('pid',p.pid)
