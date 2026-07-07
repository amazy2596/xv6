# Lab: mmap (Hard)

`mmap` 和 `munmap` 系统调用允许 UNIX 程序对其地址空间进行精细控制。它们可用于在进程之间共享内存、将文件映射到进程地址空间，以及作为用户级页面错误方案（如课堂上讨论的垃圾回收算法）的一部分。在本次实验中，您将向 xv6 添加 `mmap` 和 `munmap`，重点是内存映射文件。

获取本次实验的 xv6 源码并检出到 `mmap` 分支：

```
  $ git fetch
  $ git checkout mmap
  $ make clean
```

手册页（运行 `man 2 mmap`）显示了 `mmap` 的声明：
```
void *mmap(void *addr, size_t len, int prot, int flags,
           int fd, off_t offset);
```

`mmap` 可以以多种方式调用，但本实验仅需要与内存映射文件相关的子集功能。您可以假定 `addr` 将始终为零，这意味着内核应该决定映射文件的虚拟地址。`mmap` 返回该地址，如果失败则返回 `0xffffffffffffffff`。`len` 是要映射的字节数；它可能与文件的长度不相同。`prot` 指示内存是应该被映射为可读、可写和/或可执行的；您可以假定 `prot` 为 `PROT_READ` 或 `PROT_WRITE` 或两者兼有。`flags` 将是 `MAP_SHARED`（意味着对映射内存的修改应该写回文件）或 `MAP_PRIVATE`（意味着不应该写回）。您无需实现 `flags` 中的任何其他位。`fd` 是要映射的文件的打开文件描述符。您可以假定 `offset` 为零（它是映射在文件中的起始点）。

您的实现应该根据页面错误（page faults）惰性（lazily）地填充页表。也就是说，`mmap` 本身不应该分配物理内存或读取文件。相反，在 `usertrap` 中（或由其调用的代码中）的页面错误处理代码中执行此操作，就像在写时复制（copy-on-write）实验中一样。采用惰性分配的原因是为了确保大文件的 `mmap` 速度快，并且能够映射大于物理内存的文件。

映射同一个 `MAP_SHARED` 文件的进程**不**共享物理页面是允许的。

手册页（运行 `man 2 munmap`）显示了 `munmap` 的声明：
```
int munmap(void *addr, size_t len);
```

`munmap` 应该删除指定地址范围内的 mmap 映射（如果有的话）。如果进程修改了内存并将其映射为 `MAP_SHARED`，则应首先将修改内容写入文件。一个 `munmap` 调用可能仅覆盖 mmap 映射区域的一部分，但您可以假定它要么在区域的开头解除映射，要么在结尾解除映射，要么解除整个区域的映射（打补丁/不在区域中间挖一个洞）。当进程退出时，它对 `MAP_SHARED` 区域所做的任何修改都应该写入相关文件，就像进程调用了 `munmap` 一样。

<div class="lab-required">
您应该实现足够的 `mmap` 和 `munmap` 功能，以使 `mmaptest` 测试程序正常工作。如果 `mmaptest` 没有使用某个 `mmap` 特性，您则无需实现该特性。您还必须确保 `usertests -q` 继续正常工作。
</div>

完成后，您应该看到类似以下的输出：
```
$ mmaptest
test basic mmap
test basic mmap: OK
test mmap private
test mmap private: OK
test mmap read-only
test mmap read-only: OK
test mmap read/write
test mmap read/write: OK
test mmap dirty
test mmap dirty: OK
test not-mapped unmap
test not-mapped unmap: OK
test lazy access
test lazy access: OK
test mmap two files
test mmap two files: OK
test fork
test fork: OK
test munmap prevents access
usertrap(): unexpected scause 0xd pid=7
            sepc=0x924 stval=0xc0001000
usertrap(): unexpected scause 0xd pid=8
            sepc=0x9ac stval=0xc0000000
test munmap prevents access: OK
test writes to read-only mapped memory
usertrap(): unexpected scause 0xf pid=9
            sepc=0xaf4 stval=0xc0000000
test writes to read-only mapped memory: OK
mmaptest: all tests succeeded
$ usertests -q
usertests starting
...
ALL TESTS PASSED
$ 
```

这里有一些提示：

