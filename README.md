# flyos

一个从零开始编写的 **x86_64 操作系统**。UEFI 引导，包含内存管理、多任务、用户态、文件系统、持久化和图形界面。

![screenshot](docs/screenshot.png)

## 功能

- UEFI + GRUB2 引导，x86_64 长模式
- IDT + 32 个 CPU 异常 + panic + 栈回溯
- PIC + PIT + PS/2 键盘 + PS/2 鼠标 + ATA + framebuffer
- 物理页 bitmap + 4 级页表 + 内核堆
- 抢占式调度 + wait_queue + sleep + 多进程
- ring3 用户程序 + syscall + ELF64 + 简易 libc
- 树形 ramfs + 路径解析 + ATA 持久化
- 图形界面：窗口、按钮、拖动、终端窗口

## 编译运行

```bash
sudo apt install build-essential nasm qemu-system-x86 xorriso \
                 grub-pc-bin grub-common grub-efi-amd64-bin mtools ovmf

make clean
make
make run