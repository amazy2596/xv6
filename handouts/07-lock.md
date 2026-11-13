# Lab: locks

在本实验中，你将获得重新设计代码以增加并行性的经验。在多核机器上并行性差的一个常见症状是高锁争用（lock contention）。提高并行性通常涉及改变数据结构和加锁策略，以减少争用。你将对 xv6 的内存分配器和块缓存（block cache）进行此项优化。

## Background

> [!NOTE]
> 在编写代码之前，请务必阅读 [xv6 书籍](xv6/book-riscv-rev5.pdf) 中的以下部分：
> * 第 7 章：“锁”（Locking）及相应的代码。
> * 第 3.5 节：“代码：物理内存分配器”

```
  $ git fetch
  $ git checkout lock
  $ make clean
```

## Memory allocator (moderate)

程序 `user/kalloctest` 对 xv6 的内存分配器进行压力测试：三个进程增长和收缩它们的地址空间，导致多次调用 `kalloc` 和 `kfree`。`kalloc` 和 `kfree` 获取 `kmem.lock`。对于 `kmem` 锁和其它一些锁，`kalloctest` 会打印出（显示为 "#test-and-set"）在 `acquire` 中由于尝试获取另一个核心已经持有的锁而导致的循环迭代次数。`acquire` 中的循环迭代次数是锁争用的粗略衡量指标。在开始实验之前，`kalloctest` 的输出类似于以下内容：

```
$ kalloctest
start test1
test1 results:
--- lock kmem/bcache stats
lock: kmem: #test-and-set 18820 #acquire() 433058
lock: bcache: #test-and-set 0 #acquire() 1744
--- top 5 contended locks:
lock: uart: #test-and-set 97375 #acquire() 117
lock: virtio_disk: #test-and-set 96764 #acquire() 183
lock: proc: #test-and-set 45428 #acquire() 522884
lock: proc: #test-and-set 25506 #acquire() 522875
lock: kmem: #test-and-set 18820 #acquire() 433058
tot= 18820
test1 FAIL
start test2
total free number of pages: 32463 (out of 32768)
..........
test2 OK
start test3
..........child done 10000

test3 OK
start test4
..........child done 100000
--- lock kmem/bcache stats
lock: kmem: #test-and-set 135309 #acquire() 3006254
lock: bcache: #test-and-set 0 #acquire() 1860
--- top 5 contended locks:
lock: wait_lock: #test-and-set 37427486 #acquire() 40008
lock: proc: #test-and-set 3108381 #acquire() 1689532
lock: proc: #test-and-set 207812 #acquire() 1588761
lock: kmem: #test-and-set 135309 #acquire() 3006254
lock: uart: #test-and-set 97375 #acquire() 1303
tot= 135309
test4 FAIL m 59382 n 135309
$ 
```

你可能会看到与此处显示不同的计数，以及前 5 个争用锁的不同顺序。

`acquire` 为每个锁维护对该锁的 `acquire` 调用计数，以及 `acquire` 中的循环试图但未能设置该锁的次数。`kalloctest` 调用一个系统调用，使内核打印 `kmem` 锁（本实验的重点）以及 5 个争用最激烈的锁的这些计数。如果存在锁争用，`acquire` 循环迭代的次数将会很大。该系统调用返回 `kmem` 锁的循环迭代次数之和。

对于本实验，你必须使用一台具有多个核心的专用空闲机器。如果你使用一台正在做其他事情的机器，`kalloctest` 打印的计数将毫无意义。你可以使用专用的 Athena 工作站或你自己的笔记本电脑，但不要使用拨号登录（dialup）的共享服务器。

`kalloctest` 中锁争用的根本原因是 `kalloc()` 有一个单一的空闲列表（free list），由单个锁保护。为了消除锁争用，你必须重新设计内存分配器以避免使用单一锁和列表。基本思路是为每个 CPU 维护一个空闲列表，每个列表都有自己的锁。由于每个 CPU 都在不同的列表上操作，在不同 CPU 上的分配和释放可以并行运行。主要的挑战将是处理一个 CPU 的空闲列表为空，但另一个 CPU 的列表有空闲内存的情况；在这种情况下，该 CPU 必须“窃取”（steal）另一个 CPU 的一部分空闲列表。窃取可能会引入锁争用，但希望这并不频繁。

