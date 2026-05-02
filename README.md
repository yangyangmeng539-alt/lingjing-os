````markdown
# Lingjing OS / 灵境 OS

```text
Lingjing OS 是一个实验性裸机操作系统项目。

当前定位不是普通玩具内核，也不是现阶段要替代 Windows / Linux 的完整桌面系统，而是在验证一套新的系统结构：

内核基础能力
+ 自检 / 医生 / 修复系统
+ 模块依赖系统
+ Capability 能力注册系统
+ Intent 意图驱动执行层
+ 用户态 / syscall / ring3 原型
+ Bootinfo / Framebuffer / Graphics
+ Real framebuffer 图形输出
+ Bitmap 字体
+ Graphical Shell / GShell 原型

核心理念：

My machine, my rules.
````

---

# 当前版本

```text
version: dev-0.4.6
stage: graphical shell layout cleanup
arch: i386
boot: multiboot1 text + multiboot2 gfxboot
platform: baremetal
```

---

# 当前项目定位

```text
Lingjing OS 当前是：

意图驱动的实验性裸机内核原型
Intent-driven bare-metal OS prototype

不是完整桌面操作系统。
不是 Linux 发行版。
不是 Windows 替代品。
当前重点是验证一套“用户控制层 OS 架构”。
```

核心方向：

```text
用户不直接操作底层资源，而是表达 intent；
系统根据 intent 自动检查 capability、module、permission、health、platform、identity、memory、paging、syscall、user、ring3、graphics 等状态；
如果系统健康，则执行；
如果系统不健康，则阻断并报告。
```

一句话：

```text
灵境OS 不是单纯追求“能画图”，而是在构建：

启动
-> 模块
-> 能力
-> 意图
-> 自诊断
-> 图形壳
-> 用户控制层

这一整条可观察、可诊断、可阻断、可恢复的系统链。
```

---

# 当前主链

当前有两条启动链：

```text
run-text：
稳定文本安全链
Multiboot1
VGA text mode
适合 shell / health / doctor / syscall / ring3 / module / intent 测试

run-gfxboot：
隔离图形实验链
Multiboot2
800x600x32 framebuffer
适合真实图形 UI / GShell 测试
```

推荐测试顺序：

```bash
cd ~/osdev/lingjing
make clean
make run-text
```

再测图形链：

```bash
cd ~/osdev/lingjing
make clean
make run-gfxboot
```

---

# 当前已经完成的关键能力

## 1. 基础裸机启动链

已完成：

```text
GRUB 启动
Multiboot1 文本链
Multiboot2 图形链
i386 32 位内核
GDT
IDT
键盘中断
Timer IRQ
Scheduler metadata
Security layer
Syscall layer
User mode metadata
Ring3 metadata
Module registry
Capability registry
Health
Doctor
Shell
```

`run-text` 当前稳定：

```text
version 正常
dash 正常
health result ok
doctor result ready
bootinfo check ready
graphics doctor ready
gshell doctor ready
```

---

## 2. Multiboot / Bootinfo

当前 bootinfo 已支持：

```text
Multiboot1
Multiboot2
boot handoff probe
magic repair
framebuffer metadata parse
```

当前文本链：

```text
protocol: multiboot1
magic:    0x2BADB002
fb addr:  0x000B8000
width:    80
height:   25
bpp:      16
type:     2
profile:  text-vga-80x25
```

当前图形链：

```text
protocol: multiboot2
framebuffer: present
width:  800
height: 600
bpp:    32
type:   rgb graphics
```

已解决的问题：

```text
run-text 下 0x2BADB0FF 低字节异常已修复为 0x2BADB002
run-gfxboot 已切到 Multiboot2 framebuffer request
run-text 和 run-gfxboot 已隔离，不互相污染
```

---

## 3. Framebuffer / Graphics

当前 graphics 已完成：

```text
framebuffer metadata
framebuffer profile
real write gate
32bpp framebuffer pixel write
rect draw
clear draw
bitmap 5x7 字体
graphics text draw
graphics panel
graphics boot auto mark
```

当前图形链已经能真实写屏：

```text
内核直接写 framebuffer
能画矩形
能画像素
能画英文字符
能画图形面板
能根据输入重绘界面
```

当前安全规则：

```text
text-mode framebuffer:
  type 2
  0xB8000
  禁止误当作像素 framebuffer 写入

graphics framebuffer:
  type 0 / 1
  32bpp
  允许 real framebuffer write
