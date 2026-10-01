//
// Created by Zhakh on 15.11.2024.
//

#ifndef TLS_TLS_H
#define TLS_TLS_H

/* Legacy client constant; the implementation uses sysconf(_SC_PAGESIZE). */
#define PAGESIZE 4096

#include <sys/mman.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <stdbool.h>

/* Returns 0 on success, -1 on invalid input or recoverable resource failure.
 * Each thread owns at most one TLS; size must be nonzero. Clones initially
 * share pages, then detach touched pages on write. All API calls are serialized.
 * A zero-length read/write permits NULL when offset <= size.
 * Buffers for nonzero transfers must address at least length valid bytes.
 * Destroy and thread exit release the caller's references and registry slot.
 * See README.md for protection, failure and concurrency limits. */
int tls_create(unsigned int size);
int tls_write(unsigned int offset, unsigned int length, char *buffer);
int tls_read(unsigned int offset, unsigned int length, char *buffer);
int tls_destroy(void);
int tls_clone(pthread_t tid);

#endif //TLS_TLS_H
