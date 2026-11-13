# Lab: Copy-on-Write Fork for xv6

虚拟内存提供了一层间接性（indirection）：内核可以通过将 PTE 标记为无效或只读来拦截 memory 引用，从而导致页面错误（page faults），并且可以通过修改 PTE 来改变地址的含义。在计算机系统中有一句名言：任何系统问题都可以通过增加一层间接性来解决。本实验将探索一个例子：写时复制（copy-on-write）fork。

要开始该实验，请切换到 `cow` 分支：
```
$ git fetch
$ git checkout cow
$ make clean
```

## The problem

xv6 中的 `fork()` 系统调用将父进程的所有用户空间内存复制到子进程中。如果父进程很大，复制可能会花费很长时间。更糟糕的是，这项工作通常在很大程度上是被浪费的：子进程中的 `fork()` 通常紧接着 `exec()`，这会丢弃复制的内存，通常甚至没有使用其中的大部分。另一方面，如果父进程和子进程都使用了一个复制的页面，并且其中一个或两个都写入了它，那么这个复制才是真正需要的。

## The solution

你在实现写时复制（COW）`fork()` 时的目标是推迟分配和复制物理内存页，直到实际需要复制时为止（如果需要的话）。

COW `fork()` 仅为子进程创建页表，用户内存的 PTE 指向父进程的物理页面。COW `fork()` 将父进程和子进程中的所有用户 PTE 都标记为只读。当任何一个进程试图写入这些 COW 页面之一时，CPU 将触发页面错误（page fault）。内核的页面错误处理程序检测到这种情况，为触发错误的进程分配一页物理内存，将原始页面复制 to 新页面中，并修改触发错误进程中的相关 PTE 以引用新页面，这次将 PTE 标记为可写。当页面错误处理程序返回时，用户进程将能够写入其页面的副本。

COW `fork()` 使得释放实现用户内存的物理页面变得稍微棘手一些。一个给定的物理页面可能会被多个进程的页表引用，并且只有在最后一个引用消失时才应该被释放。在像 xv6 这样简单的内核中，这种簿记（bookkeeping）是相当直接的，但在生产级内核中，这可能很难做对；例如，请参阅 [Patching until the COWs come home](https://lwn.net/Articles/849638/)。

## Implement copy-on-write fork (hard)

> [!IMPORTANT]
> 你的任务是在 xv6 内核中实现写时复制 fork。如果修改后的内核成功运行 `cowtest` 和 `usertests -q` 程序，你就完成了任务。

为了帮助你测试你的实现，我们提供了一个名为 `cowtest` 的 xv6 程序（源码在 `user/cowtest.c` 中）。`cowtest` 运行各种测试，但即使是第一个测试在未修改的 xv6 上也会失败。因此，最初你会看到：

```
$ cowtest
simple: fork() failed
$ 
```

"simple" 测试分配了超过一半的可用物理内存，然后执行 `fork()`。fork 失败是因为没有足够的空闲物理内存来给子进程一份父进程内存的完整副本。

当你完成时，你的内核应该通过 `cowtest` 和 `usertests -q` 中的所有测试。也就是说：

```
$ cowtest
simple: ok
simple: ok
three: ok
three: ok
three: ok
file: ok
forkfork: ok
ALL COW TESTS PASSED
$ usertests -q
...
ALL TESTS PASSED
$
```

这里有一个合理的攻击计划（实现步骤）。

1. 修改 `uvmcopy()`，将父进程的物理页面映射到子进程中，而不是分配新页面。对于设置了 `PTE_W` 的页面，清除子进程和父进程 PTE 中的 `PTE_W`。

2. 修改 `vmfault()` 以识别页面错误。当在原本可写的 COW 页面上发生写入页面错误时，使用 `kalloc()` 分配一个新页面，将旧页面复制到新页面，并在 PTE 中安装新页面并设置 `PTE_W`。原本只读的页面（未映射 `PTE_W`，例如 text 段中的页面）应保持只读并在父进程和子进程之间共享；试图写入此类页面的进程应被杀死。

3. 确保每个物理页面在对其的最后一个 PTE 引用消失时被释放——但不能在此之前。
一种好方法是，为每个物理页面记录一个“引用计数”（reference count），即引用该页面的用户页表的数量。当 `kalloc()` 分配该页时，将该页的引用计数设置为 1。当 fork 导致子进程共享该页时，增加该页的引用计数；每当有任何进程在其页表中丢弃该页时，减少该页的计数。只有当其引用计数为零时，`kfree()` 才应将页面放回空闲列表。将这些计数保存在一个固定大小的整型数组中是可以的。你必须设计一个方案来决定如何对数组进行索引以及如何选择其大小。例如，你可以用页面的物理地址除以 4096 来索引数组，并使数组的元素数量等于 `kalloc.c` 中的 `kinit()` 放到空闲列表中的任何页面的最高物理地址。可以随意修改 `kalloc.c`（例如，`kalloc()` and `kfree()`）来维护引用计数。

4. 修改 `copyout()`，使其在遇到 COW 页面时使用与页面错误相同的方案。

一些提示：

* 记录每个 PTE 是否为 COW 映射可能是有用的。你可以使用 RISC-V PTE 中的 RSW（保留给软件）位来实现这一点。
* `usertests -q` 探索了 `cowtest` 没有测试的场景，所以别忘了检查这两者的所有测试是否都通过了。
* 页表标志的一些有用宏和定义在 `kernel/riscv.h` 的末尾。
* 如果发生 COW 页面错误且没有空闲内存，则该进程应被杀死。

## Submit the lab

### Time spent

创建一个新文件 `time.txt`，并在其中放入一个整数，即你在实验上花费的小时数。
`git add` 并 `git commit` 该文件。

### Answers

如果本实验有问答题，请在 `answers-*.txt` 中写下你的答案。
`git add` 并 `git commit` 这些文件。

### Submit

作业提交由 Gradescope 处理。
你将需要一个 MIT gradescope 账号。
加入课程的准入代码请见 Piazza。
如果你需要更多关于加入课程的帮助，请使用[此链接](https://help.gradescope.com/article/gi7gm49peg-student-add-course#joining_a_course_using_a_course_code)。

当你准备好提交时，运行 `make zipball`，这将生成 `lab.zip`。
将此 zip 文件上传到对应的 Gradescope 作业。

如果你运行 `make zipball` 并且有未提交的更改或未跟踪的文件，你将看到类似于以下内容的输出：
```
 M hello.c
?? bar.c
?? foo.pyc
Untracked files will not be handed in.  Continue? [y/N]
```
检查上述行，确保你的实验解决方案所需的所有文件都已被跟踪，即未列在以 `??` 开头的行中。
你可以使用 `git add {filename}` 来让 `git` 跟踪你创建的新文件。

> [!WARNING]
> * 请运行 `make grade` 以确保你的代码通过了所有的测试。Gradescope 自动评分器将使用相同的评分程序对你的提交进行评分。
> * 在运行 `make zipball` 之前提交任何修改过的源代码。
> * 你可以在 Gradescope 上检查你提交的状态并下载提交的代码。Gradescope 上的实验成绩是你的最终实验成绩。

## Optional challenge exercise

* 测量你的 COW 实现减少了多少 xv6 复制的字节数以及分配的物理页面数。寻找并利用进一步减少这些数量的机会。
