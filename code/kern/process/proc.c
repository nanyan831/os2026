#include <proc.h>
#include <kmalloc.h>
#include <string.h>
#include <sync.h>
#include <pmm.h>
#include <error.h>
#include <sched.h>
#include <elf.h>
#include <vmm.h>
#include <trap.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <riscv.h>
#include <sbi.h>

/* ------------- process/thread mechanism design&implementation -------------
(an simplified Linux process/thread mechanism )
introduction:
  ucore implements a simple process/thread mechanism. process contains the independent memory sapce, at least one threads
for execution, the kernel data(for management), processor state (for context switch), files(in lab6), etc. ucore needs to
manage all these details efficiently. In ucore, a thread is just a special kind of process(share process's memory).
------------------------------
process state       :     meaning               -- reason
    PROC_UNINIT     :   uninitialized           -- alloc_proc
    PROC_SLEEPING   :   sleeping                -- try_free_pages, do_wait, do_sleep
    PROC_RUNNABLE   :   runnable(maybe running) -- proc_init, wakeup_proc,
    PROC_ZOMBIE     :   almost dead             -- do_exit

-----------------------------
process state changing:

  alloc_proc                                 RUNNING
      +                                   +--<----<--+
      +                                   + proc_run +
      V                                   +-->---->--+
PROC_UNINIT -- proc_init/wakeup_proc --> PROC_RUNNABLE -- try_free_pages/do_wait/do_sleep --> PROC_SLEEPING --
                                           A      +                                                           +
                                           |      +--- do_exit --> PROC_ZOMBIE                                +
                                           +                                                                  +
                                           -----------------------wakeup_proc----------------------------------
-----------------------------
process relations
parent:           proc->parent  (proc is children)
children:         proc->cptr    (proc is parent)
older sibling:    proc->optr    (proc is younger sibling)
younger sibling:  proc->yptr    (proc is older sibling)
-----------------------------
related syscall for process:
SYS_exit        : process exit,                           -->do_exit
SYS_fork        : create child process, dup mm            -->do_fork-->wakeup_proc
SYS_wait        : wait process                            -->do_wait
SYS_exec        : after fork, process execute a program   -->load a program and refresh the mm
SYS_clone       : create child thread                     -->do_fork-->wakeup_proc
SYS_yield       : process flag itself need resecheduling, -- proc->need_sched=1, then scheduler will rescheule this process
SYS_sleep       : process sleep                           -->do_sleep
SYS_kill        : kill process                            -->do_kill-->proc->flags |= PF_EXITING
                                                                 -->wakeup_proc-->do_wait-->do_exit
SYS_getpid      : get the process's pid

*/

// the process set's list
list_entry_t proc_list;

#define HASH_SHIFT 10
#define HASH_LIST_SIZE (1 << HASH_SHIFT)
#define pid_hashfn(x) (hash32(x, HASH_SHIFT))

// has list for process set based on pid
static list_entry_t hash_list[HASH_LIST_SIZE];

// idle proc
struct proc_struct *idleproc = NULL;
// init proc
struct proc_struct *initproc = NULL;
// current proc
struct proc_struct *current = NULL;

static int nr_process = 0;

void kernel_thread_entry(void);
void forkrets(struct trapframe *tf);
void switch_to(struct context *from, struct context *to);

//lab4 prerequisite; lab5 function2 2413807 update alloc_proc
static struct proc_struct *alloc_proc(void)
{
    struct proc_struct *proc = kmalloc(sizeof(*proc));
    if (proc != NULL) {
        memset(proc, 0, sizeof(*proc));
        proc->state = PROC_UNINIT;
        proc->pid = -1;
        proc->pgdir = boot_pgdir_pa;
        list_init(&proc->list_link);
        list_init(&proc->hash_link);
    }
    return proc;
}

void proc_run(struct proc_struct *proc)
{
    bool intr_flag;
    local_intr_save(intr_flag);
    if (proc != current) {
        struct proc_struct *prev = current;
        current = proc;
        lsatp(proc->pgdir);
        flush_tlb();
        switch_to(&prev->context, &proc->context);
    }
    local_intr_restore(intr_flag);
}


// set_proc_name - set the name of proc
char *
set_proc_name(struct proc_struct *proc, const char *name)
{
    memset(proc->name, 0, sizeof(proc->name));
    return memcpy(proc->name, name, strnlen(name, PROC_NAME_LEN));
}