```

---

## 4. Graphical Shell / GShell

当前 GShell 已完成到：

```text
图形 Dash
图形命令区
键盘输入桥
图形命令分发
命令结果视图
真实内核状态桥
命令历史 history
执行结果日志 log
布局清理
```

当前图形 Shell 支持的图形命令：

```text
dash
health
doctor
help
clear
history
histclear
log
logclear
```

当前图形输入链：

```text
keyboard IRQ
↓
platform_read_char
↓
shell_update
↓
gshell_input_char
↓
gshell_dispatch_command
↓
gshell_command_view 状态变化
↓
gshell_graphics_dashboard
↓
graphics_text / graphics_rect
↓
framebuffer 像素重绘
```

这说明：

```text
图形界面已经不只是画图；
它已经能接收键盘输入、识别命令、切换视图、读取内核状态并重绘。
```

---

# 当前图形 Shell 已验收

## dash

图形窗口输入：

```text
dash
```

预期：

```text
REAL DASH
UP
TICKS
MODULES
TASKS
ACTIVE
MODE
SYSCALLS
BRIDGE
```

说明：

```text
dash 已读取真实 uptime / ticks / module count / task count / scheduler mode / syscall count。
```

---

## health

图形窗口输入：

```text
health
```

预期：

```text
REAL HEALTH
DEPS      OK
TASK      OK
SECURITY  OK
MEMORY    OK
PAGING    OK
SYSCALL   OK
USER      OK
RESULT    OK
```

说明：

```text
health 图形视图已接入真实 health 状态。
```

---

## doctor

图形窗口输入：

```text
doctor
```

预期：

```text
REAL DOCTOR
MODULE   OK
TASK     OK
SWITCH   OK
SYSCALL  OK
RING3    OK
MEMORY   OK
PAGING   OK
RESULT   OK
```

说明：

```text
doctor 图形视图已接入真实诊断摘要。
```

---

## history

图形窗口输入：

```text
history
```

预期：

```text
HISTORY VIEW
dash       DASH
health     HEALTH
doctor     DOCTOR
history    HISTORY
COUNT      4
TOTAL      4
```

说明：

```text
图形 Shell 已能记录最近命令历史。
```

---

## histclear

图形窗口输入：

```text
histclear
```

预期：

```text
HISTORY CLEARED
COUNT 0
READY FOR NEW COMMANDS
```

说明：

```text
图形 Shell 已能清空 history。
```

---

## log

图形窗口输入：

```text
log
```

预期：

```text
RESULT LOG
dash       DASH OK
health     HEALTH OK
abc        UNKNOWN
log        LOG OK
COUNT      4
TOTAL      4
```

说明：

```text
图形 Shell 已能记录命令执行结果。
```

---

## logclear

图形窗口输入：

```text
logclear
```

预期：

```text
LOG CLEARED
COUNT 1
LOGCLEAR RECORDED
```

说明：

```text
图形 Shell 已能清空 result log，并记录 logclear 自己。
```

---

# 总体架构

```text
Lingjing OS Runtime
│
├─ Intent Layer
│  ├─ intent registry
│  ├─ intent plan
│  ├─ intent run / stop
│  └─ intent -> required capabilities
│
├─ Capability Layer
│  ├─ capability registry
│  ├─ capability check
│  ├─ capability doctor
│  └─ capability -> provider module
│
├─ Module Layer
│  ├─ module registry
│  ├─ module dependency check
│  ├─ module break / fix
│  ├─ load / unload
│  └─ module -> provides capabilities
│
├─ Task / Scheduler Layer
│  ├─ task table
│  ├─ task lifecycle
│  ├─ scheduler ticks
│  ├─ cooperative yield
│  └─ task switch metadata
│
├─ Kernel Core Layers
│  ├─ memory / heap
│  ├─ paging
│  ├─ syscall
│  ├─ user metadata
│  ├─ ring3
│  ├─ security
│  ├─ platform
│  ├─ identity
│  └─ health / doctor
│
├─ Boot / Display Layer
│  ├─ bootinfo
│  ├─ multiboot1 text chain
│  ├─ multiboot2 gfxboot chain
│  ├─ framebuffer metadata
│  └─ framebuffer profile
│
├─ Graphics Layer
│  ├─ real write gate
│  ├─ real framebuffer32 backend
│  ├─ pixel draw
│  ├─ rect draw
│  ├─ clear draw
│  ├─ bitmap 5x7 font
│  └─ text draw
│
├─ Graphical Shell
│  ├─ graphics dashboard
│  ├─ command zone
│  ├─ input bridge
│  ├─ command dispatch
│  ├─ kernel status bridge
│  ├─ history
│  └─ result log
│
└─ Shell
   ├─ system commands
   ├─ intent commands
   ├─ capability commands
   ├─ module commands
   ├─ diagnostic commands
   ├─ graphics commands
   └─ gshell commands
