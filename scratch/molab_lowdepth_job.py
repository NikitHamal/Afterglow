import pathlib, subprocess
job=r'''import pathlib, subprocess, json, time, os
r=pathlib.Path('/root'); sp=r/'board/status.json'
def status(p,d,n='lowdepth'):
 x=json.loads(sp.read_text()) if sp.exists() else {}
 x.update(phase=p,detail=d,updated=time.time(),commit='local'); x['pass']=n; sp.write_text(json.dumps(x))
status('Rebuilding for lip-ring + grip','Aperture-target oral IK, wider neck budget, cylindrical finger wrap.')
with (r/'lowdepth-build.log').open('w') as log:
 b=subprocess.run(['cmake','--build',str(r/'build'),'-j','12'],stdout=log,stderr=subprocess.STDOUT)
if b.returncode:
 status('Low-depth build failed','See /root/lowdepth-build.log.','lowdepth'); raise SystemExit(1)
out=r/'board/_experiments/lowdepth-r1'; out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,LIBGL_ALWAYS_SOFTWARE='1',GALLIUM_DRIVER='llvmpipe')
with (r/'lowdepth.log').open('w') as log:
 res=subprocess.run(['xvfb-run','-a','-s','-screen 0 1920x1080x24','./afterglow','--shotlist',str(r/'Afterglow/tools/qa/lowdepth_shots.txt'),'--outdir',str(out),'--res','1100x760'],cwd=r/'build/bin',env=env,stdout=log,stderr=subprocess.STDOUT)
status('Low-depth renders ready',f'Build {b.returncode}, render {res.returncode}.','lowdepth')
'''
pathlib.Path('/root/lowdepth_job.py').write_text(job)
p=subprocess.Popen(['python3','/root/lowdepth_job.py'],start_new_session=True)
print('pid',p.pid)
