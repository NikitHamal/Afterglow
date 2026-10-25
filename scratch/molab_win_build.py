import pathlib, subprocess
job = r'''import pathlib, subprocess, json, time, os
r = pathlib.Path('/root')
status_path = r / 'board/status.json'
def status(phase, detail, pass_name='win-build'):
    d = json.loads(status_path.read_text())
    d.update(phase=phase, detail=detail, updated=time.time(), commit='022019c')
    d['pass'] = pass_name
    status_path.write_text(json.dumps(d))
toolchain = r"""set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
"""
pathlib.Path('/root/ag-mingw.cmake').write_text(toolchain)
status('Building the Windows test exe', 'Cross-compiling the current native branch with MinGW on the Molab box. Nothing is being pushed.')
cfg = subprocess.run(['cmake', '-S', str(r / 'Afterglow/native'), '-B', str(r / 'build-win'),
    '-DCMAKE_TOOLCHAIN_FILE=/root/ag-mingw.cmake', '-DCMAKE_BUILD_TYPE=Release',
    '-DAG_VERSION=0.2.0-test'], capture_output=True, text=True)
(r / 'win-configure.log').write_text(cfg.stdout + cfg.stderr)
if cfg.returncode != 0:
    status('Windows configure failed', 'See /root/win-configure.log.', 'win-build')
    raise SystemExit(1)
with (r / 'win-build.log').open('w') as log:
    b = subprocess.run(['cmake', '--build', str(r / 'build-win'), '-j', '8'], stdout=log, stderr=subprocess.STDOUT)
if b.returncode != 0:
    status('Windows build failed', 'See /root/win-build.log.', 'win-build')
    raise SystemExit(1)
bins = sorted((r / 'build-win/bin').glob('*.exe'))
status('Windows exe ready', 'Cross-build finished: ' + ', '.join(p.name for p in bins) + '. Packaging next.', 'win-build')
print('EXE', bins)
'''
path = pathlib.Path('/root/win_build_job.py'); path.write_text(job)
p = subprocess.Popen(['python3', str(path)], start_new_session=True)
print('win build pid', p.pid)
