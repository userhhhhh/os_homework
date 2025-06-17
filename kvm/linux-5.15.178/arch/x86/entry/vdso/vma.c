// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2007 Andi Kleen, SUSE Labs.
 *
 * This contains most of the x86 vDSO kernel-side code.
 */
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/random.h>
#include <linux/elf.h>
#include <linux/cpu.h>
#include <linux/ptrace.h>
#include <linux/time_namespace.h>

#include <asm/pvclock.h>
#include <asm/vgtod.h>
#include <asm/proto.h>
#include <asm/vdso.h>
#include <asm/vvar.h>
#include <asm/tlb.h>
#include <asm/page.h>
#include <asm/desc.h>
#include <asm/cpufeature.h>
#include <clocksource/hyperv_timer.h>

#undef _ASM_X86_VVAR_H
#define EMIT_VVAR(name, offset)	\
	const size_t name ## _offset = offset;
#include <asm/vvar.h>
#include <asm/vdso.h>

// struct task_info __vtask_info;
// EXPORT_SYMBOL(__vtask_info);  // 让符号可用于 vDSO 模块

/*
步骤一：内核分配并维护数据
维护 struct vdso_data，用于存放要导出的信息。
步骤二：定义特殊映射
内核定义 vvar_mapping，指定映射的名字、缺页处理函数（fault）、重映射处理函数（mremap）等。
步骤三：进程启动时映射到用户空间
在进程启动时，内核会调用 arch_setup_additional_pages，
通过 map_vdso 等函数，把 vDSO 和 vvar 区域映射到用户进程的虚拟地址空间。
这些区域的物理页实际指向内核维护的数据结构。
步骤四：缺页异常时建立映射
当用户进程第一次访问 vvar 区域时，会触发缺页异常，内核的 vvar_fault 函数被调用。
vvar_fault 会把内核中的数据页（如 __vvar_page）映射到用户空间的相应虚拟地址。
步骤五：用户态直接读取
用户态程序可以直接通过指针访问 vvar 区域的数据，无需系统调用，效率极高。
例如，glibc 的 clock_gettime 实现会优先尝试从 vDSO/vvar 区域读取时间数据。
*/

// vma.c的作用：实现缺页异常，在缺页异常的时候，能把结构体传到虚拟空间里去

// 存放要导出的信息
struct vdso_data *arch_get_vdso_data(void *vvar_page)
{
	return (struct vdso_data *)(vvar_page + _vdso_data_offset);
}
#undef EMIT_VVAR

unsigned int vclocks_used __read_mostly;

#if defined(CONFIG_X86_64)
unsigned int __read_mostly vdso64_enabled = 1;
#endif

void __init init_vdso_image(const struct vdso_image *image)
{
	BUG_ON(image->size % PAGE_SIZE != 0);

	apply_alternatives((struct alt_instr *)(image->data + image->alt),
			   (struct alt_instr *)(image->data + image->alt +
						image->alt_len));
}

// vvar_mapping:指定映射的名字、缺页处理函数（fault）、重映射处理函数（mremap）等。
static const struct vm_special_mapping vvar_mapping;
struct linux_binprm;

// 作用：处理 vDSO 区域的页错误。在进程首次访问 [vdso] 区域的一页时，为该虚拟页找到对应的物理页，并将其映射进来。
static vm_fault_t vdso_fault(const struct vm_special_mapping *sm,
		      struct vm_area_struct *vma, struct vm_fault *vmf)
{
	// vma->vm_mm：当前进程
	// vdso_image：vdso 映射区的基本信息
	const struct vdso_image *image = vma->vm_mm->context.vdso_image;

	// vmf->pgoff: 触发页错误的虚拟地址在 [vdso] 区的页偏移。
	if (!image || (vmf->pgoff << PAGE_SHIFT) >= image->size)
		return VM_FAULT_SIGBUS;

	// virt_to_page：找到虚拟地址对应的物理页，可以理解成找到物理地址
	vmf->page = virt_to_page(image->data + (vmf->pgoff << PAGE_SHIFT));
	// get_page：增加页的引用计数
	get_page(vmf->page);
	return 0;
}

