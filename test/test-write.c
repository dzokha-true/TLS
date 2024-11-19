//
// Created by Zhakh on 18.11.2024.
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

    memset(buffer, 'A', sizeof(buffer));

    result = tls_create(size);
    if (result != 0) {
        printf("tls_create failed \n");
        return result;
    }
    printf("create has passed \n");

    result = tls_write(0, sizeof(buffer), buffer);
    if (result != 0) {
        printf("tls_write failed \n");
        return result;
    }
    printf("write has passed \n");

    result = tls_read(0, sizeof(read_buffer), read_buffer);
    if (result != 0) {
        printf("tls_read failed \n");
        return result;
    }
    printf("read has passed \n\n\n");

    printf("Buffer contents: %.*s\n", (int)sizeof(buffer), buffer);
    printf("Read buffer contents: %.*s\n", (int)sizeof(read_buffer), read_buffer);

    if (memcmp(buffer, read_buffer, sizeof(buffer)) == 0) {
        printf("tls_write and tls_read test passed.\n");
    } else {
        printf("tls_write and tls_read test failed.\n");
    }

    return 0;
}
