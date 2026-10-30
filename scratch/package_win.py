from pathlib import Path
import shutil
import zipfile
import glob

exe = Path("/root/build-win/bin/afterglow.exe")
out = Path("/root/win64")
out.mkdir(exist_ok=True)
shutil.copyfile(exe, out / "Afterglow.exe")
libs = []
for name in ("libgcc_s_seh-1.dll", "libstdc++-6.dll"):
    candidates = glob.glob("/usr/x86_64-w64-mingw32/bin/" + name) + glob.glob(
        "/usr/lib/gcc/x86_64-w64-mingw32/**/" + name, recursive=True
    )
    if candidates:
        shutil.copyfile(candidates[0], out / name)
        libs.append(name)
    else:
        print("missing", name)
z = out.parent / "Afterglow-win64-test.zip"
with zipfile.ZipFile(z, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    for file in out.iterdir():
        archive.write(file, arcname="Afterglow/" + file.name)
print("zip", z.stat().st_size, "libs", libs)
public = Path("/marimo/public/afterglow")
public.mkdir(parents=True, exist_ok=True)
shutil.copyfile(z, public / z.name)
print("public", (public / z.name).stat().st_size)
