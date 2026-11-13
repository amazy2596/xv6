# Lab: networking

在本实验中，你将为网络接口卡（NIC）编写一个 xv6 设备驱动程序，然后编写以太网/IP/UDP 协议处理栈的接收半部分。

获取本实验的 xv6 源码并切换到 `net` 分支：
```
  $ git fetch
  $ git checkout net
  $ make clean
```

## Background

> [!NOTE]
> 在编写代码之前，你可能会发现复习 [xv6 书籍](xv6/book-riscv-rev5.pdf) 中的“第六章：中断和设备驱动”会很有帮助。

你将使用名为 E1000 的网络设备来处理网络通信。对于 xv6（以及你编写的驱动程序），E1000 看起来就像是连接到真实以太局域网（LAN）的真实硬件。事实上，你的驱动程序与之通信的 E1000 是由 qemu 提供的模拟，连接到同样由 qemu 模拟的 LAN。在这个模拟的 LAN 上，xv6（“客户机”）的 IP 地址为 10.0.2.15。Qemu 安排运行 qemu 的计算机（“主机”）以 IP 地址 10.0.2.2 出现在 LAN 上。当 xv6 使用 E1000 向 10.0.2.2 发送数据包时，qemu 会将数据包交付给主机上的相应应用程序。

你将使用 QEMU 的“用户模式网络栈”。QEMU 官方文档中关于用户模式网络栈的更多信息在[这里](https://wiki.qemu.org/Documentation/Networking#User_Networking_.28SLIRP.29)。我们更新了 Makefile 以启用 QEMU 的用户模式网络栈和 E1000 network 卡模拟。

Makefile 配置 QEMU 将所有传入和传出的数据包记录到你的实验目录中的 `packets.pcap` 文件。复习这些记录可能有助于确认 xv6 是否正在发送和接收你期望的数据包。要显示记录的数据包：
```
tcpdump -XXnr packets.pcap
```

我们为本实验在 xv6 仓库中添加了一些文件。文件 `kernel/e1000.c` 包含了 E1000 的初始化代码，以及你将填写的用于传输和接收数据包的空函数。`kernel/e1000_dev.h` 包含了 E1000 定义的寄存器和标志位的定义，并在 Intel E1000 [软件开发人员手册](readings/8254x_GBe_SDM.pdf) 中有所描述。`kernel/net.c` 和 `kernel/net.h` 包含了一个实现 [IP](https://en.wikipedia.org/wiki/Internet_Protocol)、[UDP](https://en.wikipedia.org/wiki/User_Datagram_Protocol) 和 [ARP](https://en.wikipedia.org/wiki/Address_Resolution_Protocol) 协议的简单网络栈；`net.c` 具有用户进程发送 UDP 数据包的完整代码，但缺少接收数据包并将其递送到用户空间的大部分代码。最后，`kernel/pci.c` 包含在 xv6 启动时在 PCI 总线上搜索 E1000 网卡的代码。

## Part One: NIC (moderate)

> [!IMPORTANT]
> 你的任务是完成 `kernel/e1000.c` 中的 `e1000_transmit()` 和 `e1000_recv()`，以便驱动程序可以发送和接收数据包。当 `make grade` 表示你的解决方案通过了 "txone" 和 "rxone" 测试时，你就完成了这部分工作。

> [!NOTE]
> 在编写代码时，你将参考 E1000 [软件开发人员手册](readings/8254x_GBe_SDM.pdf)，特别是：
> * 第 3.2 节描述了数据包接收（但跳过 3.2.8 和 3.2.9）。
> * 第 3.3.1、3.3.2、3.3.3 和 3.4 节描述了传输。
> * 第 13 节描述了 E1000 的寄存器（在需要时作为参考；不要阅读整章）。

[软件开发人员手册](readings/8254x_GBe_SDM.pdf) 描述了几个密切相关的以太网控制器。QEMU 模拟的是 82540EM。你需要熟悉上面提到的第 3 章的段落。其他章节主要涉及你的驱动程序不需要交互的 E1000 的各个方面。一开始不要担心细节；只需对文档的结构有一个感觉，以便稍后查找内容。E1000 有许多高级功能，其中大部分你可以忽略。完成本实验只需要一小部分基本功能。

我们在 `e1000.c` 中为你提供的 `e1000_init()` 函数将 E1000 配置为从 RAM 中读取要传输的数据包，并将接收到的数据包写入 RAM。这种技术称为 DMA，即直接内存访问（direct memory access），是指 E1000 硬件直接从/向 RAM 读写数据包。

因为数据包的爆发性到达速度可能快于驱动程序的处理速度，所以 `e1000_init()` 为 E1000 提供了多个缓冲区，E1000 可以将接收到的数据包写入其中。E1000 要求这些缓冲区由 RAM 中的“描述符”数组来描述；每个描述符包含一个 RAM 中的地址，E1000 可以在该地址处写入接收到的数据包。`struct rx_desc` 描述了描述符格式。描述符数组称为接收环（receive ring）或接收队列（receive queue）。它是一个圆形环，其意义在于当网卡或驱动程序到达数组末尾时，它会绕回到开头。`e1000_init()` 使用 `kalloc()` 为 E1000 分配数据包缓冲区以进行 DMA。还有一个发送环（transmit ring），驱动程序应该在其中放置它希望 E1000 发送的数据包。`e1000_init()` 将这两个环配置为大小分别为 `RX_RING_SIZE` 和 `TX_RING_SIZE`。

当 `net.c` 中的网络栈需要发送数据包时，它会调用 `e1000_transmit()`，并传入指向保存要发送数据包的缓冲区的指针；`net.c` 使用 `kalloc()` 分配此缓冲区。你的传输代码必须在 TX（发送）环的描述符中放置指向数据包数据的指针。`struct tx_desc` 描述了描述符格式。你将需要确保每个缓冲区最终都会被传递给 `kfree()`，但只有在 E1000 完成数据包的传输之后（E1000 在描述符中设置 `E1000_TXD_STAT_DD` 位来指示这一点）。

当 E1000 从以太网接收到每个数据包时，它会将数据包 DMA 到下一个 RX（接收）环描述符中 `addr` 指向的内存。如果尚未挂起 E1000 中断，E1000 会要求 PLIC 在启用中断后立即递送一个中断。你的 `e1000_recv()` 代码必须扫描 RX 环，并通过调用 `net_rx()` 将每个新数据包递送给网络栈（在 `net.c` 中）。然后，你需要分配一个新缓冲区并将其放入描述符中，以便当 E1000 再次到达 RX 环中的该点时，它能找到一个新鲜的缓冲区来 DMA 新数据包。

除了读写 RAM 中的描述符环之外，你的驱动程序还需要通过其内存映射的控制寄存器与 E1000 进行交互，以检测何时有接收到的数据包可用，并通知 E1000 驱动程序已经用要发送的数据包填充了一些 TX 描述符。全局变量 `regs` 保存指向 E1000 第一个控制寄存器的指针；你的驱动程序可以通过将 `regs` 作为数组进行索引来访问其他寄存器。特别地，你将需要使用索引 `E1000_RDT` 和 `E1000_TDT`。

为了测试 `e1000_transmit()` 发送单个数据包，在一个窗口中运行 `python3 nettest.py txone`，在另一个窗口中运行 `make qemu`，然后进入 xv6 运行 `nettest txone`，它将发送单个数据包。如果一切顺利（即 qemu 的 e1000 模拟器在 DMA 环上看到了该数据包并将其转发到 qemu 之外），`nettest.py` 将打印 `txone: OK`。

如果发送工作正常，`tcpdump -XXnr packets.pcap` 应产生如下输出：
```
reading from file packets.pcap, link-type EN10MB (Ethernet)
21:27:31.688123 IP 10.0.2.15.2000 > 10.0.2.2.25603: UDP, length 5
        0x0000:  5255 0a00 0202 5254 0012 3456 0800 4500  RU....RT..4V..E.
        0x0010:  0021 0000 0000 6411 3ebc 0a00 020f 0a00  .!....d.>.......
        0x0020:  0202 07d0 6403 000d 0000 7478 6f6e 65    ....d.....txone
```

为了测试 `e1000_recv()` 接收两个数据包（一个 ARP 查询，然后一个 IP/UDP 数据包），在一个窗口中运行 `make qemu`，在另一个窗口中运行 `python3 nettest.py rxone`。`nettest.py rxone` 通过 qemu 向 xv6 发送一个 UDP 数据包；qemu 实际上首先向 xv6 发送一个 ARP 请求，并且（在 xv6 返回 ARP 答复后）qemu 会将 UDP 数据包转发给 xv6。如果 `e1000_recv()` 工作正确并将这些数据包传递给 `net_rx()`，`net.c` 应该打印：
```
arp_rx: received an ARP packet
ip_rx: received an IP packet
```
`net.c` 已经包含了检测 qemu 的 ARP 请求并调用 `e1000_transmit()` 发送其答复的代码。此测试要求 `e1000_transmit()` 和 `e1000_recv()` 都能正常工作。
此外，如果一切顺利，`tcpdump -XXnr packets.pcap` 应产生如下输出：
```
reading from file packets.pcap, link-type EN10MB (Ethernet)
21:29:16.893600 ARP, Request who-has 10.0.2.15 tell 10.0.2.2, length 28
        0x0000:  ffff ffff ffff 5255 0a00 0202 0806 0001  ......RU........
        0x0010:  0800 0604 0001 5255 0a00 0202 0a00 0202  ......RU........
        0x0020:  0000 0000 0000 0a00 020f                 ..........
21:29:16.894543 ARP, Reply 10.0.2.15 is-at 52:54:00:12:34:56, length 28
        0x0000:  5255 0a00 0202 5254 0012 3456 0806 0001  RU....RT..4V....
        0x0010:  0800 0604 0002 5254 0012 3456 0a00 020f  ......RT..4V....
        0x0020:  5255 0a00 0202 0a00 0202                 RU........
21:29:16.902656 IP 10.0.2.2.61350 > 10.0.2.15.2000: UDP, length 3
        0x0000:  5254 0012 3456 5255 0a00 0202 0800 4500  RT..4VRU......E.
        0x0010:  001f 0000 0000 4011 62be 0a00 0202 0a00  ......@.b.......
        0x0020:  020f efa6 07d0 000b fdd6 7879 7a         ..........xyz
```

你的输出看起来可能会有些许不同，但它应该包含字符串 "ARP, Request"、"ARP, Reply"、"UDP" 和 "....xyz"。

如果以上两个测试都通过，那么 `make grade` 应该显示前两个测试通过。

## e1000 hints

对于 `e1000_transmit`：

* 首先在 `e1000_transmit()` 和 `e1000_recv()` 中添加 print 语句，并在 xv6 中运行 `nettest txone`。你应该能从 print 语句中看到 `nettest txone` 产生了对 `e1000_transmit` 的调用。
* `e1000_dev.h` 中的描述符定义使用“传统”（legacy）发送描述符格式（第 3.3.3 节）。
* 首先通过读取 `E1000_TDT` 控制寄存器，向 E1000 索取它期望下一个数据包的 TX 环索引。
* 然后检查环是否溢出。如果在由 `E1000_TDT` 索引的描述符中未设置 `E1000_TXD_STAT_DD`，说明 E1000 尚未完成对应的上一次传输请求，因此返回错误。
* 否则，使用 `kfree()` 释放从该描述符传输的最后一个缓冲区（如果存在的话）。
* 然后填写描述符。设置必要的命令标志（查看 E1000 手册中的第 3.3 节）。
* 最后，通过将 `E1000_TDT` 加一并对 `TX_RING_SIZE` 取模来更新环位置。

对于 `e1000_recv`：

* 首先通过获取 `E1000_RDT` 控制寄存器并加一后对 `RX_RING_SIZE` 取模，向 E1000 索取下一个等待接收的数据包（如果存在）所在的环索引。
* 然后通过检查描述符 `status`部分中的 `E1000_RXD_STAT_DD` 位来检查新数据包是否可用。如果不可用，则停止。
* 通过调用 `net_rx()` 将数据包缓冲区递送给网络栈。
* 然后使用 `kalloc()` 分配一个新缓冲区，以替换刚刚提供给 `net_rx()` 的缓冲区。将描述符的状态位清除为零。
* 最后，更新 `E1000_RDT` 寄存器为已处理的最后一个环描述符的索引。
* `e1000_init()` 使用缓冲区初始化了 RX 环，你将需要查看它是如何实现的，并可能借鉴一些代码。
* 在某些时候，曾经到达的数据包总数将超过环的大小（16）；确保你的代码可以处理这种情况。
* E1000 可以在每次中断时递送多个数据包；你的 `e1000_recv` 应该处理这种情况。

你将需要使用锁来应对以下可能性：xv6 可能会在多个进程中同时使用 E1000，或者在中断到达时正在内核线程中使用 E1000。

## Part Two: UDP Receive (moderate)

UDP，即用户数据报协议（User Datagram Protocol），允许不同 Internet 主机上的用户进程交换单个数据包（数据报）。UDP 分层建立在 IP 之上。用户进程通过指定 32 位 IP 地址来指示它想要将数据包发送到哪个主机。每个 UDP 数据包都包含一个源端口号和一个目的端口号；进程可以请求接收发送到特定端口号的数据包，并且可以在发送时指定目的端口号。因此，不同主机上的两个进程如果知道对方的 IP 地址和各自监听的端口号，就可以使用 UDP 进行通信。例如，Google 在 IP 地址为 8.8.8.8 的主机上运行一个 DNS 域名服务器，监听 UDP 端口 53。

在此任务中，你将向 `kernel/net.c` 添加代码以接收 UDP 数据包，将它们排队，并允许用户进程读取它们。`net.c` 已经包含了用户进程传输 UDP 数据包所需的代码（你提供的 `e1000_transmit()` 除外）。

> [!IMPORTANT]
> 你的任务是填写 `kernel/net.c` 中的 `ip_rx()`、`sys_recv()` 和 `sys_bind()` 的实现。当 `make grade` 表示你的解决方案通过了所有测试时，你就完成了任务。
> 
> 你可以通过在一个窗口中启动 `python3 nettest.py grade`，然后在另一个窗口中的 xv6 内部运行 `nettest grade` 来运行与 `make grade` 相同的测试。如果一切顺利，你应该在第一个窗口（xv6 外部）中看到以下内容：
> ```
> $ python3 nettest.py grade
> txone: OK
> rxone: sending one UDP packet
> ```
> 并在 xv6 窗口中看到以下内容：
> ```
> $ nettest grade
> txone: sending one packet
> arp_rx: received an ARP packet
> ip_rx: received an IP packet
> ping0: starting
> ping0: OK
> ping1: starting
> ping1: OK
> ping2: starting
> ping2: OK
> ping3: starting
> ping3: OK
> dns: starting
> DNS arecord for pdos.csail.mit.edu. is 128.52.129.126
> dns: OK
> free: OK
> ```

本实验中关于 UDP 的系统调用 API 规范如下所示：

* `send(short sport, int dst, short dport, char *buf, int len)`：
  此系统调用向 IP 地址为 `dst` 的主机发送一个 UDP 数据包，该主机上正在监听端口 `dport` 的进程。该数据包的源端口号将是 `sport`（此端口号会报告给接收进程，以便它可以回复发送方）。UDP 数据包的内容（“有效负载”）将是地址 `buf` 处的 `len` 字节。成功时返回值为 0，失败时为 -1。

* `recv(short dport, int *src, short *sport, char *buf, int maxlen)`：
  此系统调用返回目的地端口为 `dport` 的到达的 UDP 数据包的有效负载。如果在一个或多个数据包在调用 `recv()` 之前到达，它应该立即返回最早等待的数据包。如果没有数据包正在等待，`recv()` 应该等待，直到针对 `dport` 的数据包到达。`recv()` 应该按照到达顺序查看给定端口的到达数据包。`recv()` 将数据包的 32 位源 IP 地址复制到 `*src`，将数据包的 16 位 UDP 源端口号复制到 `*sport`，将数据包 UDP 有效负载的最多 `maxlen` 字节复制到 `buf`，并将数据包从队列中移除。该系统调用返回复制的 UDP 有效负载的字节数，如果发生错误则返回 -1。

* `bind(short port)`：
  进程在调用 `recv(port, ...)` 之前应该先调用 `bind(port)`。如果一个到达的 UDP 数据包的目的端口尚未传递给 `bind()`，`net.c` 应该丢弃该数据包。此系统调用的原因是为了初始化 `net.c` 所需的任何结构，以便为随后的 `recv()` 调用存储到达的数据包。

* `unbind(short port)`：你不需要实现此系统调用，因为测试代码没有使用它。但如果你愿意，也可以实现它以与 `bind()` 保持对称。

作为参数传递给这些系统调用以及由它们返回的所有地址和端口号都必须是主机字节序（见下文）。

除了 `send()` 之外，你将需要提供系统调用的内核实现。程序 `user/nettest.c` 使用此 API。

为了使 `recv()` 工作，你需要将代码添加到 `ip_rx()` 中，`net_rx()` 会为每个接收到的 IP 数据包调用该函数。`ip_rx()` 应该决定到达的数据包是否为 UDP，以及它的目的端口是否已传递给 `bind()`；如果两者都为真，它应该将数据包保存到 `recv()` 可以找到的地方。然而，对于任何给定的端口，保存的数据包不得超过 16 个；如果已经有 16 个数据包在等待 `recv()`，则应丢弃该端口的新传入数据包。此规则的目的是防止快速或滥用的发送方迫使 xv6 耗尽内存。此外，如果因为某个端口已经有 16 个数据包等待而导致数据包被丢弃，这不应影响到达其他端口的数据包。

`ip_rx()` 查看的数据包缓冲区包含一个 14 字节的以太网头部，后跟一个 20 字节的 IP 头部，后跟一个 8 字节的 UDP 头部，再后跟 UDP 有效负载。你可以在 `kernel/net.h` 中找到这其中每一个的 C 结构体定义。维基百科对 IP 头部在[这里](https://en.wikipedia.org/wiki/Internet_Protocol_version_4#Header)有描述，对 UDP 在[这里](https://en.wikipedia.org/wiki/User_Datagram_Protocol)有描述。

生产级的 IP/UDP 实现是复杂的，需要处理协议选项并验证不变性。你只需要做足够的工作来通过 `make grade`。你的代码需要查看 IP 头部中的 `ip_p` 和 `ip_src`，以及 UDP 头部中的 `dport`、`sport` 和 `ulen`。

你将必须注意字节序。以太网、IP 和 UDP 头部字段在数据包中以最高有效字节在前的顺序放置。当 RISC-V CPU 在内存中布局多字节整数时，会将最低有效字节放在最前。这意味着，当代码从数据包中提取多字节整数时，它必须重新排列字节。这适用于 short（2字节）和 int（4字节）字段。你可以分别使用 `ntohs()` 和 `ntohl()` 函数来处理 2 字节和 4 字节字段。在查看 2 字节以太网类型字段时，可以查看 `net_rx()` 作为此操作的一个示例。

如果你的 E1000 代码中存在错误或遗漏，它们可能只会在 ping 测试期间开始导致问题。例如，ping 测试发送和接收了足够多的数据包，以至于描述符环索引会发生回绕。

一些提示：

* 创建一个结构体来跟踪绑定的端口及其队列中的数据包。
* 参考 `kernel/proc.c` 中的 `sleep(void *chan, struct spinlock *lk)` 和 `wakeup(void *chan)` 函数来实现 `recv()` 的等待逻辑。
* `sys_recv()` 复制数据包的目标地址是虚拟地址；你将需要从内核复制到当前 user 进程。
* 确保释放已被复制过去或已被丢弃的数据包。

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

## Optional Challenges:

* 在本实验中，网络栈使用中断来处理入站数据包处理，但不处理出站数据包处理。更复杂的策略是在软件中对出站数据包进行排队，并在任何一次仅向 NIC 提供有限数量的数据包。然后你可以依靠 TX 中断来填充发送环。使用这种技术，有可能对不同类型的出站流量进行优先级排序。 (easy)
* 提供的网络代码仅部分支持 ARP。实现一个完整的 [ARP 缓存](https://tools.ietf.org/html/rfc826)。 (moderate)
* E1000 支持多个 RX 和 TX 环。配置 E1000 以为每个核心提供一个环对，并修改你的网络栈以支持多个环。这样做有可能增加你的网络栈可以支持的吞吐量，并减少锁争用。 (moderate，但难以测试/测量)
* [ICMP](https://tools.ietf.org/html/rfc792) 可以提供失败网络流的通知。检测这些通知并将它们作为错误传播到用户进程。
* E1000 支持几种无状态硬件卸载（stateless hardware offloads），包括校验和计算、RSC 和 GRO。使用其中的一个或多个卸载来提高网络栈的吞吐量。 (moderate，但很难测试/测量)
* 本实验中的网络栈容易受到接收活锁（receive livelock）的影响。利用课上和阅读材料中的内容，设计并实现一个解决方案来修复它。 (moderate，但很难测试)
* 实现一个最小的 TCP 栈并下载一个网页。 (hard)

其中一些挑战旨在以在 QEMU 下可能不明显或无法测量的方式提高性能。

如果你尝试解决挑战问题，无论是与网络相关还是无关，请告诉课程教职人员！
