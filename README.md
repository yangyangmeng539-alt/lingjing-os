# Lingjing OS

Lingjing OS 是一个实验性操作系统项目。

当前目标不是替代现有桌面系统，而是探索一种更轻的系统结构：

- 极小核心启动
- 模块按需加载
- 意图驱动调度
- 用户拥有最终控制权
- 外部生态必须受系统规则约束
- 调度器以可验证、可诊断、可恢复为核心方向
- 安全层以策略检查、模块保护、审计日志为基础
- 语言层预留系统显示语言接口

当前版本：

```text
version: dev-0.0.5
stage: architecture prototype
arch: i386
boot: multiboot + grub
```

---

## 当前能力

### Kernel 基础

- GRUB / Multiboot 启动
- i386 32 位内核
- VGA 文本输出
- GDT 初始化
- IDT 初始化
- PIC 重映射
- IRQ0 系统时钟
- IRQ1 键盘中断
- Shell 命令行
- Reboot / Halt

### Memory

- `mem` 查看内核地址信息
- `kmalloc` 简单 bump allocator
- `kcalloc` 分配并清零
- `peek` 读取内存
- `poke` 写入内存
- `hexdump` 查看内存区域
- `kzero` 清零内存区域

### Module Layer

- `modules` 查看已加载模块
- `moduleinfo <name>` 查看模块信息
- `moduledeps <name>` 查看模块依赖
- `moduletree` 查看模块依赖树
- `modulecheck` 检查模块依赖健康
- `modulebreak <name>` 模拟破坏模块依赖
- `modulefix <name>` 修复模块依赖
- `load <module>` 模拟加载外部模块
- `unload <module>` 模拟卸载外部模块
- 核心模块不可卸载
- external 模块可卸载

当前模块结构示例：

```text
core
  screen
  gdt
    idt
      keyboard
        shell
      timer
        scheduler
  security
  lang
  memory
```

运行 `intent run video` 后，外部模块会按需加载：

```text
core
  screen
    gui
  gdt
    idt
      keyboard
        shell
      timer
        scheduler
        net
  security
  lang
  memory
  ai
```

### Intent Layer

- `intent list`
- `intent permissions`
- `intent policy`
- `intent audit`
- `intent doctor`
- `intent plan <name>`
- `intent run <name>`
- `intent stop`
- `intent stop <name>`
- `intent restart`
- `intent switch <name>`
- `intent status`
- `intent history`
- `intent clear-history`
- `intent reset`
- `intent allow <name>`
- `intent deny <name>`
- `intent lock`
- `intent unlock`

当前支持的 intent：

```text
video    requires: gui, net, ai
ai       requires: ai
network  requires: net
```

Intent 执行前会检查：

```text
intent lock
intent allow / deny
module dependency health
security policy
```

### Scheduler Prototype

当前调度器仍是原型，不是真正上下文切换。

已支持：

- IRQ0 tick 接入 scheduler
- scheduler ticks 统计
- yield count 统计
- task runtime ticks 统计
- 静态任务表
- active task 轮转
- cooperative yield
- run queue 查看
- scheduler log
- scheduler reset
- task state 管理
- blocked task 跳过
- idle 保底规则
- scheduler validate
- scheduler fix
- task doctor
- system doctor 集成

相关命令：

```text
tasks
taskinfo <id>
taskstate <id> <state>
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

当前任务表：

```text
0    idle     running/ready    kernel
1    shell    running/ready    system
2    intent   running/ready    system
3    timer    running/ready    kernel
```

合法任务状态：

```text
ready
running
blocked
```

调度器当前规则：

```text
yield 会切换 active task
blocked task 会被跳过
如果所有非 idle 任务 blocked，则回到 idle
当前 active 被 blocked，会自动切到下一个 runnable task
schedfix 可以手动修复 active task
```

### Security Layer

当前安全层是规则骨架，不是完整防病毒系统。

已支持：

- security policy 开关骨架
- module protection
- intent permission check
- security audit log
- security doctor check
- intent run 前安全检查
- module load 前安全检查
- module unload 前安全检查
- 核心模块卸载保护

相关命令：

```text
security
securitylog
securityclear
```

当前安全状态示例：

```text
Security:
  policy:            enabled
  module protection: enabled
  intent permission: enabled
  audit:             enabled
  trust level:       prototype
