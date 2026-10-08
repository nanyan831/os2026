# Lab 1 交互提示词汇总

**第 1 轮 · 问：** 我们现在开始做 Lab 1，起始代码已经放在项目中。这两项练习分别要解决什么问题？先从哪些文件看起比较合适？

**答：**

先围绕“最小内核如何启动”分析代码。练习一关注入口汇编如何建立栈并进入 C 函数；练习二通过 GDB 观察复位代码、OpenSBI 和内核入口之间的执行过程。

如果当前使用 Windows PowerShell，先输入：

```powershell
wsl -d Ubuntu-22.04
```

进入 Ubuntu 后，输入本机工程路径与查看命令：

```bash
cd '/mnt/d/缪臻/日常应用/南开大学/大三上/操作系统/labcodes/code'
pwd
ls
sed -n '1,80p' kern/init/entry.S
```

检查 `ls` 输出是否包含 `Makefile`、`kern`、`libs`、`tools`，再在显示的源码中找到 `la sp, bootstacktop` 和 `tail kern_init`。后续普通 shell 命令均在这个目录执行；其他机器需要换成自己的 `code/` 路径。

**下一步：** 先看 `entry.S` 的第一条指令，弄清 `la sp, bootstacktop` 的作用。

---

**第 2 轮 · 问：** `la sp, bootstacktop` 取的是地址还是内存中的值？为什么进入 C 函数前要设置栈？栈的大小又在哪里确定？

**答：**

`la` 把符号地址装入寄存器，这里把 `bootstacktop` 的地址写入栈指针 `sp`。C 函数可能使用局部变量、保存寄存器和返回地址，因此需要先建立可用的栈。

`bootstack` 在 `.data` 中通过 `.space KSTACKSIZE` 静态预留空间。查看 `kern/mm/memlayout.h` 和 `kern/mm/mmu.h`，可以找到 `KSTACKPAGE=2`、`PGSIZE=4096`，因此栈大小为 8192 字节。栈向低地址增长，初始 SP 放在高地址边界 `bootstacktop`。

**下一步：** 接着看 `tail kern_init`，分析它如何把执行流程交给 C 函数。

---

**第 3 轮 · 问：** `tail kern_init` 与普通 `call` 有什么区别？为什么这里的 C 函数要声明为 `noreturn`？

**答：**

`tail` 将控制权转交给 `kern_init`，不会建立返回到当前入口的新返回地址。普通 `call` 则会写入返回地址 `ra`。

打开 `init.c`，可以看到 `kern_init` 最后进入无限循环，执行流程不需要返回到汇编入口。`noreturn` 将这一约定告知编译器；实际的不返回行为由函数执行逻辑保证。

**下一步：** 查看链接脚本，确定 `kern_entry` 和 `kern_init` 的地址是怎样安排的。

---

**第 4 轮 · 问：** 内核入口 `0x80200000` 在哪里设置？为什么 Makefile 要生成 `bin/kernel` 和 `bin/ucore.img` 两个文件？

**答：**

`tools/kernel.ld` 设置基地址和入口符号，并安排代码、只读数据、可写数据等段。`ENTRY(kern_entry)` 设置 ELF 入口信息，实际指令位置仍需结合段布局和符号表检查。

编译、汇编先生成目标文件，链接器再生成带符号的 ELF 文件 `bin/kernel`。随后 `objcopy` 将装载内容提取为裸镜像 `bin/ucore.img`。GDB 使用 ELF 符号，QEMU 使用镜像启动；裸镜像不含 ELF 头，装载地址由启动配置决定。

**下一步：** 在 Ubuntu 中执行构建，核对工具链和实际生成的文件。

---

**第 5 轮 · 问：** 我们现在开始编译。应该执行什么命令，怎样判断构建成功？编译通过是不是就代表内核能运行？

**答：**

在 Ubuntu 普通终端中输入以下命令，不要在 GDB 提示符中输入：

```bash
riscv64-unknown-elf-gcc -dumpmachine
make
```

