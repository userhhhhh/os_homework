# QEMU Test

## task 3

### task 3.1

- 所有实现在 MyAcpiViewPkg 里面
- 退出：Ctrl+A && X

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

### task 3.3

先导入 `MyAddAcpi.efi` 文件。
注意修改 `./edk2/Conf/target.txt` 文件。
然后打开内核：

```bash
cd ~/os_homework && make init_build 
cp /home/hqs123/os_homework/edk2/Build/MyAddAcpiPkg/DEBUG_GCC5/X64/MyAddAcpi.efi ./esp
cd ~/os_homework && make only_kernel
ls /sys/kernel
```

退出，打开 uefi

```bash
cd ~/os_homework/kvm/linux-5.15.178 && make -j$(nproc) 
cp /home/hqs123/os_homework/edk2/Build/MyAddAcpiPkg/DEBUG_GCC5/X64/MyAddAcpi.efi ./uefi
cd ~/os_homework && make copy_bzImage
make start_uefi
run MyAddAcpi.efi
bzImage initrd=initramfs.cpio.gz init=/init console=ttyS0
ls /sys/kernel
cat /sys/kernel/mytb_acpi/message
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
./bin/vdso666
```

## task 5

### task 5.1

见 `/os-2024-exercise/ch3-mm/ch3_1`

### task 5.2

先在本地创建磁盘镜像

```bash
cd ~/os_homework && make qcow2
```

先编译内核再编译 busybox，最后跑内核

```bash
cd ~/os_homework/kvm/linux-5.15.178 && make -j$(nproc) 
cd ~/os_homework && make kv_test_c project=ramfs_persistence_test
cd ~/os_homework && make kv_test_c project=ramfs_consistency_test
cd ~/os_homework && make kv_test_c project=ramfs_crash_test
./bin/ramfs_persistence_test
./bin/ramfs_consistency_test
./bin/ramfs_crash_test
```

```bash
cd ~/os_homework && make kv_test_c project=ramfs_crash_test
./bin/ramfs_crash_test
```

## task 6

### task 6.2

在一个终端：

```bash
conda activate os_fuse
cd ~/os_homework/fuse && python3 ./gptfs.py ./gptfs_mount
```

在另一个终端：

```bash
cd ~/os_homework/fuse && mkdir ./gptfs_mount/test1
echo "What is Python?" > ./gptfs_mount/test1/input 
cat ./gptfs_mount/test1/output
```

## task 7

### task 7.1 

```bash
cd /home/hqs123/os_homework/tcpdump
gcc -Wall -g -o custom_tcpdump main.c custom_tcpdump.c -lpcap
sudo ./custom_tcpdump # 使用默认参数
sudo ./custom_tcpdump eth0 "tcp port 80" # 指定网络接口和过滤条件
```

```bash
cd ~/os_homework/kvm/linux-5.15.178 && make -j$(nproc) 
cd ~/os_homework && make kv_test_c project=socket_test
./bin/socket_test
```

### task 7.2

见 `network/nccl/readme.md`
