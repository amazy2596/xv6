# Lab: file system (文件系统)

在本次实验中，您将向 xv6 文件系统添加大文件和符号链接（symbolic links）。

<div class="lab-prereq">
在编写代码之前，您应该阅读 <a href="xv6/book-riscv-rev5.pdf">xv6 书籍</a> 中的“第 8 章：文件系统”并研究相应的代码。
</div>

获取本次实验的 xv6 源码并检出到 `fs` 分支：

```
  $ git fetch
  $ git checkout fs
  $ make clean
```

## File snapshots (文件快照) (Hard)

在此作业中，您将扩展 xv6 的文件系统，以高效节省磁盘空间的方式支持文件快照。其基本思想是，应用程序可以对一个文件进行快照，而文件系统将创建该文件的一个副本，即快照。您的目标是使原始文件和快照共享底层的磁盘块，而不是复制原始文件的所有块。这与写时复制（COW-fork）实验的精神类似，如果进程更新了文件（或快照）的某个块，则必须分配一个新块来存储新内容。希望这种对块的惰性分配能够避免复制那些从未被更新的块。

### 准备工作

`mkfs` 程序创建 xv6 文件系统磁盘映像，并确定该文件系统拥有的总块数；此大小由 `kernel/param.h` 中的 `FSSIZE` 控制。您会看到此实验仓库中的 `FSSIZE` 设置为 1000 块。您应该在构建输出中看到来自 `mkfs/mkfs` 的以下输出：

```
  XXX: update
nmeta 70 (boot, super, log blocks 30 inode blocks 13, bitmap blocks 25) blocks 199930 total 200000
```

此行描述了 `mkfs/mkfs` 构建的文件系统：它有 70 个元数据块（用于描述文件系统的块）和 199,930 个数据块，总计 200,000 个块。

如果在实验期间的任何时候，您发现自己必须从头开始重建文件系统，您可以运行 `make clean`，这会强制 make 重建 `fs.img`。

### 需要注意的地方

磁盘上 inode 的格式由 `fs.h` 中的 `struct dinode` 定义。您特别需要关注 `NDIRECT`、`NINDIRECT` 以及 `struct dinode` 的 `addrs[]` 元素。请参阅 xv6 教科书中的图 8.3，以获取标准 xv6 inode 的图解。

在磁盘上查找文件数据的代码位于 `fs.c` 中的 `bmap()`。看一下它，确保您理解它在做什么。读取和写入文件时都会调用 `bmap()`。在写入时，`bmap()` 会根据需要分配新块来保存文件内容，以及在需要保存块地址时分配一个间接块。

`bmap()` 处理两种块号。`bn` 参数是“逻辑块号”——文件内的块号，相对于文件开头。`ip->addrs[]` 中的块号以及传递给 `bread()` 的参数是磁盘块号。您可以将 `bmap()` 视为将文件的逻辑块号映射到磁盘块号。

### 您的任务

<div class="lab-required">
实现一个新系统调用 `snapshot`，以使快照共享文件磁盘块的方式创建现有文件的快照。当您通过 `snaptest` 中的测试时，将获得全额学分。
</div>

```
$ snaptest
...
test basic snapshot for bn 0
snaptest: ok 0
test basic snapshot for bn 3
snaptest: ok 3
test basic snapshot for bn 15
snaptest: ok 15
test basic snapshot for bn 40
snaptest: ok 40
snaptest: all tests succeeded
$ 
```

提示：

* 实现系统调用 `snapshot`，它接受两个参数：第一个是要被快照的文件，第二个是用于快照的未使用的文件名。使用 `snapshot` 的示例见 `user/snaptest.c`。要支持 `snapshot`，请向 `user/usys.pl`、`user/user.h` 添加条目，并在 `kernel/sysfile.c` 中实现一个空的 `sys_snapshot`。
* 您希望以不复制原始文件所有块的方式来实现 `sys_snapshot`。相反，您想将原始文件 inode 中的块地址复制到快照的 inode 中。您必须对原始文件和快照中的地址进行标记，以便如果进程向其写入数据，您知道必须复制将被覆盖的块，并将写入应用到副本中。例如，您可以使用地址的最高位将地址标记为写时复制（copy-on-write）。
* 为了能够支持文件的 `unlink`，您必须对指向同一块的地址数量进行引用计数，以便仅在释放对该块的所有引用时才删除该块。支持引用计数的一种方法是修改 `balloc` 和 `bfree`，使其不为每个块存储单个位，而是存储引用计数。该解决方案还需要您修改用于构建 `fs.img` 的 `mkfs/mkfs.c`。
* 一个复杂之处是大文件，它使用了一个间接块。您必须将该间接块以及该间接块中的地址标记为写时复制。
* 快照必须在重启后幸存，因此您必须确保将快照的新 inode（以及已修改的被快照文件的 inode）写入磁盘。TODO: 确保对此进行测试。
* 如果您的文件系统进入了不良状态（可能是由于崩溃引起的），请删除 `fs.img`（在 Unix 中进行，而不是在 xv6 中）。`make` 将为您构建一个新的干净的文件系统映像。
* 不要忘记对每一个您进行 `bread()` 的块执行 `brelse()`。