```

---

# 关键设计原则

```text
Intent 不直接找 module
Intent 先声明 capability
Capability 再匹配 provider module
Module 受依赖系统管理
Task Runtime 后续绑定执行上下文
Health / Doctor 统一决定系统是否 ready
Graphics / GShell 必须先通过 gate，不能破坏 VGA shell
run-text 是稳定安全链，不能随便破坏
run-gfxboot 是隔离图形实验链
```

---

# 当前模块

当前核心模块：

```text
core
screen
bootinfo
gdt
idt
keyboard
timer
scheduler
security
syscall
user
ring3
capability
framebuffer
graphics
gshell
platform
lang
identity
health
memory
paging
shell
```

当前模块状态：

```text
total:  23
ok:     23
broken: 0
```

当前模块依赖关系：

```text
core          ok
screen        depends core
bootinfo      depends core
gdt           depends core
idt           depends gdt
keyboard      depends idt
timer         depends idt
scheduler     depends timer
security      depends core
syscall       depends security
user          depends syscall
ring3         depends user
capability    depends core
framebuffer   depends screen
graphics      depends framebuffer
gshell        depends graphics
platform      depends core
lang          depends core
identity      depends security
health        depends core
memory        depends core
paging        depends memory
shell         depends keyboard
```

外部能力模块会在 intent 运行时按需加载：

```text
gui
net
ai
fs
```

---

# Capability Layer

当前 capability registry 已接入：

```text
intent
graphics
bootinfo
framebuffer
gshell
health
syscall
ring3
```

当前能力表：

```text
gui.display       provider: gui          permission: display      status: available
gui.framebuffer   provider: framebuffer  permission: display      status: ready
gui.graphics      provider: graphics     permission: display      status: ready
gui.shell         provider: gshell       permission: display      status: ready
boot.multiboot    provider: bootinfo     permission: boot         status: ready
net.http          provider: net          permission: network      status: available
ai.assist         provider: ai           permission: intent       status: available
file.read         provider: fs           permission: file         status: planned
sys.health        provider: health       permission: diagnostic   status: ready
sys.syscall       provider: syscall      permission: syscall      status: ready
user.ring3        provider: ring3        permission: ring3        status: ready
```

相关命令：

```text
capabilities
capinfo <capability>
capcheck <capability>
capdoctor
```

示例：

```text
capabilities
capcheck sys.syscall
capcheck gui.display
capcheck gui.framebuffer
capcheck gui.graphics
capcheck gui.shell
capcheck boot.multiboot
load gui
capcheck gui.display
capdoctor
```

---

# Intent Layer

Intent 层负责把用户意图转成系统执行计划。

当前支持：

```text
video
ai
network
```

当前 intent 示例：

```text
video requires capabilities:
  gui.display
  net.http
  ai.assist
```

相关命令：

```text
intent list
intent permissions
intent policy
intent audit
intent doctor
intent plan <name>
intent run <name>
intent stop
intent stop <name>
intent restart
intent switch <name>
intent status
intent context
intent history
intent clear-history
intent reset
intent allow <name>
intent deny <name>
intent lock
intent unlock
```

Intent 执行前会检查：

```text
intent lock
intent allow / deny
capability registry
provider module
module dependency health
security policy
platform health
identity health
memory health
paging health
syscall health
user health
ring3 health
```

执行流程：

```text
用户意图
↓
intent plan
↓
能力检查
↓
权限检查
↓
安全策略检查
↓
模块依赖检查
↓
platform 状态检查
↓
identity 状态检查
↓
memory / paging / syscall / user / ring3 健康检查
↓
按需加载 provider modules
↓
创建 intent task context
↓
运行 intent
↓
停止 intent
↓
清理 task context
↓
卸载外部模块
```

---

# Health / Doctor

Lingjing OS 的核心机制之一：

```text
发现异常
-> 标记 broken
-> health bad
-> doctor blocked
-> intent system blocked

修复异常
-> health ok
-> doctor ready
-> intent system ready
```

当前健康项：

```text
deps
security
task
lang
platform
identity
memory
paging
switch
syscall
user
ring3
result
```

正常输出：

```text
System health:
  deps:     ok
  security: ok
  task:     ok
  lang:     ok
  platform: ok
  identity: ok
  memory:   ok
  paging:   ok
  switch:   ok
  syscall:  ok
  user:     ok
  ring3:    ok
  result:   ok
```

当前诊断项：

```text
module dependencies
task health
scheduler
scheduler active
task switch layer
syscall layer
user mode layer
ring3 layer
security
language layer
platform layer
identity layer
memory layer
paging layer
current platform
current language
intent system
result
```

正常输出：

```text
System doctor:
  module dependencies: ok
  task health:         ok
  scheduler:           ok
  scheduler active:    ok
  task switch layer:   ok
  syscall layer:       ok
  user mode layer:     ok
  ring3 layer:         ok
  security:            ok
  language layer:      ok
  platform layer:      ok
  identity layer:      ok
  memory layer:        ok
  paging layer:        ok
  current platform:    baremetal
  current language:    en
  intent system:       ready
  result:              ready