// get_proc_name - get the name of proc
char *
get_proc_name(struct proc_struct *proc)
{
    static char name[PROC_NAME_LEN + 1];
    memset(name, 0, sizeof(name));
    return memcpy(name, proc->name, PROC_NAME_LEN);
}

// set_links - set the relation links of process
static void
set_links(struct proc_struct *proc)
{
    list_add(&proc_list, &(proc->list_link));
    proc->yptr = NULL;
    if ((proc->optr = proc->parent->cptr) != NULL)
    {
        proc->optr->yptr = proc;
    }
    proc->parent->cptr = proc;
    nr_process++;
}

// remove_links - clean the relation links of process
static void
remove_links(struct proc_struct *proc)
{
    list_del(&(proc->list_link));
    if (proc->optr != NULL)
    {
        proc->optr->yptr = proc->yptr;
    }
    if (proc->yptr != NULL)
    {
        proc->yptr->optr = proc->optr;
    }
    else
    {
        proc->parent->cptr = proc->optr;
    }
    nr_process--;
}

// get_pid - alloc a unique pid for process
static int
get_pid(void)
{
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;
    static int next_safe = MAX_PID, last_pid = MAX_PID;
    if (++last_pid >= MAX_PID)
    {
        last_pid = 1;
        goto inside;
    }
    if (last_pid >= next_safe)
    {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list;
        while ((le = list_next(le)) != list)
        {
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid)
            {
                if (++last_pid >= next_safe)
                {
                    if (last_pid >= MAX_PID)
                    {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            }
            else if (proc->pid > last_pid && next_safe > proc->pid)
            {
                next_safe = proc->pid;
            }
        }
    }
    return last_pid;
}






// forkret -- the first kernel entry point of a new thread/process
// NOTE: the addr of forkret is setted in copy_thread function
//       after switch_to, the current proc will execute here.
static void
forkret(void)
{
    forkrets(current->tf);
}

// hash_proc - add proc into proc hash_list
static void
hash_proc(struct proc_struct *proc)
{
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}

// unhash_proc - delete proc from proc hash_list
static void
unhash_proc(struct proc_struct *proc)
{
    list_del(&(proc->hash_link));
}

// find_proc - find proc frome proc hash_list according to pid
struct proc_struct *
find_proc(int pid)
{
    if (0 < pid && pid < MAX_PID)
    {
        list_entry_t *list = hash_list + pid_hashfn(pid), *le = list;
        while ((le = list_next(le)) != list)
        {
            struct proc_struct *proc = le2proc(le, hash_link);
            if (proc->pid == pid)
            {
                return proc;
            }
        }
    }
    return NULL;
}

//lab5 function2 2413807 update kernel_thread
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)
{
    struct trapframe tf = {0};
    tf.gpr.s0 = (uintptr_t)fn;
    tf.gpr.s1 = (uintptr_t)arg;
    tf.epc = (uintptr_t)kernel_thread_entry;
    tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE;
    return do_fork(clone_flags | CLONE_VM, 0, &tf);
}


// setup_kstack - alloc pages with size KSTACKPAGE as process kernel stack
static int
setup_kstack(struct proc_struct *proc)
{
    struct Page *page = alloc_pages(KSTACKPAGE);
    if (page != NULL)
    {
        proc->kstack = (uintptr_t)page2kva(page);
        return 0;
    }
    return -E_NO_MEM;
}

// put_kstack - free the memory space of process kernel stack
static void
put_kstack(struct proc_struct *proc)
{
    free_pages(kva2page((void *)(proc->kstack)), KSTACKPAGE);
}

// setup_pgdir - alloc one page as PDT
static int
setup_pgdir(struct mm_struct *mm)
{
    struct Page *page;
    if ((page = alloc_page()) == NULL)
    {
        return -E_NO_MEM;
    }
    pde_t *pgdir = page2kva(page);
    memcpy(pgdir, boot_pgdir_va, PGSIZE);

    mm->pgdir = pgdir;
    return 0;
}

// put_pgdir - free the memory space of PDT
static void
put_pgdir(struct mm_struct *mm)
{
    free_page(kva2page(mm->pgdir));
}