## Large files (大文件) (Moderate)

在此作业中，您将增加 xv6 文件的最大大小。目前，xv6 文件限制为 268 块，即 268 * BSIZE 字节（在 xv6 中，BSIZE 为 1024）。该限制源于 xv6 inode 包含 12 个“直接”块号和 1 个“一级间接”块号，该间接块号引用一个最多可容纳 256 个更多块号的块，总计 12 + 256 = 268 块。

`bigfile` 命令会创建它所能创建的最长文件，并报告该大小：

```
$ bigfile
..
wrote 268 blocks
bigfile: file is too small
$
```

测试失败，因为 `bigfile` 期望能够创建一个具有 65803 块的文件，但未修改的 xv6 限制文件为 268 块。

您将更改 xv6 文件系统代码，以支持每个 inode 中的“二级间接（doubly-indirect）”块，其中包含 256 个一级间接块的地址，每个一级间接块最多可包含 256 个数据块的地址。其结果是，一个文件将能够由多达 65803 个块组成，即 256 * 256 + 256 + 11 块（这里是 11 而不是 12，因为我们将牺牲其中一个直接块号给二级间接块）。

### 准备工作

`mkfs` 程序创建 xv6 文件系统磁盘映像并确定文件系统总共有多少个块；此大小由 `kernel/param.h` 中的 `FSSIZE` 控制。您会看到此实验仓库中的 `FSSIZE` 设置为 200,000 块。您应该在构建输出中看到来自 `mkfs/mkfs` 的以下输出：

```
nmeta 70 (boot, super, log blocks 30 inode blocks 13, bitmap blocks 25) blocks 199930 total 200000
```

该行描述了 `mkfs/mkfs` 构建的文件系统：它有 70 个元数据块和 199,930 个数据块，总计 200,000 个块。

如果在实验期间的任何时候，您发现自己必须从头开始重建文件系统，您可以运行 `make clean`，这会强制 make 重建 `fs.img`。

### 需要注意的地方

磁盘上 inode 的格式由 `fs.h` 中的 `struct dinode` 定义。您特别关注 `NDIRECT`、`NINDIRECT`、`MAXFILE` 以及 `struct dinode` 的 `addrs[]` 元素。请参阅 xv6 教科书中的图 8.3，以获取标准 xv6 inode 的图解。

在磁盘上查找文件数据的代码位于 `fs.c` 中的 `bmap()`。看一下它，确保您理解它在做什么。读取和写入文件时都会调用 `bmap()`。在写入时，`bmap()` 会根据需要分配新块来保存文件内容，以及在需要保存块地址时分配一个间接块。

`bmap()` 处理两种块号。`bn` 参数是“逻辑块号”——文件内的块号，相对于文件开头。`ip->addrs[]` 中的块号以及传递给 `bread()` 的参数是磁盘块号。您可以将 `bmap()` 视为将文件的逻辑块号映射到磁盘块号。

### 您的任务

<div class="lab-required">
修改 `bmap()`，以便除了直接块和一级间接块外，它还实现二级间接块。您只能拥有 11 个直接块，而不是 12 个，以便为您的新二级间接块腾出空间；您不能更改磁盘上 inode 的大小。`ip->addrs[]` 的前 11 个元素应该是直接块；第 12 个应该是一级间接块（就像当前的那个）；第 13 个应该是您的新二级间接块。当 `bigfile` 写入 65803 块且 `usertests -q` 成功运行时，您就完成了此练习：
</div>

```
$ bigfile
.................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................
wrote 65803 blocks
done; ok
$ usertests -q
...
ALL TESTS PASSED
$ 
```

`bigfile` 将运行至少一分半钟。

提示：