```

---

# Syscall Layer

当前 syscall 层已经完成真实 `int 0x80` 路径。

已支持：

```text
syscall table
syscall dispatch
unsupported syscall count
IDT vector 0x80
isr128
syscall interrupt handler
EAX syscall id
EBX / ECX / EDX args
register frame capture
syscall return value
syscall frame
syscall ret
syscall stats
syscall doctor
health syscall integration
doctor syscall layer integration
```

当前 syscall table：

```text
0    SYS_PING      ready
1    SYS_UPTIME    ready
2    SYS_HEALTH    ready
3    SYS_USER      metadata
4    RESERVED      reserved
5    RESERVED      reserved
6    RESERVED      reserved
7    RESERVED      reserved
```

相关命令：

```text
syscall
syscall status
syscall check
syscall doctor
syscall table
syscall stats
syscall interrupt
syscall frame
syscall ret
syscall call <id>
syscall int <id>
syscall real <id>
syscall realargs <id> <a> <b> <c>
syscallbreak
syscallfix
```

三条 syscall 路径：

```text
syscall call <id>
  shell 模拟调用 syscall dispatcher

syscall int <id>
  模拟 interrupt path，不触发真实 CPU int

syscall real <id>
  真实 int 0x80，从内核态触发

syscall realargs <id> <a> <b> <c>
  真实 int 0x80，并通过 eax / ebx / ecx / edx 传参
```

---

# User / Ring3 Layer

## User Metadata Layer

当前 user 层提供用户态元数据，不是完整进程加载器。

已支持：

```text
user mode layer
user programs metadata
user entries metadata
user stats
user doctor
user break / fix
health user integration
doctor user mode layer integration
modulecheck user depends syscall
```

当前 user programs：

```text
0    init         metadata
1    shell-user   metadata
2    intent-user  metadata
3    reserved     reserved
```

## Ring3 Layer

当前 ring3 层已经完成真实 ring0 -> ring3 跳转，并完成 ring3 syscall roundtrip。

已支持：

```text
ring3 metadata layer
ring3 transition guard
ring3 iret frame
ring3 asm stub
ring3 syscall asm stub
GDT user descriptors
TSS descriptor
ltr
user page metadata gate
hardware install gate
realenter gate
ring3 -> int 0x80 -> kernel syscall handler
syscall return / controlled user loop
```

危险命令：

```text
ring3 realenter
```

说明：

```text
ring3 realenter 会真实进入 ring3。
如果 syscall stub 已选中，会执行 ring3 -> int 0x80 -> kernel syscall handler。
随后返回 ring3 controlled user loop。
这一步通常会停住，这是当前阶段的正常结果。
```

完整 ring3 syscall 测试链：

```text
ring3 syscallprepare
ring3 syscalldryrun
ring3 syscallstubprepare
ring3 syscallstubselect
ring3 syscallgateinstall
ring3 syscallarm
ring3 syscallrealarm

ring3 gdtinstall
ring3 tssinstall
ring3 pageprepare

