
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>

#define KV_STORE_BUCKETS 1024

struct kv_node {
    int key;
    int value;
    struct hlist_node node;
};

SYSCALL_DEFINE2(write_kv, int, key, int, value) {
    struct task_struct *task = current;
    int bucket = key % KV_STORE_BUCKETS;
    struct hlist_head *head = &task->kv_store[bucket];
    struct kv_node *node;
    struct hlist_node *tmp;
    int found = 0;

    spin_lock(&task->kv_locks[bucket]);

    hlist_for_each_entry_safe(node, tmp, head, node) {
        if (node->key == key) {
            node->value = value;
            found = 1;
            break;
        }
    }

    if (!found) {
        node = kmalloc(sizeof(*node), GFP_KERNEL);
        if (!node) {
            printk(KERN_ERR "Failed to allocate memory\n");
            spin_unlock(&task->kv_locks[bucket]);
            return -1;
        }
        node->key = key;
        node->value = value;
        hlist_add_head(&node->node, head);
    }

    spin_unlock(&task->kv_locks[bucket]);
    return sizeof(int) * 2; // 返回写入的字节数
}

SYSCALL_DEFINE1(read_kv, int, key) {
    struct task_struct *task = current;
    int bucket = key % KV_STORE_BUCKETS;
    struct hlist_head *head = &task->kv_store[bucket];
    struct kv_node *node;
    bool found = false;

    spin_lock(&task->kv_locks[bucket]);

    hlist_for_each_entry(node, head, node) {
        if (node->key == key) {
            spin_unlock(&task->kv_locks[bucket]);
            return node->value;
        }
    }

    spin_unlock(&task->kv_locks[bucket]);
    return -ENOENT; 
}

