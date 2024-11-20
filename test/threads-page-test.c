// Created by Zhakh on 20.11.2024.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "tls.h"

void* thread1_test(void *arg) {
    /*
      Thread 1: Creates a TLS, writes half a page to the first page, and fills the second page.
      Expected output: The read buffer should contain 'A' for the first half page and 'B' for the second page.
    */

    int result;
    char buffer1[2048], buffer2[4096], read_buffer[4096+2048];
    memset(buffer1, 'A', sizeof(buffer1));
    memset(buffer2, 'B', sizeof(buffer2));

    result = tls_create(2*4096);
    if (result != 0) {
        printf("tls_create failed with result: %d\n", result);
        return NULL;
    }

    result = tls_write(2048, sizeof(buffer1), buffer1);
    if (result != 0) {
        printf("tls_write failed with result: %d\n", result);
        return NULL;
    }

    result = tls_write(4096, sizeof(buffer2), buffer2);
    if (result != 0) {
        printf("tls_write failed with result: %d\n", result);
        return NULL;
    }

    result = tls_read(2048, sizeof(read_buffer), read_buffer);
    if (result != 0) {
        printf("tls_read failed with result: %d\n", result);
        return NULL;
    }

    printf("Thread 1 Read buffer contents: %s\n", read_buffer);
    return NULL;
}

void* thread2_test(void *arg) {
    /*
     Thread 2: Creates a TLS, writes to the whole first page, and half of the second page.
     Expected output: The read buffer should contain 'C' for the first page and 'D' for half of the second page.
    */
    int result;
    char buffer1[4096], buffer2[2048], read_buffer[4096+2048];
    memset(buffer1, 'C', sizeof(buffer1));
    memset(buffer2, 'D', sizeof(buffer2));

    result = tls_create(2*4096);
    if (result != 0) {
        printf("tls_create failed with result: %d\n", result);
        return NULL;
    }

    result = tls_write(0, sizeof(buffer1), buffer1);
    if (result != 0) {
        printf("tls_write failed with result: %d\n", result);
        return NULL;
    }

    result = tls_write(4096, sizeof(buffer2), buffer2);
    if (result != 0) {
        printf("tls_write failed with result: %d\n", result);
        return NULL;
    }

    result = tls_read(0, sizeof(read_buffer), read_buffer);
    if (result != 0) {
        printf("tls_read failed with result: %d\n", result);
        return NULL;
    }

    printf("Thread 2 Read buffer contents: %s\n", read_buffer);
    return NULL;
}

void* thread3_test(void *arg) {
    /*
     Thread 3: Creates a TLS, writes half a page to the first page, full pages to the second and third pages, and half of the fourth page.
     Expected output: The read buffer should contain 'E' for half of the first page, 'F' for the second page, 'G' for the third page, and 'H' for half of the fourth page.
    */
    int result;
    char buffer1[2048], buffer2[4096], buffer3[4096], buffer4[2048], read_buffer[3*4096];
    memset(buffer1, 'E', sizeof(buffer1));
    memset(buffer2, 'F', sizeof(buffer2));
    memset(buffer3, 'G', sizeof(buffer3));
    memset(buffer4, 'H', sizeof(buffer4));

    result = tls_create(4*4096);
    if (result != 0) {
        printf("tls_create failed with result: %d\n", result);
        return NULL;
    }

    result = tls_write(0, sizeof(buffer1), buffer1);
    if (result != 0) {
        printf("tls_write failed with result: %d\n", result);
        return NULL;
    }

    result = tls_write(4096, sizeof(buffer2), buffer2);
    if (result != 0) {
        printf("tls_write failed with result: %d\n", result);
        return NULL;
    }

    result = tls_write(8192, sizeof(buffer3), buffer3);
    if (result != 0) {
        printf("tls_write failed with result: %d\n", result);
        return NULL;
    }

    result = tls_write(12288, sizeof(buffer4), buffer4);
    if (result != 0) {
        printf("tls_write failed with result: %d\n", result);
        return NULL;
    }

    result = tls_read(2048, sizeof(read_buffer), read_buffer);
    if (result != 0) {
        printf("tls_read failed with result: %d\n", result);
        return NULL;
    }

    printf("Thread 3 Read buffer contents: %s\n", read_buffer);
    return NULL;
}

