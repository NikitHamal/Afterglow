import pathlib
import subprocess
job = r'''import pathlib, subprocess, json, time, os
r = pathlib.Path('/root'); sp = r/'board/status.json'
def status(p,d,n='win-build'):
    x=json.loads(sp.read_text());x.update(phase=p,detail=d,updated=time.time(),commit='022019c');x['pass']=n;sp.write_text(json.dumps(x))
toolchain = r"""set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc-posix)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++-posix)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
"""
pathlib.Path('/root/ag-mingw-posix.cmake').write_text(toolchain)
status('Rebuilding with POSIX threads', 'The first cross build crashed during scene load under the Win32 thread model; rebuilding with MinGW POSIX threads.')
cfg = subprocess.run(['cmake','-S',str(r/'Afterglow/native'),'-B',str(r/'build-win-posix'),'-DCMAKE_TOOLCHAIN_FILE=/root/ag-mingw-posix.cmake','-DCMAKE_BUILD_TYPE=Release','-DAG_VERSION=0.2.0-test'],capture_output=True,text=True)
(r/'win-posix-configure.log').write_text(cfg.stdout+cfg.stderr)
if cfg.returncode: status('POSIX configure failed','See /root/win-posix-configure.log.'); raise SystemExit(1)
with (r/'win-posix-build.log').open('w') as log:
    b=subprocess.run(['cmake','--build',str(r/'build-win-posix'),'-j','8'],stdout=log,stderr=subprocess.STDOUT)
if b.returncode: status('POSIX build failed','See /root/win-posix-build.log.'); raise SystemExit(1)
exe=next((r/'build-win-posix/bin').glob('*.exe'))
status('POSIX Windows exe ready','Cross-build finished; testing next.','win-build')
print('EXE',exe,exe.stat().st_size)
'''
path=pathlib.Path('/root/win_posix_build_job.py');path.write_text(job)
p=subprocess.Popen(['python3',str(path)],start_new_session=True);print('posix pid',p.pid)
