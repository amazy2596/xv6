# Lab: networking (网络)

在本次实验中，您将为网络接口卡（NIC）编写一个 xv6 设备驱动程序，然后编写以太网/IP/UDP 协议处理栈的接收半部分。

获取本次实验的 xv6 源码并检出到 `net` 分支：

```
  $ git fetch
  $ git checkout net
  $ make clean
```

## Background (背景)

<div class="lab-prereq">
在编写代码之前，您可能会发现复习 <a href="xv6/book-riscv-rev5.pdf">xv6 书籍</a> 中的“第 6 章：中断和设备驱动程序”会很有帮助。
</div>

您将使用名为 Tulip（或 DEC 21143）的网络设备来处理网络通信。对于 xv6（以及您编写的驱动程序），Tulip 看起来像连接到真实以太网局域网（LAN）的真实硬件。事实上，您的驱动程序将与之通信的 Tulip NIC 是由 qemu 提供的仿真，连接到同样由 qemu 仿真的 LAN。在此仿真局域网中，xv6（“客户机”）的 IP 地址为 10.0.2.15。Qemu 安排运行 qemu 的计算机（“主机”）以 IP 地址 10.0.2.2 出现在局域网上。当 xv6 使用 Tulip NIC 向 10.0.2.2 发送数据包时，qemu 会将数据包投递给主机上的相应应用程序。

