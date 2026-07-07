# Lab: Multithreading (多线程)

本次实验将使您熟悉多线程。您将在用户级线程包中实现线程之间的切换，使用多个线程来加速程序，并实现一个屏障（barrier）。

<div class="lab-prereq">
在编写代码之前，您应该确保已阅读 <a href="xv6/book-riscv-rev5.pdf">xv6 书籍</a> 中的“第 7 章：调度”并研究了相应的代码。
</div>

要开始本次实验，请切换到 `thread` 分支：

```
  $ git fetch
  $ git checkout thread
  $ make clean
```

## Uthread: switching between threads (Uthread：线程之间的切换) (Moderate)

在此练习中，您将为用户级线程系统设计上下文切换机制，然后实现它。为了帮您开始，您的 xv6 有两个文件 `user/uthread.c` 和 `user/uthread_switch.S`，以及 Makefile 中的一条规则用于构建 `uthread` 程序。`uthread.c` 包含了大部分用户级线程包的内容以及三个简单测试线程的代码。该线程包缺少一些用于创建线程以及在线程之间进行切换的代码。

<div class="lab-required">
您的任务是制定一个计划来创建线程并保存/恢复寄存器以在线程之间进行切换，并实现该计划。完成后，`make grade` 应该会显示您的解决方案通过了 `uthread` 测试。
</div>

完成后，当您在 xv6 上运行 `uthread` 时，应该会看到以下输出（这三个线程启动的顺序可能会有所不同）：

```
$ make qemu
...
$ uthread
thread_a started
thread_b started
thread_c started
thread_c 0
thread_a 0
thread_b 0
thread_c 1
thread_a 1
thread_b 1
...
thread_c 99
thread_a 99
thread_b 99
thread_c: exit after 100
thread_a: exit after 100
thread_b: exit after 100
thread_schedule: no runnable threads
$
```

此输出来自这三个测试线程，其中每个线程都有一个循环，打印一行然后将 CPU 让给（yield）其他线程。

然而，在目前没有上下文切换代码的情况下，您将看不到任何输出。

您需要将代码添加到 `user/uthread.c` 中的 `thread_create()` 和 `thread_schedule()`，以及 `user/uthread_switch.S` 中的 `thread_switch`。其中一个目标是确保当 `thread_schedule()` 第一次运行给定线程时，该线程在自己的栈上执行传递给 `thread_create()` 的函数。另一个目标是确保 `thread_switch` 保存被切换出的线程的寄存器，恢复被切换入的线程的寄存器，并返回到后者线程的指令上一次中断的地方。您必须决定在何处保存/恢复寄存器；修改 `struct thread` 以保存寄存器是一个好计划。您需要在 `thread_schedule` 中添加对 `thread_switch` 的调用；您可以向 `thread_switch` 传递任何所需的参数，但其意图是从线程 `t` 切换到 `next_thread`。

一些提示：