ring3 enableswitch
ring3 dryrun
ring3 hwcheck
ring3 hwinstall
ring3 arm
ring3 realenter
```

---

# Scheduler / Task Layer

当前调度器仍是原型，不是真实硬件级上下文切换。

已支持：

```text
IRQ0 tick
scheduler ticks
yield count
task runtime ticks
static base task table
dynamic task create
active task rotation
cooperative yield
run queue
scheduler log
scheduler reset
scheduler validate
scheduler fix
task doctor
task lifecycle hardening
task create / wake / sleep / kill / exit
task priority
task broken / fix
exit code
lifecycle event count
context switch metadata
runqueue reason
system doctor integration
```

基础任务表：

```text
0    idle     kernel
1    shell    system
2    intent   system
3    timer    kernel
```

合法任务状态：

```text
created
ready
running
blocked
sleeping
killed
broken
```

相关命令：

```text
tasks
taskinfo <id>
taskstate <id> <state>
taskcreate <name>
taskkill <id>
tasksleep <id>
taskwake <id>
taskprio <id> <priority>
taskexit <id> <code>
taskbreak <id>
taskfix <id>
taskstats
taskcheck
taskdoctor
schedinfo
schedlog
schedclear
schedreset
schedvalidate
schedfix
runqueue
yield
```

---

# Task Switch Metadata Layer

当前 task switch 仍是 metadata prototype，不做真实 ESP / EIP 切换。

已支持：

```text
task stack metadata
stack base
stack size
context ready
context switch count
task switch doctor
task switch break / fix
health switch integration
doctor task switch layer integration
```

相关命令：

```text
taskswitch
taskswitchcheck
taskswitchdoctor
taskswitchbreak
taskswitchfix
```

当前定位：

```text
不是完整上下文切换
不是真实进程切换
不是 ring3 task switch
当前是后续真实 task switch 的诊断骨架
```

---

# Memory / Heap Layer

当前 memory 层是 header + free-list prototype。

已支持：

```text
placement address
kmalloc
kcalloc
kfree
block header
real size
free list
block reuse
allocated bytes
freed bytes
live bytes
live alloc count
failed free count
double free detection
heapstats
heapcheck
heapdoctor
heapbreak / heapfix
health memory integration
doctor memory layer integration
```

相关命令：

```text
mem
kmalloc <bytes>
kcalloc <bytes>
kfree <addr>
heapcheck
heapdoctor
heapstats
heapbreak
heapfix
peek <addr>
poke <addr> <value>
hexdump <addr> <len>
kzero <addr> <len>
```

当前定位：

```text
不是完整通用 heap allocator
暂不做 coalescing
暂不做复杂碎片整理
当前目标是内核动态内存生命周期可诊断
```

---

# Paging Layer

当前 paging 层是 4MB identity paging prototype。

已支持：

```text
page directory
first page table
identity mapping
CR3 load
CR0.PG enable
paging enable
paging map check
paging flags
paging stats
paging doctor
CR0 display
CR3 display
PG bit display
mapped range display
page flags display
accessed bit recognition
unmapped address recognition
health paging integration
doctor paging layer integration
```

当前映射范围：

```text
0x00000000 - 0x003FFFFF
```

当前页数：

```text
1024 pages
```

当前大小：

```text
4MB
```

相关命令：

```text
paging
paging status
paging check
paging doctor
paging map <addr>
paging flags <addr>
paging stats
paging enable
pagingbreak
pagingfix
```

当前定位：

```text
已经能开启 paging 并回到 shell
当前仍是 identity mapping
暂不做 page allocator
暂不做 map / unmap
暂不做用户态地址空间隔离
```

---

# Security Layer

当前安全层是策略骨架，不是完整防病毒系统。

已支持：

```text
security policy switch
module protection
intent permission check
security audit log
security doctor check
intent run security check
module load security check
module unload security check
core module unload protection
```

相关命令：

```text
security
securitycheck
securitylog
securityclear
```

当前定位：

```text
不是杀毒系统
不是病毒库
不是沙箱
不是完整证书体系
当前是系统安全策略入口
```

---

# Platform Layer

当前 platform 层是硬件能力抽象骨架。

当前平台：

```text
baremetal
```

当前能力状态：

```text
output
input
timer
power
```

相关命令：

```text
platform
platformcheck
platformdeps
platformboot
platformsummary
platformcaps
platformbreak <capability>
platformfix <capability>
```

当前定位：

```text
硬件能力抽象
平台状态可检查
平台故障可诊断
平台修复可恢复
上层逐步脱离硬件细节
```

---

# Identity Layer

当前 identity 层是系统身份占位层，不是真实身份系统。

当前状态：

```text
state: ready
mode: local-placeholder
public key: not-generated
```

相关命令：

```text
identity
identity status
identity doctor
identity check
identity break
identity fix
```

当前阶段不实现：

```text
identity generate
identity export
identity import
identity reset
```

---

# Language Layer

当前语言层是系统显示语言骨架。

默认语言：

```text
en
```

预留：

```text
zh
```

说明：

```text
当前仍使用 VGA 文本模式 / bitmap ASCII 字体。
暂不支持真正 UTF-8 中文显示。
中文通道目前使用 [ZH] ASCII 占位，避免乱码。
```

相关命令：

```text
lang
lang en
lang zh
```

---

# 构建环境

推荐环境：

```text
Windows + WSL2 Ubuntu + QEMU
```

依赖：

```bash
sudo apt install -y build-essential gcc make nasm qemu-system-x86 grub-pc-bin grub-common xorriso mtools gdb git curl wget
```

---

# 构建与运行

进入项目：

```bash
cd ~/osdev/lingjing
```

清理：

```bash
make clean
```

编译：

```bash
make
```

运行文本安全链：

```bash
cd ~/osdev/lingjing
make clean
make run-text
```

运行图形实验链：

```bash
cd ~/osdev/lingjing
make clean
make run-gfxboot
```

默认运行：

```bash
make run
```

退出文本 shell：

```text
halt
```

---

# 常用验收命令

## run-text 验收

```bash
cd ~/osdev/lingjing
make clean
make run-text
```

进入 shell 后输入：

```text
version
dash
health
doctor
bootinfo check
graphics doctor
gshell input
gshell status
gshell doctor
```

预期：

```text
version: dev-0.4.6
health result: ok
doctor result: ready
bootinfo check: ready
graphics doctor: ready
gshell doctor: ready
```

说明：

```text
run-text 下 graphics 仍然是 dryrun / gated 是正常的。
因为文本链 framebuffer 是 0xB8000 / 80x25 / type 2。
```

---

## run-gfxboot 验收

```bash
cd ~/osdev/lingjing
make clean
make run-gfxboot
```

图形窗口里依次输入：

```text
dash
health
doctor
history
log
abc
logclear
log
```

预期：

```text
dash 显示 REAL DASH
health 显示 REAL HEALTH
doctor 显示 REAL DOCTOR
history 显示 HISTORY VIEW
log 显示 RESULT LOG
abc 显示 ERROR VIEW
logclear 显示 LOG CLEARED
```

---

# 常用命令列表

## 系统

```text
help
version
sysinfo
dashboard
dash
status
doctor
health
uptime
sleep <seconds>
reboot
halt
```

## Bootinfo / Framebuffer

```text
bootinfo
bootinfo framebuffer
bootinfo check
bootinfo doctor
bootinfo probe

