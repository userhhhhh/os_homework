# QEMU Test

## task 3

### task 3.1

- 所有实现在 MyAcpiViewPkg 里面
- 退出：Ctrl+A && C && quit

```bash
cd ~/os_homework && make init_build 
rm ./esp/MyAcpiView.efi && cp /home/hqs123/os_homework/edk2/Build/MyAcpiViewPkg/DEBUG_GCC5/X64/MyAcpiView.efi ./esp
```

```bash
make toy_esp
```

### task 3.2

- 启动 uefi

```bash
cd ~/os_homework && make start_uefi
```

- 在 uefi 上运行 MyAcpiView.efi，修改 ACPI 表

- 在 uefi 里启动内核，然后检查 ACPI 表

```bash
bzImage initrd=initramfs.cpio.gz init=/init console=ttyS0
dmesg | grep ACPI
```

## task 4

### task 4.1

- 编译内核

```bash
cd ~/os_homework/kvm/linux-5.15.178 && make -j$(nproc) 
```

- 测试代码

```bash
cd ~/os_homework && make kv_test project=test_basic
./bin/test_basic
```

### task 4.2

先编译内核，再编译 busybox，最后跑内核

```bash
cd ~/os_homework/kvm/linux-5.15.178 && make -j$(nproc) 
make kv_test project=vdso666
```
