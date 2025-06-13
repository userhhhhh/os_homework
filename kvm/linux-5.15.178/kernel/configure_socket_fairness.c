#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/slab.h>
#include <linux/pid.h>
#include <linux/cred.h>
#include <linux/capability.h>
#include <linux/rcupdate.h>

/**
 * sys_configure_socket_fairness - 为特定进程配置Socket级别的公平管理策略
 * @pid: 需要配置的进程ID
 * @max_socket_allowed: 该进程允许打开的最大Socket数
 * @priority_level: 该进程的优先级（影响Socket分配策略）
 *
 * 返回：成功时返回0，失败返回负的错误码
 */
SYSCALL_DEFINE3(configure_socket_fairness, pid_t, pid, 
                int, max_socket_allowed, int, priority_level)
{
    struct task_struct *task;
    int ret = 0;
    
    /* 验证参数有效性 */
    if (max_socket_allowed < 0 || priority_level < 0)
        return -EINVAL;
    
    /* 根据pid查找对应的任务结构 */
    rcu_read_lock();
    task = find_task_by_vpid(pid);
    if (!task) {
        rcu_read_unlock();
        return -ESRCH;  /* 没有找到对应的进程 */
    }
    
    /* 检查权限 - 只有特权进程或进程自己才能修改这些设置 */
    if (!capable(CAP_SYS_ADMIN) && 
        !same_thread_group(current, task)) {
        rcu_read_unlock();
        return -EPERM;
    }
    
    /* 设置Socket公平性参数 */
    task_lock(task);
    task->max_socket_allowed = max_socket_allowed;
    task->priority_level = priority_level;
    /* socket_count在创建或关闭socket时更新，这里不修改 */
    task_unlock(task);
    
    rcu_read_unlock();
    return ret;
}
