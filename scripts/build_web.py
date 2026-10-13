#!/usr/bin/env python3
"""Compile the shared C/SDL2 game to a website or a self-contained HTML file."""
import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'dist' / 'web')
    parser.add_argument('--single-file', action='store_true', help='Embed code and assets into index.html')
    parser.add_argument('--test-hooks', action='store_true', help='Expose read-only state for browser tests')
    args = parser.parse_args()
    emcc = shutil.which('emcc')
    if not emcc:
        parser.error('Emscripten is not active. Activate emsdk first; see docs/WEB.md.')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    command = [emcc, '-std=c11', '-O2', '-Wall', '-Wextra', '-Wl,--threads=1',
               'src/main.c', 'src/draw.c', 'src/game.c', 'src/world.c',
               '-Isrc', '-isystem', 'third_party/stb', '-sUSE_SDL=2',
               '-sALLOW_MEMORY_GROWTH=1', '-sINITIAL_MEMORY=67108864',
               '-sSTACK_SIZE=2097152', '-sENVIRONMENT=web', '-sFORCE_FILESYSTEM=1',
               '-sEXIT_RUNTIME=0', '--shell-file', 'web/shell.html',
               '-o', str(output / 'index.html')]
    if args.single_file:
        command += ['-sSINGLE_FILE=1', '--embed-file', 'assets@/assets']
    else:
        command += ['--preload-file', 'assets@/assets']
    if args.test_hooks:
        command += ['-DPALDO_WEB_TESTING=1', '-sEXPORTED_RUNTIME_METHODS=["UTF8ToString"]']
    subprocess.run(command, cwd=ROOT, check=True)
    (output / '.nojekyll').touch()
    print(f'Browser build ready: {output / "index.html"}')


if __name__ == '__main__':
    main()
