import pathlib, subprocess
job=r'''import pathlib, subprocess, json, time, os
r=pathlib.Path('/root');sp=r/'board/status.json'
def status(p,d,n='win-build'):
 x=json.loads(sp.read_text());x.update(phase=p,detail=d,updated=time.time(),commit='local');x['pass']=n;sp.write_text(json.dumps(x))
status('Rebuilding the Windows exe with the profile fixes','Max-length glans target, two-hand grip and finger curl are in; cross-building with POSIX threads.')
with (r/'win-final-build.log').open('w') as log: b=subprocess.run(['cmake','--build',str(r/'build-win-posix'),'-j','8'],stdout=log,stderr=subprocess.STDOUT)
if b.returncode: status('Windows rebuild failed','See /root/win-final-build.log.','win-build');raise SystemExit(1)
status('Packaging the fixed Windows exe','Build succeeded; packaging exe and runtime DLLs.')
subprocess.run(['python3','/root/package_win_posix.py'],check=True,capture_output=True)
status('Fixed Windows exe ready','Packaged after the user-profile fixes.','win-build')
'''
p=pathlib.Path('/root/win_final_job.py');p.write_text(job);p=subprocess.Popen(['python3','/root/win_final_job.py'],start_new_session=True);print('pid',p.pid)
