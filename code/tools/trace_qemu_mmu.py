#!/usr/bin/env python3
"""Host GDB drives an uninstalled debug QEMU 4.1.1 and records actual MMU data."""
import argparse
import json
import os
from pathlib import Path
import re
import signal
import subprocess

ROOT = Path(__file__).resolve().parents[1]
LOGS = ROOT.parent / 'report' / 'logs'


def source_line(path, fragment):
    for number, line in enumerate(path.read_text().splitlines(), 1):
        if fragment in line:
            return number
    raise RuntimeError(f'{fragment} not found in {path}')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, default=Path.home() / 'oslab2/qemu-4.1.1')
    parser.add_argument('--compact', action='store_true')
    args = parser.parse_args()
    source = args.source.resolve()
    qemu = source / 'build-debug/riscv64-softmmu/qemu-system-riscv64'
    assert qemu.is_file(), 'Build the debug QEMU first'
    symbols = subprocess.check_output(['riscv64-unknown-elf-nm', 'bin/kernel'], cwd=ROOT, text=True)
    probe = int(re.search(r'^([0-9a-f]+)\s+\w\s+mmu_probe_word$', symbols, re.M).group(1), 16)
    expected_pa = probe - 0xffffffff40000000
    helper = source / 'target/riscv/cpu_helper.c'
    cputlb = source / 'accel/tcg/cputlb.c'
    pte_line = source_line(helper, 'target_ulong ppn = pte >> PTE_PPN_SHIFT;')
    lookup_line = source_line(cputlb, 'if (unlikely(!tlb_hit(entry->addr_code, addr)))')
    prefix = ['set pagination off', 'set confirm off', 'set disable-randomization off',
              'handle SIGPIPE nostop noprint pass', 'set print pretty off']
    scenarios = {
        'data-walk': [
            f'break get_physical_address if addr == 0x{probe:x}', 'run',
            'printf "HOST GDB: DATA TRANSLATION ENTRY\\n"', 'bt 7',
            'printf "DATA addr=%#lx satp=%#lx access=%d mmu_idx=%d\\n", addr, env->satp, access_type, mmu_idx',
            f'break {helper}:{pte_line} if addr == 0x{probe:x}', 'continue',
            'printf "PTE level_iteration=%d ptshift=%d idx=%lu root=%#lx pte_addr=%#lx pte=%#lx\\n", i, ptshift, idx, base, pte_addr, pte',
            'next', 'printf "PTE physical_page_number=%#lx\\n", ppn', 'finish',
            'printf "TRANSLATED pa=%#lx prot=%d\\n", pa, prot',
            f'break tlb_set_page_with_attrs if vaddr == 0x{probe:x}', 'continue',
            'printf "TLB_FILL va=%#lx pa=%#lx size=%lu mmu_idx=%d prot=%d\\n", vaddr, paddr, size, mmu_idx, prot',
            'bt 5', 'quit',
        ],
        'bare': [
            'break get_physical_address if addr == 0x80200000 && env->satp == 0', 'run',
            'printf "HOST GDB: BARE TRANSLATION ENTRY\\n"',
            'printf "BARE addr=%#lx satp=%#lx access=%d mmu_idx=%d\\n", addr, env->satp, access_type, mmu_idx',
            'finish', 'printf "BARE_RESULT pa=%#lx prot=%d\\n", pa, prot',
            'break tlb_set_page_with_attrs if vaddr == 0x80200000', 'continue',
            'printf "BARE_SOFTWARE_TLB va=%#lx pa=%#lx size=%lu mmu_idx=%d\\n", vaddr, paddr, size, mmu_idx',
            'quit',
        ],
        'code-tlb': [
            'set $observations = 0',
            f'break {cputlb}:{lookup_line} if (addr & 0xfffffffffffff000) == 0xffffffffc0200000',
            'commands', 'silent',
            'printf "CODE_TLB addr=%#lx tag=%#lx index=%lu hit=%d satp=%#lx\\n", addr, entry->addr_code, index, entry->addr_code == (addr & 0xfffffffffffff000), env->satp',
            'set $observations = $observations + 1', 'if $observations == 4',
            'quit', 'end', 'continue', 'end', 'run',
        ],
    }
    LOGS.mkdir(parents=True, exist_ok=True)
    results = []
    for name, commands in scenarios.items():
        script = LOGS / f'qemu-{name}.gdb'
        script.write_text('\n'.join(prefix + commands) + '\n')
        command = ['gdb', '-q', '-nx', '-batch', '-x', str(script), '--args', str(qemu),
                   '-machine', 'virt', '-nographic', '-bios',
                   str(source / 'pc-bios/opensbi-riscv64-virt-fw_jump.bin'),
                   '-kernel', 'bin/ucore.img', '-singlestep']
        process = subprocess.Popen(command, cwd=ROOT, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True, start_new_session=True)
        try:
            stdout, _ = process.communicate(timeout=70)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGTERM)
            stdout, _ = process.communicate(timeout=5)
            (LOGS / f'qemu-{name}.log').write_text(stdout)
            raise RuntimeError(f'{name}: bounded host-debug timeout')
        result = subprocess.CompletedProcess(command, process.returncode, stdout)
        (LOGS / f'qemu-{name}.log').write_text(result.stdout)
        if args.compact:
            prefixes = ('HOST GDB:', 'DATA ', 'PTE ', 'TRANSLATED ', 'TLB_FILL ', 'BARE', 'CODE_TLB ')
            print('\n'.join(line for line in stdout.splitlines() if line.startswith(prefixes)), flush=True)
        else:
            print(result.stdout[-8000:], flush=True)
        assert result.returncode == 0, f'{name}: GDB exited {result.returncode}'
        if name == 'data-walk':
            pa = int(re.search(r'TRANSLATED pa=(0x[0-9a-f]+)', result.stdout).group(1), 16)
            assert pa == expected_pa
            assert 'level_iteration=0 ptshift=18 idx=511' in result.stdout
            assert 'TLB_FILL' in result.stdout
            results.append({'name': name, 'passed': True, 'va': hex(probe), 'pa': hex(pa),
                            'leaf': 'root (1 GiB)', 'log': f'qemu-{name}.log'})
        elif name == 'bare':
            assert 'BARE_RESULT pa=0x80200000' in result.stdout
            assert 'BARE_SOFTWARE_TLB' in result.stdout
            results.append({'name': name, 'passed': True, 'log': f'qemu-{name}.log'})
        else:
            observations = [line for line in result.stdout.splitlines() if line.startswith('CODE_TLB')]
            assert len(observations) == 4
            assert any('hit=0 ' in line for line in observations)
            assert any('hit=1 ' in line for line in observations)
            results.append({'name': name, 'passed': True, 'observations': observations,
                            'log': f'qemu-{name}.log'})
    summary = {'qemu_source': str(source), 'qemu_version': '4.1.1', 'results': results}
    (LOGS / 'verification-qemu.json').write_text(json.dumps(summary, indent=2))
    print('QEMU SOURCE DEBUG: 3 scenarios PASS' if args.compact else json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
