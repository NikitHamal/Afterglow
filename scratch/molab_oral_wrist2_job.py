import pathlib, subprocess
job=r'''import pathlib, subprocess, json, time, os
r=pathlib.Path('/root');sp=r/'board/status.json'
def status(p,d,n):
 x=json.loads(sp.read_text());x.update(phase=p,detail=d,updated=time.time(),commit='pending');x['pass']=n;x['shots']=len(list((r/'board/shots').glob('**/*.png')));sp.write_text(json.dumps(x))
status('Tuning each oral wrist','Right wrist quarter-turned; comparing left wrist pronation.','oral-wrist-r2')
with (r/'oral-wrist2-build.log').open('w') as log: b=subprocess.run(['cmake','--build',str(r/'build'),'-j','12'],cwd=r/'Afterglow',stdout=log,stderr=subprocess.STDOUT)
if b.returncode: status('Wrist build failed','See /root/oral-wrist2-build.log.','oral-wrist-r2');raise SystemExit(1)
out=r/'board/shots/oral-wrist-r2';out.mkdir(parents=True,exist_ok=True);env=dict(os.environ,LIBGL_ALWAYS_SOFTWARE='1',GALLIUM_DRIVER='llvmpipe')
with (r/'oral-wrist2.log').open('w') as log: res=subprocess.run(['xvfb-run','-a','-s','-screen 0 1920x1080x24','./afterglow','--shotlist',str(r/'Afterglow/tools/qa/oral_wrist2_shots.txt'),'--outdir',str(out),'--res','1100x760'],cwd=r/'build/bin',env=env,stdout=log,stderr=subprocess.STDOUT)
status('Oral wrist combinations ready',f'Build {b.returncode}, render {res.returncode}.','oral-wrist-r2')
'''
path=pathlib.Path('/root/oral_wrist2_job.py');path.write_text(job);p=subprocess.Popen(['python3',str(path)],start_new_session=True);print('pid',p.pid)
