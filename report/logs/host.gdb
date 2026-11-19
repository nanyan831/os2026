set pagination off
set confirm off
set $seen=0
break riscv_cpu_do_interrupt if cs->exception_index == 8 && ((RISCVCPU*)cs)->env.priv == 0
commands
silent
printf "HOST ECALL exception=%d guest_priv=%lu guest_pc=%#lx\n", cs->exception_index, ((RISCVCPU*)cs)->env.priv, ((RISCVCPU*)cs)->env.pc
bt 4
set $seen=1
disable 1
continue
end
break helper_sret if $seen == 1 && env->sepc < 0x80000000
commands
silent
printf "HOST SRET priv=%lu sepc=%#lx mstatus=%#lx\n", env->priv, env->sepc, env->mstatus
bt 4
set $seen=2
disable 2
continue
end
run
