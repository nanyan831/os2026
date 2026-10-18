#!/usr/bin/env python3
"""Trace the actual guest transition through satp, using an isolated QEMU."""
import os
from pathlib import Path
import re
import signal
import subprocess
import time

root = Path(__file__).resolve().parents[1]
logs = root.parent / 'report' / 'logs'
logs.mkdir(parents=True, exist_ok=True)
asm = subprocess.check_output(['riscv64-unknown-elf-objdump', '-d', 'bin/kernel'], cwd=root, text=True)
match = re.search(r'^\s*([0-9a-f]+):[^\n]*csrw\s+satp', asm, re.M)
if not match:
    raise SystemExit('satp write not found')
csr_pa = int(match.group(1), 16) - 0xffffffff40000000
cmds = [
    'set pagination off', 'set confirm off', 'set architecture riscv:rv64',
    'file bin/kernel', 'target remote 127.0.0.1:1242',
    f'break *0x{csr_pa:x}', 'continue',
    'printf "BEFORE SATP\\n"', 'info registers pc satp', 'x/3i $pc',
    'stepi', 'printf "AFTER SATP\\n"', 'info registers pc satp',
    'stepi', 'printf "NEXT INSTRUCTION\\n"',
    'info registers pc satp scause sepc stval',
    'break kern_init', 'continue',
    'printf "HIGH KERNEL ENTRY\\n"', 'info registers pc sp satp',
    'x/gx (char*)&boot_page_table_sv39+16',
    'x/gx (char*)&boot_page_table_sv39+4088', 'detach', 'quit',
]
gdb_path = logs / 'paging.gdb'
gdb_path.write_text('\n'.join(cmds) + '\n')
with (logs / 'paging-qemu.log').open('w') as out:
    proc = subprocess.Popen(['qemu-system-riscv64', '-machine', 'virt', '-nographic',
                             '-bios', 'default', '-kernel', 'bin/ucore.img',
                             '-S', '-gdb', 'tcp:127.0.0.1:1242'], cwd=root,
                            stdout=out, stderr=subprocess.STDOUT, start_new_session=True)
    try:
        time.sleep(0.5)
        try:
            gdb = subprocess.run(['gdb-multiarch', '-q', '-nx', '-batch', '-x', str(gdb_path)],
                                 cwd=root, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                 text=True, timeout=12)
            output = gdb.stdout
        except subprocess.TimeoutExpired as error:
            output = (error.stdout or b'').decode(errors='replace') if isinstance(error.stdout, bytes) else error.stdout or ''
            output += '\nGDB bounded timeout (kernel entry not reached).\n'
        print(output, end='')
        (logs / 'paging-trace.log').write_text(output)
        if 'HIGH KERNEL ENTRY' not in output or 'Error in sourced command file' in output:
            raise SystemExit('Paging trace did not reach the high kernel entry')
    finally:
        if proc.poll() is None:
            os.killpg(proc.pid, signal.SIGTERM)
        proc.wait(timeout=3)