```

安全日志示例：

```text
Security log:
  security initialized
  intent check passed
  module load check passed
  blocked core module unload
```

当前安全层定位：

```text
不是杀毒系统
不是病毒库
不是沙箱
不是完整证书体系

它是系统安全策略入口
```

### Language Layer

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

示例：

```text
Current language: en
[ZH] Current language: zh
```

---

## 系统诊断

### 单行状态栏

```text
status
```

示例：

```text
LJ | up 6s | in none | m11 t4 | s690 y2 | coop | deps ok | doc ok
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
```

显示：

- uptime
- scheduler ticks
- scheduler mode
- active task
- next alloc
- intent layer
- security status
- language status
- dependency health
- task health
- current intent
- tasks
- registered modules

### 全局诊断

```text
doctor
```

示例：

```text
System doctor:
  module dependencies: ok
  task health:         ok
  scheduler:           ok
  scheduler active:    ok
  security:            ok
  language layer:      ok
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

安全层方向：

```text
策略先行
模块受控
权限可查
行为留痕
核心不可被普通模块破坏
```

语言层方向：

```text
系统显示语言可切换
默认英文
中文预留
后续接入图形界面后再做完整中文显示
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
make run
```

---

## 常用命令

```text
help
version
sysinfo
dashboard
status
doctor

security
securitylog
securityclear

lang
lang en
lang zh

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

mem
kmalloc <bytes>
kcalloc <bytes>
peek <addr>
poke <addr> <value>
hexdump <addr> <len>
kzero <addr> <len>

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

查看安全日志：

```text
securitylog
```

停止当前意图：

```text
intent stop
```

---

## 示例：Scheduler 调度原型

查看运行队列：

```text
runqueue
```

主动让出：

```text
yield
```

查看切换日志：

```text
schedlog
```

阻塞任务：

```text
taskstate 2 blocked
```

恢复任务：

```text
taskstate 2 ready
```

验证调度器：

```text
schedvalidate
```

修复调度器：

```text
schedfix
```

重置调度器：

```text
schedreset
```

---

## 示例：Security 安全骨架

查看安全状态：

```text
security
```

查看安全审计日志：

```text
securitylog
```

清空安全审计日志：

```text
securityclear
```

尝试卸载核心模块：

```text
unload core
```

预期：

```text
module unload blocked by security: core
```

---

## 示例：Language 显示语言骨架

查看当前语言：

```text
lang
```

切换到 zh 通道：

```text
lang zh
```

切回英文：

```text
lang en
```

当前 VGA 文本模式不支持真正 UTF-8 中文显示，因此 zh 通道暂时使用 `[ZH]` ASCII 占位。

---

## 当前阶段

Lingjing OS 目前是一个早期裸机内核原型。

它还不是完整操作系统。

当前重点是验证：

```text
极小核心启动
模块按需加载
意图驱动调度
任务调度雏形
安全规则骨架
系统显示语言骨架
用户规则控制
系统状态可诊断
```

---

## 下一阶段计划

- 继续整理 core / platform 分层
- 将 intent / module / scheduler / security 从硬件细节中逐步解耦
- 增加 platform.h / platform_baremetal.c
- 将 screen_print / timer ticks 逐步替换为 platform 接口
- 将 scheduler 从模拟轮转推进到更真实的任务抽象
- 增加更正式的 task control block
- 增加简单任务创建 / 删除
- 加强 module dependency 约束
- 增加 heap free list
- 增加 paging
- 进入 framebuffer 图形模式
- 实现第一版文本 / 图形系统面板

---

## License

当前为实验阶段，许可证暂未确定。