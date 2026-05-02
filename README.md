# Lingjing OS

Lingjing OS / 灵境 OS


Lingjing OS 是一个实验性裸机操作系统项目。

当前定位不是普通玩具内核，也不是现阶段要替代 Windows / Linux 的完整桌面系统，而是在验证一套新的系统结构：

```text
内核基础能力
+ 自检 / 医生 / 修复系统
+ 模块依赖系统
+ Capability 能力注册系统
+ Intent 意图驱动执行层
+ 用户态 / syscall / ring3 原型
```

核心方向：

```text
用户不直接操作底层资源，而是表达 intent；
系统根据 intent 自动检查 capability、module、permission、health、platform、identity、memory、paging、syscall、user、ring3 等状态；
如果系统健康，则执行；
如果系统不健康，则阻断并报告。
```

核心理念：

```text
My machine, my rules.
```

---

## 当前版本

```text
version: dev-0.1.7
stage: capability registry prototype
arch: i386
boot: multiboot + grub
platform: baremetal
```

当前目录：

```bash
cd ~/osdev/lingjing
```

构建运行：

```bash
make clean
make
make run
```

---

## 当前阶段定位

Lingjing OS 当前是：

```text
意图驱动的实验性裸机内核原型
```

不是完整桌面操作系统。

当前重点验证：

```text
boot
shell
module registry
module dependency
capability registry
intent plan / run / stop
scheduler metadata
task lifecycle
heap allocator
paging metadata
syscall int 0x80
user mode metadata
ring3 entry
ring3 -> syscall -> kernel roundtrip
health / doctor / blocked-ready 闭环
```

当前已经完成关键裸机链路：

```text
ring0 -> ring3
ring3 -> int 0x80
int 0x80 -> kernel syscall handler
kernel syscall dispatch -> SYS_PING
kernel 写回返回值
返回 ring3 controlled user loop
```

这说明：

```text
用户态代码已经能主动打进内核 syscall 层。
```

---

## 总体架构

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
└─ Shell
   ├─ system commands
   ├─ intent commands
   ├─ capability commands
   ├─ module commands
   ├─ diagnostic commands
   └─ later GUI shell
```

关键设计原则：

```text
Intent 不直接找 module
Intent 先声明 capability
Capability 再匹配 provider module
Module 受依赖系统管理
Task Runtime 后续绑定执行上下文
Health / Doctor 统一决定系统是否 ready
```

---

## 当前模块

当前 `modulecheck`：

```text
total:  19
ok:     19
broken: 0
```

当前核心模块：

```text
core
screen
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
platform
lang
identity
health
memory
paging
shell
```

当前模块依赖关系：

```text
core          ok
screen        depends core
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
platform      depends core
lang          depends core
identity      depends security
health        depends core
memory        depends core
paging        depends memory
shell         depends keyboard
```

外部能力模块会在需要时加载：

```text
gui
net
ai
```

---

## Capability Layer

当前版本 `dev-0.1.7` 新增 capability registry。

当前能力表：

```text
gui.display    provider: gui      permission: display     status: available
net.http       provider: net      permission: network     status: available
ai.assist      provider: ai       permission: intent      status: available
file.read      provider: fs       permission: file        status: planned
sys.health     provider: health   permission: diagnostic  status: ready
sys.syscall    provider: syscall  permission: syscall     status: ready
user.ring3     provider: ring3    permission: ring3       status: ready
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
load gui
capcheck gui.display
capdoctor
```

当前设计：

```text
capability 存在，不代表 ready；
capability ready 需要 provider module 已加载，并且 module dependency ok。
```

例如：

```text
gui.display provider 是 gui
gui 未加载时 result blocked
load gui 后 result ready
```

---

## Intent Layer

Intent 层负责把用户意图转成系统执行计划。

当前支持：

```text
video
ai
network
```

当前 intent 示例：

```text
video requires gui / net / ai
```

下一阶段将升级为：

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
intent history
intent clear-history
intent reset
intent allow <name>
intent deny <name>
intent lock
intent unlock
```

当前 intent 执行前会检查：

```text
intent lock
intent allow / deny
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
按需加载模块
↓
运行 intent
↓
停止 intent
↓
卸载外部模块
```

