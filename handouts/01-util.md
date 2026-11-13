# 实验：Xv6 和 Unix 实用程序 (Lab: Xv6 and Unix utilities)

本实验将使您熟悉 xv6 及其系统调用。

## 启动 xv6 (Boot xv6) (Easy)

请查看[实验工具页面](tools.html)以获取有关如何设置计算机以运行这些实验的信息。

获取该实验 xv6 源码的 git 仓库：
```
$ git clone git://g.csail.mit.edu/xv6-labs-2025
Cloning into 'xv6-labs-2025'...
...
$ cd xv6-labs-2025
```

您在此实验及后续实验中所需的文件是使用 [Git](http://www.git-scm.com/) 版本控制系统分发的。对于每个实验，您都将检出 (check out) 一个专为该实验定制的 xv6 版本。要了解关于 Git 的更多信息，请参阅 [Git 用户手册](http://www.kernel.org/pub/software/scm/git/docs/user-manual.html)，或者这篇[面向计算机科学家的 Git 概述](http://eagain.net/articles/git-for-computer-scientists/)。Git 允许您跟踪对代码所做的更改。例如，如果您完成了一个练习并希望保存进度，可以通过运行以下命令来 *commit*（提交）您的更改：

```
$ git commit -am 'my solution for util lab exercise 1'
Created commit 60d2135: my solution for util lab exercise 1
 1 files changed, 1 insertions(+), 0 deletions(-)
$
```

您可以使用 `git diff` 查看更改，该命令显示自上次提交以来的更改。`git diff origin/util` 显示相对于初始 `util` 代码的更改。`origin/util` 是此实验的 git 分支名称。

构建并运行 xv6：
```
$ make qemu
riscv64-unknown-elf-gcc    -c -o kernel/entry.o kernel/entry.S
riscv64-unknown-elf-gcc -Wall -Werror -O -fno-omit-frame-pointer -ggdb -DSOL_UTIL -MD -mcmodel=medany -ffreestanding -fno-common -nostdlib -mno-relax -I. -fno-stack-protector -fno-pie -no-pie   -c -o kernel/start.o kernel/start.c
...
riscv64-unknown-elf-ld -z max-page-size=4096 -N -e main -Ttext 0 -o user/_zombie user/zombie.o user/ulib.o user/usys.o user/printf.o user/umalloc.o
riscv64-unknown-elf-objdump -S user/_zombie > user/zombie.asm
riscv64-unknown-elf-objdump -t user/_zombie | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$/d' > user/zombie.sym
mkfs/mkfs fs.img README  user/xargstest.sh user/_cat user/_echo user/_forktest user/_grep user/_init user/_kill user/_ln user/_ls user/_mkdir user/_rm user/_sh user/_stressfs user/_usertests user/_grind user/_wc user/_zombie
nmeta 46 (boot, super, log blocks 30 inode blocks 13, bitmap blocks 1) blocks 954 total 1000
balloc: first 591 blocks have been allocated
balloc: write bitmap block at sector 45
qemu-system-riscv64 -machine virt -bios none -kernel kernel/kernel -m 128M -smp 3 -nographic -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

xv6 kernel is booting

hart 2 starting
hart 1 starting
init: starting sh
$
```

如果您在提示符下输入 `ls`，您应该会看到类似于以下内容的输出：
```
$ ls
.              1 1 1024
..             1 1 1024
README         2 2 2227
xargstest.sh   2 3 93
cat            2 4 32864
echo           2 5 31720
forktest       2 6 15856
grep           2 7 36240
init           2 8 32216
kill           2 9 31680
ln             2 10 31504
ls             2 11 34808
mkdir          2 12 31736
rm             2 13 31720
sh             2 14 54168
stressfs       2 15 32608
usertests      2 16 178800
grind          2 17 47528
wc             2 18 33816
zombie         2 19 31080
console        3 20 0
```
这些是 `mkfs` 包含在初始文件系统中的文件；大多数是您可以运行的程序。您刚刚运行了其中之一：`ls`。

xv6 没有 `ps` 命令，但是如果您输入 `Ctrl-p`，内核将打印有关每个进程的信息。如果您现在尝试，您将看到两行：一行用于 `init`，另一行用于 `sh`。

退出 qemu 请输入：`Ctrl-a x`（同时按下 `Ctrl` 和 `a`，紧接着按 `x`）。

## sleep (Easy)

本练习将使您熟悉在 xv6 上编写用户程序以及 `pause` 系统调用。

> **要求**：为 xv6 实现一个用户级 `sleep` 程序，类似于 UNIX 的 sleep 命令。您的 `sleep` 应该暂停用户指定的 tick 数。tick 是 xv6 内核定义的时间概念，即定时器芯片的两次中断之间的时间。您的解决方案应该在 `user/sleep.c` 文件中。

一些提示：
- 在开始编码之前，阅读 [xv6 书籍](xv6/book-riscv-rev5.pdf)的第一章。
- 将您的代码放在 `user/sleep.c` 中。看一下 `user/` 中的其他程序（例如 `user/echo.c`、`user/grep.c` 和 `user/rm.c`），了解如何将命令行参数传递给程序。
- 将您的 `sleep` 程序添加到 Makefile 中的 `UPROGS`；完成此操作后，`make qemu` 将编译您的程序，您将能够从 xv6 shell 运行它。
- 如果用户忘记传递参数，`sleep` 应该打印一条错误信息。
- 命令行参数以字符串形式传递；您可以使用 `atoi` 将其转换为整数（参见 `user/ulib.c`）。
- 使用系统调用 `pause()`。
- 参见 `kernel/sysproc.c` 以查看实现 `pause()` 系统调用的 xv6 内核代码（寻找 `sys_pause`），`user/user.h` 以查看可从用户程序调用的 `pause()` 的 C 定义，以及 `user/usys.S` 以查看从用户代码跳转到内核以执行 `pause()` 的汇编代码。
- 查看 Kernighan 和 Ritchie 的书《C 程序设计语言（第二版）》(K&R) 以学习 C 语言。

在 xv6 shell 中运行该程序：
```
$ make qemu
...
init: starting sh
$ sleep 10
(nothing happens for a little while)
$
```
您的程序在如上运行时应当暂停。
在您的命令行中（qemu 之外）运行 `make grade` 来查看您是否通过了 sleep 测试。

请注意，`make grade` 会运行所有测试，包括以下任务的测试。如果您想运行单个任务的评分测试，请输入：
```
$ ./grade-lab-util sleep
```
这将运行匹配 "sleep" 的评分测试。或者，您可以输入：
```
$ make GRADEFLAGS=sleep grade
```
其效果相同。

## sixfive (Moderate)

在此练习中，您将使用系统调用 `open` 和 `read`、C 字符串以及在 C 中处理文本文件。

> **要求**：对于每个输入文件，`sixfive` 必须打印文件中所有是 5 或 6 的倍数的数字。数字是由十进制数字组成的序列，由字符串 " -\r\t\n./," 中的字符分隔。因此，对于 "xv6" 中的 six，sixfive 不应该打印 6，但例如对于 "/6,"，它应该打印。

以下示例说明了 `sixfive` 的行为：
```
$ sixfive sixfive.txt
5
100
18
6
$
```

一些提示：
- 逐个字符读取输入文件。
- 您可以使用 `strchr` 测试字符是否匹配任何分隔符（参见 `user/ulib.c`）。
- 文件的开头和结尾是隐式分隔符。

## memdump (Easy)

本练习将为您提供更多使用 C 指针的练习。在开始之前，请阅读 Kernighan 和 Ritchie 的《C 程序设计语言（第二版）》(K&R) 中的第 5.1 节（指针与地址）至 5.6 节（指针数组）以及 6.4 节（指向结构的指针）。

查看 `user/memdump.c`。您的任务是实现函数 `memdump(char *fmt, char *data)`。`memdump()` 的目的是以 `fmt` 参数描述的格式打印 `data` 指向的内存内容。格式是一个 C 字符串。字符串的每个字符指示如何打印数据的连续部分。因此，例如，一个具有多个字段的 C 结构体可以使用包含多个字符的格式字符串进行打印。

您的 `memdump()` 应该处理以下格式字符：
- `i`：将接下来的 4 字节数据打印为 32 位十进制整数。
- `p`：将接下来的 8 字节数据打印为 64 位十六进制整数。
- `h`：将接下来的 2 字节数据打印为 16 位十进制整数。
- `c`：将接下来的 1 字节数据打印为 8 位 ASCII 字符。
- `s`：接下来的 8 字节数据包含指向 C 字符串的 64 位指针；打印该字符串。
- `S`：数据的其余部分包含以空字符结尾的 C 字符串的字节；打印该字符串。

欢迎在您的 `memdump()` 中使用 C 的 `printf()`。

如果不带参数执行，`memdump` 程序会使用一些示例格式字符串 and 数据调用 `memdump()`。如果 `memdump()` 正确实现，输出将是：
```
$ memdump
Example 1:
61810
2025
Example 2:
a string
Example 3:
another
Example 4:
BD0
1819438967
100
z
xyzzy
Example 5:
hello
w
o
r
l
d
```

对于 Example 4 输出的第一行，您可能会得到不同的十六进制地址。

如果调用 `memdump` 程序时带有一个参数，它将读取其标准输入直到文件结束，然后使用格式和输入数据调用 `memdump()`。因此，一旦实现了 `memdump()`：
```
$ echo deadc0de | memdump hhcccc
25956
25697
c
0
d
e
$ echo deadc0de | memdump p
64616564
$ 
```

> **要求**：实现 `memdump()`。

## find (Moderate)

本练习探讨路径名和目录，以及系统调用 `open`、`read` 和 `fstat`。

> **要求**：为 xv6 编写一个 UNIX find 程序的简单版本：在目录树中查找具有特定名称的所有文件。您的解决方案应该在 `user/find.c` 文件中。

一些提示：
- 查看 `user/ls.c` 以了解如何读取目录。
- 使用递归以允许 `find` 下钻到子目录中。
- 不要递归进入 "." 和 ".."。
- 每次您调用 `make qemu` 时，它都会构建一个新的 `fs.img`，从而删除在先前运行中创建的文件。如果您想使用先前使用过的文件系统启动 qemu，请使用 `make qemu-fs`。
- 您需要使用 C 字符串。看一下 K&R（C 语言书），例如第 5.5 节。
- 注意，`==` 不能像在 Python 中那样比较字符串。请使用 `strcmp()` 代替。
- 将程序添加到 Makefile 中的 `UPROGS`。

当文件系统包含文件 `b`、`a/b` 和 `a/aa/b` 时，您的解决方案应该产生以下输出：
```
$ make qemu
...
init: starting sh
$ echo > b
$ mkdir a
$ echo > a/b
$ mkdir a/aa
$ echo > a/aa/b
$ find . b
./b
./a/b
./a/aa/b
$
```

运行 `make grade` 看看我们的测试怎么说。

## exec (Moderate)

本练习涉及系统调用 `fork`、`exec` 和 `wait`。

> **要求**：在 `find` 中添加 "-exec *cmd*" 参数，该参数对 `find` 找到的每个文件 *f* 执行程序 "*cmd file*"，而不是打印匹配的文件名。

以下示例说明了 `find -exec` 的行为：
```
$ find . wc -exec echo hi
hi ./wc
$
```
请注意，这里的命令是 "echo hi"，文件是 "./wc"，这使得命令变成 "echo hi ./wc"，从而输出 "hi ./wc"。

一些提示：
- 使用 `fork` 和 `exec` 在每个文件上调用该命令。在父进程中使用 `wait` 等待子进程完成命令。
- `kernel/param.h` 声明了 `MAXARG`，如果您需要声明 `argv` 数组，这可能会很有用。

要测试您的 `find` 解决方案，请运行 shell 脚本 `findtest.sh`。您的解决方案应该产生以下输出：
```
$ make qemu
...
init: starting sh
$ sh < findtest.sh
$ echo DONE
$ $ $ $ $ hello
hello
hello
$ $
```
输出中有很多 `$`，因为 xv6 shell 没有意识到它正在处理来自文件的命令而不是来自控制台的命令，并为文件中的每个命令打印一个 `$`。

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

> **警告**：
> - 请运行 `make grade` 以确保您的代码通过所有测试。Gradescope 自动评分器将使用相同的评分程序为您的提交评分。
> - 在运行 `make zipball` 之前提交任何修改后的源代码。
> - 您可以在 Gradescope 上检查您的提交状态并下载已提交的代码。Gradescope 上的实验成绩是您的最终实验成绩。

## 可选挑战练习 (Optional challenge exercises)

- 使用 `uptime` 系统调用编写一个 `uptime` 程序，以 tick 为单位打印运行时间。 (Easy)
- 支持 `find` 中名称匹配的正则表达式。`grep.c` 对正则表达式有一些原始的支持。 (Easy)
- xv6 shell (`user/sh.c`) 只是另一个用户程序。它缺少真实 shell 中发现的许多功能，但您可以对其进行修改和改进。例如，修改 shell 使得在处理来自文件的 shell 命令时不打印 `$` (Moderate)；修改 shell 以支持 `wait` (Easy)；修改 shell 以支持 Tab 键补全 (Easy)；修改 shell 以保留已通过 shell 命令的历史记录 (Moderate)；或者您希望 shell 执行的任何其他操作。（如果您非常有雄心，可能必须修改内核以支持您需要的内核功能；xv6 支持的功能不多。）
