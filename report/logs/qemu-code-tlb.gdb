set pagination off
set confirm off
set disable-randomization off
handle SIGPIPE nostop noprint pass
set print pretty off
set $observations = 0
break /home/compiler/oslab2/qemu-4.1.1/accel/tcg/cputlb.c:1030 if (addr & 0xfffffffffffff000) == 0xffffffffc0200000
commands
silent
printf "CODE_TLB addr=%#lx tag=%#lx index=%lu hit=%d satp=%#lx\n", addr, entry->addr_code, index, entry->addr_code == (addr & 0xfffffffffffff000), env->satp
set $observations = $observations + 1
if $observations == 4
quit
end
continue
end
run