---

## Module Layer

已支持：

```text
module registry
module dependency declaration
module dependency check
module dependency tree
module break / fix
external module load / unload
core module unload protection
modulecheck integrated with health / doctor
```

相关命令：

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

示例：

```text
modulecheck
modulebreak screen
doctor
modulefix screen
doctor
```

预期：

```text
modulebreak screen 后 doctor blocked
modulefix screen 后 doctor ready
```

---

## Health / Doctor

Lingjing OS 的核心机制之一是：

```text
发现异常 -> 标记 broken -> health bad -> doctor blocked -> intent system blocked
修复异常 -> health ok -> doctor ready -> intent system ready
```

### health

```text
health
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

### doctor

```text
doctor
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

## Syscall Layer

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

当前 syscall 返回值：

```text
SYS_PING        -> 0
SYS_UPTIME      -> 0
SYS_HEALTH      -> 0
SYS_USER        -> 3
unsupported     -> 4294967295
```

---

## User / Ring3 Layer

### User Metadata Layer

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

相关命令：

```text
user
user status
user check
user doctor
user programs
user entries
user stats
userbreak
userfix
```

当前 user programs：

```text
0    init         metadata
1    shell-user   metadata
2    intent-user  metadata
3    reserved     reserved
```

### Ring3 Layer

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

相关命令：

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

危险命令：

```text
ring3 realenter
```

说明：

```text
ring3 realenter 会真实进入 ring3。
如果 syscall stub 已选中，会执行 ring3 -> int 0x80 -> kernel syscall handler。
随后返回 ring3 controlled user loop。
这一步通常会停住，这是正常现象。
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

成功关键输出：

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

---

## Scheduler / Task Layer

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

## Task Switch Metadata Layer

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

## Memory / Heap Layer

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

当前 allocator：

```text
header + free-list prototype
```

当前定位：

```text
不是完整通用 heap allocator
暂不做 coalescing
暂不做复杂碎片整理
当前目标是内核动态内存生命周期可诊断
```

---

## Paging Layer

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

当前定位：

```text
已经能开启 paging 并回到 shell
当前仍是 identity mapping
暂不做 page allocator
暂不做 map / unmap
暂不做用户态地址空间隔离
```

---

## Security Layer

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

## Platform Layer

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

## Identity Layer

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

## Language Layer

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
当前仍使用 VGA 文本模式，暂不支持真正 UTF-8 中文显示。
中文通道目前使用 [ZH] ASCII 占位，避免乱码。
```

相关命令：

```text
lang
lang en
lang zh
```

---

## 系统状态命令

### status

```text
status
```

示例：

```text
LJ | up 50s | in none | m19 t4 | s5067 y0 | coop | deps ok | doc ok
```

含义：

```text
up      uptime
in      current intent
m       module count
t       task count
s       scheduler ticks
y       yield count
coop    cooperative scheduler prototype
deps    module dependency health
doc     system doctor result
```

### dashboard / dash

```text
dashboard
dash
```

显示：

```text
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
```

---

## 构建环境

推荐环境：

```text
Windows + WSL2 Ubuntu + QEMU
```

依赖：

```bash
sudo apt install -y build-essential gcc make nasm qemu-system-x86 grub-pc-bin grub-common xorriso mtools gdb git curl wget
```

---

## 构建与运行

进入项目：

```bash
cd ~/osdev/lingjing
```

清理并编译：

```bash
make clean
make
```

运行：

```bash
make run
```

退出：

```text
halt
```

---

## 常用命令

```text
help
version
sysinfo
dashboard
dash
status
doctor
health

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
```

---

## 示例：Capability

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

如果 `gui` 未加载：

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

查看 capability doctor：

```text
capdoctor
```

---

## 示例：Intent

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
```

查看模块：

```text
modules
```

停止当前意图：

```text
intent stop
```

查看历史：

```text
intent history
```

预期：

```text
intent run video 后 gui / net / ai loaded
intent stop 后 gui / net / ai unloaded
```

---

## 示例：Module 依赖检查

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

## 示例：Syscall

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

## 示例：Ring3 Syscall Roundtrip

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
这是 dev-0.1.6 / dev-0.1.7 当前阶段的正常结果。
```

