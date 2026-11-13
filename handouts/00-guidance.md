# 实验指导 (Lab guidance)

## 任务难度 (Hardness of assignments)

每个任务都标明了其难度：
- **Easy**：少于一小时。这些练习通常是后续练习的温身练习。
- **Moderate**：1-2 小时。
- **Hard**：超过 2 小时。通常这些练习不需要太多代码，但代码很难写对。

这些时间是我们预期的粗略估计。对于一些可选任务，我们没有解决方案，其难度纯属猜测。如果你发现自己在某项任务上花费的时间超出了我们的预期，请在 Piazza 上联系我们或来参加答疑时间 (office hours)。

这些练习通常不需要很多行代码（几十行到几百行），但代码在概念上很复杂，而且细节往往至关重要。因此，在编写任何代码之前，请确保完成实验指定的阅读材料，通读相关文件，并咨询文档（RISC-V 手册等在[参考页面](reference.html)上）。
分步实现你的解决方案（任务通常会建议如何分解问题），并在继续下一步之前测试每一步是否工作。

> **警告**：不要在截止日期的前一天晚上才开始实验；将工作分散在几天内进行效率更高。操作系统内核中 Bug 的表现可能会令人困惑，可能需要大量的思考和仔细的调试才能理解并修复。

## 调试提示 (Debugging tips)

以下是一些调试提示：

- 确保你理解 C 语言 and 指针。Kernighan 和 Ritchie 的《C 程序设计语言（第二版）》是对 C 语言的简洁描述。看一下这段示例[代码](https://pdos.csail.mit.edu/6.828/2019/lec/pointers.c)，确保你理解它为什么会产生这样的结果。

  有一些常用的指针惯用法特别值得记住：
  - 如果 `int *p = (int*)100`，那么 `(int)p + 1` 和 `(int)(p + 1)` 是不同的数字：前者是 `101`，而后者是 `104`。当像第二种情况那样将整数加到指针上时，该整数会隐式地乘以指针所指向对象的大小。
  - `p[i]` 被定义为与 `*(p+i)` 相同，表示 p 指向的内存中的第 i 个对象。当对象大于一个字节时，上述加法规则有助于使该定义正常工作。
  - `&p[i]` 与 `(p+i)` 相同，生成 p 指向的内存中第 i 个对象的地址。

  虽然大多数 C 程序绝不需要在指针和整数之间进行强制类型转换，但操作系统经常需要。每当你看到涉及内存地址的加法时，问问自己这是整数加法还是指针加法，并确保要加的值被适当地乘以了对应的系数，或者没有被乘以。

- 如果你已经部分完成了某个练习，可以通过提交代码来保存进度。如果你以后写坏了什么，就可以回滚到你的保存点，然后以更小的步子前进。要了解关于 Git 的更多信息，请参阅 [Git 用户手册](http://www.kernel.org/pub/software/scm/git/docs/user-manual.html)，或者这篇[面向计算机科学家的 Git 概述](http://eagain.net/articles/git-for-computer-scientists/)。

- 如果你的代码测试失败，确保你明白为什么。插入 print 语句，直到你理解发生了什么。

- 你可能会发现你的 print 语句产生了大量的输出，你想要在其中进行搜索；一种方法是在 `script` 中运行 `make qemu`（在你的机器上运行 `man script`），这会将所有控制台输出记录到一个文件中，然后你就可以进行搜索。别忘了退出 `script`。

- Print 语句通常是足够强大的调试工具，但有时能够单步执行一些汇编代码或检查栈上的变量也是有帮助的。要对 xv6 使用 gdb，在一个窗口中运行 `make qemu-gdb`，在另一个窗口中运行 `gdb-multiarch`（或 `riscv64-linux-gnu-gdb` 或 `riscv64-unknown-elf-gdb`）（如果你使用的是 Athena，请确保这两个窗口在同一台 Athena 机器上），设置断点，然后输入 'c' (continue)，xv6 将运行直到触发断点。参阅[使用 GNU 调试器](https://pdos.csail.mit.edu/6.828/2019/lec/gdb_slides.pdf)获取实用的 GDB 提示。（如果你启动 gdb 并看到类似 'warning: File ".../.gdbinit" auto-loading has been declined' 的警告，请按照警告的建议编辑 ~/.gdbinit 并添加 "add-auto-load-safe-path..."。）

- 如果你想查看编译器为 xv6 内核生成的汇编代码，或者想找出某个特定内核地址处是什么指令，请查看文件 `kernel/kernel.asm`，这是 Makefile 在编译内核时生成的。（Makefile 也会为所有用户程序生成 `.asm`。）

- 如果内核导致了意外的错误（例如使用了无效的内存地址），它会打印一条错误信息，其中包含崩溃点处的程序计数器 ("sepc")；你可以搜索 `kernel.asm` 来找到包含该程序计数器的函数，或者可以运行 `addr2line -e kernel/kernel pc-value`（运行 `man addr2line` 查看详情）。如果你想要回溯（backtrace），使用 gdb 重新启动：在一个窗口中运行 'make qemu-gdb'，在另一个窗口中运行 gdb（或 riscv64-linux-gnu-gdb），在 panic 处设置断点（'b panic'），然后输入 'c' (continue)。当内核触发断点时，输入 'bt' 来获取回溯信息。

- 如果你的内核挂起了（可能是由于死锁），你可以使用 gdb 找出它在哪里挂起。在一个窗口中运行 'make qemu-gdb'，在另一个窗口中运行 gdb (riscv64-linux-gnu-gdb)，然后输入 'c' (continue)。当内核似乎挂起时，在 qemu-gdb 窗口中按 Ctrl-C 并输入 'bt' 以获取回溯。

- `qemu` 有一个“监视器 (monitor)”，允许你查询模拟主机的状态。你可以通过输入 `control-a c` 进入它（"c" 代表控制台 console）。一个特别有用的监视器命令是 `info mem`，用于打印页表。你可能需要使用 `cpu` 命令来选择 `info mem` 查看哪个核心，或者你可以使用 `make CPUS=1 qemu` 启动 qemu，使其只有一个核心。

学习上述工具是非常值得花时间的。
