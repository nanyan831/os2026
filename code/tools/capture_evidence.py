#!/usr/bin/env python3
"""Capture real X terminals executing checks/debuggers or showing named raw logs."""
import argparse
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
REPORT = ROOT.parent / 'report'


def execute(args, save=None, stop_marker=None):
    print('$ ' + ' '.join(args), flush=True)
    process = subprocess.Popen(args, cwd=ROOT, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True, start_new_session=True)
    lines = []
    stopped = False
    try:
        for line in process.stdout:
            print(line, end='', flush=True)
            lines.append(line)
            if stop_marker and stop_marker in line:
                os.killpg(process.pid, signal.SIGTERM)
                stopped = True
                break
        process.wait(timeout=8)
    finally:
        if process.poll() is None:
            os.killpg(process.pid, signal.SIGTERM)
            process.wait(timeout=3)
    output = ''.join(lines)
    if save:
        (REPORT / 'logs' / save).write_text('$ ' + ' '.join(args) + '\n' + output)
    if not stopped and process.returncode:
        raise RuntimeError(f'command exited {process.returncode}')
    if stop_marker and not stopped:
        raise RuntimeError('expected kernel marker not reached')
    return output


def worker(mode, marker):
    os.chdir(ROOT)
    print('Nankai OS 2026 - Lab 2 | actual Ubuntu terminal', flush=True)
    execute(['uname', '-srmo'])
    try:
        if mode == 'matrix':
            execute(['python3', 'tools/verify_lab2.py', '--stage', 'all', '--compact'])
        elif mode == 'boot':
            execute(['make', 'clean'])
            execute(['make', '-j4'])
            output = execute(['make', 'qemu'], save='final-boot.log', stop_marker='[MMU probe]')
            assert 'check_alloc_page() succeeded!' in output
        elif mode == 'grade':
            output = execute(['make', 'grade'], save='official-grade.log')
            assert 'Total Score: 20/20' in output
        elif mode == 'buddy':
            print('Saved raw kernel run from the final verification matrix:', flush=True)
            execute(['sed', '-n', '/memory management:/,$p', '../report/logs/boot-buddy-128m.log'])
        elif mode == 'slub':
            print('Saved raw kernel run: SLUB with Buddy as page allocator:', flush=True)
            execute(['sed', '-n', '/memory management:/,$p', '../report/logs/boot-slub-buddy.log'])
        elif mode == 'guest-paging':
            execute(['make', '-j4'])
            execute(['python3', 'tools/trace_paging.py'])
        elif mode == 'host-qemu':
            execute(['python3', 'tools/trace_qemu_mmu.py', '--compact'])
        marker.write_text(json.dumps({'passed': True}))
    except BaseException as error:
        print(f'EVIDENCE COMMAND FAILED: {error}', flush=True)
        marker.write_text(json.dumps({'passed': False, 'error': str(error)}))
    time.sleep(30)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--worker')
    parser.add_argument('--marker', type=Path)
    parser.add_argument('--only', choices=['matrix', 'boot', 'grade', 'buddy', 'slub', 'guest-paging', 'host-qemu'])
    args = parser.parse_args()
    if args.worker:
        return worker(args.worker, args.marker)
    images = REPORT / 'images'
    images.mkdir(parents=True, exist_ok=True)
    modes = ['matrix', 'boot', 'grade', 'buddy', 'slub', 'guest-paging', 'host-qemu']
    with tempfile.TemporaryDirectory(prefix='lab2-evidence-') as temporary:
        temporary = Path(temporary)
        display_no = 200 + os.getpid() % 500
        while Path(f'/tmp/.X11-unix/X{display_no}').exists():
            display_no += 1
        display = f':{display_no}'
        env = dict(os.environ, DISPLAY=display)
        xvfb = subprocess.Popen(['Xvfb', display, '-screen', '0', '1800x1600x24', '-nolisten', 'tcp'],
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            time.sleep(0.7)
            for number, mode in enumerate(modes, 1):
                if args.only and mode != args.only:
                    continue
                print(f'Capturing actual terminal: {mode}', flush=True)
                marker = temporary / f'{mode}.json'
                rows = {'matrix': 28, 'boot': 72, 'grade': 40, 'buddy': 32,
                        'slub': 40, 'guest-paging': 72, 'host-qemu': 28}[mode]
                terminal = subprocess.Popen(['xterm', '-fa', 'DejaVu Sans Mono', '-fs', '12',
                    '-geometry', f'177x{rows}+8+8', '-bg', '#0b0e14', '-fg', '#e6e6e6',
                    '-title', f'Lab 2 - {mode}', '-e', sys.executable, str(Path(__file__).resolve()),
                    '--worker', mode, '--marker', str(marker)], env=env,
                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
                try:
                    deadline = time.monotonic() + 180
                    while not marker.exists():
                        if terminal.poll() is not None or time.monotonic() >= deadline:
                            raise RuntimeError(f'{mode}: terminal command did not finish')
                        time.sleep(0.3)
                    status = json.loads(marker.read_text())
                    time.sleep(0.5)
                    path = images / f'{number:02}-{mode}.png'
                    tree = subprocess.check_output(['xwininfo', '-display', display, '-root', '-tree'], text=True)
                    window = re.search(r'(0x[0-9a-f]+)\s+"Lab 2 - ' + re.escape(mode) + r'"', tree)
                    assert window, 'xterm window not found'
                    subprocess.run(['import', '-display', display, '-window', window.group(1), str(path)], check=True)
                    assert status['passed'], status
                    print(f'Saved {path.name}', flush=True)
                finally:
                    if terminal.poll() is None:
                        os.killpg(terminal.pid, signal.SIGTERM)
                    terminal.wait(timeout=4)
        finally:
            xvfb.terminate()
            xvfb.wait(timeout=4)
    print('Captured 7 real terminal screenshots; default Best-Fit build restored.', flush=True)


if __name__ == '__main__':
    main()
