//
// Created by Zhakh on 18.11.2024.
//
#include <stdio.h>
#include <stdlib.h>
#include "tls.h"

int main() {
    int result = 0;
    unsigned int size = 10;
    result = tls_create(size);
    printf("result is: %d", result);
    return result;
}