先确认目标名称为 `riscv64-unknown-elf`。观察 `make` 是否正常结束；需要构建时会出现编译、链接及 `objcopy` 的输出。如果提示没有需要更新的目标，表示现有产物已是最新状态。紧接着输入：

```bash
echo $?
file bin/kernel bin/ucore.img
ls -lh bin/kernel bin/ucore.img obj/kernel.asm obj/kernel.sym
```

`echo $?` 应在 `make` 后立即执行，用于查看它的退出状态；0 表示成功。`file` 应将 `bin/kernel` 识别为 RISC-V ELF 文件，裸镜像可能仅显示为 `data`。再检查四个产物是否存在。构建成功还需要通过 QEMU 验证启动；若失败，先保留第一处错误信息，按该错误定位。

**下一步：** 执行 `make qemu`，检查固件输出和内核启动字符串。

---

**第 6 轮 · 问：** 使用 `-device loader,file=...,addr=0x80200000` 启动时，只看到 OpenSBI，没有内核字符串。我们应该先检查哪里，再决定修改什么？

**答：**

先在运行 QEMU 的终端中看 `Domain0 Next Address` 和 `Next Mode`。如果下一阶段地址为 `0x0`，优先检查固件与内核之间的启动交接。

按 `Ctrl+A`，松开后按 `X`，退出当前 QEMU。回到 Ubuntu shell 后，用明确的参数运行：

```bash
qemu-system-riscv64 -machine virt -nographic -bios default -kernel bin/ucore.img
```

逐项观察：下一阶段地址是否为 `0x80200000`，模式是否为 `S-mode`，最后是否出现 `(THU.CST) os is loading ...`。只有固件横幅还不能证明内核启动。

确认出现上述输出后，按下面的顺序操作：

1. **退出刚才的 QEMU。** 按 `Ctrl+A`，松开后按 `X`，回到 Ubuntu shell。

2. **修改启动参数。** 输入 `nano Makefile`，找到 `qemu` 和 `debug` 两个目标，把其中的 `-device loader,file=$(UCOREIMG),addr=0x80200000` 替换为 `-kernel $(UCOREIMG)`。保留原有缩进、行末续行符，以及 `debug` 中的 `-s -S`。已是 `-kernel $(UCOREIMG)` 的行保持原样。按 `Ctrl+O`、回车保存，再按 `Ctrl+X` 退出编辑器。

3. **检查 Makefile 的启动效果。** 在 Ubuntu shell 输入：

   ```bash
   make qemu
   ```

   预期再次看到下一阶段地址 `0x80200000`、模式 `S-mode` 和 `(THU.CST) os is loading ...`，说明通过 Makefile 启动也能进入内核。

4. **结束本次运行。** 按 `Ctrl+A`，松开后按 `X`，确认回到 Ubuntu shell，再进行下一轮调试。

**下一步：** 确认内核字符串出现后，使用 GDB 检查实际的启动路径。

---

**第 7 轮 · 问：** 我们想从第一条指令开始调试，但本机只有 `gdb-multiarch`。怎样连接 QEMU？`-s` 和 `-S` 有什么区别？

**答：**

准备两个 Ubuntu 终端，分别记为 A、B；两边都先执行第 1 轮的 `cd` 命令，进入相同的 `code/` 目录。

终端 A 输入：

```bash
make debug
```

保持 A 运行。终端 B 输入：

```bash
make gdb
```

观察 B 是否出现 `(gdb)` 提示符。从此，标为 `gdb` 的命令在 B 的这个提示符后输入；不要输入到 A，也不要输入到普通 bash 提示符。

`-S` 暂停虚拟 CPU，`-s` 开启默认 1234 端口的 GDB 服务。Makefile 可以回退到 `gdb-multiarch`，并加载 `bin/kernel` 的符号。如果连接被拒绝，先确认 A 仍在运行 `make debug`，再检查地址和端口。

**下一步：** 连接后查看 `$pc` 和附近指令，判断当前停在哪一启动阶段。

