#!/usr/bin/env python3
"""Prepare pinned SDL2 sources and its Gradle launcher without administrator access."""
import argparse
import hashlib
from pathlib import Path
import shutil
import tarfile
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
VERSION = '2.30.12'
URL = f'https://github.com/libsdl-org/SDL/releases/download/release-{VERSION}/SDL2-{VERSION}.tar.gz'
SHA256 = 'ac356ea55e8b9dd0b2d1fa27da40ef7e238267ccf9324704850d5d47375b48ea'


def prepare(archive=None):
    dest = ROOT / 'third_party' / 'SDL'
    marker = dest / '.paldo-source-sha256'
    if dest.exists() and (not marker.is_file() or marker.read_text().strip() != SHA256):
        raise SystemExit(f'{dest} already exists with unknown contents; move it aside before preparing SDL.')
    if not dest.exists():
        with tempfile.TemporaryDirectory(prefix='paldo-sdl-') as work:
            work = Path(work)
            package = Path(archive).resolve() if archive else work / 'SDL2.tar.gz'
            if not archive:
                print(f'Downloading SDL {VERSION} from its official release.')
                with urllib.request.urlopen(URL, timeout=60) as response, package.open('wb') as output:
                    shutil.copyfileobj(response, output)
            if hashlib.sha256(package.read_bytes()).hexdigest() != SHA256:
                raise SystemExit('SDL archive checksum mismatch. No source files were installed.')
            with tarfile.open(package, 'r:gz') as tf:
                for member in tf.getmembers():
                    p = Path(member.name)
                    if p.is_absolute() or '..' in p.parts :
                        raise SystemExit('Unsafe archive member refused.')
                if hasattr(tarfile, 'data_filter'):
                    tf.extractall(work, members=[m for m in tf.getmembers() if m.isfile() or m.isdir()], filter='data')
                else:
                    tf.extractall(work, members=[m for m in tf.getmembers() if m.isfile() or m.isdir()])
            shutil.copytree(work / f'SDL2-{VERSION}', dest)
            marker.write_text(SHA256 + '\n')
    sample = dest / 'android-project'
    android = ROOT / 'android'
    for name in ('gradlew', 'gradlew.bat', 'gradle/wrapper/gradle-wrapper.jar'):
        target = android / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(sample / name, target)
    (android / 'gradlew').chmod(0o755)
    print('Android dependencies ready. Open android/ in Android Studio, or run: cd android && ./gradlew assembleDebug')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', help='Use an already downloaded, checksum-verified SDL archive')
    prepare(parser.parse_args().archive)