> [!IMPORTANT]
> 你的任务是实现每个 CPU 的空闲列表，并在 CPU 的空闲列表为空时进行窃取。你必须为你的所有锁命名，且名称必须以 "kmem" 开头。也就是说，你应该对你的每个锁调用 `initlock`，并传递一个以 "kmem" 开头的名称。运行 `kalloctest` 以查看你的实现是否减少了锁争用。要检查它是否仍能分配所有内存，请运行 `usertests sbrkmuch`。你的输出将类似于下面显示的内容，`kmem` 锁的总争用大大减少，尽管具体数字会有所不同。确保 `usertests -q` 中的所有测试都通过。`make grade` 应该显示 `kalloctests` 通过。

```
$ kalloctest
start test1
test1 results:
--- lock kmem/bcache stats
lock: kmem: #test-and-set 0 #acquire() 117911
lock: kmem: #test-and-set 0 #acquire() 169464
lock: kmem: #test-and-set 0 #acquire() 145786
lock: bcache: #test-and-set 0 #acquire() 1744
--- top 5 contended locks:
lock: virtio_disk: #test-and-set 93747 #acquire() 183
lock: proc: #test-and-set 44010 #acquire() 526316
lock: proc: #test-and-set 24347 #acquire() 526309
lock: wait_lock: #test-and-set 11726 #acquire() 12
lock: pr: #test-and-set 4579 #acquire() 5
tot= 0
test1 OK
start test2
total free number of pages: 32463 (out of 32768)
..........
test2 OK
start test3
..........child done 10000

test3 OK
start test4
..........child done 100000
--- lock kmem/bcache stats
lock: kmem: #test-and-set 3673 #acquire() 827384
lock: kmem: #test-and-set 3449 #acquire() 1215152
lock: kmem: #test-and-set 1924 #acquire() 1236349
lock: kmem: #test-and-set 0 #acquire() 1014
lock: kmem: #test-and-set 0 #acquire() 1014
lock: kmem: #test-and-set 0 #acquire() 1014
lock: kmem: #test-and-set 0 #acquire() 1014
lock: kmem: #test-and-set 0 #acquire() 1014
lock: bcache: #test-and-set 0 #acquire() 1860
--- top 5 contended locks:
lock: wait_lock: #test-and-set 39121537 #acquire() 40020
lock: proc: #test-and-set 6853704 #acquire() 1672258
lock: proc: #test-and-set 214194 #acquire() 1614201
lock: uart: #test-and-set 195773 #acquire() 1459
lock: virtio_disk: #test-and-set 93747 #acquire() 183
tot= 9046

test4 OK
$ usertests sbrkmuch
usertests starting
test sbrkmuch: OK
ALL TESTS PASSED
$ usertests -q
...
ALL TESTS PASSED
$
```

一些提示：

* 你可以使用 `kernel/param.h` 中的常量 `NCPU`。
* 让 `freerange` 将所有空闲内存分配给运行 `freerange` 的 CPU。
* 函数 `cpuid` 返回当前核心编号，但只有在关闭中断时调用它并使用其结果才是安全的。你应该使用 `push_off()` 和 `pop_off()` 来关闭和开启中断。
* 看看 `kernel/sprintf.c` 中的 `snprintf` 函数以获取字符串格式化的思路。不过，仅仅将所有锁命名为 "kmem" 也是可以的。
* 可选地，使用 xv6 的数据竞争检测器运行你的解决方案：
  ```
  $ make clean
  $ make KCSAN=1 qemu
  $ kalloctest
    ..
  ```
  `kalloctest` 可能会失败，但你不应该看到任何数据竞争（race）。如果 xv6 的数据竞争检测器检测到竞争，它将沿着以下几行打印描述竞争的两个堆栈轨迹（stack traces）：
  ```
   == race detected ==
   backtrace for racing load
   0x000000008000ab8a
   0x000000008000ac8a
   0x000000008000ae7e
   0x0000000080000216
   0x00000000800002e0
   0x0000000080000f54
   0x0000000080001d56
   0x0000000080003704
   0x0000000080003522
   0x0000000080002fdc
   backtrace for watchpoint:
   0x000000008000ad28
   0x000000008000af22
   0x000000008000023c
   0x0000000080000292
   0x0000000080000316
   0x000000008000098c
   0x0000000080000ad2
   0x000000008000113a
   0x0000000080001df2
   0x000000008000364c
   0x0000000080003522
   0x0000000080002fdc
   ==========
  ```
  在你的操作系统上，你可以通过将堆栈轨迹复制粘贴到 `addr2line` 中，将其转换为带有行号的函数名称：
  ```
   $ riscv64-linux-gnu-addr2line -e kernel/kernel
   0x000000008000ab8a
   0x000000008000ac8a
   0x000000008000ae7e
   0x0000000080000216
   0x00000000800002e0
   0x0000000080000f54
   0x0000000080001d56
   0x0000000080003704
   0x0000000080003522
   0x0000000080002fdc
  ctrl-d
  kernel/kcsan.c:157
  kernel/kcsan.c:241
  kernel/kalloc.c:174
  kernel/kalloc.c:211
  kernel/vm.c:255
  kernel/proc.c:295
  kernel/sysproc.c:54
  kernel/syscall.c:251
  ```
  你不被强制要求运行数据竞争检测器，但你可能会发现它很有帮助。注意，数据竞争检测器会显着降低 xv6 的运行速度，因此在运行 `usertests` 时你可能不想使用它。

