import glob, os, pathlib

# Desktop shaders are GLSL 330; Android's GLES 3 backend needs ESSL 300.
# Everything the shaders use (VAOs, MRT, depth textures, textureLod) exists in ES 3.00, so the port is the version
# line plus precision qualifiers. Output from here is embedded by native/android/app/src/main/cpp/CMakeLists.txt.

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC = ROOT / 'native/shaders'
DST = ROOT / 'native/shaders_es'

HEADER = '#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;\n'


def main() -> int:
    DST.mkdir(exist_ok=True)
    for f in glob.glob(str(SRC / '*')):
        name = os.path.basename(f)
        if not name.endswith(('.vert', '.frag')):
            continue
        text = pathlib.Path(f).read_text(encoding='utf-8')
        text = text.replace('#version 330 core', HEADER.rstrip('\n'))
        (DST / name).write_text(text, encoding='utf-8')
    print(f'wrote {len(list(DST.glob("*")))} GLES shaders')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
