# Executable name
OVMF_DIR = ./edk2/Build/OvmfX64/DEBUG_GCC5/FV
KERNEL_DIR = ./kvm/linux-5.15.178/arch/x86/boot/bzImage
BUSYBOX_DIR = ./kvm/busybox-1.35.0/initramfs.cpio.gz
HM_DIR = /home/hqs123/os_homework

only_kernel:
	sudo qemu-system-x86_64 \
		-kernel "${KERNEL_DIR}" \
		-initrd "${BUSYBOX_DIR}" \
		-nographic \
		-append "init=/init console=ttyS0" \
		-enable-kvm

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

copy_MyAcpiView:
	cp ./esp/MyAcpiView.efi ./uefi

copy_bzImage:
	cp ./kvm/linux-5.15.178/arch/x86/boot/bzImage ./uefi
	
	
	