* 首先将 `_mmaptest` 添加到 `UPROGS` 中，并在 `user/mmaptest.c` 中添加 `mmap` 和 `munmap` 系统调用以通过编译。目前，只需从 `mmap` 和 `munmap` 返回错误。我们已在 `kernel/fcntl.h` 中为您定义了 `PROT_READ` 等。运行 `mmaptest`，它将在第一次 mmap 调用时失败。
* 跟踪每个进程所映射的内容。定义一个结构体，对应于 "virtual memory for applications" 讲座中描述的 VMA（虚拟内存区域，virtual memory area）。这应该记录由 `mmap` 创建的虚拟内存范围的地址、长度、权限、文件等。由于 xv6 内核中没有大小可变的内存分配器，因此声明一个固定大小 VMA 数组并根据需要从该数组中分配是可行的。大小为 16 就足够了。
* 实现 `mmap`：在进程的地址空间中找到一个未使用的区域来映射文件，并将一个 VMA 添加到进程的映射区域表中。VMA 应该包含一个指向被映射文件的 `struct file` 指针；`mmap` 应该增加文件的引用计数，以便在文件关闭时该结构不会消失（提示：参见 `filedup`）。运行 `mmaptest`：第一个 `mmap` 应该成功，但对 mmap 映射内存的第一次访问将导致页面错误并杀死 `mmaptest`。
* 添加代码以使在 mmap 区域发生的页面错误分配一页物理内存，将相关文件的 4096 字节读取到该页中，并将其映射到用户地址空间。使用 `readi` 读取文件，它接受一个在文件中读取的偏移量参数（但您必须对传递给 `readi` 的 inode 进行加锁/解锁）。不要忘记正确设置该页的权限。运行 `mmaptest`；它应该能执行到第一个 `munmap`。
* 实现 `munmap`：查找地址范围的 VMA 并解除映射指定的页面（提示：使用 `uvmunmap`）。如果 `munmap` 移除了之前 `mmap` 的所有页面，它应该递减对应 `struct file` 的引用计数。如果已解除映射的页面被修改过且文件被映射为 `MAP_SHARED`，则将该页写回文件。可以从 `filewrite` 中寻找灵感。
* 理想情况下，您的实现应该只写回程序实际修改过的 `MAP_SHARED` 页面。RISC-V PTE 中的脏位（`D`）指示页面是否已被写入。但是， `mmaptest` 不会检查非脏页是否未被写回；因此，您可以在不查看 `D` 位的情况下直接将页面写回。
* 修改 `exit` 以解除映射进程的映射区域，就像调用了 `munmap` 一样。运行 `mmaptest`；所有通过 `test mmap two files` 的测试都应该通过，但可能无法通过 `test fork`。
* 修改 `fork` 以确保子进程具有与父进程相同的映射区域。不要忘记增加 VMA 的 `struct file` 的引用计数。在子进程的页面错误处理程序中，分配一个新的物理页面而不是与父进程共享页面是可以的。后者会更酷，但需要更多的实现工作。运行 `mmaptest`；它应该通过所有测试。

运行 `usertests -q` 以确保一切仍然正常工作。

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

## 可选的挑战练习

* 如果两个进程具有相同的 mmap 映射文件（如 fork 测试中），共享它们的物理页面。您将需要在物理页面上使用引用计数。
* 您的解决方案可能会为从 mmap 映射文件读取的每个页面分配一个新的物理页面，即使该数据已经在内核内存的缓冲区缓存（buffer cache）中。修改您的实现以使用该物理内存，而不是分配一个新页面。这要求文件块的大小与页面大小相同（将 `BSIZE` 设置为 4096）。您需要将 mmap 映射块固定到缓冲区缓存中。您需要考虑引用计数。
  
  修复这种双重缓存的一个好处是，它还有助于使 `read()` 和 `write()` 与 `mmap` 保持一致。也就是说，如果某些 mmap 映射的文件数据通过内存映射被修改，`read` 应该返回这些修改，同样，如果应用程序调用 `write`，该写入应该出现在该文件的任何活动内存映射中。您可能会对阅读关于[统一缓冲区缓存 (unified buffer cache)](https://www.usenix.org/legacy/publications/library/proceedings/usenix2000/freenix/full_papers/silvers/silvers.pdf)的论文感兴趣。

* 消除惰性分配（lazy allocation）实现与 mmap 映射文件实现之间的冗余。（提示：为惰性分配区域创建一个 VMA。）
* 修改 `exec` 以对二进制文件的不同部分使用 VMA，从而获得按需分页的可执行文件。这将使启动程序更快，因为 `exec` 不需要从文件系统中读取任何数据。
* 实现换出（page-out）和换入（page-in）：当物理内存不足时，让内核将进程的某些部分移动到磁盘。然后，当进程引用已换出的内存时，将其换入。

---
有关 6.1810 的问题或意见？请发电子邮件给课程工作人员：[61810-staff@lists.csail.mit.edu](mailto:61810-staff@lists.csail.mit.edu)。

<a rel="license" href="https://creativecommons.org/licenses/by/3.0/us/"><img alt="Creative Commons License" style="border-width:0" src="https://i.creativecommons.org/l/by/3.0/us/88x31.png" ></a>
