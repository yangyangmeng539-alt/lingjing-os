# Lingjing OS

Lingjing OS 是一个实验性操作系统项目。

当前目标不是替代现有桌面系统，而是探索一种更轻的系统结构：

- 极小核心启动
- 模块按需加载
- 意图驱动调度
- 用户拥有最终控制权
- 外部生态必须受系统规则约束

当前版本：

```text
version: dev-0.0.2
stage: intent prototype
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
- `load <module>` 模拟加载外部模块
- `unload <module>` 模拟卸载外部模块
- 核心模块不可卸载
- external 模块可卸载

### Intent Layer

- `intent list`
- `intent permissions`
- `intent policy`
- `intent audit`
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
按需加载模块
↓
运行 intent
↓
停止 intent
↓
释放外部模块
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
modules
moduleinfo <name>
load <module>
unload <module>

intent list
intent permissions
intent policy
intent audit
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

停止当前意图：

```text
intent stop
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
用户规则控制
```

---

## 下一阶段计划

- 整理目录结构
- 加强 module dependency
- 增加 intent plan 表达能力
- 增加简单任务调度雏形
- 增加分页 paging
- 进入 framebuffer 图形模式
- 实现第一版文本 / 图形系统面板

---

## License

当前为实验阶段，许可证暂未确定。