* `thread_switch` 仅需要保存/恢复被调用者保存的寄存器（callee-save registers）。为什么？
* 您可以在 `user/uthread.asm` 中看到 `uthread` 的汇编代码，这对于调试可能会很有用。
* 为了测试您的代码，使用 `riscv64-linux-gnu-gdb` 单步执行 `thread_switch` 可能会有所帮助。您可以这样开始：

  ```
  (gdb) file user/_uthread
  Reading symbols from user/_uthread...
  (gdb) b uthread.c:60
  ```

  这将在 `uthread.c` 的第 60 行设置一个断点。在您运行 `uthread` 之前，该断点可能会（或可能不会）被触发。这是怎么发生的？

  一旦您的 xv6 shell 运行，输入 `uthread`，gdb 将在第 60 行中断。如果您从另一个进程触发了该断点，请继续运行，直到在 `uthread` 进程中触发断点。现在您可以输入类似于以下的命令来检查 `uthread` 的状态：

  ```
  (gdb) p/x *next_thread
  ```

  使用 "x"，您可以检查内存位置的内容：

  ```
  (gdb) x/x next_thread->stack
  ```

  您可以这样跳转到 `thread_switch` 的开始：

  ```
  (gdb) b thread_switch
  (gdb) c
  ```

  您可以使用以下命令单步执行汇编指令：

  ```
  (gdb) si
  ```

  gdb 的在线文档在[这里](https://sourceware.org/gdb/current/onlinedocs/gdb/)。

## Using threads (使用线程) (Moderate)

在此作业中，您将使用哈希表探索带有线程 and 锁的并行编程。您应该在具有多个核心的真实 Linux 或 MacOS 计算机（不是 xv6，不是 qemu）上进行此作业。大多数最新的笔记本电脑都配有多核处理器。

此作业使用 UNIX `pthread` 线程库。您可以通过手册页通过 `man pthreads` 查找相关信息，也可以在网上查看，例如在[这里](https://pubs.opengroup.org/onlinepubs/007908799/xsh/pthread_mutex_lock.html)、[这里](https://pubs.opengroup.org/onlinepubs/007908799/xsh/pthread_mutex_init.html)和[这里](https://pubs.opengroup.org/onlinepubs/007908799/xsh/pthread_create.html)。

文件 `notxv6/ph.c` 包含一个简单的哈希表，如果从单个线程使用它是正确的，但从多个线程使用时则是错误的。在您的主 xv6 目录（可能是 `~/xv6-labs-2021`）中，输入以下内容：

```
$ make ph
$ ./ph 1
```

请注意，为了构建 `ph`，Makefile 使用的是您系统的 gcc，而不是 6.1810 的工具。传给 `ph` 的参数指定了在哈希表上执行 put 和 get 操作的线程数。运行一小会儿后，`ph 1` 将产生类似于以下的输出：

```
100000 puts, 3.991 seconds, 25056 puts/second
0: 0 keys missing
100000 gets, 3.981 seconds, 25118 gets/second
```

您看到的数字可能与此示例输出相差两倍或更多，这取决于您的计算机有多快、是否有多核以及是否正忙于做其他事情。

`ph` 运行两个基准测试。首先，它通过调用 `put()` 向哈希表中添加大量键，并打印以每秒 puts 数表示的达成速率。然后它通过 `get()` 从哈希表中获取键。它打印由于 puts 而本应存在于哈希表中但实际缺失的键数（在此情况下为零），并打印它达成的每秒 gets 数。

您可以通过给 `ph` 传递一个大于 1 的参数，告诉它同时从多个线程使用其哈希表。尝试运行 `ph 2`：

```
$ ./ph 2
100000 puts, 1.885 seconds, 53044 puts/second
1: 16579 keys missing
0: 16579 keys missing
200000 gets, 4.322 seconds, 46274 gets/second
```

此 `ph 2` 输出的第一行指示当两个线程并发地向哈希表添加条目时，它们达到了每秒 53,044 次插入的总速率。这大约是运行 `ph 1` 的单线程速率的两倍。这是一个大约 2x 的极好“并行加速比（parallel speedup）”，达到了人们所能期望的最大值（即双倍的核心产生了双倍的单位时间工作量）。

然而，显示 `16579 keys missing` 的两行指示大量本应在哈希表中的键并不在其中。也就是说，puts 本应将这些键添加到哈希表中，但出了些问题。看看 `notxv6/ph.c`，特别是 `put()` 和 `insert()`。

<div class="lab-question">
为什么 2 个线程会出现键缺失，而 1 个线程不会？指出 2 个线程导致键缺失的事件序列。将您的序列和简短解释提交到 `answers-thread.txt` 中。
</div>

<div class="lab-required">

为避免这种事件序列，在 `notxv6/ph.c` 中的 `put` 和 `get` 中插入 lock 和 unlock 语句，以确保在两个线程的情况下缺失的键数始终为 0。相关的 pthread 调用是：

```
pthread_mutex_t lock;            // 声明一个锁
pthread_mutex_init(&lock, NULL); // 初始化该锁
pthread_mutex_lock(&lock);       // 获取锁
pthread_mutex_unlock(&lock);     // 释放锁
```

当 `make grade` 显示您的代码通过 `ph_safe` 测试（该测试要求在两个线程的情况下缺失的键数为零）时，您就完成了这部分。在此阶段 `ph_fast` 测试失败是没关系的。
</div>

不要忘记调用 `pthread_mutex_init()`。首先使用 1 个线程测试您的代码，然后使用 2 个线程测试它。它是否正确（即您是否消除了缺失的键？）？与单线程版本相比，双线程版本是否实现了并行加速比（即单位时间内完成的总工作量更多）？

在某些情况下，并发的 `put()` 在哈希表中读取或写入的内存没有重叠，因此不需要锁来防止彼此冲突。您是否可以修改 `ph.c` 以利用这种情况，从而为某些 `put()` 获得并行加速比？提示：每个哈希桶一个锁如何？

<div class="lab-required">
修改您的代码，以便某些 `put` 操作可以并行运行，同时保持正确性。当 `make grade` 显示您的代码同时通过 `ph_safe` 和 `ph_fast` 测试时，您就完成了。`ph_fast` 测试要求两个线程产生的每秒 puts 数至少是单个线程的 1.25 倍。
</div>

## Barrier (屏障) (Moderate)

在此作业中，您将实现一个[屏障 (barrier)](http://en.wikipedia.org/wiki/Barrier_(computer_science))：应用程序中的一个点，所有参与的线程都必须在该点等待，直到所有其他参与的线程也到达该点。您将使用 pthread 条件变量（condition variables），这是一种类似于 xv6 的 sleep 和 wakeup 的序列协调技术。

您应该在真实的计算机上进行此作业（不是 xv6，不是 qemu）。

文件 `notxv6/barrier.c` 包含一个损坏的屏障。

```
$ make barrier
$ ./barrier 2
barrier: notxv6/barrier.c:42: thread: Assertion `i == t' failed.
```

2 指定在屏障上进行同步的线程数（`barrier.c` 中的 `nthread`）。每个线程执行一个循环。在每次循环迭代中，线程都会调用 `barrier()`，然后随机睡眠若干微秒。断言触发了，因为一个线程在另一个线程到达屏障之前离开了屏障。期望的行为是每个线程都在 `barrier()` 中阻塞，直到所有的 `nthreads` 个线程都调用了 `barrier()`。

<div class="lab-required">
您的目标是实现期望的屏障行为。除了您在 `ph` 作业中看到的锁原语之外，您还需要以下新的 pthread 原语；有关详细信息，请查看<a href="https://pubs.opengroup.org/onlinepubs/007908799/xsh/pthread_cond_wait.html">这里</a>和<a href="https://pubs.opengroup.org/onlinepubs/007908799/xsh/pthread_cond_broadcast.html">这里</a>。

```
pthread_cond_wait(&cond, &mutex);  // 在 cond 上入睡，释放锁 mutex，醒来时重新获取锁
pthread_cond_broadcast(&cond);     // 唤醒在 cond 上入睡的每个线程
```

确保您的解决方案通过了 `make grade` 的 `barrier` 测试。
</div>

`pthread_cond_wait` 在调用时释放 `mutex`，并在返回前重新获取 `mutex`。

我们已为您提供了 `barrier_init()`。您的任务是实现 `barrier()` 以便不会发生 panic。我们为您定义了 `struct barrier`；它的字段供您使用。

有两个问题使您的任务变得复杂：

* 您必须处理一系列屏障调用，我们将其中的每一次调用称为一轮（round）。`bstate.round` 记录当前轮数。每次所有线程都到达屏障时，您应该递增 `bstate.round`。
* 您必须处理一个线程在其他线程尚未退出屏障时就抢先循环一圈的情况。特别地，您正在从一轮到下一轮重复使用 `bstate.nthread` 变量。确保离开屏障并抢先循环一圈的线程在上一轮仍在使用 `bstate.nthread` 时不会递增它。

使用一个、两个以及两个以上的线程测试您的代码。

## Double-linked list (双向链表) (Moderate)

在此作业中，您将修改双向链表实现，使其在多个核心上使用时正确，并具有一定程度的并行性。

此特定双向链表的目的是维护一个排序好的数字列表。新数字可以被添加到列表中，并且实现必须根据顺序将它们插入到正确的位置。`dll_insert_from_bigger(head, x)` 和 `dll_insert_from_smaller(head, x)` 函数执行此插入操作，它们分别从较大数字的末尾或较小数字的末尾扫描排序列表以寻找合适的插入点。`dll_lookup_from_smaller(head, i)` 函数返回列表中第 i 个数字，从最小的数字扫描到最大的数字；对于此函数，最小的插入数字的索引为 0。`dll_lookup_from_bigger(head, i)` 函数返回列表中第 i 个数字，从最大的数字扫描到最小的数字；对于此函数，最大的插入数字的索引为 0。

起始代码在 `notxv6` 中：`dll.h`、`dll.c`、`dll_test.c`。

在 `dll.c` and `dll.h` 中，您会发现一个在顺序运行时（在单个线程中）是正确的实现。每个链表元素（见 `dll.h`）包含一个整数值、一个指向下一个较小链表元素的指针以及一个指向下一个较大链表元素的指针。

空链表由一个单一的“头（head）”元素组成，其较大和较小的元素都指向自身（见 `dll.c` 中的 `dll_head`）。头元素中的整数值不是链表的一部分（您的代码应该忽略它）。这种设置方便了遍历链表，因为代码不必检查较大或较小元素是否为 null。

非空链表从头节点开始，可以通过 `bigger` 或 `smaller` 指针以增加或减少的顺序进行遍历，并以其 next 指针指向头节点的元素结束（例如，参见 `dll.c` 中的 `dll_insert_from_bigger`）。每个链表元素都包含一个 pthread 互斥锁 `mu`；我们提供给您的实现并未使用这些锁，但您的代码应该使用它们。

<div class="lab-required">
`dll_test.c` 包含了您的解决方案必须通过的测试。我们提供的代码通过了 `test0`，但其余三个测试均失败。完成后，您的代码应该通过所有四个测试：

```
$ cc dll.c dll_test.c
$ ./a.out
test0: start
test0: OK
test1: start
test1: OK
test2: start
test2: OK
test3: start
test3: OK
```
</div>

一些规则：
* 您的解决方案必须允许并行执行插入，除非它们使用相同的链表元素（无论是扫描它们还是修改它们）。您的代码**不能**使用保护整个链表的锁。当您的插入代码沿着链表扫描时，允许锁定它在扫描中到达的一个链表元素，加上（如果需要）其任意一侧的元素。但您的插入代码不得持有任何其他锁。
* 前一条规则意味着，您的代码只有在遍历头部或遍历第一或最后一个链表元素时，才允许持有链表头部的 `mu` 锁。
* 您只能使用 `struct dll` 中已经声明的 `mu` 锁；不允许使用其他锁。
* 请不要修改 `dll.h` 或 `dll_test.c`。
* 请不要修改 `dll_lookup_from_smaller()` 或 `dll_lookup_from_bigger()`。测试代码从不与其他任何内容并行调用它们，因此它们不需要加锁。

一些提示：
* 避免死锁可能是此作业中最难的部分。
* 您只需修改 `dll_insert_from_bigger()` 和 `dll_insert_from_smaller()`。
* 没有删除操作，因此您无需担心并发删除和插入。

<a name="submit"></a>
## 提交实验

### 花费的时间

创建一个新文件 `time.txt`，并在其中放入一个单整数，即您在实验上花费的小时数。`git add` 并 `git commit` 该文件。

### 答案

如果本次实验有疑问，请在 `answers-*.txt` 中写下您的答案。`git add` 并 `git commit` 这些文件。

### 提交

作业提交由 Gradescope 处理。您需要一个 MIT gradescope 账号。加入班级的准入代码见 Piazza。如果需要更多加入帮助，请使用[此链接](https://help.gradescope.com/article/gi7gm49peg-student-add-course#joining_a_course_using_a_course_code)。

当您准备好提交时，运行 `make zipball`，这将生成 `lab.zip`。将此 zip 文件上传到相应的 Gradescope 作业中。

如果您运行 `make zipball`，并且您有未提交的更改或未跟踪的文件，您将看到类似于以下的输出：

```
 M hello.c
?? bar.c
?? foo.pyc
Untracked files will not be handed in.  Continue? [y/N]
```

检查上述内容，并确保您的实验解决方案所需的所有文件都已被跟踪，即没有列在以 `??` 开头的行中。您可以使用 `git add {filename}` 来让 `git` 跟踪您创建的新文件。

<div class="warning">
<ul>
  <li>请运行 `make grade` 以确保您的代码通过所有测试。Gradescope 自动评分器将使用相同的评分程序为您的提交打分。</li>
  <li>在运行 `make zipball` 之前提交任何已修改的源代码。</li>
  <li>您可以在 Gradescope 上检查提交状态并下载已提交的代码。Gradescope 上的实验成绩是您的最终实验成绩。</li>
</ul>
</div>

## Uthread 的可选挑战练习

用户级线程包在几个方面与操作系统的交互较差。例如，如果一个用户级线程在系统调用中阻塞，另一个用户级线程将不会运行，因为用户级线程调度程序不知道其线程之一已被 xv6 调度程序取消调度。作为另一个例子，两个用户级线程不会在不同的核心上并发运行，因为 xv6 调度程序没有意识到有多个线程可以并行运行。请注意，如果两个用户级线程真正并行运行，由于存在若干竞态条件，此实现将无法工作（例如，不同处理器上的两个线程可能会并发调用 `thread_schedule`，选择同一个 runnable 线程，并在不同的处理器上运行它）。

有几种方法可以解决这些问题。一种是使用[调度器激活 (scheduler activations)](http://en.wikipedia.org/wiki/Scheduler_activations)，另一种是每个用户级线程使用一个内核线程（如 Linux 内核所做的那样）。在 xv6 中实现这些方法之一。这并不容易做对；例如，在更新多线程用户进程的页表时，您需要实现 TLB 击落（shootdown）。

将锁、条件变量、屏障等添加到您的线程包中。

---
有关 6.1810 的问题或意见？请发电子邮件给课程工作人员：[61810-staff@lists.csail.mit.edu](mailto:61810-staff@lists.csail.mit.edu)。

<a rel="license" href="https://creativecommons.org/licenses/by/3.0/us/"><img alt="Creative Commons License" style="border-width:0" src="https://i.creativecommons.org/l/by/3.0/us/88x31.png" ></a>
