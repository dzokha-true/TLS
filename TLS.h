//
// Created by Zhakh on 15.11.2024.
//

#ifndef TLS_TLS_H
#define TLS_TLS_H

#include <sys/mman.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

int tls_create(unsigned int size);
int tls_write(unsigned int offset, unsigned int length, char *buffer);
int tls_read(unsigned int offset, unsigned int length, char *buffer);
int tls_destroy();
int tls_clone(pthread_t tid);

#endif //TLS_TLS_H