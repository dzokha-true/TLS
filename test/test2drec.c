//
// Created by Zhakh on 19.11.2024.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tls.h"

int main() {
    int result;
    unsigned int size = 8192; // 2 pages
    char buffer[4096];
    char read_buffer[2];

    memset(buffer, 'A', sizeof(buffer));
    buffer[4096] = 'B';

    result = tls_create(size);
    if (result != 0) {
        printf("tls_create failed with result: %d\n", result);
        return result;
    }

    result = tls_write(1, sizeof(buffer), buffer);
    if (result != 0) {
        printf("tls_write failed with result: %d\n", result);
        return result;
    }

    result = tls_read(4095, 2, read_buffer);
    if (result != 0) {
        printf("tls_read failed with result: %d\n", result);
        return result;
    }

    printf("Read buffer contents: %s\n", read_buffer);

    if (read_buffer[0] == 'A' && read_buffer[1] == 'B') {
        printf("tls_read test passed.\n");
    } else {
        printf("tls_read test failed.\n");
    }

    // Destroy TLS
    result = tls_destroy();
    if (result != 0) {
        printf("tls_destroy failed with result: %d\n", result);
        return result;
    }

    return 0;
}