static void vdso_fix_landing(const struct vdso_image *image,
		struct vm_area_struct *new_vma)
{
#if defined CONFIG_X86_32 || defined CONFIG_IA32_EMULATION
	if (in_ia32_syscall() && image == &vdso_image_32) {
		struct pt_regs *regs = current_pt_regs();
		unsigned long vdso_land = image->sym_int80_landing_pad;
		unsigned long old_land_addr = vdso_land +
			(unsigned long)current->mm->context.vdso;

		/* Fixing userspace landing - look at do_fast_syscall_32 */
		if (regs->ip == old_land_addr)
			regs->ip = new_vma->vm_start + vdso_land;
	}
#endif
}

// 作用：当 vDSO 被 mremap() 移动时，更新进程上下文中的 vDSO 地址，并修复一些地址错误
static int vdso_mremap(const struct vm_special_mapping *sm,
		struct vm_area_struct *new_vma)
{
	const struct vdso_image *image = current->mm->context.vdso_image;

	vdso_fix_landing(image, new_vma);
	current->mm->context.vdso = (void __user *)new_vma->vm_start;

	return 0;
}

#ifdef CONFIG_TIME_NS
static struct page *find_timens_vvar_page(struct vm_area_struct *vma)
{
	if (likely(vma->vm_mm == current->mm))
		return current->nsproxy->time_ns->vvar_page;

	/*
	 * VM_PFNMAP | VM_IO protect .fault() handler from being called
	 * through interfaces like /proc/$pid/mem or
	 * process_vm_{readv,writev}() as long as there's no .access()
	 * in special_mapping_vmops().
	 * For more details check_vma_flags() and __access_remote_vm()
	 */

	WARN(1, "vvar_page accessed remotely");

	return NULL;
}

/*
 * The vvar page layout depends on whether a task belongs to the root or
 * non-root time namespace. Whenever a task changes its namespace, the VVAR
 * page tables are cleared and then they will re-faulted with a
 * corresponding layout.
 * See also the comment near timens_setup_vdso_data() for details.
 */
int vdso_join_timens(struct task_struct *task, struct time_namespace *ns)
{
	struct mm_struct *mm = task->mm;
	struct vm_area_struct *vma;

	mmap_read_lock(mm);

	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		unsigned long size = vma->vm_end - vma->vm_start;

		if (vma_is_special_mapping(vma, &vvar_mapping))
			zap_page_range(vma, vma->vm_start, size);
	}

	mmap_read_unlock(mm);
	return 0;
}
#else
static inline struct page *find_timens_vvar_page(struct vm_area_struct *vma)
{
	return NULL;
}
#endif

/* map_vdso() 创建了一个映射区域，然后 vtask 触发页错误 */
static vm_fault_t vtask_fault(const struct vm_special_mapping *sm,
		      struct vm_area_struct *vma, struct vm_fault *vmf)
{
    struct task_struct *task = current;
	unsigned long task_phys_addr = virt_to_phys(task);
	unsigned long task_start_page = task_phys_addr >> PAGE_SHIFT;

    if (vmf->pgoff > TASK_STRUCT_PAGE_NUMBER || vmf->pgoff < 0) {
        printk("pgoff error\n");
        return -1;
    } else if (vmf->pgoff < TASK_STRUCT_PAGE_NUMBER) { // 缺页的是 task_struct 部分，注意是 0_base
		printk(KERN_ERR "task_struct page fault pgoff: %ld\n", vmf->pgoff);
        return vmf_insert_pfn_prot(vma, vmf->address, task_start_page + vmf->pgoff, 
            pgprot_noncached(vma->vm_page_prot));
    } else { // 缺页的是 my_task_info 部分
		struct my_task_info{
			unsigned long offset; // 第一页的 offset
			unsigned long page_number; // 占据的页的个数
			unsigned long struct_size; // 整个task的大小
			unsigned long judge;
		} *the_info;
		struct page* the_info_page;
		printk(KERN_ERR "my_task_info page fault pgoff: %ld\n", vmf->pgoff);
		the_info_page = alloc_page(GFP_KERNEL);
		if (!the_info_page) {
			printk(KERN_ERR "fuck you\n");
			return VM_FAULT_OOM;
		}
		the_info = page_address(the_info_page);
		the_info->offset = task_phys_addr & ~PAGE_MASK;
		the_info->page_number = 6;
		the_info->struct_size = sizeof(struct task_struct);
		the_info->judge = 114514;
		vmf->page = the_info_page;
		return 0;
    }
}

