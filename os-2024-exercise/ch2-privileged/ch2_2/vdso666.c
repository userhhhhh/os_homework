// #define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/syscall.h>
/* 定义与内核中相同的结构体 */
struct task_info {
    pid_t pid;
    void *task_struct_ptr;
};

typedef int (*get_task_info_fn)(struct task_info *info);

int main() {
    void *vdso_handle;
    get_task_info_fn get_task_info;
    struct task_info info = {0};
    int ret;
    
    printf("测试 vDSO get_task_struct_info 功能\n");
    
    /* 获取 vDSO 库句柄 */
    vdso_handle = dlopen("linux-vdso.so.1", RTLD_LAZY);
    if (!vdso_handle) {
        fprintf(stderr, "无法打开 vDSO 库: %s\n", dlerror());
        return 1;
    }
    
    /* 查找 get_task_struct_info 函数 */
    get_task_info = (get_task_info_fn)dlsym(vdso_handle, "get_task_struct_info");
    if (!get_task_info) {
        fprintf(stderr, "无法找到 get_task_struct_info 函数: %s\n", dlerror());
        dlclose(vdso_handle);
        return 1;
    }
    
    printf("进程 PID (通过 getpid()): %d\n", getpid());
    
    /* 调用 vDSO 函数 */
    ret = get_task_info(&info);
    if (ret != 0) {
        fprintf(stderr, "get_task_struct_info 调用失败: %d\n", ret);
        dlclose(vdso_handle);
        return 1;
    }
    
    printf("调用 vDSO 函数成功!\n");
    printf("获取到的 PID: %d\n", info.pid);
    printf("task_struct 指针地址: %p\n", info.task_struct_ptr);

    // printf("play: %d\n", *(int *)(info.task_struct_ptr+0x1000));
    
    /* 可以尝试读取更多 task_struct 信息，但需要谨慎 */
    if (info.task_struct_ptr) {
        /* 
         * 注意：直接访问 task_struct 可能会导致段错误
         * 因为它可能不是完全映射的或映射的内容可能有限
         */
        printf("task_struct 可以在用户空间访问\n");
    }
    
    /* 清理 */
    dlclose(vdso_handle);
    return 0;
}