---

## 示例：Task Lifecycle

创建任务：

```text
taskcreate worker
```

查看任务：

```text
tasks
taskinfo 4
```

唤醒任务：

```text
taskwake 4
```

睡眠任务：

```text
tasksleep 4
```

退出任务：

```text
taskexit 4 7
```

查看统计：

```text
taskstats
taskdoctor
```

预期：

```text
taskcreate 后 state created
taskwake 后 ready
tasksleep 后 sleeping
taskexit 后 killed + exit code 7
taskdoctor result ok
```

---

## 示例：Heap

查看 heap：

```text
mem
heapdoctor
heapstats
```

分配：

```text
kmalloc 16
```

释放：

```text
kfree <addr>
```

复用：

```text
kmalloc 8
```

double free 检测：

```text
kfree <same_addr>
kfree <same_addr>
heapdoctor
```

预期：

```text
double free: 1
heapdoctor result broken
health memory bad
doctor memory layer broken
```

修复：

```text
heapfix
```

预期：

```text
heapdoctor result ok
doctor ready
```

---

## 示例：Paging

查看 paging：

```text
paging
paging stats
```

查看映射：

```text
paging map 0x00100000
paging map 0x00400000
```

查看 flags：

```text
paging flags 0x00100000
```

启用 paging：

```text
paging enable
```

预期：

```text
paging enable 后 PG bit on
CR3 指向 page directory
0x00000000 - 0x003FFFFF identity mapped
0x00400000 unmapped
doctor ready
```

---

## 示例：Platform

查看平台状态：

```text
platform
```

模拟破坏 timer 能力：

```text
platformbreak timer
```

修复 timer 能力：

```text
platformfix timer
```

模拟破坏全部 platform 能力：

```text
platformbreak all
```

修复全部 platform 能力：

```text
platformfix all
```

预期：

```text
platformbreak timer/all 后 doctor blocked
platformfix timer/all 后 doctor ready
```

---

## 示例：Identity

查看 identity 状态：

```text
identity
```

模拟破坏 identity 层：

```text
identity break
```

修复 identity 层：

```text
identity fix
```

预期：

```text
identity break 后 doctor blocked
identity fix 后 doctor ready
```

---

## 当前已验收

当前已验收：

```text
version dev-0.1.7
modulecheck total 19 / ok 19 / broken 0
health result ok
doctor result ready

capabilities 正常
capcheck sys.syscall ready
capcheck gui.display 在 gui 未加载时 blocked
load gui 后 capcheck gui.display ready
capdoctor 正常

syscall realargs 0 11 22 33 正常
syscall frame 正常
syscall ret 正常
ring3 syscallresult 正常

ring3 -> int 0x80 -> kernel syscall handler -> SYS_PING dispatch 成功
ring3 syscall return / controlled user loop 成功
```

---

## 当前阶段限制

当前还不是完整 OS。

暂未实现：

```text
完整文件系统
真实用户态进程模型
真实进程地址空间隔离
真实 task context switch
完整 page allocator
完整 GUI
窗口系统
网络协议栈
驱动模型
动态 ELF 加载
权限弹窗
完整身份系统
```

当前重点是：

```text
每一层先可观察
每一层先可诊断
每一层先可阻断
每一层先可修复
再逐步变成真实功能
```

---

## 版本链

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
```

---

## 下一阶段计划

### dev-0.1.8 intent capability check prototype

目标：

```text
intent plan video
不再只展示 required modules
而是展示 required capabilities:

gui.display
net.http
ai.assist
```

执行前检查：

```text
capability exists
provider module exists
provider module loaded
provider dependency ok
```

预期效果：

```text
intent run video
先检查 capability
capability 不 ready -> blocked
capability ready -> continue
```

### 后续路线

```text
dev-0.1.8 intent capability check prototype
dev-0.1.9 module provides capability / task context prototype
dev-0.2.0 framebuffer / graphics preparation
dev-0.2.1 basic graphics output
dev-0.2.2 graphical shell prototype
dev-0.2.3 intent panel / status panel prototype
```

---

## 长期方向

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
```

形成自己的交互方式。

---

## License

当前为实验阶段，许可证暂未确定。

