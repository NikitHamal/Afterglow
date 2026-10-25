import pathlib, subprocess
job = r'''import pathlib, subprocess, json, time, os, shutil, zipfile, hashlib
r=pathlib.Path('/root'); sp=r/'board/status.json'
def status(p,d,n='deepen'):
 x=json.loads(sp.read_text()) if sp.exists() else {}
 x.update(phase=p,detail=d,updated=time.time(),commit='bfc9193'); x['pass']=n; sp.write_text(json.dumps(x))
status('Cross-building the Windows exe (r2)','MinGW POSIX, 50-90 mm lip-ring travel.')
with (r/'win-r2-build.log').open('w') as log:
 b=subprocess.run(['cmake','--build',str(r/'build-win-posix'),'-j','10'],stdout=log,stderr=subprocess.STDOUT)
if b.returncode:
 status('Windows r2 build failed','See /root/win-r2-build.log.','deepen'); raise SystemExit(1)
exe=list((r/'build-win-posix/bin').glob('*.exe'))
if not exes if False else not exe:
 status('Windows r2 produced no exe','No .exe in build-win-posix/bin.','deepen'); raise SystemExit(1)
exe=exe[0]
pkg=r/'board/win-r2-pkg'; shutil.rmtree(pkg, ignore_errors=True); (pkg/'Afterglow').mkdir(parents=True)
shutil.copyfile(exe, pkg/'Afterglow'/'Afterglow.exe')
for name in ['libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll']:
    hit=next((p for root in [pathlib.Path('/usr/x86_64-w64-mingw32'), pathlib.Path('/usr/lib/gcc/x86_64-w64-mingw32')] if root.exists() for p in root.rglob(name)), None)
    if hit: shutil.copyfile(hit, pkg/'Afterglow'/name)
    else: print('MISSING', name)
zip_path=r/'board/Afterglow-win64-r2.zip'
with zipfile.ZipFile(zip_path,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for p in (pkg/'Afterglow').glob('*'): z.write(p, 'Afterglow/'+p.name)
pub=r/'../marimo/public/afterglow'; pub.mkdir(parents=True, exist_ok=True)
shutil.copyfile(zip_path, pub/'Afterglow-win64-r2.zip')
size=exe.stat().st_size; zsize=zip_path.stat().st_size
sha=hashlib.sha256(exe.read_bytes()).hexdigest()[:12]
status('Windows r2 exe ready',f'{size/1048576:.2f} MB exe, {zsize/1048576:.1f} MB zip, sha {sha}.','deepen')
print('EXE',exe,'size',size,'sha',sha,'zip',zsize)
'''
pathlib.Path('/root/win_r2_job.py').write_text(job)
p=subprocess.Popen(['python3','/root/win_r2_job.py'],start_new_session=True)
print('pid',p.pid)
