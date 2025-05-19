#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include "./impl.h"

// Test file path
#define TEST_FILE "test_file.txt"

void setup() {
    // Create a test file
    FILE *f = fopen(TEST_FILE, "w");
    if (f) {
        fprintf(f, "This is a test file for xattr operations\n");
        fclose(f);
    } else {
        printf("Failed to create test file\n");
        exit(1);
    }
}

void cleanup() {
    // Remove test file
    unlink(TEST_FILE);
}

void test_inode_info() {
    printf("Testing get_inode_info()...\n");
    get_inode_info(TEST_FILE);
    printf("Test get_inode_info() completed\n\n");
}

void test_xattr_operations() {
    printf("Testing xattr operations...\n");
    
    // Set an extended attribute
    const char *name = "user.test";
    const char *value = "test_value";
    
    printf("Setting xattr %s=%s\n", name, value);
    int result = set_xattr(TEST_FILE, name, value);
    assert(result == 1);
    
    // List all extended attributes
    printf("Listing all xattrs:\n");
    list_xattrs(TEST_FILE);
    
    // Get the extended attribute
    printf("Getting xattr %s\n", name);
    char *retrieved_value = get_xattr(TEST_FILE, name);
    if (retrieved_value) {
        printf("Got value: %s\n", retrieved_value);
        assert(strcmp(retrieved_value, value) == 0);
        free(retrieved_value);
    } else {
        printf("Failed to get xattr\n");
        assert(0);
    }
    
    // Remove the extended attribute
    printf("Removing xattr %s\n", name);
    result = remove_xattr(TEST_FILE, name);
    assert(result == 1);
    
    // Verify attribute was removed
    printf("Listing xattrs after removal:\n");
    list_xattrs(TEST_FILE);
    
    // Try to get removed attribute
    retrieved_value = get_xattr(TEST_FILE, name);
    assert(retrieved_value == NULL || strlen(retrieved_value) == 0);
    if (retrieved_value) free(retrieved_value);

    printf("Test xattr operations completed\n\n");
}

int main() {
    printf("Starting xattr tests\n");
    
    setup();
    
    test_inode_info();
    test_xattr_operations();
    
    cleanup();
    
    printf("All tests completed successfully\n");
    return 0;
}