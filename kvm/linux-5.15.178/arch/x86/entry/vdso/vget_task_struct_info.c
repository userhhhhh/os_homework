// vget_task_struct_info.c

#include <linux/time.h>
#include <linux/types.h>
#include <linux/vdso_task.h>
#include <asm/unistd.h>
#include <asm/vgtod.h>
#include <asm/vdso.h>
#include <asm/vvar.h>
#include <asm/processor.h>

// 核心函数：直接读取 vtask 映射区域中的 task_info 内容
int __vdso_get_task_struct_info(struct task_info *info)
{
    const struct vdso_data *vdata;
    void __user *vtask_start_addr, *vinfo_start_addr;
    struct my_task_info{
		unsigned long offset; // 第一页的 offset
        unsigned long page_number; // 占据的页的个数
        unsigned long struct_size; // 整个task的大小
        unsigned long judge;
	} *the_info;

    if (!info) return -1;
    vdata = __arch_get_vdso_data();
    if (!vdata) return -EINVAL;

    /* 获取 vdata 地址，然后取整到页面边界，往前一页就是 vinfo 的地址 */
    vinfo_start_addr = (void __user *)(((unsigned long)vdata >> PAGE_SHIFT << PAGE_SHIFT) - PAGE_SIZE);  
    
    the_info = (struct my_task_info *)vinfo_start_addr;
    if (the_info->judge != 114514){
        return -1;
    }
    vtask_start_addr = vinfo_start_addr - TASK_STRUCT_PAGE_NUMBER * PAGE_SIZE + the_info->offset; 

    info->task_struct_ptr = vtask_start_addr;
    info->pid = *(pid_t *)((char *)info->task_struct_ptr + offsetof(struct task_struct, pid));

    return 0;
}

// 提供给用户空间的函数入口
int get_task_struct_info(struct task_info *info)
    __attribute__((weak, alias("__vdso_get_task_struct_info")));
