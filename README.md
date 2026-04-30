
````markdown
# Lingjing OS

Lingjing OS 是一个实验性操作系统项目。

当前目标不是替代现有桌面系统，而是探索一种更轻的系统结构：

- 极小核心启动
- 模块按需加载
- 意图驱动调度
- 用户拥有最终控制权
- 外部生态必须受系统规则约束
- 调度器以可验证、可诊断、可恢复为核心方向
- 内存层以可分配、可释放、可复用、可诊断为核心方向
- 分页层以 identity mapping、CR0/CR3 可观测、可诊断为核心方向
- syscall 层以 int 0x80、寄存器帧、返回值为核心原型
- user 层当前为 metadata-only，为后续 ring3 做准备
- 安全层以策略检查、模块保护、审计日志为基础
- 语言层预留系统显示语言接口
- platform 层预留硬件能力抽象接口
- identity 层预留系统身份占位接口

当前版本：

```text
version: dev-0.1.3
stage: user mode preparation prototype
arch: i386
boot: multiboot + grub
platform: baremetal
````

---

## 项目定位

Lingjing OS 当前不是完整桌面操作系统。

当前阶段定位：

```text
意图驱动的实验性内核原型
```

它现在重点验证：

```text
boot
shell
module
intent
scheduler
task lifecycle
heap
paging
task switch metadata
syscall interrupt
user metadata
health / doctor / blocked-ready 闭环
```

当前系统已经从早期“架构原型”推进到“实验性内核骨架”阶段。

---

## 当前能力总览

### Kernel 基础

* GRUB / Multiboot 启动
* i386 32 位内核
* VGA 文本输出
* GDT 初始化
* IDT 初始化
* PIC 重映射
* IRQ0 系统时钟
* IRQ1 键盘中断
* Shell 命令行
* Reboot / Halt

### Module Layer

* 模块注册
* 模块依赖声明
* 模块依赖检查
* 模块依赖树
* 模块 break / fix
* 外部模块模拟 load / unload
* 核心模块卸载保护
* modulecheck 集成 health / doctor 主链

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
platform
lang
identity
health
memory
paging
shell
```

运行 `intent run video` 后，外部模块会按需加载：

```text
gui
net
ai
```

---

## Intent Layer

Intent 层负责把用户意图转成系统执行计划。

当前支持的 intent：

```text
video    requires: gui, net, ai
ai       requires: ai
network  requires: net
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

Intent 执行前会检查：

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
health / doctor 主链检查
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

## Scheduler / Task Layer

当前调度器仍是原型，不是真正硬件级上下文切换。

已支持：

* IRQ0 tick 接入 scheduler
* scheduler ticks 统计
* yield count 统计
* task runtime ticks 统计
* 静态基础任务表
* dynamic task create
* active task 轮转
* cooperative yield
* run queue 查看
* scheduler log
* scheduler reset
* scheduler validate
* scheduler fix
* task doctor
* task lifecycle hardening
* task create / wake / sleep / kill / exit
* task priority
* task broken / fix
* exit code
* lifecycle event count
* context switch metadata
* runqueue reason 显示
* system doctor 集成

当前基础任务表：

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

当前调度规则：

```text
yield 会切换 active task
created task 默认不进入 runnable
sleeping task 会被跳过
blocked task 会被跳过
killed task 会被跳过
broken task 会被跳过并影响 health
idle task 不可 kill / sleep / break
如果所有非 idle 任务不可运行，则回到 idle
当前 active 被 blocked / sleeping / killed / broken，会自动切到下一个 runnable task
```

---

## Task Switch Metadata Layer

当前 task switch 仍是 metadata prototype，不做真实 ESP/EIP 切换。

已支持：

* task stack metadata
* stack base
* stack size
* context ready
* context switch count
* task switch doctor
* task switch break / fix
* health switch 集成
* doctor task switch layer 集成

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

它是后续真实 task switch 的诊断骨架
```

---

## Memory / Heap Layer

当前 memory 层已经从 bump allocator 推进到 header + free-list prototype。

已支持：

* placement address
* kmalloc
* kcalloc
* kfree
* block header
* real size 记录
* free list
* block reuse
* allocated bytes
* freed bytes
* live bytes
* live alloc count
* failed free count
* double free 检测
* heapstats
* heapcheck
* heapdoctor
* heapbreak / heapfix
* health memory 集成
* doctor memory layer 集成

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

当前支持的诊断项：

