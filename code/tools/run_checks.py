#!/usr/bin/env python3
"""Build and boot actual guest programs, stopping after verified teardown."""
import argparse
import pathlib
import subprocess
import time
import select
import os

parser = argparse.ArgumentParser()
parser.add_argument('--qemu', default='/usr/bin/qemu-system-riscv64')
parser.add_argument('--programs', nargs='+', default=['cowtest', 'forktree', 'waitkill'])
args = parser.parse_args()
os.chdir(pathlib.Path(__file__).resolve().parent.parent)
report_dir = pathlib.Path('report') if pathlib.Path('report').is_dir() else pathlib.Path('../report')
logs = report_dir / 'logs'
version = 'qemu7' if '/opt/' in args.qemu else 'qemu6'
for program in args.programs:
    with (logs / f'build-{program}-{version}.log').open('w') as build:
        subprocess.run(['make', f'build-{program}', 'DEFS+=-DCOW_SELFTEST' if program == 'cowtest' else 'DEFS+='], stdout=build, stderr=subprocess.STDOUT, check=True)
    proc = subprocess.Popen([args.qemu, '-machine', 'virt', '-nographic', '-bios', 'default', '-kernel', 'bin/ucore.img'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = b''
    deadline = time.monotonic() + 60
    while time.monotonic() < deadline:
        ready, _, _ = select.select([proc.stdout], [], [], 0.2)
        if ready:
            chunk = os.read(proc.stdout.fileno(), 4096)
            if not chunk: break
            output += chunk
            if b'init check memory pass.' in output or b'kernel panic' in output: break
    proc.terminate()
    proc.wait(timeout=5)
    (logs / f'{program}-{version}.log').write_bytes(output)
    assert b'init check memory pass.' in output, output[-2000:]
    assert b'user panic' not in output and b'kernel panic' not in output
    if program == 'cowtest':
        assert b'COW kernel selftests PASS' in output and b'cowtest pass.' in output
    print(f'{program} {version} PASS', flush=True)
