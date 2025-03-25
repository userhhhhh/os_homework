#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <errno.h>

#ifndef SYS_write_kv
#define SYS_write_kv 449
#endif
#ifndef SYS_read_kv
#define SYS_read_kv  450
#endif

int write_kv(int k, int v) {
    return syscall(SYS_write_kv, k, v);
}

int read_kv(int k) {
    return syscall(SYS_read_kv, k);
}
