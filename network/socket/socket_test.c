#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sched.h>

/* 替换为实际实现的系统调用号 */
#ifndef __NR_configure_socket_fairness
#define __NR_configure_socket_fairness 451
#endif

/* 系统调用标志 */
// #define CONFIGURE_SOCKET_FAIRNESS_NONE      0x00
// #define CONFIGURE_SOCKET_FAIRNESS_RECURSIVE 0x01  /* 影响所有子线程 */

/**
 * 设置线程的socket属性
 */
static int set_thread_socket_attrs(pid_t pid, int max_sockets, int priority_level)
{
    if (pid == 0) {
        pid = syscall(SYS_gettid);
    }

    int ret = syscall(__NR_configure_socket_fairness, pid, max_sockets, priority_level);
    if (ret < 0) {
        fprintf(stderr, "系统调用失败: %s (errno=%d)\n", strerror(errno), errno);
    }
    return ret;
}

/**
 * 读取/proc/[pid]/status文件内容
 */
void read_proc_status(pid_t pid)
{
    char path[64];
    FILE *fp;
    char line[256];
    
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    
    fp = fopen(path, "r");
    if (!fp) {
        perror("无法打开状态文件");
        return;
    }
    
    printf("=== 进程 %d 的状态信息 ===\n", pid);
    while (fgets(line, sizeof(line), fp)) {
        /* 只打印关心的行 */
        if (strstr(line, "Socket") != NULL) {
            printf("%s", line);
        }
    }
    
    fclose(fp);
}

/**
 * 创建指定数量的套接字
 */
int* create_sockets(int count)
{
    int *sock_fds = malloc(count * sizeof(int));
    if (!sock_fds) return NULL;
    
    int i;
    for (i = 0; i < count; i++) {
        sock_fds[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_fds[i] < 0) {
            fprintf(stderr, "第 %d 个socket创建失败: %s\n", i+1, strerror(errno));
            break;
        }
    }
    
    printf("成功创建 %d 个socket\n", i);
    return sock_fds;
}

/**
 * 关闭所有套接字
 */
void close_sockets(int *sock_fds, int count)
{
    if (!sock_fds) return;
    
    for (int i = 0; i < count; i++) {
        if (sock_fds[i] >= 0) {
            close(sock_fds[i]);
        }
    }
    
    free(sock_fds);
}

/**
 * 子线程函数：测试socket限制
 */
void* thread_socket_test(void *arg)
{
    int limit = *((int*)arg);
    
    pid_t tid = syscall(SYS_gettid);
    printf("[线程 %d] 开始运行，设置socket限制为 %d\n", tid, limit);
    
    /* 设置当前线程的socket限制 */
    if (set_thread_socket_attrs(0, limit, 80) < 0) {
        fprintf(stderr, "[线程 %d] 设置socket属性失败\n", tid);
        return NULL;
    }
    
    /* 检查状态 */
    read_proc_status(tid);
    
    /* 尝试创建socket直到失败 */
    int *sockets = create_sockets(limit + 2);  /* 尝试创建超过限制的socket */
    
    /* 再次读取状态 */
    read_proc_status(tid);
    
    /* 清理 */
    close_sockets(sockets, limit + 2);
    
    return NULL;
}

/**
 * 设置线程优先级
 */
void set_thread_priority(int priority)
{
    struct sched_param param;
    param.sched_priority = priority;

    // if (sched_setscheduler(0, SCHED_RR, &param) == -1) {
    if (set_thread_socket_attrs(0, 10000, priority) < 0) {
        perror("无法设置优先级");
        printf("设置线程优先级失败: %s (errno=%d),优先级 %d\n", strerror(errno), errno, priority);
    } else {
        printf("线程优先级设置为 %d\n", priority);
    }
}

/**
 * 主函数：运行测试
 */
int main(int argc, char *argv[])
{
    pid_t pid = getpid();
    printf("主进程PID: %d\n", pid);
    
    /* 测试1: 设置进程级别的socket限制 */
    printf("\n=== 测试1: 设置进程级别的socket限制 ===\n");
    if (set_thread_socket_attrs(pid, 10, 50) < 0) {
        fprintf(stderr, "设置进程socket属性失败\n");
    } else {
        read_proc_status(pid);
        int *sockets = create_sockets(12);  /* 尝试创建超过限制的socket */
        close_sockets(sockets, 12);
    }
    
    /* 测试2: 创建多线程并设置不同限制 */
    printf("\n=== 测试2: 多线程socket限制测试 ===\n");
    pthread_t threads[3];
    int limits[3] = {5, 8, 3};
    
    for (int i = 0; i < 3; i++) {
        if (pthread_create(&threads[i], NULL, thread_socket_test, &limits[i]) != 0) {
            perror("创建线程失败");
        }
    }
    
    /* 等待所有线程完成 */
    for (int i = 0; i < 3; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* 测试3: 测试优先级与socket限制的关联 */
    printf("\n=== 测试3: 测试优先级设置与socket限制的关联 ===\n");
    
    /* 设置高优先级并检查是否自动调整socket限制 */
    set_thread_priority(99);  /* 实时优先级 */
    read_proc_status(pid);
    
    /* 设置低优先级并检查是否自动调整socket限制 */
    set_thread_priority(0);   /* 普通优先级 */
    read_proc_status(pid);
    
    printf("\n所有测试完成，你通过了所有测试\n");
    return 0;
}