## Read-write lock (moderate)

本部分的作业独立于第一部分；无论你是否完成了第一部分，你都可以在本部分开展工作（并通过测试）。

考虑 xv6 的 `sys_pause` 和 `sys_uptime` 函数，它们读取全局 `ticks` 变量。由于该变量可能由 `clockintr` 并发更新，这两个函数在读取 `ticks` 之前都会获取 `tickslock` 自旋锁。重要的是，这防止了 `clockintr` 并发修改 `ticks`，但这同时也防止了多个核心同时读取 `ticks`，而这本是完全可以的。在后一种情况下，自旋锁无谓地降低了性能。

解决此问题的一个常见方案是使用读写锁（read-write lock）。读写锁引入了两种锁持有者的概念：读者（readers）和写者（writers）。同一时间最多只能有一个写者（并且当有写者时，不能有读者），但可以同时有多个读者（只要没有写者）。典型的读写锁 API 如下（见 `kernel/defs.h`）：

```
void            initrwlock(struct rwspinlock*);
void            read_acquire(struct rwspinlock*);
void            read_release(struct rwspinlock*);
void            write_acquire(struct rwspinlock*);
void            write_release(struct rwspinlock*);
```

读写锁可能出现的一个微妙问题是，如果存在许多读者，写者可能永远没有机会运行。换句话说，即使读者不断获取和释放锁，实际上也从未出现读者为零的时刻，因此写者可能永远无法获取锁。为了解决这个问题，读写锁通常实现写者优先（writer priority）方案：一旦写者尝试获取锁，随后的读者必须等待，直到写者成功获取并释放锁（当然，在允许写者获取锁之前，写者需要等待当前的读者释放锁）。

> [!IMPORTANT]
> 按照上述 API 和语义，在 xv6 中实现一个读写自旋锁。确保读者不会使写者饥饿：如果有任何挂起的写者，后续的读者将无法获取锁。
> 
> 你需要填写 `kernel/spinlock.c` 中读写自旋锁 API 的桩函数（stubbed-out functions），并可能需要更改 `kernel/spinlock.h` 中 `struct rwspinlock` 的定义。
> 
> 完成后，通过运行 `rwlktest` 测试你的 `rwspinlock` 实现。你应该看到类似于以下的输出：
> 
> ```
> $ rwlktest
> rwspinlock_test: step 1
> rwspinlock_test: step 2
> rwspinlock_test: step 3
> rwspinlock_test: step 4
> rwspinlock_test: step 5
> rwspinlock_test: step 6
> rwspinlock_test: step 7
> rwspinlock_test(0): 0
> rwspinlock_test(2): 0
> rwspinlock_test(1): 0
> rwlktest: passed 3/3
> $
> ```
> 
> 确保 `usertests -q` 仍然通过。完成后，`make grade` 应该通过所有测试。

一些提示：

* 首先看看 `kernel/spinlock.c` 中的 `sys_rwlktest`；你应该能够分阶段测试你的实现。
* 如果你在不修改 `kernel/spinlock.c` 中任何内容的情况下在 xv6 内运行 `rwlktest`，内核将打印 `panic: acquire`，因为原始的自旋锁实现仅允许锁由一个线程一次持有。你需要用你自己的锁实现替换 `write/read_acquire_inner` 和 `write/read_release_inner` 中的 `acquire()` 和 `release()` 调用。
* 注意可能的交叉执行（interleaving）。例如，如果读者看到没有挂起的写者，并想要获取锁以进行读取，你怎么知道获取锁仍然是安全的？
* 了解 GCC 用于原子操作的内置函数可能会有所帮助；请参阅 [https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html](https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html)。

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
