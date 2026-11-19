set pagination off
set confirm off
set remotetimeout 20
target remote localhost:1235
break exception_handler if tf->cause == 8
continue
printf "GUEST ECALL cause=%lu epc=%#lx syscall=%lu\n", tf->cause, tf->epc, tf->gpr.a0
x/i tf->epc
disable 1
break *0xffffffffc0200e2a
continue
printf "GUEST before SRET sepc=%#lx sstatus=%#lx sscratch=%#lx\n", $sepc, $sstatus, $sscratch
x/i $pc
set $user_return=$sepc
disable 2
tbreak *$user_return
continue
printf "GUEST after SRET pc=%#lx priv=%lu\n", $pc, $priv
detach
quit