```text
placement
allocator
alloc count
free count
live count
reuse count
free list count
failed free
double free
allocated bytes
freed bytes
live bytes
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

当前 paging 层已经完成 4MB identity paging 原型。

已支持：

* page directory
* first page table
* identity mapping
* CR3 load
* CR0.PG enable
* paging enable
* paging map check
* paging flags
* paging stats
* paging doctor
* CR0 显示
* CR3 显示
* PG bit 显示
* mapped range 显示
* page flags 显示
* accessed bit 识别
* unmapped 地址识别
* health paging 集成
* doctor paging layer 集成

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

## Syscall Layer

当前 syscall 层已经接入真实 `int 0x80` 中断路径。

已支持：

* syscall table
* syscall metadata
* syscall dispatch
* unsupported syscall count
* syscall interrupt metadata path
* IDT vector 0x80
* isr128
* syscall int 0x80 handler
* 从 EAX 读取 syscall id
* EBX / ECX / EDX 参数捕获
* register frame 捕获
* syscall return value
* syscall frame
* syscall ret
* syscall stats
* syscall doctor
* health syscall 集成
* doctor syscall layer 集成

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
  真实 int 0x80，并通过 eax/ebx/ecx/edx 传参
```

当前 syscall 返回值：

```text
SYS_PING        -> 0
SYS_UPTIME      -> 0
SYS_HEALTH      -> 0
SYS_USER        -> 3
unsupported     -> 4294967295
```

当前定位：

```text
已经完成真实 int 0x80 内核态触发
已经完成寄存器帧捕获
已经完成返回值原型
当前还没有 ring3 用户态触发 syscall
```

---

## User Mode Metadata Layer

当前 user 层是 metadata-only，不是真实 ring3。

已支持：

* user mode layer
* user programs metadata
* user entries metadata
* user stats
* user doctor
* user break / fix
* health user 集成
* doctor user mode layer 集成
* modulecheck user depends syscall

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

当前定位：

```text
不是真实用户态
不是 ring3
不是进程加载器
当前只做 user mode metadata，为后续 ring3 entry 做准备
```

---

## Security Layer

当前安全层是规则骨架，不是完整防病毒系统。

已支持：

* security policy 开关骨架
* module protection
* intent permission check
* security audit log
* security doctor check
* intent run 前安全检查
* module load 前安全检查
* module unload 前安全检查
* 核心模块卸载保护

相关命令：

```text
security
securitycheck
securitylog
securityclear
```

当前安全层定位：

```text
不是杀毒系统
不是病毒库
不是沙箱
不是完整证书体系

它是系统安全策略入口
```

---

## Language Layer

当前语言层是系统显示语言骨架。

默认语言：

```text
en
```

已预留：

```text
zh
```

由于当前仍使用 VGA 文本模式，暂不支持真正 UTF-8 中文显示。
中文通道目前使用 `[ZH]` ASCII 占位，避免乱码。

相关命令：

```text
lang
lang en
lang zh
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

已支持：

* platform 初始化状态检查
* platform capability check
* platform break / fix
* system doctor 集成
* health 集成
* platform broken 时阻塞 intent system

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

---

## Identity Layer

当前 identity 层是系统身份占位层，不是真实身份系统。

当前状态：

```text
state: ready
mode: local-placeholder
public key: not-generated
```

已支持：

* identity 初始化状态检查
* identity doctor
* identity break / fix
* system doctor 集成
* health 集成
* identity broken 时阻塞 intent system

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

这些属于后续操作层 / 身份管理层。

---

## 系统诊断

### 单行状态栏

```text
status
```

示例：

```text
LJ | up 50s | in none | m17 t4 | s5067 y0 | coop | deps ok | doc ok
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

### 多行面板

```text
dashboard
dash
```

显示：

* uptime
* scheduler ticks
* scheduler mode
* active task
* next alloc
* platform status
* intent layer
* security status
* language status
* dependency health
* task health
* memory health
* paging health
* syscall health
* user health
* identity health
* current intent
* tasks
* registered modules

### 全局健康

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
result
```

示例：

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
  result:   ok
```

### 全局诊断

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

示例：