framebuffer
fb
fb prepare
fb check
fb doctor
fb profile
```

## Graphics

```text
graphics
gfx
gfx backend
gfx backendcheck
gfx backenddoctor
gfx realarm
gfx realdisarm
gfx realgate
gfx font
gfx textbackend
gfx textarm
gfx textdisarm
gfx textgate
gfx textmetrics <text>
gfx bootmode
gfx bootplan
gfx bootdoctor
gfx bootpreflight
gfx boot text
gfx boot graphics
gfx clear
gfx pixel
gfx rect
gfx text
gfx panel
gfxbreak
gfxfix
```

## GShell 文本命令

```text
gshell
gshell status
gshell check
gshell doctor
gshell input
gshell gfxdash
gshell panel
gshell runtime
gshell intent
gshell system
gshell dashboard
gshell textpanel
gshell textdash
gshell textcompact
gshell statusbar
gshell output
gshell outputdoctor
gshell output text
gshell output graphics
gshell selfcheck
gshell selfrender
gshell runtimecheck
gshellbreak
gshellfix
```

## GShell 图形命令区命令

在 `make run-gfxboot` 的图形窗口中输入：

```text
dash
health
doctor
help
clear
history
histclear
log
logclear
```

## Capability

```text
capabilities
capinfo <capability>
capcheck <capability>
capdoctor
```

## Module

```text
modules
moduleinfo <name>
moduledeps <name>
moduletree
modulecheck
modulebreak <name>
modulefix <name>
load <module>
unload <module>
```

## Intent

```text
intent list
intent permissions
intent policy
intent audit
intent doctor
intent plan video
intent run video
intent status
intent context
intent stop
intent history
intent reset
intent allow <name>
intent deny <name>
intent lock
intent unlock
```

## Security

```text
security
securitycheck
securitylog
securityclear
```

## Platform

```text
platform
platformcheck
platformdeps
platformboot
platformsummary
platformcaps
platformbreak <capability>
platformfix <capability>
```

## Identity

```text
identity
identity status
identity doctor
identity check
identity break
identity fix
```

## Task / Scheduler

```text
tasks
taskinfo <id>
taskstate <id> <state>
taskcreate <name>
taskkill <id>
tasksleep <id>
taskwake <id>
taskprio <id> <priority>
taskexit <id> <code>
taskbreak <id>
taskfix <id>
taskstats
taskcheck
taskdoctor
schedinfo
schedlog
schedclear
schedreset
schedvalidate
schedfix
runqueue
yield
```

## Task Switch

```text
taskswitch
taskswitchcheck
taskswitchdoctor
taskswitchbreak
taskswitchfix
```

## Memory / Heap

```text
mem
kmalloc <bytes>
kcalloc <bytes>
kfree <addr>
heapcheck
heapdoctor
heapstats
heapbreak
heapfix
peek <addr>
poke <addr> <value>
hexdump <addr> <len>
kzero <addr> <len>
```

## Paging

```text
paging
paging check
paging doctor
paging map <addr>
paging flags <addr>
paging stats
paging enable
pagingbreak
pagingfix
```

## Syscall

```text
syscall
syscall table
syscall stats
syscall interrupt
syscall frame
syscall ret
syscall call <id>
syscall int <id>
syscall real <id>
syscall realargs <id> <a> <b> <c>
syscallbreak
syscallfix
```

## User

```text
user
user check
user doctor
user programs
user entries
user stats
userbreak
userfix
```

## Ring3

```text
ring3
ring3 status
ring3 check
ring3 doctor
ring3 guard
ring3 dryrun
ring3 stub
ring3 stubcheck
ring3 gdt
ring3 gdtinstall
ring3 tss
ring3 tssinstall
ring3 pageprepare
ring3 enableswitch
ring3 hwcheck
ring3 hwinstall
ring3 arm
ring3 disarm
ring3 realenter

