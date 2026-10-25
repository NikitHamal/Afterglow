from pathlib import Path
import shutil, zipfile, glob
exe = Path('/root/build-win-posix/bin/afterglow.exe')
out = Path('/root/win64-posix'); out.mkdir(exist_ok=True)
shutil.copyfile(exe, out / 'Afterglow.exe')
libs = []
for name in ('libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll'):
    cand = glob.glob('/usr/x86_64-w64-mingw32/bin/' + name) + glob.glob('/usr/lib/gcc/x86_64-w64-mingw32/**/' + name, recursive=True) + glob.glob('/usr/x86_64-w64-mingw32/lib/' + name)
    if cand:
        shutil.copyfile(cand[0], out / name); libs.append(name)
    else:
        print('missing', name)
z = out.parent / 'Afterglow-win64-posix.zip'
with zipfile.ZipFile(z, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    for file in out.iterdir():
        archive.write(file, archive if False else 'Afterglow/' + file.name)
print('zip', z.stat().st_size, 'libs', libs)
public = Path('/marimo/public/afterglow'); public.mkdir(parents=True, exist_ok=True)
shutil.copyfile(z, public / z.name)
print('public', (public / z.name).stat().st_size)
