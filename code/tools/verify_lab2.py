#!/usr/bin/env python3
"""Build and check real RISC-V guests; preserve logs and stop owned processes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
LOGS = ROOT.parent / 'report' / 'logs'
FIXED_TEST_HASH = 'b6c6f8cd0608f21e2710f19ce23847f74c36b68d67dea90e8eef9faacdc9022a'


def audit():
    text = (ROOT / 'kern/mm/best_fit_pmm.c').read_text()
    suffix = text[text.index('// Framework tests:'):]
    actual = hashlib.sha256(suffix.encode()).hexdigest()
    assert actual == FIXED_TEST_HASH, 'Best-Fit fixed tests changed'
    assert '#include <test_lab2.h>' not in (ROOT / 'kern/mm/pmm.c').read_text()
    assert 'kern/test' not in (ROOT / 'Makefile').read_text()
    return {'best_fit_fixed_tests_sha256': actual, 'injection_source_clean': True}


def command(args, name, timeout=80):
    path = LOGS / f'{name}.log'
    result = subprocess.run(args, cwd=ROOT, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, timeout=timeout)
    path.write_text('$ ' + ' '.join(args) + '\n' + result.stdout)
    if result.returncode:
        raise RuntimeError(f'{name} exited {result.returncode}:\n{result.stdout[-6000:]}')
    return result.stdout


def boot(args, name, required, timeout=25):
    path = LOGS / f'{name}.log'
    with path.open('w') as out:
        out.write('$ ' + ' '.join(args) + '\n')
        out.flush()
        proc = subprocess.Popen(args, cwd=ROOT, stdout=out,
                                stderr=subprocess.STDOUT, start_new_session=True)
        deadline = time.monotonic() + timeout
        text = ''
        try:
            while time.monotonic() < deadline:
                try:
                    text = path.read_text(errors='replace')
                except OSError as error:
                    if error.errno != 61:
                        raise
                    # DrvFS can briefly invalidate a read during a WSL transition.
                    time.sleep(0.1)
                    continue
                if 'kernel panic at' in text or 'FAIL' in text:
                    raise RuntimeError(f'{name} kernel failure:\n{text[-5000:]}')
                if all(item in text for item in required):
                    break
                if proc.poll() is not None:
                    raise RuntimeError(f'{name} exited early:\n{text[-5000:]}')
                time.sleep(0.1)
            else:
                raise RuntimeError(f'{name} timed out, missing {[x for x in required if x not in text]}:\n{text[-5000:]}')
        finally:
            if proc.poll() is None:
                os.killpg(proc.pid, signal.SIGTERM)
            proc.wait(timeout=4)
    text = path.read_text(errors='replace')
    match = re.search(r'free PA:.*pages=(\d+)', text)
    return {'name': name, 'passed': True, 'free_pages': int(match.group(1)) if match else None,
            'log': path.name, 'required_markers': required}


def build(pmm, slub=False):
    options = [f'PMM={pmm}'] + (['SLUB_TEST=1'] if slub else [])
    tag = pmm + ('-slub' if slub else '')
    command(['make', 'clean'], f'clean-{tag}')
    command(['make', '-j4'] + options, f'build-{tag}')


def main():
    global LOGS
    parser = argparse.ArgumentParser()
    parser.add_argument('--stage', choices=['core', 'buddy', 'slub', 'all'], default='all')
    parser.add_argument('--compact', action='store_true')
    parser.add_argument('--qemu', default=shutil.which('qemu-system-riscv64'))
    parser.add_argument('--log-label', default='')
    args = parser.parse_args()
    assert args.qemu and Path(args.qemu).is_file(), 'QEMU executable not found'
    if args.log_label:
        assert re.fullmatch(r'[A-Za-z0-9_-]+', args.log_label)
        LOGS = LOGS / args.log_label
    qemu = str(Path(args.qemu).resolve())
    qemu_version = subprocess.check_output([qemu, '--version'], text=True).splitlines()[0]
    LOGS.mkdir(parents=True, exist_ok=True)
    results = []
    source_audit = audit()
    stages = ['core', 'buddy', 'slub'] if args.stage == 'all' else [args.stage]
    if 'core' in stages:
        for pmm in ['first_fit', 'best_fit']:
            build(pmm)
            for ram in [64, 128]:
                required = [f'memory management: {"default" if pmm == "first_fit" else "best_fit"}_pmm_manager',
                            'check_alloc_page() succeeded!', 'satp physical address:']
                if pmm == 'first_fit':
                    required.append('[First-Fit regression] tail insertion/coalescing: PASS')
                results.append(boot([qemu, '-machine', 'virt', '-nographic', '-bios',
                                     'default', '-m', f'{ram}M', '-kernel', 'bin/ucore.img'],
                                    f'boot-{pmm}-{ram}m', required))
                print(f'PASS: {pmm}, RAM={ram} MiB', flush=True)
        results.append(boot([qemu, '-machine', 'virt', '-nographic', '-bios',
                             'default', '-m', '256M', '-kernel', 'bin/ucore.img'],
                            'boot-best_fit-256m-capped',
                            ['managed: [0x0000000080000000, 0x0000000088000000)',
                             'check_alloc_page() succeeded!']))
        command(['make', 'clean'], 'clean-before-test')
        results.append(boot(['make', 'test', f'QEMU={qemu}'], 'best-fit-test',
                            ['grading: 6 / 6 points', 'check_alloc_page() succeeded!',
                             'satp physical address:'], timeout=50))
        output = command(['make', 'grade', f'QEMU={qemu}'], 'official-grade')
        assert 'Total Score: 20/20' in output
        results.append({'name': 'official-grade', 'passed': True, 'score': '20/20', 'log': 'official-grade.log'})
        print('PASS: Best-Fit internal checks and official grade 20/20', flush=True)
    if 'buddy' in stages:
        build('buddy')
        for ram in [64, 128]:
            results.append(boot([qemu, '-machine', 'virt', '-nographic', '-bios',
                                 'default', '-m', f'{ram}M', '-kernel', 'bin/ucore.img'],
                                f'boot-buddy-{ram}m', ['[Buddy] ALL TESTS PASSED',
                                 'check_alloc_page() succeeded!', 'satp physical address:'], timeout=35))
            print(f'PASS: Buddy, RAM={ram} MiB', flush=True)
    if 'slub' in stages:
        for pmm in ['best_fit', 'buddy']:
            build(pmm, slub=True)
            results.append(boot([qemu, '-machine', 'virt', '-nographic', '-bios',
                                 'default', '-kernel', 'bin/ucore.img'], f'boot-slub-{pmm}',
                                ['[SLUB] ALL TESTS PASSED', 'slub physical pages in use: 0',
                                 'check_alloc_page() succeeded!'], timeout=35))
            print(f'PASS: SLUB on {pmm}', flush=True)
    summary = {'source_audit': source_audit, 'stage': args.stage,
               'qemu': {'path': qemu, 'version': qemu_version}, 'results': results}
    (LOGS / f'verification-{args.stage}.json').write_text(json.dumps(summary, indent=2))
    if args.compact:
        print(f'VERIFIED: {len(results)} scenarios; fixed Best-Fit tests unchanged; grading injection clean.')
        print('Runtime: ' + qemu_version + ' (' + qemu + ')')
        print('Detailed results: ' + str(LOGS / f'verification-{args.stage}.json'))
    else:
        print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
