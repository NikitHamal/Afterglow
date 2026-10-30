import pathlib, subprocess
job=r'''import pathlib, subprocess, json, time, os
r=pathlib.Path('/root');sp=r/'board/status.json'
def status(p,d,n):
 x=json.loads(sp.read_text());x.update(phase=p,detail=d,updated=time.time(),commit='pending');x['pass']=n;x['shots']=len(list((r/'board/shots').glob('**/*.png')));sp.write_text(json.dumps(x))
status('Final oral grip check','Right wrist quarter-turned to remove the backward palm; rendering side, FPV and face.','oral-grip-r1')
with (r/'oral-grip-build.log').open('w') as log: b=subprocess.run(['cmake','--build',str(r/'build'),'-j','12'],cwd=r/'Afterglow',stdout=log,stderr=subprocess.STDOUT)
if b.returncode: status('Grip build failed','See /root/oral-grip-build.log.','oral-grip-r1');raise SystemExit(1)
out=r/'board/shots/oral-grip-r1';out.mkdir(parents=True,exist_ok=True);env=dict(os.environ,LIBGL_ALWAYS_SOFTWARE='1',GALLIUM_DRIVER='llvmpipe')
with (r/'oral-grip.log').open('w') as log: res=subprocess.run(['xvfb-run','-a','-s','-screen 0 1920x1080x24','./afterglow','--shotlist',str(r/'Afterglow/tools/qa/oral_grip_final_shots.txt'),'--outdir',str(out),'--res','1100x760'],cwd=r/'build/bin',env=env,stdout=log,stderr=subprocess.STDOUT)
status('Final oral grip ready',f'Build {b.returncode}, render {res.returncode}.','oral-grip-r1')
'''
path=pathlib.Path('/root/oral_grip_job.py');path.write_text(job);p=subprocess.Popen(['python3',str(path)],start_new_session=True);print('pid',p.pid)
