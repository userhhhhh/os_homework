#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <string.h>
#include <sys/xattr.h>

#define SET_XATTR 188
#define READ_KV_SYSCALL 191
#define REMOVE_XATTR 197

int _set_xattr(const char *path, const char *name, const char *value){
    return syscall(SET_XATTR, path, name, value);
};

// Function to get an extended attribute
// If name does not exist, return -1
// If name exists, return 1 and copy value to dst
int _get_xattr(const char *path, const char *name, char *dst){
    return syscall(READ_KV_SYSCALL, path, name, dst);
};

// Function to remove an extended attribute
int _remove_xattr(const char *path, const char *name){
    return syscall(REMOVE_XATTR, path, name);
};

void get_inode_info(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == -1) {
        perror("stat failed");
        return;
    }
    printf("Inode: %lu\n", st.st_ino);
    printf("Mode: %o\n", st.st_mode);
    printf("Links: %lu\n", st.st_nlink);
    printf("UID: %u\n", st.st_uid);
    printf("GID: %u\n", st.st_gid);
    printf("Size: %ld bytes\n", st.st_size);
    printf("Access: %s", ctime(&st.st_atime));
    printf("Modify: %s", ctime(&st.st_mtime));
    printf("Change: %s", ctime(&st.st_ctime));
}

void list_xattrs(const char *filename) {
    // 第2个参数传 NULL，第3个参数传 0，表示只想知道需要多大的缓冲区，而不是实际获取属性名。
    // 返回值 buf_len 就是存放所有属性名所需的字节数
    ssize_t buf_len = listxattr(filename, NULL, 0);
    if (buf_len == -1) {
        perror("listxattr failed");
        return;
    }
    if (buf_len == 0) {
        printf("No xattr found.\n");
        return;
    }
    char *buf = (char *)malloc(buf_len);
    if (!buf) {
        perror("malloc failed");
        return;
    }
    buf_len = listxattr(filename, buf, buf_len);
    if (buf_len == -1) {
        perror("listxattr failed");
        free(buf);
        return;
    }
    char *key = buf;
    while (key < buf + buf_len) {
        printf("%s\n", key);
        key += strlen(key) + 1;
    }
    free(buf);
}

// 获取指定扩展属性的值，返回堆分配的字符串，需调用者 free
char* get_xattr(const char *filename, const char *name) {
    ssize_t value_len = getxattr(filename, name, NULL, 0);
    if (value_len == -1) {
        perror("getxattr failed");
        return NULL;
    }
    char *value = (char *)malloc(value_len + 1);
    if (!value) {
        perror("malloc failed");
        return NULL;
    }
    ssize_t ret = getxattr(filename, name, value, value_len);
    if (ret == -1) {
        perror("getxattr failed");
        free(value);
        return NULL;
    }
    value[value_len] = '\0';
    return value;
}

// 设置扩展属性，成功返回1，失败返回-1
// task：调用 set_xattr 给测试文件 test_file.txt 设置名为 user.test、值为 test_value 的扩展属性。
int set_xattr(const char *path, const char *name, const char *value) {
    int ret = setxattr(path, name, value, strlen(value), 0);
    return (ret == 0) ? 1 : -1;
}

// 移除扩展属性，成功返回1，失败返回-1
// task：调用 remove_xattr 移除测试文件 test_file.txt 的名为 user.test 的扩展属性。
int remove_xattr(const char *path, const char *name) {
    int ret = removexattr(path, name);
    return (ret == 0) ? 1 : -1;
}