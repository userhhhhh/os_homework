# Guide

`syscall_64.tbl`文件

```markdown
[系统调用编号] [ABI类型] [系统调用名称] [内核中实现函数名]
0 common read sys_read
```

用编号的原因：

- 名字要传字符串指针，而编号只要一条机器指令
- 占内存小，多语言支持

## vdso

`./kvm/linux-5.15.178/arch/x86/entry/vdso/vma.c`
`./kvm/linux-5.15.178/arch/x86/entry/vdso/vget_task_struct_info.c`

原因：syscall 有切换开销。但一部分 syscall 并不会泄露内核的状态也不会修改内核。对于这些系统调用可以在用户态以只读共享页面的方式直接进行而无需进入操作系统内核。vDSO 支持这一想法。
目标：在 vDSO 中新增一个读取当前进程对应的 task_struct 中的所有信息的函数。

对物理地址，它原本映射的虚拟地址是不可用的，创建一个新的虚拟地址映射，这个新的虚拟地址是可以读的。

这个新的虚拟地址位于 vvar 前面，我创建了一个 vtask 去存储它。vtask 前面几个页是内容，最后一个页是 info

每次触发 page fault 会把东西刷过去。

## ramfs

`./kvm/linux-5.15.178/fs/ramfs/inode.c`

目标：内存文件系统（RAMfs）无法持久化。现在需要为 RAMfs 提供简单的持久化支持，当某个文件被 flush 到 RAMfs 时，该文件同步刷到一个预先设置的同步目录中。

思路：在 ramfs 里面创建一个 proc，用来与 bind、sync 函数交互。bind 是为了将 sync_path 存起来然后给后续使用。sync 就是同步机制。sync 的想法是先创建一个 .tmp 文件，先写入 .tmp 文件来保证操作的原子性。然后将 .tmp 文件重命名。这样就实现了同步。