static vm_fault_t vvar_fault(const struct vm_special_mapping *sm,
		      struct vm_area_struct *vma, struct vm_fault *vmf)
{
	// vma：当前触发缺页异常的虚拟内存区域。
	// vm_mm：指向该虚拟内存区域所属的内存描述符（struct mm_struct *），即进程的内存空间。
	// context：mm_struct 里的一个结构体，保存与体系结构相关的上下文信息。
	// image：当前进程的 vDSO 镜像信息
	const struct vdso_image *image = vma->vm_mm->context.vdso_image;
	unsigned long pfn;
	long sym_offset;

	if (!image)
		return VM_FAULT_SIGBUS;

	// vmf：struct vm_fault *，表示本次缺页异常的相关信息。
	// pgoff：page offset，表示当前缺页的虚拟地址在该 VMA 区域内，从起始地址算起是第几页。
	// PAGE_SHIFT：页大小的位数，通常 12，即 4KB 页。
	sym_offset = (long)(vmf->pgoff << PAGE_SHIFT) +
		image->sym_vvar_start;

	/*
	 * Sanity check: a symbol offset of zero means that the page
	 * does not exist for this vdso image, not that the page is at
	 * offset zero relative to the text mapping.  This should be
	 * impossible here, because sym_offset should only be zero for
	 * the page past the end of the vvar mapping.
	 */
	if (sym_offset == 0)
		return VM_FAULT_SIGBUS;

	if (sym_offset == image->sym_vvar_page) {
		struct page *timens_page = find_timens_vvar_page(vma);

		pfn = __pa_symbol(&__vvar_page) >> PAGE_SHIFT;

		/*
		 * If a task belongs to a time namespace then a namespace
		 * specific VVAR is mapped with the sym_vvar_page offset and
		 * the real VVAR page is mapped with the sym_timens_page
		 * offset.
		 * See also the comment near timens_setup_vdso_data().
		 */
		if (timens_page) {
			unsigned long addr;
			vm_fault_t err;

			/*
			 * Optimization: inside time namespace pre-fault
			 * VVAR page too. As on timens page there are only
			 * offsets for clocks on VVAR, it'll be faulted
			 * shortly by VDSO code.
			 */
			addr = vmf->address + (image->sym_timens_page - sym_offset);
			err = vmf_insert_pfn(vma, addr, pfn);
			if (unlikely(err & VM_FAULT_ERROR))
				return err;

			pfn = page_to_pfn(timens_page);
		}

		return vmf_insert_pfn(vma, vmf->address, pfn);
	} else if (sym_offset == image->sym_pvclock_page) {
		struct pvclock_vsyscall_time_info *pvti =
			pvclock_get_pvti_cpu0_va();
		if (pvti && vclock_was_used(VDSO_CLOCKMODE_PVCLOCK)) {
			return vmf_insert_pfn_prot(vma, vmf->address,
					__pa(pvti) >> PAGE_SHIFT,
					pgprot_decrypted(vma->vm_page_prot));
		}
	} else if (sym_offset == image->sym_hvclock_page) {
		struct ms_hyperv_tsc_page *tsc_pg = hv_get_tsc_page();

		if (tsc_pg && vclock_was_used(VDSO_CLOCKMODE_HVCLOCK))
			return vmf_insert_pfn(vma, vmf->address,
					virt_to_phys(tsc_pg) >> PAGE_SHIFT);
	} else if (sym_offset == image->sym_timens_page) {
		struct page *timens_page = find_timens_vvar_page(vma);

		if (!timens_page)
			return VM_FAULT_SIGBUS;

		pfn = __pa_symbol(&__vvar_page) >> PAGE_SHIFT;
		return vmf_insert_pfn(vma, vmf->address, pfn);
	}

