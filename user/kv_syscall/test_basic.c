
#include "kv.h"

int main() {
    int key = 1, value = 100;

    if (write_kv(key, value) < 0) {
        perror("write_kv failed");
        return -1;
    }
    printf("Key %d written with value %d\n", key, value);

    int result = read_kv(key);
    if (result < 0) {
        perror("read_kv failed");
        return -1;
    }
    printf("Key %d has value %d\n", key, result);

    return 0;
}