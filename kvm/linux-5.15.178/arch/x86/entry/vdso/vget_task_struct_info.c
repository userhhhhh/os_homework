// vget_task_struct_info.c

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/unistd.h>
#include <asm/vdso.h>
#include <asm/vvar.h>
#include <asm/vdso/gettimeofday.h>

// 用户空间结构体定义（应与用户态一致）
struct task_info {
    pid_t pid;
    void *task_struct_ptr;
    // 你可以扩展其他字段
};

// __vtask_base 是链接符号，位于 vtask 区域起始
extern const struct task_info __vtask_info;

// 核心函数：直接读取 vtask 映射区域中的 task_info 内容
notrace int __vdso_get_task_struct_info(struct task_info *info)
{
    if (!info)
        return -1;

    // 拷贝只读共享区域的数据到用户传入的 info
    *info = __vtask_info;

    return 0;
}

// 提供给用户空间的函数入口
int get_task_struct_info(struct task_info *info)
    __attribute__((weak, alias("__vdso_get_task_struct_info")));
