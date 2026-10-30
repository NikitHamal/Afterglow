import pathlib, subprocess
job = r'''import pathlib, subprocess, json, time, os, shutil, zipfile, hashlib
r=pathlib.Path('/root'); sp=r/'board/status.json'
def status(p,d,n='aperture-grip'):
 x=json.loads(sp.read_text()) if sp.exists() else {}
 x.update(phase=p,detail=d,updated=time.time(),commit='92b56d6'); x['pass']=n; sp.write_text(json.dumps(x))
status('Cross-building the Windows test exe','MinGW POSIX, aperture mouth target and wrapped grip.')
with (r/'win-r1-build.log').open('w') as log:
 b=subprocess.run(['cmake','--build',str(r/'build-win-posix'),'-j','10'],stdout=log,stderr=subprocess.STDOUT)
if b.returncode:
 status('Windows build failed','See /root/win-r1-build.log.','aperture-grip'); raise SystemExit(1)
exes=list((r/'build-win-posix/bin').glob('*.exe'))
if not exes:
 status('Windows build produced no exe','No .exe in build-win-posix/bin.','aperture-grip'); raise SystemExit(1)
exe=exes[0]
pkg=r/'board/win-r1-pkg'; shutil.rmtree(pkg, ignore_errors=True); (pkg/'Afterglow').mkdir(parents=True)
shutil.copyfile(exe, pkg/'Afterglow'/'Afterglow.exe')
want=['libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll']
roots=[pathlib.Path('/usr/x86_64-w64-mingw32'), pathlib.Path('/usr/lib/gcc/x86_64-w64-mingw32')]
for name in want:
    hit=next((p for root in roots if root.exists() for p in root.rglob(name)), None)
    if hit: shutil.copyfile(hit, pkg/'Afterglow'/name)
    else: print('MISSING', name)
zip_path=r/'board/Afterglow-win64-r1.zip'
with zipfile.ZipFile(zip_path,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for p in (pkg/'Afterglow').glob('*'): z.write(p, 'Afterglow/'+p.name)
pub=r/'../marimo/public/afterglow'; pub.mkdir(parents=True, exist_ok=True)
shutil.copyfile(zip_path, pub/'Afterglow-win64-r1.zip')
size=exe.stat().st_size; zsize=zip_path.stat().st_size
sha=hashlib.sha256(exe.read_bytes()).hexdigest()[:12]
status('Windows test exe ready',f'{size/1048576:.2f} MB exe, {zsize/1048576:.1f} MB zip, sha {sha}.','aperture-grip')
print('EXE',exe,'size',size,'sha',sha,'zip',zsize,'pub',(pub/'Afterglow-win64-r1.zip').exists())
'''
pathlib.Path('/root/win_r1_job.py').write_text(job)
p=subprocess.Popen(['python3','/root/win_r1_job.py'],start_new_session=True)
print('pid',p.pid)
