# Lingjing OS

Lingjing OS 是一个实验性操作系统项目。

当前目标不是替代现有桌面系统，而是探索一种更轻的系统结构：

- 极小核心启动
- 模块按需加载
- 意图驱动调度
- 用户拥有最终控制权
- 外部生态必须受系统规则约束
- 调度器以可验证、可诊断、可恢复为核心方向

当前版本：

```text
version: dev-0.0.3
stage: scheduler prototype
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
    gui
  gdt
    idt
      keyboard
        shell
      timer
        scheduler
        net
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

### Scheduler Prototype

当前调度器仍是原型，不是真正上下文切换。

已支持：

- IRQ0 tick 接入 scheduler
- scheduler ticks 统计
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

---

## 系统诊断

### 单行状态栏

```text
status
```

示例：

```text
LJ | up 6s | in none | m9 t4 | s690 y2 | coop | deps ok | doc ok
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

modules
moduleinfo <name>
moduledeps <name>
moduletree
modulecheck

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
intent policy
intent doctor
intent plan video
intent run video
intent status
intent stop

mem
uptime
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
用户规则控制
系统状态可诊断
```

---

## 下一阶段计划

- 整理目录结构
- 将 scheduler 从模拟轮转推进到更真实的任务抽象
- 增加更正式的 task control block
- 增加简单任务创建 / 删除
- 加强 module dependency 约束
- 增加 paging
- 进入 framebuffer 图形模式
- 实现第一版文本 / 图形系统面板

---

## License

当前为实验阶段，许可证暂未确定。