//lab5 function3 2413807 update copy_mm
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc)
{
    struct mm_struct *oldmm = current->mm, *mm;
    if (oldmm == NULL) return 0;
    if (clone_flags & CLONE_VM) {
        mm = oldmm;
    } else {
        if ((mm = mm_create()) == NULL) return -E_NO_MEM;
        int ret = setup_pgdir(mm);
        if (ret != 0) { mm_destroy(mm); return ret; }
        lock_mm(oldmm);
        ret = dup_mmap(mm, oldmm);
        unlock_mm(oldmm);
        if (ret != 0) {
            exit_mmap(mm);
            put_pgdir(mm);
            mm_destroy(mm);
            return ret;
        }
    }
    mm_count_inc(mm);
    proc->mm = mm;
    proc->pgdir = PADDR(mm->pgdir);
    return 0;
}

static void copy_thread(struct proc_struct *proc, uintptr_t stack, struct trapframe *tf)
{
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1;
    *proc->tf = *tf;
    proc->tf->gpr.a0 = 0;
    proc->tf->gpr.sp = stack ? stack : proc->kstack + KSTACKSIZE;
    proc->tf->status &= ~SSTATUS_SIE;
    proc->context.ra = (uintptr_t)forkret;
    proc->context.sp = (uintptr_t)proc->tf;
}



//lab5 function2 2413807 ELF loading and exec
static int load_icode(unsigned char *binary, size_t size)
{
    if (size < sizeof(struct elfhdr)) return -E_INVAL_ELF;
    struct elfhdr *elf = (struct elfhdr *)binary;
    if (elf->e_magic != ELF_MAGIC || elf->e_machine != 243 ||
        elf->e_phentsize != sizeof(struct proghdr) ||
        elf->e_phoff > size || elf->e_phnum > (size - elf->e_phoff) / sizeof(struct proghdr))
        return -E_INVAL_ELF;
    struct mm_struct *mm = mm_create();
    if (mm == NULL) return -E_NO_MEM;
    int ret = setup_pgdir(mm);
    if (ret != 0) { mm_destroy(mm); return ret; }
    struct proghdr *ph = (void *)(binary + elf->e_phoff);
    for (int i = 0; i < elf->e_phnum; i++, ph++) {
        if (ph->p_type != ELF_PT_LOAD || ph->p_memsz == 0) continue;
        ret = -E_INVAL_ELF;
        if (ph->p_filesz > ph->p_memsz || ph->p_offset > size ||
            ph->p_filesz > size - ph->p_offset || ph->p_va >= USERTOP ||
            ph->p_memsz > USERTOP - ph->p_va) goto fail;
        uint32_t flags = 0, perm = PTE_U;
        if (ph->p_flags & ELF_PF_R) { flags |= VM_READ; perm |= PTE_R; }
        if (ph->p_flags & ELF_PF_W) { flags |= VM_WRITE; perm |= PTE_W | PTE_R; }
        if (ph->p_flags & ELF_PF_X) { flags |= VM_EXEC; perm |= PTE_X; }
        if (!(perm & (PTE_R | PTE_X))) goto fail;
        ret = mm_map(mm, ph->p_va, ph->p_memsz, flags, NULL);
        if (ret != 0) goto fail;
        uintptr_t end = ph->p_va + ph->p_memsz;
        for (uintptr_t va = ROUNDDOWN(ph->p_va, PGSIZE); va < end; va += PGSIZE) {
            struct Page *page = pgdir_alloc_page(mm->pgdir, va, perm);
            if (page == NULL) { ret = -E_NO_MEM; goto fail; }
            memset(page2kva(page), 0, PGSIZE);
            uintptr_t lo = va > ph->p_va ? va : ph->p_va;
            uintptr_t hi = va + PGSIZE < ph->p_va + ph->p_filesz ?
                           va + PGSIZE : ph->p_va + ph->p_filesz;
            if (hi > lo) memcpy(page2kva(page) + (lo - va),
                                binary + ph->p_offset + lo - ph->p_va, hi - lo);
        }
    }
    struct vma_struct *entry = find_vma(mm, elf->e_entry);
    if (entry == NULL || !(entry->vm_flags & VM_EXEC)) { ret = -E_INVAL_ELF; goto fail; }
    ret = mm_map(mm, USTACKTOP - USTACKSIZE, USTACKSIZE,
                 VM_READ | VM_WRITE | VM_STACK, NULL);
    if (ret != 0) goto fail;
    for (int i = 1; i <= 4; i++) {
        struct Page *page = pgdir_alloc_page(mm->pgdir, USTACKTOP - i * PGSIZE,
                                             PTE_U | PTE_R | PTE_W);
        if (page == NULL) { ret = -E_NO_MEM; goto fail; }
        memset(page2kva(page), 0, PGSIZE);
    }
    mm_count_inc(mm);
    current->mm = mm;
    current->pgdir = PADDR(mm->pgdir);
    lsatp(current->pgdir);
    flush_tlb();
    asm volatile("fence.i" ::: "memory");
    struct trapframe *tf = current->tf;
    memset(tf, 0, sizeof(*tf));
    tf->gpr.sp = USTACKTOP;
    tf->epc = elf->e_entry;
    tf->status = SSTATUS_SPIE | SSTATUS_SUM;
    return 0;
fail:
    exit_mmap(mm);
    put_pgdir(mm);
    mm_destroy(mm);
    return ret;
}

