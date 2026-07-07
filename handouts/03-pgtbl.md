# 实验：页表 (Lab: page tables)

在本实验中，您将探索页表并修改它们以实现常见的操作系统功能。

> [!NOTE]
> 在开始编码之前，阅读 [xv6 书籍](xv6/book-riscv-rev5.pdf)的第 3 章，以及相关文件：
> - `kernel/memlayout.h`，它定义了内存的布局。
> - `kernel/vm.c`，它包含大多数虚拟内存 (VM) 代码。
> - `kernel/kalloc.c`，它包含分配和释放物理内存的代码。
> 
> 咨询 [RISC-V 特权架构手册](https://drive.google.com/file/d/17GeetSnT5wW3xNuAHI95-SI1gPGd5sJ_/view?usp=drive_link)也可能会有帮助。

要开始实验，请切换到 pgtbl 分支：
```
$ git fetch
$ git checkout pgtbl
$ make clean
```

## 检查用户进程页表 (Inspect a user-process page table) (Easy)

为了帮助您理解 RISC-V 页表，您的第一个任务是解释用户进程的页表。

运行 `make qemu` 并运行用户程序 `pgtbltest`。`print_pgtbl` 函数使用我们为此实验添加到 xv6 的 `pgpte` 系统调用，打印出 `pgtbltest` 进程的前 10 个和最后 10 个页面的页表项。输出如下所示：
```
va 0 pte 0x21FCF45B pa 0x87F3D000 perm 0x5B
va 1000 pte 0x21FCE85B pa 0x87F3A000 perm 0x5B
...
va 0xFFFFD000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFFE000 pte 0x21FD80C7 pa 0x87F60000 perm 0xC7
va 0xFFFFF000 pte 0x20001C4B pa 0x80007000 perm 0x4B
```

> **问题**：对于 `print_pgtbl` 输出中的每个页表项，解释它在逻辑上包含什么以及它的权限位是什么。xv6 书籍中的图 3.4 可能会有帮助，但请注意，该图中的页面集可能与此处正在检查的进程略有不同。注意，xv6 并没有将虚拟页面连续地放置在物理内存中。

## 加速系统调用 (Speed up system calls) (Easy)

某些操作系统（例如 Linux）通过在用户空间和内核之间共享只读区域中的数据来加速某些系统调用。这消除了在执行这些系统调用时进行内核切换的需要。为了帮助您学习如何向页表中插入映射，您的任务是在 xv6 中为 `getpid()` 系统调用实现此优化。

> **要求**：在创建每个进程时，在 USYSCALL（一个在 `memlayout.h` 中定义的虚拟地址）处映射一个只读页面。在此页面的开头，存储一个 `struct usyscall`（也在 `memlayout.h` 中定义），并将其初始化为存储当前进程的 PID。对于此实验，用户空间端已提供了 `ugetpid()`，它将自动使用 USYSCALL 映射。如果运行 `pgtbltest` 时 `ugetpid` 测试用例通过，您将获得本部分实验的满分。

一些提示：
- 选择允许用户空间仅读取该页面的权限位。
- 在新页面的生命周期内需要做几件事。为了获得灵感，请理解 `kernel/proc.c` 中的 trapframe 处理。

> **问题**：还有哪些 xv6 系统调用可以通过使用此共享页面来加快速度？解释如何加速。

## 打印页表 (Print a page table) (Easy)

为了帮助您直观地了解 RISC-V 页表，并可能有助于未来的调试，您的下一个任务是编写一个打印页表内容的函数。

> **要求**：我们添加了一个系统调用 `kpgtbl()`，它调用 `vm.c` 中的 `vmprint()`。它接受一个 `pagetable_t` 参数，您的任务是以如下所述的格式打印该页表。

当您运行 `print_kpgtbl()` 测试时，您的实现应该打印以下输出：
```
page table 0x0000000087f22000
 ..0x0000000000000000: pte 0x0000000021fc7801 pa 0x0000000087f1e000
 .. ..0x0000000000000000: pte 0x0000000021fc7401 pa 0x0000000087f1d000
 .. .. ..0x0000000000000000: pte 0x0000000021fc7c5b pa 0x0000000087f1f000
 .. .. ..0x0000000000001000: pte 0x0000000021fc705b pa 0x0000000087f1c000
 .. .. ..0x0000000000002000: pte 0x0000000021fc6cd7 pa 0x0000000087f1b000
 .. .. ..0x0000000000003000: pte 0x0000000021fc6807 pa 0x0000000087f1a000
 .. .. ..0x0000000000004000: pte 0x0000000021fc64d7 pa 0x0000000087f19000
 ..0x0000003fc0000000: pte 0x0000000021fc8401 pa 0x0000000087f21000
 .. ..0x0000003fffe00000: pte 0x0000000021fc8001 pa 0x0000000087f20000
 .. .. ..0x0000003fffffd000: pte 0x0000000021fd4813 pa 0x0000000087f52000
 .. .. ..0x0000003fffffe000: pte 0x0000000021fd00c7 pa 0x0000000087f40000
 .. .. ..0x0000003ffffff000: pte 0x0000000020001c4b pa 0x0000000080007000
```

第一行显示传递给 `vmprint` 的参数。之后是每个 PTE 的一行，包括指向树中更深层页表页面的 PTE。每个 PTE 行都缩进一定数量的 `" .."`，以指示它在树中的深度。每个 PTE 行显示其虚拟地址、pte 位以及从 PTE 中提取的物理地址。不要打印无效的 PTE。在上述示例中，顶级页表页面映射了索引 0 和 255。下一层的索引 0 仅映射了索引 0，而该索引 0 的底层则映射了几个项。

您的代码可能会发出与上面显示的不同的物理地址。条目数和虚拟地址应该相同。

一些提示：
- 使用文件 `kernel/riscv.h` 末尾的宏。
- 函数 `freewalk` 可能会提供灵感。
- 在 printf 调用中使用 `%p` 打印出完整的 64 位十六进制 PTE 和地址，如示例所示。

> **问题**：对于 `vmprint` 输出中的每个叶子页面，解释它在逻辑上包含什么，它的权限位是什么，以及它与上面较早的 `print_pgtbl()` 练习的输出有什么关系。xv6 书籍中的图 3.4 可能会有帮助，但请注意，该图中的页面集可能与此处正在检查的进程略有不同。

## 使用超级页 (Use superpages) (Moderate/Hard)

RISC-V 分页硬件支持 2MB 的页面以及普通的 4096 字节页面。较大页面的通用概念被称为超级页 (superpages)，并且（由于 RISC-V 支持多种尺寸）2MB 的页面被称为兆页 (megapages)。操作系统通过在 1 级 PTE 中设置 `PTE_V` 和 `PTE_R` 位，并将物理页号设置为指向物理内存中 2MB 区域的起始位置来创建超级页。此物理地址必须是 2MB 对齐的（即 2MB 的倍数）。您可以通过搜索 megapage 和 superpage 在 RISC-V 特权手册中阅读相关内容；特别是第 112 页的顶部。

使用超级页可以减少页表使用的物理内存量，并可以减少 TLB 缓存中的未命中率。对于某些程序，这会带来性能的大幅提升。

> **要求**：您的任务是修改 xv6 内核以使用超级页。特别是，如果用户程序调用 `sbrk()` 且大小为 2MB 或更大，并且新创建的地址范围包含一个或多个 2MB 对齐且大小至少为 2MB 的区域，则内核应使用单个超级页（而不是数百个普通页面）。如果运行 `pgtbltest` 时 `superpg_fork` 和 `superpg_free` 测试用例通过，您将获得本部分实验的满分。

一些提示：
- 阅读 `user/pgtbltest.c` 中的 `superpg_fork` 和 `superpg_free`。
- 一个好的起点是 `kernel/sysproc.c` 中的 `sys_sbrk`，它被 `sbrk` 系统调用调用。沿着代码路径找到为 `sbrk` 积极分配内存的 `growproc` 函数。
- 您的内核将需要能够分配和释放 2MB 区域。修改 `kalloc.c` 以留出物理内存的几个 2MB 区域，并创建 `superalloc()` 和 `superfree()` 函数。您只需要少数几个 2MB 的内存块。
- 当具有超级页的进程 fork 时，必须分配超级页，并在其退出时释放超级页；您需要修改 `uvmcopy()` 和 `uvmunmap()`。
- 当 `sbrk` 部分释放超级页（例如，释放超级页的最后 4096 字节）时，您将需要将超级页“降级 (demote)”为普通页面。

真正的操作系统会动态地将一组页面提升为超级页。以下参考文献解释了为什么这是一个好主意，以及在更严肃的设计中有什么困难：[Juan Navarro, Sitaram Iyer, Peter Druschel, and Alan Cox. Practical, transparent operating system support for superpages. SIGOPS Oper. Syst. Rev., 36(SI):89-104, December 2002.](https://www.usenix.org/conference/osdi-02/practical-transparent-operating-system-support-superpages)
此参考文献总结了不同操作系统的超级页实现：[A comprehensive analysis of superpage management mechanism and policies](https://www.usenix.org/conference/atc20/presentation/zhu-weixi)。

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
> - 在运行 `make zipball` 之前提交任何修改后的源代码。
> - 您可以在 Gradescope 上检查您的提交状态并下载已提交的代码。Gradescope 上的实验成绩是您的最终实验成绩。

## 可选挑战练习 (Optional challenge exercises)

- 实现上面引用的论文中的一些想法，使您的超级页设计更真实。
- 取消映射用户进程的第一页，以便对空指针进行解引用将导致错误。您必须更改 `user.ld` 以将用户文本段起始地址设为（例如）4096，而不是 0。
- 添加一个系统调用，使用 `PTE_D` 报告脏页（已修改的页面）。