	return VM_FAULT_SIGBUS;
}

static const struct vm_special_mapping vdso_mapping = {
	.name = "[vdso]",
	.fault = vdso_fault,
	.mremap = vdso_mremap,
};
static const struct vm_special_mapping vvar_mapping = {
	.name = "[vvar]",
	.fault = vvar_fault,
};
static const struct vm_special_mapping vtask_mapping = {
    .name = "[vtask]",
    .fault = vtask_fault,
};

/*
 * Add vdso and vvar mappings to current process.
 * @image          - blob to map
 * @addr           - request a specific address (zero to map at free addr)
 */
 
// #define VDSO_VTASK_PAGES 5
// #define VDSO_VTASK_SIZE  (VDSO_VTASK_PAGES * PAGE_SIZE)

static int map_vdso(const struct vdso_image *image, unsigned long addr)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	unsigned long text_start;
	unsigned long vtask_len;
	int ret = 0;

	if (mmap_write_lock_killable(mm))
		return -EINTR;

	// addr：在进程的虚拟地址空间中寻找一块未映射的连续内存区域
	addr = get_unmapped_area(NULL, addr,
				 image->size - image->sym_vvar_start, 0, 0);

	vtask_len = TASK_STRUCT_PAGE_NUMBER * PAGE_SIZE + PAGE_SIZE;
	addr += vtask_len;

	printk(KERN_ERR "start map_vdso: %lx\n", addr);

	if (IS_ERR_VALUE(addr)) {
		ret = addr;
		goto up_fail;
	}

	text_start = addr - image->sym_vvar_start;

	// 映射 vdso
	vma = _install_special_mapping(mm,
				       text_start,
				       image->size,
				       VM_READ|VM_EXEC|
				       VM_MAYREAD|VM_MAYWRITE|VM_MAYEXEC,
				       &vdso_mapping);

	if (IS_ERR(vma)) {
		ret = PTR_ERR(vma);
		goto up_fail;
	}

	// 映射 vvar
	vma = _install_special_mapping(mm,
				       addr,
				       -image->sym_vvar_start,
				       VM_READ|VM_MAYREAD|VM_IO|VM_DONTDUMP|
				       VM_PFNMAP,
				       &vvar_mapping);

	if (IS_ERR(vma)) {
		ret = PTR_ERR(vma);
		do_munmap(mm, text_start, image->size, NULL);
		goto up_fail;
	} 

	// 映射 vtask
	vma = _install_special_mapping(mm,
				       addr - vtask_len,
				       vtask_len,
				       VM_READ|VM_MAYREAD|VM_IO|VM_DONTDUMP|
				       VM_PFNMAP,
				       &vtask_mapping);
	
	if (IS_ERR(vma)) {
		ret = PTR_ERR(vma);
		do_munmap(mm, text_start, image->size, NULL);
		do_munmap(mm, addr, -image->sym_vvar_start, NULL);
		goto up_fail;
	} else {
		current->mm->context.vdso = (void __user *)text_start;
		current->mm->context.vdso_image = image;
		current->mm->context.vtask = (void __user *)(addr - vtask_len);
	}

	printk(KERN_ERR "end map_vdso: %lx\n", addr);

up_fail:
	mmap_write_unlock(mm);
	return ret;
}

#ifdef CONFIG_X86_64
/*
 * Put the vdso above the (randomized) stack with another randomized
 * offset.  This way there is no hole in the middle of address space.
 * To save memory make sure it is still in the same PTE as the stack
 * top.  This doesn't give that many random bits.
 *
 * Note that this algorithm is imperfect: the distribution of the vdso
 * start address within a PMD is biased toward the end.
 *
 * Only used for the 64-bit and x32 vdsos.
 */