---

**第 8 轮 · 问：** GDB 中初始 PC 是 `0x1000`，这里就是 OpenSBI 吗？怎样观察复位代码跳转到固件？

**答：**

在终端 B 的 `(gdb)` 提示符后，逐行输入：

```gdb
p/x $pc
x/6i $pc
x/2gx 0x1018
```

`p/x` 按十六进制显示 PC；`x/6i` 查看当前位置的六条指令；`x/2gx` 查看两个 8 字节的数据值。本次 `virt` 配置下，初始 PC 预期为 `0x1000`，这里是复位 ROM。数据区中应能看到固件入口 `0x80000000`。

然后输入：

```gdb
b *0x80000000
c
p/x $pc
```

`b` 设置执行断点，`c` 继续运行。观察是否出现断点停止信息，以及此时 PC 是否为 `0x80000000`。该入口属于 OpenSBI，复位代码在跳转前准备 hart ID、设备树等参数。

**下一步：** 在内核入口设置断点，观察 OpenSBI 向内核移交控制权。

---

**第 9 轮 · 问：** 怎样证明程序进入了 `kern_entry`，并且 `la` 确实设置了 SP？我们应该如何单步检查？

**答：**

保持当前 GDB 会话，在 OpenSBI 入口断点处输入：

```gdb
b *0x80200000
c
info registers pc sp a0 a1
p/x &bootstacktop
x/3i $pc
```

观察 PC 是否到达 `kern_entry`。此时先记下 SP，它尚未执行内核的栈设置。`p/x &bootstacktop` 给出应设置的栈顶；反汇编用于确认 `la` 展开成几条机器指令。

本次展开为两条，因此接着逐行输入：

```gdb
si
si
info registers pc sp
p $sp == (unsigned long)&bootstacktop
```

每次 `si` 只执行一条机器指令。两次之后，预期 PC 位于尾跳转处，SP 等于栈顶；最后的比较表达式应得到 1，表示相等。当前构建的栈顶为 `0x80203000`，具体数值以本次符号为准。

最后输入：

```gdb
b kern_init
c
p/x $pc
```

观察是否停在 C 入口 `kern_init`，从而验证汇编到 C 的交接。

**下一步：** 进入 `kern_init` 后，检查 `memset` 初始化的地址范围。

---

**第 10 轮 · 问：** `memset(edata, 0, end - edata)` 中的两个符号在哪里定义？如果调试时发现它们相等，是不是出错了？

**答：**

保持在 `kern_init` 的断点处，在 GDB 中输入：

```gdb
p/x &edata
p/x &end
p (unsigned long)&end - (unsigned long)&edata
```

前两条显示边界地址，第三条计算两者相差的字节数。当前构建中预期两者均为 `0x80203008`，差值为 0，说明没有非空 BSS 区域需要在这里清零。

`edata` 和 `end` 由链接脚本提供，C 中通过 `extern` 声明使用其地址。若换了构建后差值不为 0，就结合段布局分析该范围。长度为 0 本身并不说明初始化失败。

**下一步：** 继续看 `cprintf`，追踪启动字符串如何输出到终端。

---

**第 11 轮 · 问：** 内核还没有完整设备驱动，`cprintf` 是怎样输出字符的？这里的 `ecall` 属于普通用户程序的系统调用吗？

**答：**

保持 A 的 QEMU 和 B 的 GDB 会话，再开一个 Ubuntu 普通终端 C，执行第 1 轮的 `cd` 命令，然后输入：

```bash
sed -n '1,110p' kern/libs/stdio.c
sed -n '1,80p' kern/driver/console.c
sed -n '1,80p' libs/sbi.c
```

在这些源码中依次找到 `cprintf`、`vcprintf`、`cputch`、`cons_putc`、`sbi_console_putchar` 和 `sbi_call`，再观察内联汇编中的寄存器赋值与 `ecall`。

