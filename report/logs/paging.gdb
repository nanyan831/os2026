set pagination off
set confirm off
set architecture riscv:rv64
file bin/kernel
target remote 127.0.0.1:1242
break *0x80200034
continue
printf "BEFORE SATP\n"
info registers pc satp
x/3i $pc
stepi
printf "AFTER SATP\n"
info registers pc satp
stepi
printf "NEXT INSTRUCTION\n"
info registers pc satp scause sepc stval
break kern_init
continue
printf "HIGH KERNEL ENTRY\n"
info registers pc sp satp
x/gx (char*)&boot_page_table_sv39+16
x/gx (char*)&boot_page_table_sv39+4088
detach
quit