static unsigned long vdso_addr(unsigned long start, unsigned len)
{
	unsigned long addr, end;
	unsigned offset;

	/*
	 * Round up the start address.  It can start out unaligned as a result
	 * of stack start randomization.
	 */
	start = PAGE_ALIGN(start);

	/* Round the lowest possible end address up to a PMD boundary. */
	end = (start + len + PMD_SIZE - 1) & PMD_MASK;
	if (end >= DEFAULT_MAP_WINDOW)
		end = DEFAULT_MAP_WINDOW;
	end -= len;

	if (end > start) {
		offset = get_random_int() % (((end - start) >> PAGE_SHIFT) + 1);
		addr = start + (offset << PAGE_SHIFT);
	} else {
		addr = start;
	}

	/*
	 * Forcibly align the final address in case we have a hardware
	 * issue that requires alignment for performance reasons.
	 */
	addr = align_vdso_addr(addr);

	return addr;
}

static int map_vdso_randomized(const struct vdso_image *image)
{
	unsigned long addr = vdso_addr(current->mm->start_stack, image->size-image->sym_vvar_start);

	return map_vdso(image, addr);
}
#endif

int map_vdso_once(const struct vdso_image *image, unsigned long addr)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;

	mmap_write_lock(mm);
	/*
	 * Check if we have already mapped vdso blob - fail to prevent
	 * abusing from userspace install_special_mapping, which may
	 * not do accounting and rlimit right.
	 * We could search vma near context.vdso, but it's a slowpath,
	 * so let's explicitly check all VMAs to be completely sure.
	 */
	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		if (vma_is_special_mapping(vma, &vdso_mapping) ||
				vma_is_special_mapping(vma, &vvar_mapping)) {
			mmap_write_unlock(mm);
			return -EEXIST;
		}
	}
	mmap_write_unlock(mm);

	return map_vdso(image, addr);
}

#if defined(CONFIG_X86_32) || defined(CONFIG_IA32_EMULATION)
static int load_vdso32(void)
{
	if (vdso32_enabled != 1)  /* Other values all mean "disabled" */
		return 0;

	return map_vdso(&vdso_image_32, 0);
}
#endif

#ifdef CONFIG_X86_64
int arch_setup_additional_pages(struct linux_binprm *bprm, int uses_interp)
{
	if (!vdso64_enabled)
		return 0;

	return map_vdso_randomized(&vdso_image_64);
}

#ifdef CONFIG_COMPAT
int compat_arch_setup_additional_pages(struct linux_binprm *bprm,
				       int uses_interp, bool x32)
{
#ifdef CONFIG_X86_X32_ABI
	if (x32) {
		if (!vdso64_enabled)
			return 0;
		return map_vdso_randomized(&vdso_image_x32);
	}
#endif
#ifdef CONFIG_IA32_EMULATION
	return load_vdso32();
#else
	return 0;
#endif
}
#endif
#else
int arch_setup_additional_pages(struct linux_binprm *bprm, int uses_interp)
{
	return load_vdso32();
}
#endif

bool arch_syscall_is_vdso_sigreturn(struct pt_regs *regs)
{
#if defined(CONFIG_X86_32) || defined(CONFIG_IA32_EMULATION)
	const struct vdso_image *image = current->mm->context.vdso_image;
	unsigned long vdso = (unsigned long) current->mm->context.vdso;

	if (in_ia32_syscall() && image == &vdso_image_32) {
		if (regs->ip == vdso + image->sym_vdso32_sigreturn_landing_pad ||
		    regs->ip == vdso + image->sym_vdso32_rt_sigreturn_landing_pad)
			return true;
	}
#endif
	return false;
}

#ifdef CONFIG_X86_64
static __init int vdso_setup(char *s)
{
	vdso64_enabled = simple_strtoul(s, NULL, 0);
	return 1;
}
__setup("vdso=", vdso_setup);

static int __init init_vdso(void)
{
	BUILD_BUG_ON(VDSO_CLOCKMODE_MAX >= 32);

	init_vdso_image(&vdso_image_64);

#ifdef CONFIG_X86_X32_ABI
	init_vdso_image(&vdso_image_x32);
#endif

	return 0;
}
subsys_initcall(init_vdso);
#endif /* CONFIG_X86_64 */