void* thread4_test(void *arg) {
    /*
     Thread 4: Creates a TLS, writes half a page to the first page, and full pages to the second and third pages.
     Expected output: The read buffer should contain 'I' for half of the first page, 'J' for the second page, and 'K' for the third page.
    */
    int result;
    char buffer1[2048], buffer2[4096], buffer3[4096], read_buffer[12288];
    memset(buffer1, 'I', sizeof(buffer1));
    memset(buffer2, 'J', sizeof(buffer2));
    memset(buffer3, 'K', sizeof(buffer3));

    result = tls_create(3*4096);
    if (result != 0) {
        printf("tls_create failed with result: %d\n", result);
        return NULL;
    }

    result = tls_write(0, sizeof(buffer1), buffer1);
    if (result != 0) {
        printf("tls_write failed with result: %d\n", result);
        return NULL;
    }

    result = tls_write(4096, sizeof(buffer2), buffer2);
    if (result != 0) {
        printf("tls_write failed with result: %d\n", result);
        return NULL;
    }

    result = tls_write(8192, sizeof(buffer3), buffer3);
    if (result != 0) {
        printf("tls_write failed with result: %d\n", result);
        return NULL;
    }

    result = tls_read(2048, sizeof(read_buffer), read_buffer);
    if (result != 0) {
        printf("tls_read failed with result: %d\n", result);
        return NULL;
    }

    printf("Thread 4 Read buffer contents: %s\n", read_buffer);
    return NULL;
}

void* thread5_test(void *arg) {
    /*
     Thread 5: Creates a TLS, writes to the first two pages, and half of the third page.
     Expected output: The read buffer should contain 'L' for the first page, 'M' for the second page, and 'N' for half of the third page.
    */
    int result;
    char buffer1[4096], buffer2[4096], buffer3[2048], read_buffer[2*4096+2048];
    memset(buffer1, 'L', sizeof(buffer1));
    memset(buffer2, 'M', sizeof(buffer2));
    memset(buffer3, 'N', sizeof(buffer3));

    result = tls_create(3*4096);
    if (result != 0) {
        printf("tls_create failed with result: %d\n", result);
        return NULL;
    }

    result = tls_write(0, sizeof(buffer1), buffer1);
    if (result != 0) {
        printf("tls_write failed with result: %d\n", result);
        return NULL;
    }

    result = tls_write(4096, sizeof(buffer2), buffer2);
    if (result != 0) {
        printf("tls_write failed with result: %d\n", result);
        return NULL;
    }

    result = tls_write(8192, sizeof(buffer3), buffer3);
    if (result != 0) {
        printf("tls_write failed with result: %d\n", result);
        return NULL;
    }

    result = tls_read(0, sizeof(read_buffer), read_buffer);
    if (result != 0) {
        printf("tls_read failed with result: %d\n", result);
        return NULL;
    }

    printf("Thread 5 Read buffer contents: %s\n", read_buffer);
    return NULL;
}

void* thread6_test(void *arg) {
    /*
     Thread 6: Creates a TLS, writes half a page to the first page, and half to the second page.
     Expected output: The read buffer should contain 'O' for half of the first page and 'P' for half of the second page.
    */
    int result;
    char buffer1[2048], buffer2[2048], read_buffer[8192];
    memset(buffer1, 'O', sizeof(buffer1));
    memset(buffer2, 'P', sizeof(buffer2));

    result = tls_create(8192);
    if (result != 0) {
        printf("tls_create failed with result: %d\n", result);
        return NULL;
    }

    result = tls_write(2048, sizeof(buffer1), buffer1);
    if (result != 0) {
        printf("tls_write failed with result: %d\n", result);
        return NULL;
    }

    result = tls_write(4096, sizeof(buffer2), buffer2);
    if (result != 0) {
        printf("tls_write failed with result: %d\n", result);
        return NULL;
    }

    result = tls_read(2048, sizeof(read_buffer), read_buffer);
    if (result != 0) {
        printf("tls_read failed with result: %d\n", result);
        return NULL;
    }

    printf("Thread 6 Read buffer contents: %s\n", read_buffer);
    return NULL;
}

void* thread7_test(void *arg) {
    /*
      Thread 7: Creates a TLS, writes to two pages.
      Expected output: The read buffer should contain 'Q' for the first page and 'R' for the second page.
    */
    int result;
    char buffer1[4096], buffer2[4096], read_buffer[8192];
    memset(buffer1, 'Q', sizeof(buffer1));
    memset(buffer2, 'R', sizeof(buffer2));

    result = tls_create(8192);
    if (result != 0) {
        printf("tls_create failed with result: %d\n", result);
        return NULL;
    }

    result = tls_write(0, sizeof(buffer1), buffer1);
    if (result != 0) {
        printf("tls_write failed with result: %d\n", result);
        return NULL;
    }

    result = tls_write(4096, sizeof(buffer2), buffer2);
    if (result != 0) {
        printf("tls_write failed with result: %d\n", result);
        return NULL;
    }

    result = tls_read(0, sizeof(read_buffer), read_buffer);
    if (result != 0) {
        printf("tls_read failed with result: %d\n", result);
        return NULL;
    }

    printf("Thread 7 Read buffer contents: %s\n", read_buffer);
    return NULL;
}

