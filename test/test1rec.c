//
// Created by Zhakh on 19.11.2024.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tls.h"

int main() {
    int result;
    unsigned int size = 8192;
    char buffer[4096];
    char read_buffer[4096];

    // Initialize buffer with some data
    memset(buffer, 'A', sizeof(buffer));

    // Create TLS
    result = tls_create(size);
    if (result != 0) {
        printf("tls_create failed with result: %d\n", result);
        return result;
    }

    // Write to TLS
    result = tls_write(0, sizeof(buffer), buffer);
    if (result != 0) {
        printf("tls_write failed with result: %d\n", result);
        return result;
    }

    // Read from TLS to verify the write
    result = tls_read(0, sizeof(read_buffer), read_buffer);
    if (result != 0) {
        printf("tls_read failed with result: %d\n", result);
        return result;
    }

    // Print the contents of both buffers
    printf("Buffer contents: %.*s\n", (int)sizeof(buffer), buffer);
    printf("Read buffer contents: %.*s\n", (int)sizeof(read_buffer), read_buffer);

    // Verify the data
    if (memcmp(buffer, read_buffer, sizeof(buffer)) == 0) {
        printf("tls_write and tls_read test passed.\n");
    } else {
        printf("tls_write and tls_read test failed.\n");
    }

    // Destroy TLS
    result = tls_destroy();
    if (result != 0) {
        printf("tls_destroy failed with result: %d\n", result);
        return result;
    }

    return 0;
}