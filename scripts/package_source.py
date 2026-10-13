#!/usr/bin/env python3
"""Package authored C project files, assets and bundled stb; exclude build outputs/SDL download."""
from pathlib import Path
import argparse
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ROOT_FILES = {'CMakeLists.txt', 'README.md', 'THIRD_PARTY_NOTICES.md', '.gitignore', '.gitattributes', '.clang-format'}

def source_files():
    for path in sorted(ROOT.rglob('*')):
        if not path.is_file():
            continue
        rel = path.relative_to(ROOT)
        parts = rel.parts
        if '__pycache__' in parts:
            continue
        keep = str(rel) in ROOT_FILES or parts[0] in {'src','assets','docs','tests','scripts','web','.github'}
        keep |= len(parts)>2 and parts[:2]==('third_party','stb')
        if parts[0]=='android':
            excluded = {'build','.gradle','.cxx','local.properties','gradlew','gradlew.bat','gradle-wrapper.jar'}
            keep = not any(part in excluded for part in parts)
        if keep:
            yield path

def package(output):
    output = Path(output).resolve()
    output.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(output,'w',compression=zipfile.ZIP_DEFLATED) as archive:
        for path in source_files():
            archive.write(path,Path('paldo-yadam-c') / path.relative_to(ROOT))
    print(f'{output} ({output.stat().st_size} bytes)')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output',type=Path)
    package(parser.parse_args().output)