void* thread8_test(void *arg) {
    /*
      Thread 8: Creates a TLS, writes half a page to the first page, and full pages to the second and third pages.
      Expected output: The read buffer should contain 'S' for half of the first page, 'T' for the second page, and 'U' for the third page.
    */
    int result;
    char buffer1[2048], buffer2[4096], buffer3[4096], read_buffer[12288];
    memset(buffer1, 'S', sizeof(buffer1));
    memset(buffer2, 'T', sizeof(buffer2));
    memset(buffer3, 'U', sizeof(buffer3));

    result = tls_create(4096*3);
    if (result != 0) {
        printf("tls_create failed with result: %d\n", result);
        return NULL;
    }

    result = tls_write(2048, sizeof(buffer1), buffer1);
    if (result != 0) {
        printf("tls_write failed with result: %d\n", result);
        return NULL;
    }

    result = tls_write(4096, sizeof(buffer2), buffer2);
    if (result != 0) {
        printf("tls_write failed with result: %d\n", result);
        return NULL;
    }

    result = tls_write(8192, sizeof(buffer3), buffer3);
    if (result != 0) {
        printf("tls_write failed with result: %d\n", result);
        return NULL;
    }

    result = tls_read(2048, sizeof(read_buffer), read_buffer);
    if (result != 0) {
        printf("tls_read failed with result: %d\n", result);
        return NULL;
    }

    printf("Thread 8 Read buffer contents: %s\n", read_buffer);
    return NULL;
}


void* thread9_test(void *arg) {
    /*
      Thread 9: Creates a TLS, writes to two whole pages.
      Expected output: The read buffer should contain 'V' for the first page and 'W' for the second page.
    */
    int result;
    char buffer1[4096], buffer2[4096], read_buffer[8192];
    memset(buffer1, 'V', sizeof(buffer1));
    memset(buffer2, 'W', sizeof(buffer2));

    result = tls_create(8192);
    if (result != 0) {
        printf("tls_create failed with result: %d\n", result);
        return NULL;
    }

    result = tls_write(0, sizeof(buffer1), buffer1);
    if (result != 0) {
        printf("tls_write failed with result: %d\n", result);
        return NULL;
    }

    result = tls_write(4096, sizeof(buffer2), buffer2);
    if (result != 0) {
        printf("tls_write failed with result: %d\n", result);
        return NULL;
    }

    result = tls_read(0, sizeof(read_buffer), read_buffer);
    if (result != 0) {
        printf("tls_read failed with result: %d\n", result);
        return NULL;
    }

    printf("Thread 9 Read buffer contents: %s\n", read_buffer);
    return NULL;
}

int main() {
    pthread_t thread1, thread2, thread3, thread4, thread5, thread6, thread7, thread8, thread9;

    pthread_create(&thread1, NULL, thread1_test, NULL);
    printf("\n\n");
    pthread_create(&thread2, NULL, thread2_test, NULL);
    printf("\n\n");
    pthread_create(&thread3, NULL, thread3_test, NULL);
    printf("\n\n");
    pthread_create(&thread4, NULL, thread4_test, NULL);
    printf("\n\n");
    pthread_create(&thread5, NULL, thread5_test, NULL);
    printf("\n\n");
    pthread_create(&thread6, NULL, thread6_test, NULL);
    printf("\n\n");
    pthread_create(&thread7, NULL, thread7_test, NULL);
    printf("\n\n");
    pthread_create(&thread8, NULL, thread8_test, NULL);
    printf("\n\n");
    pthread_create(&thread9, NULL, thread9_test, NULL);
    printf("\n\n");

    pthread_join(thread1, NULL);
    printf("thread1: \n\n");
    pthread_join(thread2, NULL);
    printf("thread2: \n\n");
    pthread_join(thread3, NULL);
    printf("thread3: \n\n");
    pthread_join(thread4, NULL);
    printf("thread4: \n\n");
    pthread_join(thread5, NULL);
    printf("thread5: \n\n");
    pthread_join(thread6, NULL);
    printf("thread6: \n\n");
    pthread_join(thread7, NULL);
    printf("thread7: \n\n");
    pthread_join(thread8, NULL);
    printf("thread8: \n\n");
    pthread_join(thread9, NULL);
    printf("thread9: \n\n");

    return 0;
}