* 确保您理解 `bmap()`。画一张图来表示 `ip->addrs[]`、一级间接块、二级间接块以及它所指向的一级间接块和数据块之间的关系。确保您理解为什么添加二级间接块会使最大文件大小增加 256 * 256 块（实际上是 -1，因为您必须将直接块的数量减少一个）。
* 思考您将如何使用逻辑块号对二级间接块以及它所指向的一级间接块进行索引。
* 如果您更改了 `NDIRECT` 的定义，您可能必须更改 `file.h` 中 `struct inode` 中 `addrs[]` 的声明。确保 `struct inode` 和 `struct dinode` 在它们的 `addrs[]` 数组中具有相同数量 of 元素。
* 如果您更改了 `NDIRECT` 的定义，请确保创建一个新的 `fs.img`，因为 `mkfs` 使用 `NDIRECT` 来构建文件系统。
* 如果您的文件系统进入了不良状态（可能是由于崩溃引起的），请删除 `fs.img`（在 Unix 中进行，而不是在 xv6 中）。`make` 将为您构建一个新的干净的文件系统映像。
* 不要忘记对每一个您进行 `bread()` 的块执行 `brelse()`。
* 您应该只在需要时分配一级间接块和二级间接块，就像原始的 `bmap()` 一样。
* 确保 `itrunc` 释放文件的所有块，包括二级间接块。
* `usertests` 的运行时间比以前的实验要长，因为本实验的 `FSSIZE` 更大，且大文件也更大。

## Symbolic links (符号链接) (Moderate)

在此练习中，您将把符号链接添加到 xv6。符号链接（or 软链接，soft links）通过路径名引用链接的文件或目录；当打开符号链接时，内核会查找被链接到的名字。符号链接类似于硬链接，但硬链接仅限于指向同一磁盘上的文件，不能引用目录，并且绑定到特定的目标 inode，而不是像符号链接那样，引用目标名称当前恰好所在的任何内容（如果有的话）。实现此系统调用是理解路径名查找工作原理的良好练习。

对于本实验，您无需处理指向目录的符号链接；唯一需要知道如何跟随符号链接的系统调用是 `open()`。

### 您的任务

<div class="lab-required">
您将实现 `symlink(char *target, char *path)` 系统调用，该系统调用在 path 处创建一个新的符号链接，该链接引用 target 命名的文件。有关更多信息，请参见 man 手册页 symlink。要进行测试，请将 symlinktest 添加到 Makefile 并运行它。当测试产生以下输出（包括 usertests 成功）时，您的解决方案就完成了。
</div>

```
$ symlinktest
Start: test symlinks
test symlinks: ok
Start: test concurrent symlinks
test concurrent symlinks: ok
$ usertests -q
...
ALL TESTS PASSED
$ 
```

提示：

* 首先，为 symlink 创建一个新的系统调用号，在 `user/usys.pl`、`user/user.h` 中添加一个条目，并在 `kernel/sysfile.c` 中实现一个空的 `sys_symlink`。
* 向 `kernel/stat.h` 添加一个新的文件类型（`T_SYMLINK`）以表示符号链接。
* 向 `kernel/fcntl.h` 添加一个新标志（`O_NOFOLLOW`），该标志可与 `open` 系统调用一起使用。请注意，传递给 `open` 的标志是使用按位或（OR）运算符组合的，因此您的新标志不应与任何现有标志重叠。这将在您将 `user/symlinktest.c` 添加到 Makefile 后允许对其进行编译。
* 实现 `symlink(target, path)` 系统调用，在 path 处创建一个新的符号链接，引用 target。请注意，目标文件无需存在系统调用即可成功。您需要选择某个地方来存储符号链接的目标路径，例如，在 inode 的数据块中。`symlink` 应该返回一个表示成功（0）或失败（-1）的整数，类似于 `link` 和 `unlink`。
* 修改 `open` 系统调用以处理路径引用符号链接的情况。如果文件不存在，`open` 必须失败。当进程在 `open` 的标志中指定 `O_NOFOLLOW` 时，`open` 应该打开符号链接本身（而不去跟随该符号链接）。
* 如果被链接的文件也是一个符号链接，您必须递归地跟随它，直到到达一个非链接文件。如果链接形成环（cycle），您必须返回一个错误代码。如果链接的深度达到某个阈值（例如 10），您可以通过返回错误代码来对此进行近似。
* 其他系统调用（例如 `link` 和 `unlink`）不得跟随符号链接；这些系统调用操作的是符号链接本身。

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

* 支持三级间接（triple-indirect）块。

### 致谢

感谢 UW CSEP551（2019 年秋季）的工作人员提供符号链接练习。

---
有关 6.1810 的问题或意见？请发电子邮件给课程工作人员：[61810-staff@lists.csail.mit.edu](mailto:61810-staff@lists.csail.mit.edu)。

<a rel="license" href="https://creativecommons.org/licenses/by/3.0/us/"><img alt="Creative Commons License" style="border-width:0" src="https://i.creativecommons.org/l/by/3.0/us/88x31.png" ></a>
