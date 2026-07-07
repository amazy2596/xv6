# 实验：陷阱 (Lab: traps)

本实验探索如何使用陷阱实现系统调用。您将首先使用栈进行温身练习，然后实现一个用户级陷阱处理的示例。

> [!NOTE]
> 在开始编码之前，阅读 [xv6 书籍](xv6/book-riscv-rev5.pdf)的第 4 章，以及相关源文件：
> - `kernel/trampoline.S`：涉及从用户空间切换到内核空间以及返回的汇编代码
> - `kernel/trap.c`：处理所有中断的代码

要开始实验，请切换到 trap 分支：
```
$ git fetch
$ git checkout traps
$ make clean
```

## RISC-V 汇编 (RISC-V assembly) (Easy)

理解一点 RISC-V 汇编将是非常重要的，您在 6.1910 (6.004) 中已经接触过。您的 xv6 仓库中有一个文件 `user/call.c`。`make fs.img` 会编译它，并在 `user/call.asm` 中生成该程序的可读汇编版本。

阅读 `call.asm` 中函数 `g`、`f` 和 `main` 的代码。RISC-V 的指令手册在[参考页面](reference.html)上。在 `answers-traps.txt` 中回答以下问题：

> **问题**：哪些寄存器包含函数的参数？例如，在 main 对 `printf` 的调用中，哪个寄存器保存了 13？
>
> **问题**：在 main 的汇编代码中，对函数 `f` 的调用在哪里？对 `g` 的调用在哪里？（提示：编译器可能会内联函数。）
>
> **问题**：函数 `printf` 位于什么地址？
>
> **问题**：在 `main` 中向 `printf` 执行 `jalr` 之后，寄存器 `ra` 中的值是多少？
>
> **问题**：运行以下代码。
> ```c
> unsigned int i = 0x00646c72;
> printf("H%x Wo%s", 57616, (char *) &i);
> ```
> 输出是什么？[这里有一个 ASCII 表](https://www.asciitable.com/)，用于将字节映射到字符。
> 
> 输出取决于 RISC-V 是小端序（little-endian）这一事实。如果 RISC-V 相反是大端序（big-endian），为了产生相同的输出，您需要将 `i` 设置为什么？您需要将 `57616` 更改为不同的值吗？
> 
> [这里有关于小端序和大端序的说明](http://www.webopedia.com/TERM/b/big_endian.html)以及[一个更具奇思妙想的说明](https://www.rfc-editor.org/ien/ien137.txt)。
>
> **问题**：在以下代码中，在 `'y='` 之后会打印什么？（注意：答案不是一个具体的值。）为什么会发生这种情况？
> ```c
> printf("x=%d y=%d", 3);
> ```

## 回溯 (Backtrace) (Moderate)

对于调试，通常获取回溯 (backtrace) 很有用：回溯是发生错误点之上的栈上的函数调用列表。为了帮助进行回溯，编译器生成的机器代码在栈上维护一个对应于当前调用链中每个函数的栈帧 (stack frame)。每个栈帧由返回地址和指向调用者栈帧的“帧指针 (frame pointer)”组成。寄存器 `s0` 包含指向当前栈帧的指针（它实际上指向栈上保存的返回地址的地址加上 8）。您的 `backtrace` 应该使用帧指针在栈上向上遍历，并打印每个栈帧中保存的返回地址。

> **要求**：在 `kernel/printf.c` 中实现一个 `backtrace()` 函数。在 `sys_pause` 中插入对该函数的调用，然后运行 `bttest`，它会调用 `sys_pause`。您的输出应该是如下格式的返回地址列表（但具体数字可能会有所不同）：
> ```
> backtrace:
> 0x0000000080002cda
> 0x0000000080002bb6
> 0x0000000080002898
> ```
> 在 `bttest` 退出 qemu 后。在终端窗口中：运行 `addr2line -e kernel/kernel`（或 `riscv64-unknown-elf-addr2line -e kernel/kernel`）并复制粘贴来自回溯的地址，如下所示：
> ```
> $ addr2line -e kernel/kernel
> 0x0000000080002de2
> 0x0000000080002f4a
> 0x0000000080002bfc
> Ctrl-D
> ```
> 您应该会看到类似于以下的内容：
> ```
> kernel/sysproc.c:74
> kernel/syscall.c:224
> kernel/trap.c:85
> ```

一些提示：
- 将您的 `backtrace()` 原型添加到 `kernel/defs.h`，以便您可以在 `sys_pause` 中调用 `backtrace`。
- GCC 编译器将当前执行函数的帧指针存储在寄存器 `s0` 中。在由 `#ifndef __ASSEMBLER__ ... #endif` 标记的部分中，将以下函数添加到 `kernel/riscv.h` :
  ```c
  static inline uint64
  r_fp()
  {
    uint64 x;
    asm volatile("mv %0, s0" : "=r" (x) );
    return x;
  }
  ```
  并在 `backtrace` 中调用此函数以读取当前帧指针。`r_fp()` 使用[内联汇编](https://gcc.gnu.org/onlinedocs/gcc/Using-Assembly-Language-with-C.html)来读取 `s0`。
- 这些[讲义](https://pdos.csail.mit.edu/6.1810/2023/lec/l-riscv.txt)有一张关于栈帧布局的图。请注意，返回地址位于距离栈帧的帧指针固定偏移量 (-8) 处，而保存的帧指针位于距离帧指针固定偏移量 (-16) 处。
- 您的 `backtrace()` 将需要一种方法来识别它已经看到了最后一个栈帧，并应该停止。一个有用的事实是：为每个内核栈分配的内存由单个页对齐的页面组成，因此给定栈的所有栈帧都在同一个页面上。您可以使用 `PGROUNDDOWN(fp)`（参见 `kernel/riscv.h`）来识别帧指针所引用的页面。

一旦您的回溯正常工作，请在 `kernel/printf.c` 中的 `panic` 中调用它，以便您在内核 panic 时看到内核的回溯。

## 定时器警报 (Alarm) (Hard)

> **要求**：在此练习中，您将向 xv6 添加一个功能，该功能在进程使用 CPU 时间时定期向其发出警报。这对于想要限制其消耗多少 CPU 时间的计算密集型进程，或者对于想要进行计算但又想定期执行某些操作的进程可能很有用。更广泛地说，您将实现一种原始形式的用户级中断/陷阱处理程序；例如，您可以使用类似的方法来处理应用程序中的页错误。如果您的解决方案通过了 alarmtest 和 'usertests -q'，则它是正确的。

您应该添加一个新的 `sigalarm(interval, handler)` 系统调用。如果应用程序调用 `sigalarm(n, fn)`，那么在程序消耗每 `n` 个 CPU 时间 "ticks" 之后，内核应该促使调用应用程序函数 `fn`。当 `fn` 返回时，应用程序应该恢复到它被中断的地方。在 xv6 中，tick 是一个相当任意的时间单位，由硬件定时器产生中断的频率决定。如果应用程序调用 `sigalarm(0, 0)`，内核应该停止产生定期警报调用。

您会在 xv6 仓库中找到一个文件 `user/alarmtest.c`。将其添加到 Makefile 中。在您添加了 `sigalarm` 和 `sigreturn` 系统调用之前，它将无法正确编译（见下文）。

`alarmtest` 在 `test0` 中调用 `sigalarm(2, periodic)`，以请求内核强制每 2 个 ticks 调用一次 `periodic()`，然后自旋一段时间。您可以在 `user/alarmtest.asm` 中看到 `alarmtest` 的汇编代码，这对于调试可能会派上用场。当 `alarmtest` 产生如下输出且 `usertests -q` 也正确运行时，您的解决方案是正确的：
```
$ alarmtest
test0 start
........alarm!
test0 passed
test1 start
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
test1 passed
test2 start
................alarm!
test2 passed
test3 start
test3 passed
$ usertests -q
...
ALL TESTS PASSED
$
```

完成后，您的解决方案仅需几行代码，但要写对它可能很棘手。我们将使用原始仓库中的 `alarmtest.c` 版本来测试您的代码。您可以修改 `alarmtest.c` 以帮助您调试，但请确保原始的 `alarmtest` 报告所有测试都已通过。

### test0: 调用处理程序 (test0: invoke handler)

首先修改内核以跳转到用户空间中的警报处理程序，这将导致 test0 打印 "alarm!"。先不要担心打印 "alarm!" 之后会发生什么；如果您的程序在打印 "alarm!" 之后崩溃，目前也是可以接受的。以下是一些提示：
- 您需要修改 Makefile，以使 `alarmtest.c` 被编译为 xv6 用户程序。
- 放入 `user/user.h` 中的正确声明是：
  ```c
  int sigalarm(int ticks, void (*handler)());
  int sigreturn(void);
  ```
- 更新 `user/usys.pl`（它生成 `user/usys.S`）、`kernel/syscall.h` 和 `kernel/syscall.c`，以允许 `alarmtest` 调用 `sigalarm` 和 `sigreturn` 系统调用。
- 目前，您的 `sys_sigreturn` 应该只返回零。
- 您的 `sys_sigalarm()` 应该将警报间隔和指向处理函数的指针存储在 `proc` 结构（在 `kernel/proc.h` 中）的新字段中。
- 您需要跟踪自上次调用警报处理程序以来已经过去了多少个 ticks（或者距离下一次调用还剩多少个 ticks）；您也需要在 `struct proc` 中为此添加一个新字段。您可以在 `proc.c` 中的 `allocproc()` 中初始化 `proc` 字段。
- 每个 tick，硬件时钟都会强制执行一个中断，该中断在 `kernel/trap.c` 中的 `usertrap()` 中进行处理。
- 您只想在发生定时器中断时操纵进程的警报 ticks；您需要类似于以下的内容：
  ```c
  if(which_dev == 2) ...
  ```
- 仅在进程有未处理的定时器时才调用警报函数。请注意，用户警报函数的地址可能是 0（例如，在 `user/alarmtest.asm` 中，`periodic` 位于地址 0）。
- 您需要修改 `usertrap()`，以便当进程的警报间隔过期时，用户进程执行处理程序函数。当 RISC-V 上的陷阱返回用户空间时，什么决定了用户空间代码恢复执行的指令地址？
- 如果您告诉 qemu 仅使用一个 CPU，那么使用 gdb 查看陷阱会更容易，您可以通过运行以下命令来做到这一点：
  ```
  make CPUS=1 qemu-gdb
  ```
- 如果 `alarmtest` 打印 "alarm!"，则说明您已成功。

### test1/test2()/test3(): 恢复被中断的代码 (test1/test2()/test3(): resume interrupted code)

很可能 `alarmtest` 在打印 "alarm!" 之后在 test0 或 test1 中崩溃，或者 `alarmtest`（最终）打印 "test1 failed"，或者 `alarmtest` 在没有打印 "test1 passed" 的情况下退出。要修复此问题，您必须确保当警报处理程序完成时，控制权返回到用户程序最初被定时器中断中断的指令。您必须确保寄存器内容恢复到它们在中断发生时持有的值，以便用户程序可以在警报之后不受干扰地继续运行。最后，您应该在每次警报触发后“重新启用 (re-arm)”警报计数器，以便定期调用处理程序。

作为起点，我们已经为您做出了一个设计决策：用户警报处理程序被要求在完成后调用 `sigreturn` 系统调用。有关示例，请查看 `alarmtest.c` 中的 `periodic`。这意味着您可以向 `usertrap` 和 `sys_sigreturn` 添加代码，它们相互协作以使用户进程在处理警报后能够正确恢复。

一些提示：
- 您的解决方案将要求您保存和恢复寄存器——您需要保存和恢复哪些寄存器才能正确恢复被中断的代码？（提示：会很多）。
- 让 `usertrap` 在定时器过期时在 `struct proc` 中保存足够的状态，以便 `sigreturn` 可以正确返回到被中断的用户代码。
- 防止对处理程序的重入调用——如果一个处理程序还没有返回，内核就不应该再次调用它。`test2` 对此进行了测试。
- 确保恢复 `a0`。`sigreturn` 是一个 system call，其返回值存储在 `a0` 中。

一旦您通过了 `test0`、`test1`、`test2` 和 `test3`，运行 `usertests -q` 以确保您没有破坏内核的其他任何部分。

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

- 在 `backtrace()` 中打印函数名称和行号，而不是数值地址。 (Hard)
