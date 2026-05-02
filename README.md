# Lingjing OS

```text
Lingjing OS / 灵境 OS

Lingjing OS 是一个实验性裸机操作系统项目。

当前定位不是普通玩具内核，也不是现阶段要替代 Windows / Linux 的完整桌面系统，而是在验证一套新的系统结构：

内核基础能力
+ 自检 / 医生 / 修复系统
+ 模块依赖系统
+ Capability 能力注册系统
+ Intent 意图驱动执行层
+ 用户态 / syscall / ring3 原型
+ Bootinfo / Framebuffer / Graphics / Text-mode GShell 原型

核心方向：

用户不直接操作底层资源，而是表达 intent；
系统根据 intent 自动检查 capability、module、permission、health、platform、identity、memory、paging、syscall、user、ring3、graphics 等状态；
如果系统健康，则执行；
如果系统不健康，则阻断并报告。

核心理念：

My machine, my rules.
当前版本
version: dev-0.3.0
stage: text-mode graphical shell prototype
arch: i386
boot: multiboot + grub
platform: baremetal

当前阶段重点：

bootinfo / framebuffer / graphics / gshell 已接入 runtime 架构；
已验证 GRUB 可提供 VGA text framebuffer；
已识别 0xB8000 / 80x25 / type 2 text-mode；
graphics real write gate 已能阻断 text-mode；
已完成 text-mode graphical shell 原型。

当前目录：

cd ~/osdev/lingjing

构建运行：

make clean
make
make run

当前推荐运行方式：

make run-text

说明：

run-text：文本安全模式，适合当前 VGA shell / text-mode gshell 测试
run-gfx：图形窗口测试模式，后续 framebuffer 图形模式验证使用
当前阶段定位

Lingjing OS 当前是：

意图驱动的实验性裸机内核原型

不是完整桌面操作系统。

当前重点验证：

boot
bootinfo
framebuffer metadata
graphics metadata / backend gate
text-mode graphical shell
shell
module registry
module dependency
capability registry
intent -> capability check
intent -> task context
scheduler metadata
task lifecycle
heap allocator
paging metadata
syscall int 0x80
user mode metadata
ring3 entry
ring3 -> syscall -> kernel roundtrip
health / doctor / blocked-ready 闭环

当前已经完成关键裸机链路：

ring0 -> ring3
ring3 -> int 0x80
int 0x80 -> kernel syscall handler
kernel syscall dispatch -> SYS_PING
kernel 写回返回值
返回 ring3 controlled user loop

这说明：

用户态代码已经能主动打进内核 syscall 层。

当前图形链路完成到：

GRUB
↓
bootinfo
↓
framebuffer metadata
↓
graphics backend gate
↓
gshell text-mode UI
总体架构
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
├─ Graphics Layer
│  ├─ bootinfo
│  ├─ framebuffer metadata
│  ├─ graphics dryrun backend
│  ├─ real write gate
│  ├─ bitmap font metadata
│  └─ text-mode graphical shell
│
└─ Shell
   ├─ system commands
   ├─ intent commands
   ├─ capability commands
   ├─ module commands
   ├─ diagnostic commands
   ├─ graphics commands
   └─ gshell text-mode UI

关键设计原则：

Intent 不直接找 module
Intent 先声明 capability
Capability 再匹配 provider module
Module 受依赖系统管理
Task Runtime 后续绑定执行上下文
Health / Doctor 统一决定系统是否 ready
Graphics / GShell 必须先通过 gate，不能破坏 VGA shell
当前模块

当前 modulecheck：

total:  23
ok:     23
broken: 0

当前核心模块：

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

当前模块依赖关系：

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

外部能力模块会在 intent 运行时按需加载：

gui
net
ai
fs
Capability Layer

当前 capability registry 已接入 intent / graphics / bootinfo / gshell。

当前能力表：

gui.display       provider: gui          permission: display      status: available
gui.framebuffer   provider: framebuffer  permission: display      status: ready
gui.graphics      provider: graphics     permission: display      status: ready
gui.shell         provider: gshell       permission: display      status: ready
boot.multiboot    provider: bootinfo     permission: boot         status: ready
net.http          provider: net          permission: network      status: available
ai.assist         provider: ai           permission: intent       status: available
file.read         provider: fs           permission: file         status: planned
sys.health        provider: health       permission: diagnostic   status: ready
sys.syscall       provider: syscall      permission: syscall     status: ready
user.ring3        provider: ring3        permission: ring3        status: ready

相关命令：

capabilities
capinfo <capability>
capcheck <capability>
capdoctor

示例：

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

当前设计：

capability 存在，不代表 ready；
capability ready 需要 provider module 已加载，并且 module dependency ok。

例如：

gui.display provider 是 gui
gui 未加载时 result blocked
load gui 后 result ready
Bootinfo / Framebuffer / Graphics / GShell

当前显示链路：

GRUB
↓
bootinfo
↓
framebuffer metadata
↓
graphics backend gate
↓
gshell text-mode UI

当前已验证：

bootinfo 能读取 multiboot info addr
bootinfo 能读取 framebuffer 信息
当前 framebuffer 是 VGA text mode
address: 0x000B8000
width:   80
height:  25
bpp:     16
type:    2

当前 graphics 安全门：

type 0 / 1：graphics framebuffer，未来允许 real write
type 2：text-mode framebuffer，禁止 real pixel write

当前 gfx realgate 在 text-mode 下会阻断：

result: blocked
reason: framebuffer is text mode

当前已支持命令：

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

graphics
gfx
gfx backend
gfx backendcheck
gfx backenddoctor
gfx realarm
gfx realdisarm
gfx realgate
gfx font
gfx textmetrics <text>
gfx text <x> <y> <text>
gfx pixel <x> <y> <color>
gfx rect <x> <y> <w> <h> <color>
gfx panel

gshell
gshell check
gshell doctor
gshell panel
gshell runtime
gshell intent
gshell system
gshell dashboard
gshell textpanel
gshell textdash
gshell runtimecheck

当前 text-mode graphical shell：

gshell textpanel
gshell textdash

说明：

当前不是完整 GUI；
当前是基于 VGA text mode 的安全 UI 原型；
真正 framebuffer 图形模式会在后续阶段单独接入。
Intent Layer

Intent 层负责把用户意图转成系统执行计划。

当前支持：

video
ai
network

当前 intent 示例：

video requires capabilities:
  gui.display
  net.http
  ai.assist

相关命令：

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

当前 intent 执行前会检查：

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

执行流程：

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
Module Layer

已支持：

module registry
module dependency declaration
module dependency check
module dependency tree
module break / fix
external module load / unload
core module unload protection
module provides capability
modulecheck integrated with health / doctor

相关命令：

modules
moduleinfo <name>
moduledeps <name>
moduletree
modulecheck
modulebreak <name>
modulefix <name>
load <module>
unload <module>

示例：

modulecheck
modulebreak screen
doctor
modulefix screen
doctor

预期：

modulebreak screen 后 doctor blocked
modulefix screen 后 doctor ready
Health / Doctor

Lingjing OS 的核心机制之一是：

发现异常 -> 标记 broken -> health bad -> doctor blocked -> intent system blocked
修复异常 -> health ok -> doctor ready -> intent system ready
health
health

当前健康项：

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

正常输出：

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
doctor
doctor

当前诊断项：

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

正常输出：

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
Syscall Layer

当前 syscall 层已经完成真实 int 0x80 路径。

已支持：

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

当前 syscall table：

0    SYS_PING      ready
1    SYS_UPTIME    ready
2    SYS_HEALTH    ready
3    SYS_USER      metadata
4    RESERVED      reserved
5    RESERVED      reserved
6    RESERVED      reserved
7    RESERVED      reserved

相关命令：

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

三条 syscall 路径：

syscall call <id>
  shell 模拟调用 syscall dispatcher

syscall int <id>
  模拟 interrupt path，不触发真实 CPU int

syscall real <id>
  真实 int 0x80，从内核态触发

syscall realargs <id> <a> <b> <c>
  真实 int 0x80，并通过 eax / ebx / ecx / edx 传参

当前 syscall 返回值：

SYS_PING        -> 0
SYS_UPTIME      -> 0
SYS_HEALTH      -> 0
SYS_USER        -> 3
unsupported     -> 4294967295
User / Ring3 Layer
User Metadata Layer

当前 user 层提供用户态元数据，不是完整进程加载器。

已支持：

user mode layer
user programs metadata
user entries metadata
user stats
user doctor
user break / fix
health user integration
doctor user mode layer integration
modulecheck user depends syscall

相关命令：

user
user status
user check
user doctor
user programs
user entries
user stats
userbreak
userfix

当前 user programs：

0    init         metadata
1    shell-user   metadata
2    intent-user  metadata
3    reserved     reserved
Ring3 Layer

当前 ring3 层已经完成真实 ring0 -> ring3 跳转，并完成 ring3 syscall roundtrip。

已支持：

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

相关命令：

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

危险命令：

ring3 realenter

说明：

ring3 realenter 会真实进入 ring3。
如果 syscall stub 已选中，会执行 ring3 -> int 0x80 -> kernel syscall handler。
随后返回 ring3 controlled user loop。
这一步通常会停住，这是正常现象。

完整 ring3 syscall 测试链：

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

成功关键输出：

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
Scheduler / Task Layer

当前调度器仍是原型，不是真实硬件级上下文切换。

已支持：

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

基础任务表：

0    idle     kernel
1    shell    system
2    intent   system
3    timer    kernel

合法任务状态：

created
ready
running
blocked
sleeping
killed
broken

相关命令：

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
Task Switch Metadata Layer

当前 task switch 仍是 metadata prototype，不做真实 ESP / EIP 切换。

已支持：

task stack metadata
stack base
stack size
context ready
context switch count
task switch doctor
task switch break / fix
health switch integration
doctor task switch layer integration

相关命令：

taskswitch
taskswitchcheck
taskswitchdoctor
taskswitchbreak
taskswitchfix

当前定位：

不是完整上下文切换
不是真实进程切换
不是 ring3 task switch
当前是后续真实 task switch 的诊断骨架
Memory / Heap Layer

当前 memory 层是 header + free-list prototype。

已支持：

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

相关命令：

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

当前 allocator：

header + free-list prototype

当前定位：

不是完整通用 heap allocator
暂不做 coalescing
暂不做复杂碎片整理
当前目标是内核动态内存生命周期可诊断
Paging Layer

当前 paging 层是 4MB identity paging prototype。

已支持：

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

相关命令：

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

当前映射范围：

0x00000000 - 0x003FFFFF

当前页数：

1024 pages

当前大小：

4MB

当前定位：

已经能开启 paging 并回到 shell
当前仍是 identity mapping
暂不做 page allocator
暂不做 map / unmap
暂不做用户态地址空间隔离
Security Layer

当前安全层是策略骨架，不是完整防病毒系统。

已支持：

security policy switch
module protection
intent permission check
security audit log
security doctor check
intent run security check
module load security check
module unload security check
core module unload protection

相关命令：

security
securitycheck
securitylog
securityclear

当前定位：

不是杀毒系统
不是病毒库
不是沙箱
不是完整证书体系
当前是系统安全策略入口
Platform Layer

当前 platform 层是硬件能力抽象骨架。

当前平台：

baremetal

当前能力状态：

output
input
timer
power

相关命令：

platform
platformcheck
platformdeps
platformboot
platformsummary
platformcaps
platformbreak <capability>
platformfix <capability>

当前定位：

硬件能力抽象
平台状态可检查
平台故障可诊断
平台修复可恢复
上层逐步脱离硬件细节
Identity Layer

当前 identity 层是系统身份占位层，不是真实身份系统。

当前状态：

state: ready
mode: local-placeholder
public key: not-generated

相关命令：

identity
identity status
identity doctor
identity check
identity break
identity fix

当前阶段不实现：

identity generate
identity export
identity import
identity reset
Language Layer

当前语言层是系统显示语言骨架。

默认语言：

en

预留：

zh

说明：

当前仍使用 VGA 文本模式，暂不支持真正 UTF-8 中文显示。
中文通道目前使用 [ZH] ASCII 占位，避免乱码。

相关命令：

lang
lang en
lang zh
系统状态命令
status
status

示例：

LJ | up 50s | in none | m23 t4 | s5067 y0 | coop | deps ok | doc ok

含义：

up      uptime
in      current intent
m       module count
t       task count
s       scheduler ticks
y       yield count
coop    cooperative scheduler prototype
deps    module dependency health
doc     system doctor result
dashboard / dash
dashboard
dash

显示：

uptime
scheduler ticks
scheduler mode
active task
next alloc
platform status
intent layer
security status
language status
dependency health
task health
memory health
paging health
syscall health
user health
ring3 health
identity health
current intent
tasks
registered modules
gshell textpanel / textdash
gshell textpanel
gshell textdash

显示：

text-mode graphical shell
intent status
runtime chain
module dependency status
graphics status
capability status
intent context
构建环境

推荐环境：

Windows + WSL2 Ubuntu + QEMU

依赖：

sudo apt install -y build-essential gcc make nasm qemu-system-x86 grub-pc-bin grub-common xorriso mtools gdb git curl wget
构建与运行

进入项目：

cd ~/osdev/lingjing

清理并编译：

make clean
make

运行文本安全模式：

make run-text

运行图形测试模式：

make run-gfx

默认运行：

make run

当前建议：

优先使用 make run-text

退出：

halt
常用命令
help
version
sysinfo
dashboard
dash
status
doctor
health

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

graphics
gfx
gfx backend
gfx backendcheck
gfx backenddoctor
gfx realarm
gfx realdisarm
gfx realgate
gfx font
gfx textmetrics <text>
gfx text <x> <y> <text>
gfx pixel <x> <y> <color>
gfx rect <x> <y> <w> <h> <color>
gfx panel

gshell
gshell check
gshell doctor
gshell panel
gshell runtime
gshell intent
gshell system
gshell dashboard
gshell textpanel
gshell textdash
gshell runtimecheck

capabilities
capinfo <capability>
capcheck <capability>
capdoctor

modules
moduleinfo <name>
moduledeps <name>
moduletree
modulecheck
modulebreak <name>
modulefix <name>
load <module>
unload <module>

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

security
securitycheck
securitylog
securityclear

lang
lang en
lang zh

platform
platformcheck
platformdeps
platformboot
platformsummary
platformcaps
platformbreak <capability>
platformfix <capability>

identity
identity status
identity doctor
identity check
identity break
identity fix

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

taskswitch
taskswitchcheck
taskswitchdoctor
taskswitchbreak
taskswitchfix

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

paging
paging check
paging doctor
paging map <addr>
paging flags <addr>
paging stats
paging enable
pagingbreak
pagingfix

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

user
user check
user doctor
user programs
user entries
user stats
userbreak
userfix

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

uptime
sleep <seconds>
reboot
halt
示例：Bootinfo / Framebuffer

查看 bootinfo：

bootinfo
bootinfo framebuffer
bootinfo check
bootinfo doctor

当前预期：

framebuffer: yes
addr low:    0x000B8000
width:       80
height:      25
bpp:         16
type:        2
type name:   text-mode

准备 framebuffer：

fb prepare
fb

预期：

Framebuffer prepare from bootinfo:
  width:    80
  height:   25
  bpp:      16
  pitch:    160
  address:  0x000B8000
  type:     2
  result:   prepared
示例：Graphics Gate

查看 graphics backend：

gfx backend
gfx backendcheck
gfx backenddoctor

启动 real gate：

gfx realarm
gfx realgate

当前 text-mode 下预期：

Graphics real gate:
  armed:      yes
  framebuffer: ready
  address:    0x000B8000
  fb type:    2
  graphics fb: no
  result:     blocked
  reason:     framebuffer is text mode

说明：

当前即使有 framebuffer 地址，也不会误把 text-mode 当像素 framebuffer 写。
示例：Graphics Text Metadata

查看字体元数据：

gfx font

预期：

Graphics font:
  name:      builtin-8x16-ascii
  type:      bitmap metadata
  charset:   ascii
  width:     8
  height:    16
  real draw: gated
  result:    ready

测量文本：

gfx textmetrics Lingjing Runtime

预期：

chars:  16
cell:   8x16
width:  128
height: 16
result: measured

绘制文本 dryrun：

gfx text 16 16 Lingjing Runtime

预期：

Graphics text dryrun:
  x:      16
  y:      16
  text:   Lingjing Runtime
  font:   builtin-8x16-ascii
  cell:   8x16
  width:  128
  height: 16
  result: staged
示例：Text-mode GShell

准备 framebuffer：

fb prepare

显示 text panel：

gshell textpanel

显示 text dashboard：

gshell textdash

配合 intent：

intent run video
gshell textdash
intent stop
gshell textdash

运行时预期：

intent:  video
running: yes
taskId:  100
modules: gui, net, ai

停止后预期：

intent:  none
running: no
cleanup: 增加
示例：Capability

查看能力表：

capabilities

检查 syscall 能力：

capcheck sys.syscall

预期：

result: ready

检查 GUI 能力：

capcheck gui.display

如果 gui 未加载：

module: not loaded
result: blocked

加载 GUI 模块：

load gui
capcheck gui.display

预期：

module: loaded
result: ready

查看 capability doctor：

capdoctor
示例：Intent

查看计划：

intent plan video

运行意图：

intent run video

查看状态：

intent status
intent context

查看模块：

modules

停止当前意图：

intent stop

查看历史：

intent history

预期：

intent run video 后 gui / net / ai loaded
intent context 记录 taskId / bound modules / bound capabilities
intent stop 后 gui / net / ai unloaded
intent context cleanup 增加
示例：Module 依赖检查

检查模块依赖：

modulecheck

模拟破坏模块：

modulebreak screen

查看系统诊断：

doctor

修复模块：

modulefix screen

预期：

modulebreak screen 后 doctor blocked
modulefix screen 后 doctor ready
示例：Syscall

查看 syscall 表：

syscall table

模拟调用：

syscall call 0

模拟 interrupt path：

syscall int 0

真实 int 0x80：

syscall real 0

带参数真实 int 0x80：

syscall realargs 0 11 22 33

查看寄存器帧：

syscall frame

查看返回值：

syscall ret

预期：

eax = syscall id
ebx / ecx / edx = syscall args
SYS_PING return value = 0
示例：Ring3 Syscall Roundtrip

先准备 ring3 syscall：

ring3 syscallprepare
ring3 syscalldryrun
ring3 syscallstubprepare
ring3 syscallstubselect
ring3 syscallgateinstall
ring3 syscallarm
ring3 syscallrealarm

准备 ring3 进入条件：

ring3 gdtinstall
ring3 tssinstall
ring3 pageprepare
ring3 enableswitch
ring3 dryrun
ring3 hwcheck
ring3 hwinstall
ring3 arm

真实进入 ring3：

ring3 realenter

预期：

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

说明：

此时系统会停在 ring3 controlled user loop。
这是当前阶段的正常结果。
示例：Task Lifecycle

创建任务：

taskcreate worker

查看任务：

tasks
taskinfo 4

唤醒任务：

taskwake 4

睡眠任务：

tasksleep 4

退出任务：

taskexit 4 7

查看统计：

taskstats
taskdoctor

预期：

taskcreate 后 state created
taskwake 后 ready
tasksleep 后 sleeping
taskexit 后 killed + exit code 7
taskdoctor result ok
示例：Heap

查看 heap：

mem
heapdoctor
heapstats

分配：

kmalloc 16

释放：

kfree <addr>

复用：

kmalloc 8

double free 检测：

kfree <same_addr>
kfree <same_addr>
heapdoctor

预期：

double free: 1
heapdoctor result broken
health memory bad
doctor memory layer broken

修复：

heapfix

预期：

heapdoctor result ok
doctor ready
示例：Paging

查看 paging：

paging
paging stats

查看映射：

paging map 0x00100000
paging map 0x00400000

查看 flags：

paging flags 0x00100000

启用 paging：

paging enable

预期：

paging enable 后 PG bit on
CR3 指向 page directory
0x00000000 - 0x003FFFFF identity mapped
0x00400000 unmapped
doctor ready
示例：Platform

查看平台状态：

platform

模拟破坏 timer 能力：

platformbreak timer

修复 timer 能力：

platformfix timer

模拟破坏全部 platform 能力：

platformbreak all

修复全部 platform 能力：

platformfix all

预期：

platformbreak timer/all 后 doctor blocked
platformfix timer/all 后 doctor ready
示例：Identity

查看 identity 状态：

identity

模拟破坏 identity 层：

identity break

修复 identity 层：

identity fix

预期：

identity break 后 doctor blocked
identity fix 后 doctor ready
当前已验收

当前已验收：

version dev-0.3.0
stage text-mode graphical shell prototype

modulecheck total 23 / ok 23 / broken 0
health result ok
doctor result ready

capabilities total 11
boot.multiboot ready
gui.framebuffer ready
gui.graphics ready
gui.shell ready
sys.syscall ready
user.ring3 ready

intent plan video 正常
intent run video 正常
intent context 正常
intent stop 后 cleanup 正常

syscall realargs 0 11 22 33 正常
syscall frame 正常
syscall ret 正常
ring3 syscallresult 正常

ring3 -> int 0x80 -> kernel syscall handler -> SYS_PING dispatch 成功
ring3 syscall return / controlled user loop 成功

bootinfo framebuffer 读取成功
当前 framebuffer 为 VGA text-mode:
  address 0x000B8000
  width   80
  height  25
  bpp     16
  type    2

graphics real write gate 能阻断 text-mode
gfx font / textmetrics / text dryrun 正常
gshell textpanel 正常
gshell textdash 正常
当前阶段限制

当前还不是完整 OS。

暂未实现：

完整文件系统
真实用户态进程模型
真实进程地址空间隔离
真实 task context switch
完整 page allocator
真实 framebuffer GUI
窗口系统
真实像素字体渲染
真实鼠标输入
网络协议栈
驱动模型
动态 ELF 加载
权限弹窗
完整身份系统

当前重点是：

每一层先可观察
每一层先可诊断
每一层先可阻断
每一层先可修复
再逐步变成真实功能
版本链
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
下一阶段计划
dev-0.3.1 text-mode shell layout hardening

目标：

强化 text-mode graphical shell
整理 gshell textpanel / textdash 布局
减少输出错位
增加 runtime 状态块
增加 intent / capability / module / health / doctor 区块
dev-0.3.2 graphics text backend gate

目标：

为未来真实 framebuffer 模式下的文字绘制做 gate
继续保持 text-mode 安全输出
不直接破坏 VGA shell
dev-0.3.3 framebuffer graphics mode re-entry plan

目标：

重新设计 graphics boot mode
只有在 graphics shell 可自输出时才允许切换图形 framebuffer
避免再次出现 VGA shell 在图形模式下乱码
后续路线
dev-0.3.1 text-mode shell layout hardening
dev-0.3.2 graphics text backend gate
dev-0.3.3 framebuffer graphics mode re-entry plan
dev-0.3.4 real pixel write prototype
dev-0.3.5 bitmap font real draw prototype
dev-0.3.6 graphical shell real framebuffer prototype
长期方向
Intent-driven OS
Capability-based runtime
User-controlled system policy
Self-diagnosing kernel
Recoverable module ecosystem
Graphical shell based on intent / capability / task state

最终目标不是复制传统桌面，而是让系统围绕：

intent
capability
module
task
health
doctor
permission
identity
graphics

形成自己的交互方式。

License

当前为实验阶段，许可证暂未确定。