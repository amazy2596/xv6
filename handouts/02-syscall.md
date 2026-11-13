# 实验：系统调用 (Lab: system calls)

在上一个实验中，您使用系统调用编写了一些实用程序。在本实验中，您将向 xv6 添加一个新的系统调用，这将帮助您理解它们是如何工作的，并使您接触到 xv6 内核的一些内部结构。您将在以后的实验中添加更多的系统调用。

> [!NOTE]
> 在开始编码之前，阅读 [xv6 书籍](xv6/book-riscv-rev5.pdf)的第 2 章、第 4 章的第 4.3 和 4.4 节，以及相关的源文件：
> - 用户空间中将系统调用路由到内核的“存根 (stubs)”在 `user/usys.S` 中，这是在您运行 `make` 时由 `user/usys.pl` 生成的。声明在 `user/user.h` 中。
> - 将系统调用路由到实现它的内核函数的内核空间代码在 `kernel/syscall.c` 和 `kernel/syscall.h` 中。
> - 与进程相关的代码是 `kernel/proc.h` 和 `kernel/proc.c`。

要开始实验，请切换到 syscall 分支：
```
$ git fetch
$ git checkout syscall
$ make clean
```

## 使用 gdb (Using gdb) (Easy)

在许多情况下，print 语句足以调试您的内核，但有时单步执行代码或获取栈回溯 (stack backtrace) 是很有用的。GDB 调试器可以提供帮助。

为了帮助您熟悉 gdb，请运行 `make qemu-gdb`，然后在另一个窗口中启动 gdb（参见[指导页面](labs/guidance.html)上的 gdb 资料）。一旦打开了两个窗口，在 gdb 窗口中输入：
```
(gdb) b syscall
Breakpoint 1 at 0x80002142: file kernel/syscall.c, line 243.
(gdb) c
Continuing.
[Switching to Thread 1.2]

Thread 2 hit Breakpoint 1, syscall () at kernel/syscall.c:243
243     {
(gdb) layout src
(gdb) backtrace
```

`layout` 命令将窗口一分为二，显示 gdb 在源代码中的位置。`backtrace` 打印栈回溯。

在 `answers-syscall.txt` 中回答以下问题。

> **问题**：查看 backtrace 的输出，是哪个函数调用了 `syscall`？

输入 `n` 几次以单步跳过 `struct proc *p = myproc();`。一旦跳过该语句，输入 `p /x *p`，它将以十六进制形式打印当前进程的 `proc struct`（参见 `kernel/proc.h`）。

> **问题**：`p->trapframe->a7` 的值是多少？该值代表什么？（提示：查看 `user/init.c`——xv6 启动的第一个用户程序，以及它编译后的汇编代码 `user/init.asm`。）

