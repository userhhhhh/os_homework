#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/wait.h>

/* 定义与内核中相同的结构体 */
struct task_info {
    pid_t pid;
    void *task_struct_ptr;
};

typedef int (*get_task_info_fn)(struct task_info *info);

/* 测试vDSO功能的函数 */
int test_vdso_func() {
    void *vdso_handle;
    get_task_info_fn get_task_info;
    struct task_info info = {0};
    int ret;
    
    printf("[PID %d] 测试 vDSO get_task_struct_info 功能\n", getpid());
    
    /* 获取 vDSO 库句柄 */
    vdso_handle = dlopen("linux-vdso.so.1", RTLD_LAZY);
    if (!vdso_handle) {
        fprintf(stderr, "[PID %d] 无法打开 vDSO 库: %s\n", getpid(), dlerror());
        return 1;
    }
    
    /* 查找 get_task_struct_info 函数 */
    get_task_info = (get_task_info_fn)dlsym(vdso_handle, "get_task_struct_info");
    if (!get_task_info) {
        fprintf(stderr, "[PID %d] 无法找到 get_task_struct_info 函数: %s\n", getpid(), dlerror());
        dlclose(vdso_handle);
        return 1;
    }
    
    printf("[PID %d] 进程 PID (通过 getpid()): %d\n", getpid(), getpid());
    
    /* 调用 vDSO 函数 */
    ret = get_task_info(&info);
    if (ret != 0) {
        fprintf(stderr, "[PID %d] get_task_struct_info 调用失败: %d\n", getpid(), ret);
        dlclose(vdso_handle);
        return 1;
    }
    
    printf("[PID %d] 调用 vDSO 函数成功!\n", getpid());
    printf("[PID %d] 获取到的 PID: %d\n", getpid(), info.pid);
    printf("[PID %d] task_struct 指针地址: %p\n", getpid(), info.task_struct_ptr);
    
    /* 清理 */
    dlclose(vdso_handle);
    return 0;
}

int main() {
    pid_t child_pid;
    int status;
    
    printf("父进程开始，PID: %d\n\n", getpid());
    
    /* 在父进程中测试 vDSO 函数 */
    printf("=== 父进程测试开始 ===\n");
    if (test_vdso_func() != 0) {
        fprintf(stderr, "父进程测试失败\n");
        return 1;
    }
    printf("=== 父进程测试结束 ===\n\n");
    
    /* 创建子进程 */
    child_pid = fork();
    
    if (child_pid < 0) {
        /* fork 失败 */
        fprintf(stderr, "fork 失败: %s\n", strerror(errno));
        return 1;
    } else if (child_pid == 0) {
        /* 子进程中 */
        printf("\n=== 子进程测试开始 ===\n");
        int result = test_vdso_func();
        printf("=== 子进程测试结束 ===\n");
        exit(result);  // 子进程退出
    } else {
        /* 父进程中，等待子进程结束 */
        printf("等待子进程 (PID: %d) 结束...\n", child_pid);
        if (waitpid(child_pid, &status, 0) == -1) {
            fprintf(stderr, "waitpid 失败: %s\n", strerror(errno));
            return 1;
        }
        
        if (WIFEXITED(status)) {
            printf("子进程以状态码 %d 退出\n", WEXITSTATUS(status));
            if (WEXITSTATUS(status) != 0) {
                printf("子进程测试失败\n");
                return 1;
            }
        } else if (WIFSIGNALED(status)) {
            printf("子进程被信号 %d 终止\n", WTERMSIG(status));
            return 1;
        }
    }
    
    printf("\n所有测试完成!\n");
    return 0;
}