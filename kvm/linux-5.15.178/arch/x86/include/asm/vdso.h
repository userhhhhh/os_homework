/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_X86_VDSO_H
#define _ASM_X86_VDSO_H

#include <asm/page_types.h>
#include <linux/linkage.h>
#include <linux/init.h>

#ifndef __ASSEMBLER__

#include <linux/mm_types.h>

struct vdso_image {
	void *data;
	unsigned long size;   /* Always a multiple of PAGE_SIZE */

	unsigned long alt, alt_len;
	unsigned long extable_base, extable_len;
	const void *extable;

	long sym_vvar_start;  /* Negative offset to the vvar area */

	long sym_vvar_page;
	long sym_pvclock_page;
	long sym_hvclock_page;
	long sym_timens_page;
	long sym_VDSO32_NOTE_MASK;
	long sym___kernel_sigreturn;
	long sym___kernel_rt_sigreturn;
	long sym___kernel_vsyscall;
	long sym_int80_landing_pad;
	long sym_vdso32_sigreturn_landing_pad;
	long sym_vdso32_rt_sigreturn_landing_pad;
};

#define TASK_STRUCT_PAGE_NUMBER (5) 

// struct task_info {
//     pid_t pid;
//     void *task_struct_ptr;
//     // ...其他字段
// } __attribute__((packed));

// // 声明 VDSO 导出的共享结构体指针
// extern struct task_info __vtask_info;

// // 声明你定义的 vdso 函数
// int __vdso_get_task_struct_info(struct task_info *info);

// struct task_info {
//     pid_t pid;
//     void *task_struct_ptr;
//     // ... 可以扩展
// } __attribute__((packed));

// // 声明为变量（不是 const），要被写入
// DECLARE_VVAR(128, struct task_info, __vtask_info);  // 偏移可以根据你实际放置的位置设置

#ifdef CONFIG_X86_64
extern const struct vdso_image vdso_image_64;
#endif

#ifdef CONFIG_X86_X32
extern const struct vdso_image vdso_image_x32;
#endif

#if defined CONFIG_X86_32 || defined CONFIG_COMPAT
extern const struct vdso_image vdso_image_32;
#endif

extern void __init init_vdso_image(const struct vdso_image *image);

extern int map_vdso_once(const struct vdso_image *image, unsigned long addr);

extern bool fixup_vdso_exception(struct pt_regs *regs, int trapnr,
				 unsigned long error_code,
				 unsigned long fault_addr);
#endif /* __ASSEMBLER__ */

#endif /* _ASM_X86_VDSO_H */