static void release_mm(struct proc_struct *proc)
{
    struct mm_struct *mm = proc->mm;
    if (mm != NULL) {
        if (proc == current) { lsatp(boot_pgdir_pa); flush_tlb(); }
        proc->mm = NULL;
        proc->pgdir = boot_pgdir_pa;
        if (mm_count_dec(mm) == 0) {
            exit_mmap(mm);
            put_pgdir(mm);
            mm_destroy(mm);
        }
    }
}

int do_execve(const char *name, size_t len, unsigned char *binary, size_t size)
{
    char local_name[PROC_NAME_LEN + 1] = {0};
    if (len > PROC_NAME_LEN) len = PROC_NAME_LEN;
    if (!copy_from_user(current->mm, local_name, name, len, 0)) return -E_INVAL;
    unsigned char *owned = NULL;
    if (current->mm != NULL) {
        if (size == 0 || size > 16 * 1024 * 1024) return -E_INVAL;
        owned = kmalloc(size);
        if (owned == NULL) return -E_NO_MEM;
        if (!copy_from_user(current->mm, owned, binary, size, 0)) {
            kfree(owned); return -E_INVAL;
        }
        binary = owned;
    } else if (!user_mem_check(NULL, (uintptr_t)binary, size, 0)) return -E_INVAL;
    release_mm(current);
    int ret = load_icode(binary, size);
    kfree(owned);
    if (ret != 0) do_exit(ret);
    set_proc_name(current, local_name);
    return 0;
}

int do_wait(int pid, int *code_store)
{
    if (pid < 0) return -E_BAD_PROC;
    if (code_store != NULL && !user_mem_check(current->mm, (uintptr_t)code_store,
                                             sizeof(int), 1)) return -E_INVAL;
    bool intr_flag;
repeat:
    local_intr_save(intr_flag);
    bool has_child = 0;
    struct proc_struct *proc = current->cptr;
    for (; proc != NULL; proc = proc->optr) {
        if (pid != 0 && proc->pid != pid) continue;
        has_child = 1;
        if (proc->state == PROC_ZOMBIE) {
            if (code_store != NULL && !copy_to_user(current->mm, code_store,
                                                    &proc->exit_code, sizeof(int))) {
                local_intr_restore(intr_flag); return -E_INVAL;
            }
            unhash_proc(proc);
            remove_links(proc);
            put_kstack(proc);
            kfree(proc);
            local_intr_restore(intr_flag);
            return 0;
        }
    }
    if (!has_child) { local_intr_restore(intr_flag); return -E_BAD_PROC; }
    current->state = PROC_SLEEPING;
    current->wait_state = WT_CHILD;
    schedule();
    local_intr_restore(intr_flag);
    if (current->flags & PF_EXITING) do_exit(-E_KILLED);
    goto repeat;
}


//lab5 function3 2413807
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)
{
    if (nr_process >= MAX_PROCESS) return -E_NO_FREE_PROC;
    struct proc_struct *proc = alloc_proc();
    if (proc == NULL) return -E_NO_MEM;
    proc->parent = current;
    int ret = setup_kstack(proc);
    if (ret != 0) goto free_proc;
    ret = copy_mm(clone_flags, proc);
    if (ret != 0) goto free_stack;
    copy_thread(proc, stack, tf);
    bool intr_flag;
    local_intr_save(intr_flag);
    ret = proc->pid = get_pid();
    hash_proc(proc);
    set_links(proc);
    wakeup_proc(proc);
    local_intr_restore(intr_flag);
    return ret;
free_stack:
    put_kstack(proc);
free_proc:
    kfree(proc);
    return ret;
}


