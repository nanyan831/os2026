set pagination off
set confirm off
set disable-randomization off
handle SIGPIPE nostop noprint pass
set print pretty off
break get_physical_address if addr == 0x80200000 && env->satp == 0
run
printf "HOST GDB: BARE TRANSLATION ENTRY\n"
printf "BARE addr=%#lx satp=%#lx access=%d mmu_idx=%d\n", addr, env->satp, access_type, mmu_idx
finish
printf "BARE_RESULT pa=%#lx prot=%d\n", pa, prot
break tlb_set_page_with_attrs if vaddr == 0x80200000
continue
printf "BARE_SOFTWARE_TLB va=%#lx pa=%#lx size=%lu mmu_idx=%d\n", vaddr, paddr, size, mmu_idx
quit