ring3 syscallprepare
ring3 syscalldryrun
ring3 syscallstubprepare
ring3 syscallstubselect
ring3 syscallgate
ring3 syscallgateinstall
ring3 syscallgateclear
ring3 syscallarm
ring3 syscallrealarm
ring3 syscallresult
```

---

# 示例：Bootinfo / Framebuffer

查看 bootinfo：

```text
bootinfo
bootinfo framebuffer
bootinfo check
bootinfo doctor
bootinfo probe
```

文本链预期：

```text
protocol: multiboot1
framebuffer: yes
addr low:    0x000B8000
width:       80
height:      25
bpp:         16
type:        2
type name:   text-mode
```

图形链预期：

```text
protocol: multiboot2
framebuffer: yes
width:       800
height:      600
bpp:         32
type:        rgb graphics
```

---

# 示例：Graphics Gate

查看 graphics backend：

```text
gfx backend
gfx backendcheck
gfx backenddoctor
```

文本链启动 real gate：

```text
gfx realarm
gfx realgate
```

文本模式下预期：

```text
Graphics real gate:
  armed:      yes
  framebuffer: ready
  address:    0x000B8000
  fb type:    2
  graphics fb: no
  result:     blocked
  reason:     framebuffer is text mode
```

说明：

```text
当前即使有 framebuffer 地址，也不会误把 text-mode 当像素 framebuffer 写。
```

图形模式下：

```text
run-gfxboot 已经能真实写 framebuffer。
```

---

# 示例：图形 Shell

运行：

```bash
cd ~/osdev/lingjing
make clean
make run-gfxboot
```

图形窗口输入：

```text
dash
```

预期：

```text
REAL DASH
UP
TICKS
MODULES
TASKS
ACTIVE
MODE
SYSCALLS
BRIDGE
```

输入：

```text
health
```

预期：

```text
REAL HEALTH
RESULT OK
```

输入：

```text
doctor
```

预期：

```text
REAL DOCTOR
RESULT OK
```

输入：

```text
history
```

预期：

```text
HISTORY VIEW
```

输入：

```text
log
```

预期：

```text
RESULT LOG
```

---

# 示例：Capability

查看能力表：

```text
capabilities
```

检查 syscall 能力：

```text
capcheck sys.syscall
```

预期：

```text
result: ready
```

检查 GUI 能力：

```text
capcheck gui.display
```

如果 gui 未加载：

```text
module: not loaded
result: blocked
```

加载 GUI 模块：

```text
load gui
capcheck gui.display
```

预期：

```text
module: loaded
result: ready
```

---

# 示例：Intent

查看计划：

```text
intent plan video
```

运行意图：

```text
intent run video
```

查看状态：

```text
intent status
intent context
```

停止当前意图：

```text
intent stop
```

预期：

```text
intent run video 后 gui / net / ai loaded
intent context 记录 taskId / bound modules / bound capabilities
intent stop 后 gui / net / ai unloaded
intent context cleanup 增加
```

---

# 示例：Module 依赖检查

检查模块依赖：

```text
modulecheck
```

模拟破坏模块：

```text
modulebreak screen
```

查看系统诊断：

```text
doctor
```

修复模块：

```text
modulefix screen
```

预期：

```text
modulebreak screen 后 doctor blocked
modulefix screen 后 doctor ready
```

---

# 示例：Syscall

查看 syscall 表：

```text
syscall table
```

模拟调用：

```text
syscall call 0
```

模拟 interrupt path：

```text
syscall int 0
```

真实 int 0x80：

```text
syscall real 0
```

带参数真实 int 0x80：

```text
syscall realargs 0 11 22 33
```

查看寄存器帧：

```text
syscall frame
```

查看返回值：

```text
syscall ret
```

预期：

```text
eax = syscall id
ebx / ecx / edx = syscall args
SYS_PING return value = 0
```

---

# 示例：Ring3 Syscall Roundtrip

先准备 ring3 syscall：

```text
ring3 syscallprepare
ring3 syscalldryrun
ring3 syscallstubprepare
ring3 syscallstubselect
ring3 syscallgateinstall
ring3 syscallarm
ring3 syscallrealarm
```

准备 ring3 进入条件：

```text
ring3 gdtinstall
ring3 tssinstall
ring3 pageprepare
ring3 enableswitch
ring3 dryrun
ring3 hwcheck
ring3 hwinstall
ring3 arm
```

真实进入 ring3：

```text
ring3 realenter
```

预期：

```text
Ring3 real enter:
  result: executing iretd

Syscall int 0x80 handler:
  vector:  0x80
  eax:     0
  ebx:     11
  ecx:     22
  edx:     33

Syscall dispatch:
  id:     0
  name:   SYS_PING
  status: ready
  return: pong
  retv:   0
  result: ok
```

说明：

```text
此时系统会停在 ring3 controlled user loop。
这是当前阶段的正常结果。
```

---

# 当前已验收

当前已验收：

```text
version dev-0.4.6
stage graphical shell layout cleanup

run-text 正常
run-gfxboot 正常