调用链为 `cprintf → vcprintf → vprintfmt → cputch → cons_putc → sbi_console_putchar → sbi_call → ecall`。本工程使用 legacy SBI：`a7` 放服务号，`a0` 等寄存器放参数，字符输出服务号为 1。这是 S 模式内核请求 M 模式 OpenSBI 提供服务，与 U 模式应用向操作系统发起调用的层次不同。

**下一步：** 结合入口处的内存内容，进一步检查内核镜像的装载时机。

---

**第 12 轮 · 问：** 我们用 `watch *0x80200000` 观察内核装载，却没有看到对应写入。怎样判断是没有装载，还是观察时机不合适？

**答：**

这个观察必须从一次新的启动开始。当前程序已经推进到 C 入口，不能直接把此时的内存内容当成复位状态。

先在 B 的 GDB 中输入：

```gdb
detach
quit
```

再到 A 按 `Ctrl+A`，松开后按 `X`，退出 QEMU。A 重新执行 `make debug`，B 重新执行 `make gdb`。连接后先不要执行 `c`，在 B 输入：

```gdb
p/x $pc
x/3i 0x80200000
```

观察是否同时满足：PC 尚在 `0x1000`，但 `0x80200000` 已经显示出内核入口指令。若满足，说明 QEMU 在 CPU 执行前已经装载镜像。之后设置的 watch 无法捕捉此前的写入；随后可以用入口断点验证 CPU 到达该地址。

**下一步：** 检查栈顶地址附近的符号，确认 GDB 的名称标注是否影响对 SP 的判断。

---

**第 13 轮 · 问：** GDB 把设置后的 SP 标成 `0x80203000 <SBI_CONSOLE_PUTCHAR>`，这是不是说明栈指向了输出函数？怎样核对它是否仍位于正确的栈边界？

**答：**

在第 12 轮新启动的 GDB 会话中，先执行：

```gdb
b *0x80200000
c
x/3i $pc
si
si
```

先确认反汇编仍显示 `la` 展开为两条指令，再执行两次 `si`。此时停在设置完栈的位置，接着输入：

```gdb
p/x &bootstack
p/x &bootstacktop
p/x &SBI_CONSOLE_PUTCHAR
info registers sp
p $sp == (unsigned long)&bootstacktop
```

比较每条输出的数值。当前构建中，`bootstack=0x80201000`，`bootstacktop` 和 `SBI_CONSOLE_PUTCHAR` 的地址均为 `0x80203000`。SP 应等于栈顶，比较表达式应返回 1。

`SBI_CONSOLE_PUTCHAR` 是保存 SBI 服务号的全局变量，不是输出函数。`bootstacktop` 是上边界标签，本身不分配空间；后面的变量可以从同一地址开始。GDB 选择某个符号名称显示，不代表栈与变量重叠。实际栈区间为 `[bootstack, bootstacktop)`，向低地址增长。

**下一步：** 继续运行到初始化后的循环，检查当前 PC，并结束调试。

---

**第 14 轮 · 问：** 内核打印字符串后就没有新输出了，怎样判断它仍在正常运行？最后应该怎样退出 GDB 和 QEMU？

**答：**

在 GDB 中先输入：

```gdb
c
```

观察终端 A 是否打印启动字符串。若 B 又停在一个已有断点，核对当前位置后再继续。看到字符串后，回到 B 按 `Ctrl+C`，让 GDB 暂停目标并恢复提示符，然后输入：

```gdb
info registers pc
x/i $pc
```

核对当前指令是否处于 `kern_init` 末尾的循环位置。该循环不会重复打印，因此没有新输出符合当前逻辑；判断时要结合 PC 和反汇编。

退出时，在 B 输入：

```gdb
detach
quit
```

然后到 A 按 `Ctrl+A`，松开后按 `X`。确认两边恢复普通 shell 提示符，表示本次调试和 QEMU 运行已结束。

**下一步：** 核对启动字符串、关键入口与 `sp == &bootstacktop` 的证据，保存输出后结束本次操作。

---

以上问答为依据实际源码与运行证据整理补写的分阶段提示词。
