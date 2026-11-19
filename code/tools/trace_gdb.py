#!/usr/bin/env python3
import pathlib
import subprocess
import time
import os

os.chdir(pathlib.Path(__file__).resolve().parent.parent)
report_dir = pathlib.Path('report') if pathlib.Path('report').is_dir() else pathlib.Path('../report')
logs = report_dir / 'logs'
subprocess.run(['make', 'build-hello'], check=True, stdout=subprocess.DEVNULL)
host_script = logs / 'host.gdb'
host_script.write_text('''set pagination off
set confirm off
set $seen=0
break riscv_cpu_do_interrupt if cs->exception_index == 8 && ((RISCVCPU*)cs)->env.priv == 0
commands
silent
printf "HOST ECALL exception=%d guest_priv=%lu guest_pc=%#lx\\n", cs->exception_index, ((RISCVCPU*)cs)->env.priv, ((RISCVCPU*)cs)->env.pc
bt 4
set $seen=1
disable 1
continue
end
break helper_sret if $seen == 1 && env->sepc < 0x80000000
commands
silent
printf "HOST SRET priv=%lu sepc=%#lx mstatus=%#lx\\n", env->priv, env->sepc, env->mstatus
bt 4
set $seen=2
disable 2
continue
end
run
''')
guest_script = logs / 'guest.gdb'
disasm = subprocess.check_output(['riscv64-unknown-elf-objdump', '-d', 'bin/kernel'], text=True)
start = disasm.index('<__trapret>:')
import re
sret = re.search(r'^\s*([0-9a-f]+):.*\bsret\b', disasm[start:], re.M).group(1)
guest_script.write_text(f'''set pagination off
set confirm off
set remotetimeout 20
target remote localhost:1235
break exception_handler if tf->cause == 8
continue
printf "GUEST ECALL cause=%lu epc=%#lx syscall=%lu\\n", tf->cause, tf->epc, tf->gpr.a0
x/i tf->epc
disable 1
break *0x{sret}
continue
printf "GUEST before SRET sepc=%#lx sstatus=%#lx sscratch=%#lx\\n", $sepc, $sstatus, $sscratch
x/i $pc
set $user_return=$sepc
disable 2
tbreak *$user_return
continue
printf "GUEST after SRET pc=%#lx priv=%lu\\n", $pc, $priv
detach
quit
''')
with (logs / 'gdb-host.log').open('w') as hostlog, (logs / 'gdb-guest.log').open('w') as guestlog:
    host = subprocess.Popen(['gdb', '-batch', '-x', str(host_script), '--args', '/opt/qemu/bin/qemu-system-riscv64', '-machine', 'virt', '-nographic', '-serial', 'file:' + str(logs / 'gdb-serial.log'), '-monitor', 'none', '-bios', 'default', '-kernel', 'bin/ucore.img', '-S', '-gdb', 'tcp::1235'], stdin=subprocess.PIPE, stdout=hostlog, stderr=subprocess.STDOUT, start_new_session=True)
    try:
        time.sleep(2)
        result = subprocess.run(['gdb-multiarch', '-batch', 'bin/kernel', '-x', str(guest_script)], stdout=guestlog, stderr=subprocess.STDOUT, timeout=40)
        assert result.returncode == 0
    finally:
        import signal
        os.killpg(host.pid, signal.SIGTERM)
        host.wait(timeout=5)
assert 'HOST ECALL' in (logs / 'gdb-host.log').read_text()
assert 'HOST SRET' in (logs / 'gdb-host.log').read_text()
print('double GDB trace PASS')
