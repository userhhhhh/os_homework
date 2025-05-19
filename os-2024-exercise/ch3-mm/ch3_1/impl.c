#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

/**
 * @brief 重新映射一块虚拟内存区域
 * @param addr 原始映射的内存地址，如果为 NULL 则由系统自动选择一个合适的地址
 * @param size 需要映射的大小（单位：字节）
 * @return 成功返回映射的地址，失败返回 NULL
 * @details 该函数用于重新映射一个新的虚拟内存区域。如果 addr 参数为 NULL，
 *          系统会自动选择一个合适的地址进行映射。映射的内存区域大小为 size 字节。
 *          映射失败时返回 NULL。
 */

// 理解1：在相同的虚拟地址 addr 上重新映射一块新的 size 大小的物理内存。
// 理解2：在新的虚拟地址 new_addr 上重新映射一块新的 size 大小的物理内存，内容与 addr 的一样。（代码按照这个写的）
void* mmap_remap(void *addr, size_t size) {
    // TODO: TASK1
    // return NULL; // 需要返回映射的地址
    // if(addr != NULL) {
    //     // munmap：解除映射，释放虚拟内存。
    //     if (munmap(addr, size) == -1) {
    //         perror("munmap failed");
    //         return NULL;
    //     }
    // }
    void* new_addr = mmap(addr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_addr == MAP_FAILED) {
        perror("mmap failed");
        return NULL;
    }
    // if(new_addr != addr && addr != NULL) {
    //     printf("Warning: mmap returned a different address than expected.\n");
    // }
    memcpy(new_addr, addr, size);
    return new_addr;
}

/**
 * @brief 使用 mmap 进行文件读写
 * @param filename 待操作的文件路径
 * @param offset 写入文件的偏移量（单位：字节）
 * @param content 要写入文件的内容
 * @return 成功返回 0，失败返回 -1
 * @details 该函数使用内存映射（mmap）的方式进行文件写入操作。
 *          通过 filename 指定要写入的文件，
 *          offset 指定写入的起始位置，
 *          content 指定要写入的内容。
 *          写入成功返回 0，失败返回 -1。
 */
int file_mmap_write(const char* filename, size_t offset, char* content) {
    // TODO: TASK2
    // return -1; // 需要返回正确的执行状态

    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        perror("open failed");
        return -1;
    }

    // fstat：系统调用，用于获取 fd 所指文件的状态信息，将获取的文件信息存入st
    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("fstat failed");
        close(fd);
        return -1;
    }
    
    size_t content_len = strlen(content);
    if (offset + content_len > st.st_size) {
        // ftruncate：系统调用，用于更改文件大小。
        if (ftruncate(fd, offset + content_len) == -1) {
            perror("ftruncate failed");
            close(fd);
            return -1;
        }
    }

    /**
    * @brief 使用 mmap 映射文件
    * @param addr   映射的建议起始地址，NULL 让内核选择映射地址。
    * @param length 映射的长度。
    * @param prot	访问权限，如 PROT_READ | PROT_WRITE 指映射区域可读写
    * @param flags	映射类型，如 MAP_SHARED 或 MAP_PRIVATE，MAP_SHARED 指写入会同步到文件
    * @param fd 被映射的文件描述符
    * @param offset	映射文件的起始偏移（必须是页大小的整数倍），0 指映射从文件起始偏移
    * @return 成功返回 0，失败返回 -1
    * @details void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset);
    *          mmap 可以将文件映射到当前进程的虚拟地址空间中。像数组一样读写，
    *          不再需要反复调用 read() 或 write()。避免了多次内核态与用户态切换。
    */
    void* map = mmap(NULL, offset + content_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        return -1;
    }

    memcpy((char*)map + offset, content, content_len);

    // msync：将映射区域的修改同步到磁盘。length 表示同步的长度，MS_SYNC 表示阻塞直到写入完成。
    if (msync(map, offset + content_len, MS_SYNC) == -1) {
        perror("msync failed");
        munmap(map, offset + content_len);
        close(fd);
        return -1;
    }

    // munmap：解除映射，释放虚拟内存。
    if (munmap(map, offset + content_len) == -1) {
        perror("munmap failed");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}
