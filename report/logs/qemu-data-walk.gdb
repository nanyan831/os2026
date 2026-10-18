set pagination off
set confirm off
set disable-randomization off
handle SIGPIPE nostop noprint pass
set print pretty off
break get_physical_address if addr == 0xffffffffc0207000
run
printf "HOST GDB: DATA TRANSLATION ENTRY\n"
bt 7
printf "DATA addr=%#lx satp=%#lx access=%d mmu_idx=%d\n", addr, env->satp, access_type, mmu_idx
break /home/compiler/oslab2/qemu-4.1.1/target/riscv/cpu_helper.c:254 if addr == 0xffffffffc0207000
continue
printf "PTE level_iteration=%d ptshift=%d idx=%lu root=%#lx pte_addr=%#lx pte=%#lx\n", i, ptshift, idx, base, pte_addr, pte
next
printf "PTE physical_page_number=%#lx\n", ppn
finish
printf "TRANSLATED pa=%#lx prot=%d\n", pa, prot
break tlb_set_page_with_attrs if vaddr == 0xffffffffc0207000
continue
printf "TLB_FILL va=%#lx pa=%#lx size=%lu mmu_idx=%d prot=%d\n", vaddr, paddr, size, mmu_idx, prot
bt 5
quit