您将使用 QEMU 的“用户模式网络栈”。QEMU 的文档在[这里](https://wiki.qemu.org/Documentation/Networking#User_Networking_.28SLIRP.29)有更多关于用户模式栈的介绍。我们已更新 Makefile 以启用 QEMU 的用户模式网络栈和 Tulip 网卡仿真。

Makefile 配置 QEMU 将所有传入和传出的数据包记录到您的实验目录中的 `packets.pcap` 文件。复习这些记录以确认 xv6 正在发送和接收您期望的数据包可能会有所帮助。显示已记录的数据包：

```
tcpdump -XXnr packets.pcap
```

我们为本次实验在 xv6 仓库中添加了一些文件。您应该将 Tulip 驱动代码添加到 `kernel/tulip.c`。`kernel/net.c` 和 `kernel/net.h` 包含一个简单的网络栈，它实现了 [IP](https://en.wikipedia.org/wiki/Internet_Protocol)、[UDP](https://en.wikipedia.org/wiki/User_Datagram_Protocol) 和 [ARP](https://en.wikipedia.org/wiki/Address_Resolution_Protocol) 协议；`net.c` 具有用户进程发送 UDP 数据包的完整代码，但缺少接收数据包并将其投递给用户空间的大大部分代码。最后，`kernel/pci.c` 包含当 xv6 启动时在 PCI 总线上搜索 Tulip 卡的代码。

## Part One: NIC (第一部分：网络接口卡) (Moderate)

<div class="lab-required">
您的任务是在 `kernel/tulip.c` 中实现一个 Tulip 驱动程序，使 Tulip 能够发送和接收数据包。当 `make grade` 显示您的解决方案通过 "txone" 和 "rxone" 测试时，您就完成了这部分。
</div>

<div class="lab-prereq">
在编写代码时，您需要参考 Tulip <a href="readings/21143.pdf">硬件参考手册</a>。以下章节尤为重要，在编写代码之前您应该阅读（或至少粗读）它们：
    <ul>
      <li>1.1（引言）</li>
      <li>3.2（控制和状态寄存器）</li>
      <li>4.2（DMA 描述符和设置帧）</li>
      <li>4.3.4（中断）</li>
      <li>4.3.6（接收）</li>
      <li>4.3.7（发送）</li>
    </ul>
</div>

Tulip 具有控制和状态寄存器（CSR），软件使用它们来初始化 Tulip 并控制其某些行为。我们给您的代码将 Tulip 的 CSR 映射到内核内存中的地址，并将 CSR0 的地址传递给 `tulip_init()`。您需要向 `tulip_init()` 添加一些代码来配置 Tulip 硬件。

Tulip 使用 DMA（直接内存访问）从 RAM 读取发送数据包，并将接收数据包复制到 RAM 中以便软件进行处理。为了使 Tulip 硬件和驱动软件能够批量处理数据包，而不必强行同步运行，驱动程序可以为 Tulip 提供多个要发送的数据包，以及多个用于放置接收数据包的缓冲区。驱动程序使用发送或接收描述符描述每个数据包缓冲区（缓冲区地址、长度和状态），如 Tulip 手册第 4.2 节所述。Tulip 支持将描述符作为环（ring）使用，以便在发送或接收到固定大小描述符数组的最后一项后，Tulip 返回到第一个描述符。

当 `net.c` 中的网络栈需要发送数据包时，它会调用 `tulip_xmit()`，并带有一个指向保存要发送的数据包的缓冲区的指针；`net.c` 使用 `kalloc()` 分配此缓冲区。您的发送代码必须将指向数据包数据的指针放在发送环的描述符中。您必须确保每个缓冲区最终都传递给 `kfree()`，但只有在 Tulip 完成数据包发送之后。`tulip_xmit()` 不应该阻塞：它应该在将数据包放入发送 DMA 环后返回。

Tulip 在接收和发送数据包后可以生成中断。它可能会批量处理中断：它可能仅引发一个中断来发出信号表示它已接收和发送了多个数据包。Tulip 在 IRQ 33 上生成中断。您需要修改 `trap.c` 以调用您在 `tulip.c` 中实现的中断处理程序。您的处理程序必须扫描接收描述符环，并通过调用 `net_rx()` 将每个新数据包投递给网络栈（在 `net.c` 中）。然后，处理程序应该使用 `kalloc()` 分配一个新缓冲区并将其放入描述符中，以便当 Tulip 再次到达接收环的该点时，它会找到一个新鲜的缓冲区来 DMA 新的数据包。`net_rx()` 将会 `kfree()` 传给它的每个数据包。

### Tulip Transmit (Tulip 发送)

要测试 `tulip_xmit()` 发送单个数据包，请在一个窗口中（xv6 之外）运行 `python3 nettest.py txone`，并在另一个窗口中在 xv6 内运行 `nettest txone`，这将发送单个数据包。如果一切顺利，这两个窗口应该看起来像：

```
$ nettest txone
txone: sending one packet
```

```
$ python3 nettest.py txone
tx: listening for a UDP packet
txone: OK
```

`tcpdump -XXnr packets.pcap` 应该产生类似于以下的输出：

```
reading from file packets.pcap, link-type EN10MB (Ethernet)
21:27:31.688123 IP 10.0.2.15.2000 > 10.0.2.2.25603: UDP, length 5
        0x0000:  5255 0a00 0202 5254 0012 3456 0800 4500  RU....RT..4V..E.
        0x0010:  0021 0000 0000 6411 3ebc 0a00 020f 0a00  .!....d.>.......
        0x0020:  0202 07d0 6403 000d 0000 7478 6f6e 65    ....d.....txone
```

您应该通过在一个窗口上运行 `python3 nettest.py tx32`，然后在 xv6 内运行 `nettest tx32` 来测试您的驱动程序发送多个数据包的能力。在第一个窗口中您应该看到：

```
$ python3 nettest.py tx32
tx: waiting for 32 UDP packets...
tx: OK
```

### Tulip Receive (Tulip 接收)

要测试接收，请在一个窗口中启动 xv6，然后在另一个窗口中运行 `python3 nettest.py rxone`。`nettest.py rxone` 向 xv6 发送两个数据包：一个 ARP 请求，然后是一个 UDP/IP 数据包。`net.c` 包含检测 ARP 请求并调用 `tulip_xmit()` 发送 ARP 答复的代码。在 xv6 窗口中您应该看到：

```
init: starting sh
$ arp_rx: received an ARP packet
ip_rx: received an IP packet
.
```

在外部窗口中：

```
$ python3 nettest.py rxone
txone: sending one UDP packet
```

如果一切顺利，`tcpdump -XXnr packets.pcap` 应该产生如下输出：

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

要测试您的驱动程序是否可以连续接收许多数据包，请在一个窗口中启动 xv6，在另一个窗口中运行 `python3 nettest.py rx32`。在 xv6 窗口中您应该看到：

```
init: starting sh
$ arp_rx: received an ARP packet
ip_rx: received an IP packet
................................
```

`net.c` 为 Tulip 驱动程序传递给 `net_rx()` 的每个 IP 数据包打印一个点；应该有 32 个点。

### Tulip 提示

* 驱动程序必须告诉 Tulip 其 48 位 MAC（以太网）地址，以便 Tulip 可以忽略发送给其他主机的接收数据包，仅投递传入的广播数据包和专门发送给当前主机 MAC 地址的数据包。Xv6 的 MAC 地址应设置为 52:54:00:12:34:56。您可以使用第 4.2.3 节中描述的设置帧（Setup Frame）来完成此操作（不要忘记第 4.2.2.2 节中的设置数据包位）。
* 在本次实验中，如果在初始化 Tulip 时驱动程序设置了 CSR6 中的混杂位（Promiscuous bit），则可以省略设置帧。此位导致 NIC 将其看到的所有数据包都投递给驱动程序，而不光是发送给 NIC MAC 地址的数据包。
* 您可以使用 qemu 的 `--trace` 选项让 qemu 的 Tulip 模拟器打印跟踪（调试）信息。执行此操作的一种简单方法是将以下行中的一行或多行添加到 xv6 的 Makefile 中：

  ```
  QEMUOPTS += --trace 'tulip_reg_write'
  QEMUOPTS += --trace 'tulip_reg_read'
  QEMUOPTS += --trace 'tulip_descriptor'
  QEMUOPTS += --trace 'tulip_receive'
  QEMUOPTS += --trace 'tulip_tx_state'
  QEMUOPTS += --trace 'tulip_rx_state'
  QEMUOPTS += --trace 'tulip_irq'
  QEMUOPTS += --trace 'tulip_*'
  ```

  在 Makefile 中查找包含 `--trace` 的行，以查看 Tulip 跟踪选项。
* 一旦 `make grade` 显示前三个测试通过，您就可以进入本实验的下一部分：

  ```
  $ make grade
  ...
  == Test   nettest: txone == 
    nettest: txone: OK 
  == Test   nettest: arp_rx == 
    nettest: arp_rx: OK 
  == Test   nettest: ip_rx == 
    nettest: ip_rx: OK 
  ```

## Part Two: UDP Receive (第二部分：UDP 接收) (Moderate)

UDP（用户数据报协议）允许不同 Internet 主机上的用户进程交换单个数据包（数据报）。UDP 分层在 IP 之上。用户进程通过指定 32 位 IP 地址来指示它要将数据包发送到哪个主机。每个 UDP 数据包包含一个源端口号和一个目的端口号；进程可以请求接收发送给特定端口号的数据包，并且可以在发送时指定目的端口号。因此，如果两个不同主机上的进程知道彼此的 IP 地址和各自正在监听的端口号，它们就可以使用 UDP 进行通信。例如，Google 在 IP 地址为 8.8.8.8 的主机上运行一个 DNS 域名服务器，监听 UDP 端口 53。

在本次任务中，您将把代码添加到 `kernel/net.c` 中以接收 UDP 数据包，将它们排队，并允许用户进程读取它们。`net.c` 已经包含了用户进程发送 UDP 数据包所需的代码（您提供的 `tulip_xmit()` 除外）。

<div class="lab-required">
您的任务是填写 `kernel/net.c` 中 `ip_rx()`、`sys_recv()` 和 `sys_bind()` 的实现。当 `make grade` 显示您的解决方案通过了所有测试时，您就完成了。

您可以通过在一个窗口中启动 `python3 nettest.py grade`，然后在另一个窗口中在 xv6 内运行 `nettest grade` 来运行与 `make grade` 运行相同的测试。如果一切顺利，您应该在第一个（xv6 之外）窗口中看到：

```
$ python3 nettest.py grade
txone: OK
rxone: sending one UDP packet
```

并在 xv6 窗口中看到：

```
$ nettest grade
txone: sending one packet
arp_rx: received an ARP packet
ip_rx: received an IP packet
ping0: starting
ping0: OK
ping1: starting
ping1: OK
ping2: starting
ping2: OK
ping3: starting
ping3: OK
dns: starting
DNS arecord for pdos.csail.mit.edu. is 128.52.129.126
dns: OK
free: OK
```
</div>

本实验的 UDP 系统调用 API 规范如下：

* `send(short sport, int dst, short dport, char *buf, int len)`：此系统调用向 IP 地址为 `dst` 的主机发送一个 UDP 数据包，该主机上监听 `dport` 端口的进程接收该数据包。数据包的源端口号将为 `sport`（此端口号会报告给接收进程，以便其回复发送者）。UDP 数据包的内容（“有效负载”，payload）将是地址 `buf` 处的 `len` 字节。成功时返回值为 0，失败时返回值为 -1。
* `recv(short dport, int *src, short *sport, char *buf, int maxlen)`：此系统调用返回目的端口为 `dport` 的到达 UDP 数据包的有效负载。如果在调用 `recv()` 之前到达了一个或多个数据包，它应该立即返回最早等待的数据包。如果没有数据包在等待，`recv()` 应该等待，直到发送给 `dport` 的数据包到达。`recv()` 应该按到达顺序查看给定端口的到达数据包。`recv()` 将数据包的 32 位源 IP 地址复制到 `*src`，将数据包的 16 位 UDP 源端口号复制到 `*sport`，将数据包的 UDP 有效负载的最多 `maxlen` 字节复制到 `buf`，并从队列中移除该数据包。系统调用返回复制的 UDP 有效负载的字节数，如果有错误则返回 -1。
* `bind(short port)`：进程在调用 `recv(port, ...)` 之前应该先调用 `bind(port)`。如果到达的 UDP 数据包的目的端口尚未传递给 `bind()`，`net.c` 应该丢弃该数据包。此系统调用的原因是为了初始化 `net.c` 需要的任何结构，以便为随后的 `recv()` 调用存储到达的数据包。
* `unbind(short port)`：您不需要实现此系统调用，因为测试代码不使用它。但如果您愿意，也可以实现它以提供与 `bind()` 的对称性。

传递给这些系统调用以及由它们返回的所有地址和端口号必须采用主机字节序（见下文）。

除了 `send()` 之外，您需要提供系统调用的内核实现。程序 `user/nettest.c` 使用此 API。

为了使 `recv()` 工作，您需要向 `ip_rx()` 添加代码，`net_rx()` 会为每个收到的 IP 数据包调用该函数。`ip_rx()` 应该决定到达的数据包是否为 UDP，以及其目的端口是否已传递给 `bind()`；如果两者都为真，它应该将数据包保存在 `recv()` 可以找到的地方。然而，对于任何给定的端口，最多只应保存 16 个数据包；如果有 16 个数据包已在等待 `recv()`，则应丢弃该端口的新传入数据包。该规则的目的是防止快速或滥用的发送方迫使 xv6 耗尽内存。此外，如果由于某个端口已有 16 个数据包在等待而导致数据包被丢弃，那不应该影响发送给其他端口的数据包。

`ip_rx()` 查看的数据包缓冲区包含 14 字节以太网头部，后跟 20 字节 IP 头部，后跟 8 字节 UDP 头部，后跟 UDP 有效负载。您将在 `kernel/net.h` 中找到其中每一个的 C 结构体定义。维基百科上在[这里](https://en.wikipedia.org/wiki/Internet_Protocol_version_4#Header)有 IP 头部的描述，在[这里](https://en.wikipedia.org/wiki/User_Datagram_Protocol)有 UDP 头部的描述。

生产环境的 IP/UDP 实现非常复杂，需要处理协议选项并验证不变量。您只需要做足够的工作以通过 `make grade`。您的代码需要查看 IP 头部中的 `ip_p` 和 `ip_src`，以及 UDP 头部中的 `dport`、`sport` 和 `ulen`。

注意字节序（byte order）。包含多字节整数的以太网、IP 和 UDP 头部字段在数据包中将最高有效字节排在最前（大端序）。RISC-V CPU 在内存中布置多字节整数时，将最低有效字节排在最前（小端序）。这意味着，当代码从数据包中提取多字节整数时，必须重新排列字节。这适用于 short（2 字节）和 int（4 字节）字段。您可以分别对 2 字节和 4 字节字段使用 `ntohs()` 和 `ntohl()` 函数。在查看 2 字节以太网类型字段时，请参考 `net_rx()` 的示例。

如果您的 Tulip 驱动程序中存在错误或遗漏，它们可能仅在 ping 测试期间才开始引起问题。例如，ping测试发送和接收了足够多的数据包，以至于描述符环索引会发生回绕（wrap around）。

一些提示：

* 创建一个结构体来跟踪已绑定的端口及其队列中的数据包。
* 参考 `kernel/proc.c` 中的 `sleep(void *chan, struct spinlock *lk)` 和 `wakeup(void *chan)` 函数来实现 `recv()` 的等待逻辑。
* `sys_recv()` 将数据包复制到的目的地址是虚拟地址；您将必须从内核复制到当前 user 进程。
* 确保释放已被复制过去或已被丢弃的数据包。

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

* 在本次实验中，网络栈使用中断来处理入站数据包处理，但不用来处理出站数据包处理。更复杂的策略是在软件中对出站数据包进行排队，并且一次仅提供有限数量的数据包给 NIC。然后，您可以依靠发送中断来重新填满发送环。使用该技术，就可以优先处理不同类型的出站流量。 (Easy)
* 提供的网络代码仅部分支持 ARP。实现一个完整的 [ARP 缓存 (ARP cache)](https://tools.ietf.org/html/rfc826)。 (Moderate)
* [ICMP](https://tools.ietf.org/html/rfc792) 可以提供网络流失败的通知。检测这些通知并将它们作为错误传播给用户进程。
* 本实验中的网络栈容易受到接收活锁（livelock）的影响。使用讲座和阅读作业中的材料，设计并实现一个解决方案来修复它。 (Moderate)，但很难测试。
* 实现一个极简的 TCP 栈并下载一个网页。 (Hard)

一些挑战练习旨在提高性能，其效果在 QEMU 下可能不明显或无法衡量。

如果您尝试了挑战问题，无论它是否与网络有关，请告诉课程工作人员！

---
有关 6.1810 的问题或意见？请发电子邮件给课程工作人员：[61810-staff@lists.csail.mit.edu](mailto:61810-staff@lists.csail.mit.edu)。

<a rel="license" href="https://creativecommons.org/licenses/by/3.0/us/"><img alt="Creative Commons License" style="border-width:0" src="https://i.creativecommons.org/l/by/3.0/us/88x31.png" ></a>