//lab5 function4 2413807
int do_exit(int error_code)
{
    assert(current != idleproc && current != initproc);
    release_mm(current);
    bool intr_flag;
    local_intr_save(intr_flag);
    current->state = PROC_ZOMBIE;
    current->exit_code = error_code;
    struct proc_struct *parent = current->parent;
    if (parent->wait_state == WT_CHILD) wakeup_proc(parent);
    while (current->cptr != NULL) {
        struct proc_struct *child = current->cptr;
        current->cptr = child->optr;
        if (child->optr != NULL) child->optr->yptr = NULL;
        child->parent = initproc;
        child->yptr = NULL;
        child->optr = initproc->cptr;
        if (child->optr != NULL) child->optr->yptr = child;
        initproc->cptr = child;
        if (child->state == PROC_ZOMBIE && initproc->wait_state == WT_CHILD)
            wakeup_proc(initproc);
    }
    schedule();
    panic("zombie process returned");
}

int do_yield(void) { current->need_resched = 1; return 0; }

int do_kill(int pid)
{
    bool intr_flag;
    local_intr_save(intr_flag);
    struct proc_struct *proc = find_proc(pid);
    int ret = -E_INVAL;
    if (proc != NULL && proc != initproc && proc->state != PROC_ZOMBIE) {
        ret = -E_KILLED;
        if (!(proc->flags & PF_EXITING)) {
            proc->flags |= PF_EXITING;
            if (proc->wait_state & WT_INTERRUPTED) wakeup_proc(proc);
            ret = 0;
        }
    }
    local_intr_restore(intr_flag);
    return ret;
}


#define __KERNEL_EXECVE(name, binary, size) ({           \
    cprintf("kernel_execve: pid = %d, name = \"%s\".\n", \
            current->pid, name);                         \
    kernel_execve(name, binary, (size_t)(size));         \
})

#define KERNEL_EXECVE(x) ({                                    \
    extern unsigned char _binary_obj___user_##x##_out_start[], \
        _binary_obj___user_##x##_out_size[];                   \
    __KERNEL_EXECVE(#x, _binary_obj___user_##x##_out_start,    \
                    _binary_obj___user_##x##_out_size);        \
})

#define __KERNEL_EXECVE2(x, xstart, xsize) ({   \
    extern unsigned char xstart[], xsize[];     \
    __KERNEL_EXECVE(#x, xstart, (size_t)xsize); \
})

#define KERNEL_EXECVE2(x, xstart, xsize) __KERNEL_EXECVE2(x, xstart, xsize)


//lab5 function2 2413807
static int kernel_execve(const char *name, unsigned char *binary, size_t size)
{
    // Build the first U-mode frame on this kernel stack, then use trap return.
    struct trapframe tf;
    current->tf = &tf;
    int ret = do_execve(name, strlen(name), binary, size);
    if (ret != 0) return ret;
    forkrets(&tf);
    panic("exec returned");
}

static int user_main(void *arg)
{
#ifdef TEST
    KERNEL_EXECVE2(TEST, TESTSTART, TESTSIZE);
#else
    KERNEL_EXECVE(hello);
#endif
    panic("user_main returned");
}

static int init_main(void *arg)
{
#ifdef COW_SELFTEST
    extern void check_cow(void);
    check_cow();
#endif
    kmalloc_trim();
    size_t free_before = nr_free_pages();
    int pid = kernel_thread(user_main, NULL, 0);
    assert(pid > 0);
    while (do_wait(0, NULL) == 0) schedule();
    cprintf("all user-mode processes have quit.\n");
    kmalloc_trim();
    assert(nr_process == 2 && list_next(&proc_list) == &initproc->list_link);
    assert(nr_free_pages() == free_before);
    cprintf("init check memory pass.\n");
    // The official grader stops at readline using GDB.
    extern void kmonitor(struct trapframe *tf);
    kmonitor(NULL);
    return 0;
}

void proc_init(void)
{
    list_init(&proc_list);
    for (int i = 0; i < HASH_LIST_SIZE; i++) list_init(&hash_list[i]);
    idleproc = alloc_proc();
    assert(idleproc != NULL);
    idleproc->pid = 0;
    idleproc->state = PROC_RUNNABLE;
    idleproc->kstack = (uintptr_t)bootstack;
    idleproc->need_resched = 1;
    set_proc_name(idleproc, "idle");
    nr_process++;
    current = idleproc;
    int pid = kernel_thread(init_main, NULL, 0);
    assert(pid > 0);
    initproc = find_proc(pid);
    assert(initproc != NULL && initproc->pid == 1);
    set_proc_name(initproc, "init");
}

void cpu_idle(void)
{
    while (1) if (current->need_resched) schedule();
}



