# Executable name
OVMF_DIR = ./edk2/Build/OvmfX64/DEBUG_GCC5/FV
KERNEL_DIR = ./kvm/linux-5.15.178/arch/x86/boot/bzImage
BUSYBOX_DIR = ./kvm/busybox-1.35.0/initramfs.cpio.gz
HM_DIR = /home/hqs123/os_homework
QCOW2 = ./disk.qcow2

only_kernel:
	sudo qemu-system-x86_64 \
		-kernel "${KERNEL_DIR}" \
		-initrd "${BUSYBOX_DIR}" \
		-nographic \
		-append "init=/init console=ttyS0" \
		-enable-kvm \
		-smp 4 \
		-drive file=$(QCOW2),format=qcow2,if=ide,index=1

only_ovmf:
	sudo qemu-system-x86_64 \
		-nographic \
		-drive if=pflash,format=raw,unit=0,file="${OVMF_DIR}/OVMF_CODE.fd",readonly=on \
		-drive if=pflash,format=raw,unit=1,file="${OVMF_DIR}/OVMF_VARS.fd" \
		-enable-kvm\

start_uefi:
	sudo qemu-system-x86_64 \
		-m 4096 \
		-nographic \
		-drive if=pflash,format=raw,unit=0,file="${OVMF_DIR}/OVMF_CODE.fd",readonly=on \
		-drive if=pflash,format=raw,unit=1,file="${OVMF_DIR}/OVMF_VARS.fd" \
		-drive file=fat:rw:./uefi,format=raw,if=ide,index=0 \
		-enable-kvm\

kernel_and_ovmf:
	sudo qemu-system-x86_64 \
		-kernel "${KERNEL_DIR}" \
		-initrd "${BUSYBOX_DIR}" \
		-nographic \
		-drive if=pflash,format=raw,unit=0,file="${OVMF_DIR}/OVMF_CODE.fd",readonly=on \
		-drive if=pflash,format=raw,unit=1,file="${OVMF_DIR}/OVMF_VARS.fd" \
		-append "init=/init console=ttyS0" \
		-enable-kvm

server_bios:
	sudo qemu-system-x86_64 \
		-m 4096 \
		-cdrom ./ubuntu-24.04.1-live-server-amd64.iso \
		-bios ./edk2/Build/OvmfX64/DEBUG_GCC5/FV/OVMF.fd\
		-nographic \
		-enable-kvm

server:
	sudo qemu-system-x86_64 \
		-m 4096 \
		-cdrom ./ubuntu-24.04.1-live-server-amd64.iso \
		-drive if=pflash,format=raw,unit=0,file="${OVMF_DIR}/OVMF_CODE.fd",readonly=on \
		-drive if=pflash,format=raw,unit=1,file="${OVMF_DIR}/OVMF_VARS.fd" \
		-nographic \
		-enable-kvm

toy_esp:
	sudo qemu-system-x86_64 \
		-nographic \
		-drive if=pflash,format=raw,unit=0,file="${OVMF_DIR}/OVMF_CODE.fd",readonly=on \
		-drive if=pflash,format=raw,unit=1,file="${OVMF_DIR}/OVMF_VARS.fd" \
		-drive format=raw,file=fat:rw:./esp -net none \
		-enable-kvm

init_build:
	@bash -c "cd edk2 && \
	export EDK_TOOLS_PATH=${HM_DIR}/edk2/BaseTools && \
	source edksetup.sh BaseTools && \
	build"

kv_test:
	g++ -static -o ./ctest/$(project) ./ctest/$(project).cpp
	cp ./ctest/$(project) ./kvm/busybox-1.35.0/_install/bin/
	cd kvm/busybox-1.35.0/_install && \
	find . -print0 | cpio --null -ov --format=newc | gzip -9 > ../initramfs.cpio.gz
	make only_kernel

kv_test_c:
	gcc -static -o ./ctest/$(project) ./ctest/$(project).c
	cp ./ctest/$(project) ./kvm/busybox-1.35.0/_install/bin/
	cd kvm/busybox-1.35.0/_install && \
	find . -print0 | cpio --null -ov --format=newc | gzip -9 > ../initramfs.cpio.gz
	make only_kernel

copy_MyAcpiView:
	cp ./esp/MyAcpiView.efi ./uefi

copy_bzImage:
	cp ./kvm/linux-5.15.178/arch/x86/boot/bzImage ./uefi

qcow2:
	@if [ -f $(QCOW2) ]; then \
        echo "发现现有的磁盘镜像: $(QCOW2)"; \
        read -p "是否删除并重新创建? (y/n): " answer; \
        if [ "$$answer" = "y" ]; then \
            echo "删除现有磁盘镜像..."; \
            rm -f $(QCOW2); \
        else \
            echo "保留现有磁盘镜像，退出操作"; \
            exit 0; \
        fi; \
    fi
	@echo "=== 创建并格式化 QEMU 磁盘镜像 ==="
    # 创建磁盘镜像
	qemu-img create -f qcow2 $(QCOW2) 256M
	@echo "已创建磁盘镜像: $(QCOW2)"
	# 加载 NBD 内核模块
	sudo modprobe nbd max_part=8
	# 连接磁盘镜像到 NBD 设备
	@echo "连接磁盘镜像到 NBD 设备..."
	sudo qemu-nbd --disconnect /dev/nbd0
	sudo qemu-nbd --connect=/dev/nbd0 $(QCOW2)
	# 创建分区
	@echo "创建分区..."
	printf "n\np\n1\n\n\nw\n" | sudo fdisk /dev/nbd0
	# 刷新分区表
	@echo "刷新分区表..."
	sudo partprobe /dev/nbd0
	# 格式化为 ext4 文件系统
	@echo "格式化分区为 ext4 文件系统..."
	sudo mkfs.ext4 /dev/nbd0p1
	@echo "断开 NBD 连接..."
	sudo qemu-nbd --disconnect /dev/nbd0
	@echo "=== disk.qcow2 已创建并格式化为 ext4 文件系统 ==="
	
	
	
	