处理器正运行在主管模式 (supervisor mode) 下，我们可以打印特权寄存器，例如 `sstatus`（参见 [RISC-V 特权指令](https://github.com/riscv/riscv-isa-manual/releases/download/Priv-v1.12/riscv-privileged-20211203.pdf)以获取说明）：
```
(gdb) p /x $sstatus
```

> **问题**：CPU 之前处于什么模式？

xv6 内核代码包含一致性检查，其失败会导致内核 panic；您可能会发现您的内核修改会导致 panic。例如，将 `syscall` 开头的语句 `num = p->trapframe->a7;` 替换为 `num = * (int *) 0;`，运行 `make qemu`，您将看到类似于以下内容：
```
xv6 kernel is booting

hart 2 starting
hart 1 starting
scause=0xd sepc=0x80001bfe stval=0x0
panic: kerneltrap
```
退出 `qemu`。

要追踪内核页错误 (page-fault) panic 的来源，请在包含已编译内核汇编的 `kernel/kernel.asm` 文件中搜索您刚刚看到的 panic 打印的 `sepc` 值。

> **问题**：写下内核发生 panic 处的汇编指令。哪个寄存器对应变量 `num`？

要在出错的指令处检查处理器和内核的状态，请启动 gdb，并在出错的 `epc` 处设置断点，如下所示：
```
(gdb) b *0x80001bfe
Breakpoint 1 at 0x80001bfe: file kernel/syscall.c, line 138.
(gdb) layout asm
(gdb) c
Continuing.
[Switching to Thread 1.3]

Thread 3 hit Breakpoint 1, syscall () at kernel/syscall.c:138
```

确认出错的汇编指令与您在上面找到的相同。

> **问题**：为什么内核会崩溃？提示：看书中的图 3-3；地址 0 在内核地址空间中被映射了吗？上面的 `scause` 值是否确认了这一点？（参见 [RISC-V 特权指令](https://github.com/riscv/riscv-isa-manual/releases/download/Priv-v1.12/riscv-privileged-20211203.pdf)中对 `scause` 的描述）

请注意，`scause` 是由上面的内核 panic 打印的，但通常您需要查看其他信息来追踪导致 panic 的问题。例如，要找出内核 panic时正在运行哪个用户进程，您可以打印该进程的名称：
```
(gdb) p p->name
```

> **问题**：内核 panic 时正在运行的进程名称是什么？它的进程 ID (`pid`) 是多少？

您可以根据需要重新访问[使用 GNU 调试器](https://pdos.csail.mit.edu/6.828/2019/lec/gdb_slides.pdf)。[指导页面](labs/guidance.html)也有调试提示。

## 沙箱化命令 (Sandbox a command) (Moderate)

> **要求**：在此任务中，您将“沙箱化 (sandbox)”一个进程，以限制它可以进行的系统调用。例如，可能会沙箱化一个进程以禁止打开文件。您将创建一个新的 `interpose` 系统调用，该系统调用将指定内核应拒绝来自调用进程的哪些系统调用。`interpose` 应该接受两个参数：一个整数掩码 (mask) 和一个路径。掩码的位指定要拒绝哪些系统调用。第二个参数您将在下一个任务中使用，在此任务中它总是 `"-"`。例如，为了让一个进程阻止自己使用 open 系统调用，它应该调用 `interpose(1 << SYS_open, "-")`，其中 `SYS_open` 是来自 `kernel/syscall.h` 的系统调用号。您的实现应该使掩码被 fork 的子进程继承，以便子进程继承父进程的限制。

我们为您提供了一个 `user/sandbox.c` 用户程序，它会 fork，在子进程中调用 `interpose()`，然后在子进程中 exec 一个程序。
当您完成 `interpose()` 的实现后，您应该会看到如下输出：
```
$ sandbox 32768 - cat README
cat: cannot open README
$  
```

在此示例中，`cat README` 是被沙箱化的命令。32768 是要拒绝的系统调用掩码；在此示例中它是 `1<<SYS_open`。如果您的解决方案正确，您应该会看到 "cat: cannot open README"。

如果您的解决方案通过了 `grade-lab-syscall sandbox_mask` 中的测试，即可获得满分：
```
$ ./grade-lab-syscall sandbox_mask
== Test sandbox_mask == sandbox_mask: OK (1.5s) 
```

一些提示：
- 将 `$U/_sandbox` 添加到 Makefile 中的 `UPROGS`。
- 运行 `make qemu`，您将看到编译器无法编译 `user/sandbox.c`，因为 `interpose` 系统调用的用户空间存根还不存在：在 `user/user.h` 中添加 `interpose` 的原型，在 `user/usys.pl` 中添加存根，在 `kernel/syscall.h` 中添加系统调用号。Makefile 会调用 perl 脚本 `user/usys.pl`，该脚本生成 `user/usys.S`（实际的系统调用存根），它们使用 RISC-V `ecall` 指令转换到内核。一旦您修复了编译问题，在 xv6 shell 中运行 `sandbox 32768 - cat README`；它会失败，因为您还没有在内核中实现该系统调用。
- 在 `kernel/sysproc.c` 中添加一个 `sys_interpose()` 函数，该函数通过在 `proc` 结构（参见 `kernel/proc.h`）的一个新字段中记录掩码参数来实现新的系统调用。从用户空间检索系统调用参数的函数在 `kernel/syscall.c` 中，您可以在 `kernel/sysproc.c` 中看到它们的使用示例。将您的新 `sys_interpose` 添加到 `kernel/syscall.c` 中的 `syscalls` 数组。
- 修改 `kfork()`（参见 `kernel/proc.c`）以将掩码从父进程复制到子进程。
- 修改 `kernel/syscall.c` 中的 `syscall()` 函数以检查系统调用是否必须被拒绝。

## 带允许路径名的沙箱 (Sandbox with allowed pathnames) (Easy)

> **要求**：在此任务中，您将扩展沙箱，以允许被掩码限制的 `open` 和 `exec` 系统调用基于它们使用的路径名执行。`sys_interpose` 的第二个参数是允许的路径名。如果 `open` 或 `exec` 被屏蔽，但路径名与允许的路径名匹配，则应允许这些系统调用。

完成后，您应该会看到如下输出：
```
$ sandbox 32768 README grep xv6 README
xv6 is a re-implementation of Dennis Ritchie's and Ken Thompson's Unix
Version 6 (v6).  xv6 loosely follows the structure and style of v6,
xv6 is inspired by John Lions's Commentary on UNIX 6th Edition (Peer
(kaashoek,rtm@mit.edu).  The main purpose of xv6 is as a teaching
$ sandbox 32768 README grep xv6 x
grep: cannot open x
```

在此示例中，沙箱允许访问 `README` 的 `open` 调用，但不允许访问任何其他 file（例如 x）。

如果您的解决方案通过了 `make grade` 中的沙箱测试，则它是正确的：
```
$ make grade
...
== Test sandbox_mask == 
$ make qemu-gdb
sandbox_mask: OK (9.5s) 
== Test sandbox_fork == 
$ make qemu-gdb
sandbox_fork: OK (0.3s) 
== Test sandbox_path == 
$ make qemu-gdb
sandbox_path: OK (1.1s) 
== Test sandbox_most == 
$ make qemu-gdb
sandbox_most: OK (0.8s) 
== Test sandbox_minus == 
$ make qemu-gdb
sandbox_minus: OK (1.1s) 
...
```

Some hints:
- 修改 `sys_interpose()` 以记住允许的路径名。`argstr` 在检索路径名时会派上用场。您可以在 `proc` 结构体中声明一个大小为 `MAXPATH` 的缓冲区。
- 如果 `open` 或 `exec` 被掩码限制，检查路径名是否与允许的路径名匹配。如果是，允许执行这些系统调用。

## 攻击 xv6 (Attack xv6) (Moderate)

xv6 内核将用户程序彼此隔离，并将内核与用户程序隔离。正如您在上述任务中所看到的，应用程序不能直接调用内核或另一个用户程序中的函数；相反，交互仅通过系统调用发生。然而，如果内核对系统调用的实现中存在 Bug，攻击者可能会利用该 Bug 来打破隔离边界。为了让您了解如何利用 Bug，我们在 xv6 中引入了一个 Bug，您的目标是利用该 Bug 从另一个进程中窃取秘密。

这个 Bug 是：在编译此实验时，`kernel/vm.c` 中 `uvmalloc()` 里清除新分配页面的 `memset(mem, 0, sz)` 调用被省略了。同样，在为此实验编译 `kernel/kalloc.c` 时，使用 `memset` 将垃圾放入空闲页面的两行也被省略了。省略这 3 行（均由 `ifndef LAB_SYSCALL` 标记）的净效果是：新分配的内存保留了先前使用时的内容。因此，调用 `sbrk()` 分配内存的应用程序可能会收到包含先前使用时残留数据的页面。尽管删除了这 3 行，xv6 大部分时间仍能正常工作；它甚至能通过大部分 `usertests`。

> **要求**：`user/secret.c` 在其内存中写入一个秘密字符串，然后退出（这会释放其内存）。您的目标是在 `user/attack.c` 中添加几行代码，以找出先前执行 `secret.c` 写入内存的秘密，并单独打印该秘密。
> 您的 `attack.c` 必须与未修改的 xv6 和未修改的 `secret.c` 配合工作。您可以更改任何内容以帮助您进行实验和调试，但在最终测试和提交之前必须恢复这些更改。

`secret` 程序将秘密作为参数。您可以通过带有一些参数运行 `secret`，然后运行 `attack`，并查看 `attack` 是否精确打印了传递给 `secret` 的参数来测试您的 `attack` 程序。这里是一个成功的运行过程：
```
$ secret xyzzy
$ attack
xyzzy
$
```

取决于您具体如何实现攻击，您可能需要运行 `attack` 第二次才能找到秘密。评测程序会运行 `attack` 两次，以防万一，如果其中任何一次产生了秘密，就算通过。

在 xv6 外部，您可以使用 `./grade-lab-syscall attack` 或 `make grade` 来查看您的攻击是否通过了我们的测试。测试生成的秘密字符串保证仅包含数字以及大小写字母。

与此示例一样，不直接影响正确性的 Bug 有时也可以被用来破坏安全性。仔细的编程和广泛的测试可以减少 Bug 的数量，但不能保证其不存在。xv6 过去也曾有过 Bug，并且可能还有更多未发现的错误。真实内核的代码量比 xv6 大得多，它们在历史上充斥着此类 Bug。例如，参见公开的 [Linux 漏洞](https://www.opencve.io/cve?vendor=linux&product=linux_kernel) 以及[如何报告漏洞](https://docs.kernel.org/process/security-bugs.html)。

## 提交实验 (Submit the lab)

### 花费时间 (Time spent)

创建一个新文件 `time.txt`，并在其中放入一个整数，即您在实验上花费的小时数。使用 `git add` 和 `git commit` 提交该文件。

### 答案 (Answers)

如果此实验有提问，请在 `answers-*.txt` 中写下您的答案。使用 `git add` 和 `git commit` 提交这些文件。

### 提交 (Submit)

任务提交由 Gradescope 处理。您将需要一个 MIT gradescope 账号。查看 Piazza 以获取加入班级的入口代码。如果您在加入时需要更多帮助，请使用[此链接](https://help.gradescope.com/article/gi7gm49peg-student-add-course#joining_a_course_using_a_course_code)。

当您准备好提交时，运行 `make zipball`，这将生成 `lab.zip`。将此 zip 文件上传到相应的 Gradescope 任务。

如果您运行 `make zipball` 并且有未提交的更改或未跟踪的文件，您将看到类似于以下内容的输出：
```
 M hello.c
?? bar.c
?? foo.pyc
Untracked files will not be handed in.  Continue? [y/N]
```
检查上述行，确保您的实验解决方案所需的所有文件都已被跟踪，即未列在以 `??` 开头的行中。您可以使用 `git add {filename}` 使 `git` 跟踪您创建的新文件。

> [!WARNING]
> - 请运行 `make grade` 以确保您的代码通过所有测试。Gradescope 自动评分器将使用相同的评分程序为您的提交评分。
> - 在运行 `make zipball` 之前提交 any 修改后的源代码。
> - 您可以在 Gradescope 上检查您的提交状态并下载已提交的代码。Gradescope 上的实验成绩是您的最终实验成绩。

## 可选挑战练习 (Optional challenge exercises)

- 寻找 xv6 中允许对手破坏进程隔离或使内核崩溃的 Bug 并告诉我们。（诸如 Meltdown 的侧信道超出了范围，尽管我们将在课程中介绍它们。）
