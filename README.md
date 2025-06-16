# OS 2024 HomeWork

See tutorial at [os-2024-tutorial](https://github.com/peterzheng98/os-2024-tutorial).

## TODO

### 系统启动

- [x] 基础：Read ACPI Table
  - [x] learn: [HelloWorldPkg](https://www.bilibili.com/video/BV1HL4y1W7dJ/)、[ACPI Table](https://blog.csdn.net/u011280717/article/details/124959776)
  - [impl](./edk2/MyAcpiViewPkg/MyAcpiView.c)
- [x] 实践：Hack ACPI Table
  - [impl](./edk2/MyAcpiViewPkg/MyAcpiView.c)
- [x] 设计：UEFI 运行时服务
  - [impl](./kvm/linux-5.15.178/drivers/acpi/mytb_reader.c)

### 系统调用

- [x] 基础：用户定义的系统调用
  - [x] learn: [Adding a New System Call](https://www.kernel.org/doc/html/v5.15/process/adding-syscalls.html)
  - [impl](./kvm/linux-5.15.178/kernel/)
- [x] 实践：vDSO
- [ ] 设计：无需中断的系统调用

### 内存管理

- [x] 基础：页表和文件页
  - [impl](./os-2024-exercise/ch3-mm/ch3_1/)
- [x] 实践：内存文件系统
  - [impl](./kvm/linux-5.15.178/fs/ramfs/)
- [ ] 设计：内存压力导向的内存管理

### 文件系统

- [x] 基础：inode 和扩展属性管理
  - [impl](./os-2024-exercise/ch4-fs/ch4_1/)
- [x] 实践：FUSE
  - [impl](./fuse/)
- [ ] 设计：用户空间下的内存磁盘

### 网络与外部设备

- [x] 基础：tcpdump 和 socket 管理
  - [x] learn: [数据包结构](https://zhuanlan.zhihu.com/p/532166995)
  - [x] tcpdump [impl](./network/tcpdump/)
  - [x] socket [impl](./network/socket/)
- [x] 实践：NCCL
  - [x] [impl](./network/nccl/)
- [ ] 设计：DPDK
