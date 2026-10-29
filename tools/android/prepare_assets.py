import json, pathlib, shutil, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC = ROOT / 'native/assets'
DST = ROOT / 'native/android/app/src/main/assets'

# the app ships exactly two faces: Vivian (her) and Marcus (him). Stories never name another model.
DROP_PREFIXES = ('mei', 'amara', 'kari', 'daniel')


def keep(rel: pathlib.Path) -> bool:
    parts = rel.parts
    if not parts:
        return False
    name = rel.name
    if parts[0] == 'characters' and len(parts) > 1 and name.startswith(DROP_PREFIXES):
        return False
    return True


def main() -> int:
    if DST.exists():
        shutil.rmtree(DST)
    files = []
    for p in sorted(SRC.rglob('*')):
        if not p.is_file():
            continue
        rel = p.relative_to(SRC)
        if not keep(rel):
            continue
        out = DST / rel
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(p, out)
        files.append(rel.as_posix())

    roster_path = DST / 'characters/roster.json'
    roster = json.loads(roster_path.read_text(encoding='utf-8'))
    roster['her'] = [e for e in roster['her'] if e['id'] == 'vivian']
    roster['him'] = [e for e in roster['him'] if e['id'] == 'marcus']
    roster_path.write_text(json.dumps(roster, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')

    manifest = DST / 'assets_manifest.json'
    manifest.write_text('\n'.join(sorted(files)) + '\n', encoding='utf-8')
    size = sum((DST / f).stat().st_size for f in files) / (1024 * 1024)
    print(f'android assets: {len(files)} files, {size:.0f} MB -> {DST}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
