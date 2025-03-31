# QEMU Test

## task 3

- 所有实现在 MyAcpiViewPkg 里面
- 退出：Ctrl+A && C && quit

```bash
cd ~/os_homework
make init_build 
rm ./esp/MyAcpiView.efi 
cp /home/hqs123/os_homework/edk2/Build/MyAcpiViewPkg/DEBUG_GCC5/X64/MyAcpiView.efi ./esp
```

```bash
make toy_esp
```

## task 4

## task 4.1

- 编译内核

```bash
cd ~/os_homework/kvm/linux-5.15.178 && make -j$(nproc) 
```

- 测试代码

```bash
cd ~/os_homework && make kv_test project=test_basic
./bin/test_basic
```