```text
System doctor:
  module dependencies: ok
  task health:         ok
  scheduler:           ok
  scheduler active:    ok
  task switch layer:   ok
  syscall layer:       ok
  user mode layer:     ok
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

## 核心理念

```text
My machine, my rules.
```

系统默认只加载核心能力。

生态模块不会默认常驻，而是由用户意图触发加载：

```text
intent plan video
intent run video
intent status
intent stop
```

对应流程：

```text
用户意图
↓
查看执行计划
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
memory / paging / syscall / user 健康检查
↓
按需加载模块
↓
运行 intent
↓
停止 intent
↓
释放外部模块
```

调度器方向：

```text
任务可见
状态可控
异常可诊断
调度可恢复
```

内存层方向：

```text
分配可查
释放可控
复用可见
错误可诊断
```

分页层方向：

```text
地址映射可见
CR0 / CR3 可查
页表 flags 可诊断
分页开启可验证
```

syscall 层方向：

```text
调用入口明确
寄存器帧可见
返回值可查
用户态边界逐步建立
```

安全层方向：

```text
策略先行
模块受控
权限可查
行为留痕
核心不可被普通模块破坏
```

platform 层方向：

```text
硬件能力抽象
平台状态可检查
平台故障可诊断
平台修复可恢复
上层逐步脱离硬件细节
```

identity 层方向：

```text
系统身份能力预留
当前只做占位
不在架构原型阶段实现真实身份策略
```

---

## 构建环境

当前推荐环境：

```text
Windows + WSL2 Ubuntu + QEMU
```

需要安装：

```bash
sudo apt install -y build-essential gcc make nasm qemu-system-x86 grub-pc-bin grub-common xorriso mtools gdb git curl wget
```

---

## 构建与运行

进入项目目录：

```bash
cd ~/osdev/lingjing
```

清理并运行：

```bash
make clean
make
make run
```

QEMU 退出方式：

```text
halt
```

或者在 QEMU curses 模式下使用对应终端退出快捷键。

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

modules
moduleinfo <name>
moduledeps <name>
moduletree
modulecheck
modulebreak <name>
modulefix <name>

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

uptime
sleep <seconds>
reboot
halt
```

---

## 示例：Intent 调度

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
paging enable 后 pg bit on
CR3 指向 page directory
0x00000000 - 0x003FFFFF identity mapped
0x00400000 unmapped
doctor ready
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
syscall realargs 3 100 200 300
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
SYS_USER return value = 3
unsupported return value = 4294967295
```

---

## 示例：User Metadata

查看 user 层：

```text
user
```

查看 user programs：

```text
user programs
```

查看 user entries：

```text
user entries
```

查看 user doctor：

```text
user doctor
```

预期：

```text
user mode layer ok
ring3 disabled
metadata-only
```

---

## 示例：Platform 平台层

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

## 示例：Identity 占位层

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

## 当前阶段

Lingjing OS 目前是一个早期裸机实验性内核原型。

它还不是完整操作系统。

当前版本 `dev-0.1.3 user mode preparation prototype` 已完成 user segment / stack / entry frame / boundary hardening 原型验收。

```text
int 0x80 gate
isr128
syscall interrupt handler
syscall register frame
eax / ebx / ecx / edx 参数捕获
syscall return value
syscall frame / ret / stats / doctor
```

当前重点已经从“架构能跑”推进到：

```text
内核关键机制逐步接入
每一层都可诊断
每一层都可 break / fix
全局 health / doctor 统一 blocked / ready
```

当前已验收：

```text
dash / health / doctor / status / modulecheck 初始健康
intent plan video
intent run video
gui / net / ai 按需加载
intent stop 后 gui / net / ai 卸载
intent history 记录 run / stop

modulebreak screen 后 doctor blocked
modulefix screen 后 doctor ready

platformbreak timer/all 后 doctor blocked
platformfix timer/all 后 doctor ready

identity break 后 doctor blocked
identity fix 后 doctor ready

heap block header / kfree / reuse / double free 检测
heapfix 后 doctor ready

task created / sleeping / killed / broken / exit code
taskfix 后 doctor ready

paging enable 后 shell 正常
paging stats / flags / doctor 正常

syscall real int 0x80 正常
syscall frame / ret 正常

user metadata layer 正常

securitycheck ok
taskdoctor ok
taskswitchdoctor ok
schedvalidate ok
```

当前版本标签：

```text
v0.1.2-syscall-interrupt-prototype
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
```

---

## 下一阶段计划

### dev-0.1.4 ring3 entry prototype

- TSS metadata
- kernel stack for privilege transition
- GDT user segment verification
- ring3 entry frame preparation
- iret transition prototype
- ring3 entry doctor

### 后续方向

```text
dev-0.1.4 ring3 entry prototype
dev-0.1.5 user syscall roundtrip prototype
dev-0.1.6 basic process/task isolation prototype
dev-0.1.7 framebuffer / graphics mode preparation
dev-0.1.8 basic graphics output prototype
dev-0.1.9 window/compositor metadata prototype
dev-0.2.0 GUI shell prototype
```

长期方向：

* 继续整理 core / platform 分层
* 将 intent / module / scheduler / security 从硬件细节中逐步解耦
* 将 screen_print / timer ticks 逐步替换为 platform 接口
* 进入 framebuffer 图形模式
* 实现第一版文本 / 图形系统面板

---

## License

当前为实验阶段，许可证暂未确定。

```
```