modulecheck total 23 / ok 23 / broken 0
health result ok
doctor result ready

bootinfo multiboot1 text chain ready
bootinfo multiboot2 gfxboot chain ready

framebuffer text-vga-80x25 ready
framebuffer graphics 800x600x32 ready

graphics real framebuffer write ready
bitmap font draw ready
gshell graphical dashboard ready
gshell keyboard input bridge ready
gshell command dispatch ready
gshell kernel status bridge ready
gshell history ready
gshell result log ready
gshell layout cleanup ready

syscall realargs 0 11 22 33 正常
syscall frame 正常
syscall ret 正常

ring3 -> int 0x80 -> kernel syscall handler -> SYS_PING dispatch 成功
ring3 syscall return / controlled user loop 成功
```

---

# 当前阶段限制

当前还不是完整 OS。

暂未实现：

```text
完整文件系统
真实用户态进程模型
真实进程地址空间隔离
真实 task context switch
完整 page allocator
窗口系统
真实鼠标输入
网络协议栈
驱动模型
动态 ELF 加载
权限弹窗
完整身份系统
图形终端滚动区
图形命令自动补全
图形历史上下键
图形窗口管理器
```

当前重点：

```text
每一层先可观察
每一层先可诊断
每一层先可阻断
每一层先可修复
再逐步变成真实功能
```

---

# 版本链

```text
v0.0.2-intent-prototype
v0.0.3-scheduler-prototype
v0.0.4-security-language-prototype
v0.0.5-architecture-prototype
v0.0.6-task-lifecycle-prototype
v0.0.7-heap-prototype
v0.0.8-paging-prototype
v0.0.9-task-switch-prototype

v0.1.0-syscall-user-prototype
v0.1.1-core-hardening-prototype
v0.1.2-syscall-interrupt-prototype
v0.1.3-user-mode-preparation-prototype
v0.1.4-ring3-entry-prototype
v0.1.5-user-syscall-roundtrip-prototype
v0.1.6-user-syscall-return-controlled-loop
v0.1.7-capability-registry-prototype
v0.1.8-intent-capability-check-prototype
v0.1.9-module-provides-capability-task-context-prototype

v0.2.0-framebuffer-graphics-preparation
v0.2.1-basic-graphics-output-metadata-prototype
v0.2.2-real-framebuffer-write-gate-graphics-backend-prototype
v0.2.3-graphical-shell-prototype
v0.2.4-graphical-runtime-panel-hardening
v0.2.5-multiboot-framebuffer-info-preparation
v0.2.6-bootinfo-magic-handoff-fix
v0.2.7-boot-handoff-probe
v0.2.8-multiboot-header-hardening
v0.2.9-bootinfo-relaxed-validation

v0.3.0-text-mode-graphical-shell-prototype
v0.3.1-multiboot2-framebuffer-probe
v0.3.2-early-graphics-mark
v0.3.3-real-pixel-write-backend
v0.3.4-run-text-bootinfo-handoff-repair
v0.3.5-gfxboot-continue-into-kernel-mainflow
v0.3.6-primitive-bitmap-text-renderer
v0.3.7-graphical-dash-prototype
v0.3.8-graphical-shell-command-zone
v0.3.9-keyboard-event-bridge-for-graphical-shell

v0.4.0-graphical-shell-command-dispatch
v0.4.1-graphical-command-result-views
v0.4.2-graphical-kernel-status-bridge
v0.4.3-graphical-command-history
v0.4.4-graphical-history-command-view
v0.4.5-graphical-result-log
v0.4.6-graphical-shell-layout-cleanup
```

---

# 下一阶段计划

## dev-0.4.7 graphical shell input polish

目标：

```text
修图形输入区细节
优化光标位置
优化右侧 history / log 显示
避免面板边界压字
增加输入状态更清晰的显示
```

## dev-0.4.8 graphical terminal output area

目标：

```text
把 command zone 从单行输入变成简易图形终端区域
支持多行输出
为真正 GUI shell 做准备
```

## dev-0.4.9 graphical command parser hardening

目标：

```text
整理图形命令解析
加入大小写兼容或明确策略
加入错误提示
加入 command help 表
```

## dev-0.5.0 graphical shell milestone

目标：

```text
形成第一版可演示图形 Shell 里程碑
图形输入
图形命令
图形状态
图形日志
图形历史
内核真实状态桥
```

---

# 长期方向

```text
Intent-driven OS
Capability-based runtime
User-controlled system policy
Self-diagnosing kernel
Recoverable module ecosystem
Graphical shell based on intent / capability / task state
```

最终目标不是复制传统桌面，而是让系统围绕：

```text
intent
capability
module
task
health
doctor
permission
identity
graphics
user control
```

形成自己的交互方式。

---

# License

```text
当前为实验阶段，许可证暂未确定。
```

```
```
