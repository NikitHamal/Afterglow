import pathlib, subprocess
job=r'''import pathlib, subprocess, json, time, os
r=pathlib.Path('/root');sp=r/'board/status.json'
def status(p,d,n='user-curl'):
 x=json.loads(sp.read_text());x.update(phase=p,detail=d,updated=time.time(),commit='local');x['pass']=n;sp.write_text(json.dumps(x))
status('Testing the oral hand curl','Four finger-curl amounts on the user profile; the max-size glans target is already in place.')
with (r/'user-curl-build.log').open('w') as log: b=subprocess.run(['cmake','--build',str(r/'build'),'-j','12'],cwd=r/'Afterglow',stdout=log,stderr=subprocess.STDOUT)
if b.returncode: status('Hand curl build failed','See /root/user-curl-build.log.','user-curl');raise SystemExit(1)
out=r/'board/_experiments/user-curl-r1';out.mkdir(parents=True,exist_ok=True);env=dict(os.environ,LIBGL_ALWAYS_SOFTWARE='1',GALLIUM_DRIVER='llvmpipe')
with (r/'user-curl.log').open('w') as log: res=subprocess.run(['xvfb-run','-a','-s','-screen 0 1920x1080x24','./afterglow','--shotlist',str(r/'Afterglow/tools/qa/user_curl_shots.txt'),'--outdir',str(out),'--res','1100x760'],cwd=r/'build/bin',env=env,stdout=log,stderr=subprocess.STDOUT)
status('Hand curl comparison ready',f'Build {b.returncode}, render {res.returncode}.','user-curl')
'''
p=pathlib.Path('/root/user_curl_job.py');p.write_text(job);p=subprocess.Popen(['python3','/root/user_curl_job.py'],start_new_session=True);print('pid',p.pid)
