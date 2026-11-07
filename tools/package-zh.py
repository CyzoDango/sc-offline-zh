"""Package a clean zh derivative and full source, including the pinned sco-core.

Output must be new and outside the source tree. Never reads a local game install,
running mod directory, user settings or the repository's .git metadata.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
BASE = '1ac5308a65ac0ba7955e4108bd1dc800449cfa69'
CORE = '2197dfbf5a630e5a16a201526e628f1d28df908a'
SKIP_DIRS = {'.git', '.vs', '__pycache__', 'node_modules'}


def files(folder):
    for path in sorted(folder.rglob('*')):
        rel = path.relative_to(folder)
        if any(part in SKIP_DIRS for part in rel.parts):
            continue
        if path.is_symlink():
            raise ValueError(f'Symlink is not packaged: {rel}')
        if path.is_file() and path.suffix not in {'.pyc', '.pdb', '.obj', '.exe', '.dll', '.lib'}:
            yield path, rel


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    out, build = args.out.resolve(), args.build.resolve()
    if out == ROOT or ROOT in out.parents or out.exists():
        raise ValueError('Output must be new and outside the source tree')
    version = re.search(r'#define SCO_VERSION\s+"([^"]+)"', (ROOT/'src/version.h').read_text()).group(1)
    binaries = {
        'sc-offline.exe': build/'bin/Release/sc-offline.exe',
        'dinput8.dll': build/'bin/Release/dinput8.dll',
        'data/plugins/creative/creative.dll': build/'data/plugins/creative/creative.dll',
        'data/plugins/discord/discord.dll': build/'data/plugins/discord/discord.dll',
    }
    for path in binaries.values():
        if not path.is_file():
            raise ValueError(f'Missing built file: {path}')
    for private in ['ui-v3-local', 'local-reference', 'storage', 'scripts']:
        if (ROOT/'data'/private).exists():
            raise ValueError(f'Private local data must remain outside the source tree: data/{private}')
    source_files = list(files(ROOT))
    if not (ROOT/'external/sco-core/include/sco/app.h').is_file():
        raise ValueError('Full sco-core source missing')
    source_hash = hashlib.sha256()
    for path, rel in source_files:
        source_hash.update((rel.as_posix()+'\0'+digest(path)+'\n').encode('utf-8'))
    program = out/f'sc-offline-v{version}'
    program.mkdir(parents=True)
    for folder in ['data', 'docs']:
        shutil.copytree(ROOT/folder, program/folder)
    for filename in ['README.md', 'CHANGELOG.md', 'LICENSE', 'README.zh-CN.md', 'CHANGELOG.zh-CN.md', 'NOTICE.zh-CN.txt']:
        shutil.copy2(ROOT/filename, program/filename)
    shutil.copy2(ROOT/'launcher/sc-offline.ini', program/'sc-offline.ini')
    for rel, path in binaries.items():
        target = program/rel
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
    for path, rel in source_files:
        if path.name.upper().startswith(('LICENSE', 'LICENCE', 'COPYING', 'NOTICE', 'THIRD_PARTY_NOTICE')):
            target = program/'LICENSES'/rel
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
    receipt = {
        'version': version, 'modified_on': '2026-11-03',
        'upstream': 'https://github.com/scubamount/sc-offline',
        'upstream_commit': BASE, 'sco_core_commit': CORE,
        'sco_core_local_changes': ['include/sco/game/world.h', 'src/game/world_sigs.cpp', 'include/sco/datacore.h', 'include/sco/datacore_pack.h', 'src/datacore/patch.cpp', 'src/datacore/pack.cpp', 'docs/datacore.md'],
        'full_source_files': len(source_files),
        'full_source_content_index_sha256': source_hash.hexdigest(),
        'binaries': {rel: digest(path) for rel, path in binaries.items()},
        'source_archive': f'sc-offline-v{version}-source.zip',
        'published': False,
    }
    (program/'build-info.json').write_text(json.dumps(receipt, indent=2)+'\n', encoding='utf-8')
    source_zip = out/receipt['source_archive']
    with zipfile.ZipFile(source_zip, 'x', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for path, rel in source_files:
            archive.write(path, f'sc-offline-v{version}-source/{rel.as_posix()}')
    manifest_tool = ROOT/'tools/release-manifest.py'
    subprocess.run([sys.executable, str(manifest_tool), 'write', str(program), '--tag', 'v'+version,
                    '--version-h', str(ROOT/'src/version.h')], check=True)
    subprocess.run([sys.executable, str(manifest_tool), 'check', str(program)], check=True)
    program_zip = out/f'sc-offline-v{version}.zip'
    with zipfile.ZipFile(program_zip, 'x', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for path in sorted(program.rglob('*')):
            if path.is_file():
                archive.write(path, f'{program.name}/{path.relative_to(program).as_posix()}')
    for path in [source_zip, program_zip]:
        with zipfile.ZipFile(path) as archive:
            bad = archive.testzip()
            if bad:
                raise ValueError(f'ZIP integrity check failed: {bad}')
    (out/'SHA256SUMS.txt').write_text(''.join(f'{digest(path)}  {path.name}\n' for path in [program_zip,source_zip]), encoding='ascii')
    print(json.dumps({'version':version, 'source_files':len(source_files), 'program_zip':str(program_zip),
                      'source_zip':str(source_zip)}, indent=2))


if __name__ == '__main__':
    main()
