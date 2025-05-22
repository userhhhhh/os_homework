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
int test_vdso_func(int generation) {
    void *vdso_handle;
    get_task_info_fn get_task_info;
    struct task_info info = {0};
    int ret;
    
    printf("[Gen %d][PID %d] 测试 vDSO get_task_struct_info 功能\n", 
           generation, getpid());
    
    /* 获取 vDSO 库句柄 */
    vdso_handle = dlopen("linux-vdso.so.1", RTLD_LAZY);
    if (!vdso_handle) {
        fprintf(stderr, "[Gen %d][PID %d] 无法打开 vDSO 库: %s\n", 
                generation, getpid(), dlerror());
        return 1;
    }
    
    /* 查找 get_task_struct_info 函数 */
    get_task_info = (get_task_info_fn)dlsym(vdso_handle, "get_task_struct_info");
    if (!get_task_info) {
        fprintf(stderr, "[Gen %d][PID %d] 无法找到 get_task_struct_info 函数: %s\n", 
                generation, getpid(), dlerror());
        dlclose(vdso_handle);
        return 1;
    }
    
    printf("[Gen %d][PID %d] 进程 PID (通过 getpid()): %d\n", 
           generation, getpid(), getpid());
    
    /* 调用 vDSO 函数 */
    ret = get_task_info(&info);
    if (ret != 0) {
        fprintf(stderr, "[Gen %d][PID %d] get_task_struct_info 调用失败: %d\n", 
                generation, getpid(), ret);
        dlclose(vdso_handle);
        return 1;
    }
    
    printf("[Gen %d][PID %d] 调用 vDSO 函数成功!\n", generation, getpid());
    printf("[Gen %d][PID %d] 获取到的 PID: %d\n", generation, getpid(), info.pid);
    printf("[Gen %d][PID %d] task_struct 指针地址: %p\n", 
           generation, getpid(), info.task_struct_ptr);
    
    /* 清理 */
    dlclose(vdso_handle);
    return 0;
}

/* 创建子进程并等待其完成的函数 */
int fork_and_test(int current_generation, int max_generations) {
    pid_t child_pid;
    int status;
    
    if (current_generation >= max_generations) {
        return test_vdso_func(current_generation);
    }
    
    /* 先在当前进程测试 */
    printf("\n=== 第%d代进程测试开始 (PID: %d) ===\n", 
           current_generation, getpid());
    if (test_vdso_func(current_generation) != 0) {
        fprintf(stderr, "[Gen %d][PID %d] 测试失败\n", 
                current_generation, getpid());
        return 1;
    }
    
    /* 创建下一代进程 */
    child_pid = fork();
    
    if (child_pid < 0) {
        /* fork 失败 */
        fprintf(stderr, "[Gen %d][PID %d] fork 失败: %s\n", 
                current_generation, getpid(), strerror(errno));
        return 1;
    } else if (child_pid == 0) {
        /* 子进程中 */
        return fork_and_test(current_generation + 1, max_generations);
    } else {
        /* 父进程中，等待子进程结束 */
        printf("[Gen %d][PID %d] 等待子进程 (PID: %d) 结束...\n", 
               current_generation, getpid(), child_pid);
        
        if (waitpid(child_pid, &status, 0) == -1) {
            fprintf(stderr, "[Gen %d][PID %d] waitpid 失败: %s\n", 
                    current_generation, getpid(), strerror(errno));
            return 1;
        }
        
        if (WIFEXITED(status)) {
            printf("[Gen %d][PID %d] 子进程以状态码 %d 退出\n", 
                   current_generation, getpid(), WEXITSTATUS(status));
            if (WEXITSTATUS(status) != 0) {
                printf("[Gen %d][PID %d] 子进程测试失败\n", 
                       current_generation, getpid());
                return 1;
            }
        } else if (WIFSIGNALED(status)) {
            printf("[Gen %d][PID %d] 子进程被信号 %d 终止\n", 
                   current_generation, getpid(), WTERMSIG(status));
            return 1;
        }
        
        printf("=== 第%d代进程测试结束 (PID: %d) ===\n\n", 
               current_generation, getpid());
        return 0;
    }
}

int main() {
    printf("多级fork vDSO测试程序开始\n");
    
    /* 从第0代（祖先进程）开始，最多创建3代进程 */
    int result = fork_and_test(0, 3);
    
    if (result == 0) {
        printf("\n所有测试成功完成!\n");
    } else {
        printf("\n测试过程中出现错误!\n");
    }
    
    return result;
}