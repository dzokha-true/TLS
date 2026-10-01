// Created by Zhakhangir on 12.11.2024.
#include "TLS.h"

#include <limits.h>

#define TLS_CAPACITY 128

struct page {
    void *address;
    unsigned int ref_count;
};

typedef struct thread_local_storage {
    pthread_t tid;
    unsigned int size;
    unsigned int page_num;
    struct page **pages;
} TLS;

/* The lock covers metadata and every temporary page-protection change. */
static TLS *tls_array[TLS_CAPACITY];
static pthread_mutex_t tls_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t tls_once = PTHREAD_ONCE_INIT;
static pthread_key_t tls_key;
static size_t page_size;
static int init_error;

static void thread_cleanup(void *value);

static void initialize(void)
{
    long size = sysconf(_SC_PAGESIZE);
    if (size <= 0 || (unsigned long)size > UINT_MAX) {
        init_error = 1;
        return;
    }
    page_size = (size_t)size;
    init_error = pthread_key_create(&tls_key, thread_cleanup);
}

/* Cancellation must not strand the mutex or leave a page accessible. */
static int enter(int *old_state)
{
    if (pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, old_state) != 0)
        return -1;
    if (pthread_once(&tls_once, initialize) != 0 || init_error ||
        pthread_mutex_lock(&tls_lock) != 0) {
        pthread_setcancelstate(*old_state, NULL);
        return -1;
    }
    return 0;
}

static int leave(int old_state, int result)
{
    pthread_mutex_unlock(&tls_lock);
    pthread_setcancelstate(old_state, NULL);
    return result;
}

static int find_thread(pthread_t tid)
{
    for (int i = 0; i < TLS_CAPACITY; i++)
        if (tls_array[i] && pthread_equal(tls_array[i]->tid, tid))
            return i;
    return -1;
}

static int find_free(void)
{
    for (int i = 0; i < TLS_CAPACITY; i++)
        if (!tls_array[i]) return i;
    return -1;
}

static void protect(struct page *page)
{
    /* Returning with an accessible page would break the protection contract. */
    if (mprotect(page->address, page_size, PROT_NONE) != 0) abort();
}

static void release_page(struct page *page)
{
    if (--page->ref_count == 0) {
        if (munmap(page->address, page_size) != 0) abort();
        free(page);
    }
}

static void release_tls(TLS *tls)
{
    for (unsigned int i = 0; i < tls->page_num; i++)
        if (tls->pages[i]) release_page(tls->pages[i]);
    free(tls->pages);
    free(tls);
}

static void thread_cleanup(void *value)
{
    int old_state;
    if (enter(&old_state) != 0) abort();
    for (int i = 0; i < TLS_CAPACITY; i++) {
        if (tls_array[i] == value) {
            tls_array[i] = NULL;
            release_tls(value);
            break;
        }
    }
    leave(old_state, 0);
}

static struct page *allocate_page(int protection)
{
    struct page *page = calloc(1, sizeof(*page));
    if (!page) return NULL;
    page->address = mmap(NULL, page_size, protection,
                         MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (page->address == MAP_FAILED) {
        free(page);
        return NULL;
    }
    page->ref_count = 1;
    return page;
}

static TLS *allocate_tls(unsigned int size, unsigned int count)
{
    TLS *tls = calloc(1, sizeof(*tls));
    if (!tls) return NULL;
    tls->pages = calloc(count, sizeof(*tls->pages));
    if (!tls->pages) {
        free(tls);
        return NULL;
    }
    tls->tid = pthread_self();
    tls->size = size;
    tls->page_num = count;
    return tls;
}

int tls_create(unsigned int size)
{
    int old_state, slot;
    if (size == 0 || enter(&old_state) != 0) return -1;
    if (find_thread(pthread_self()) != -1 || (slot = find_free()) == -1)
        return leave(old_state, -1);
    unsigned int count = size / page_size + (size % page_size != 0);
    TLS *tls = allocate_tls(size, count);
    if (!tls) return leave(old_state, -1);
    for (unsigned int i = 0; i < count; i++) {
        /* Anonymous mappings are already zero-filled. */
        tls->pages[i] = allocate_page(PROT_NONE);
        if (!tls->pages[i]) {
            release_tls(tls);
            return leave(old_state, -1);
        }
    }
    if (pthread_setspecific(tls_key, tls) != 0) {
        release_tls(tls);
        return leave(old_state, -1);
    }
    tls_array[slot] = tls;
    return leave(old_state, 0);
}

static int valid_range(TLS *tls, unsigned int offset, unsigned int length,
                       const char *buffer)
{
    return offset <= tls->size && length <= tls->size - offset &&
           (length == 0 || buffer != NULL);
}

static struct page *copy_page(struct page *source)
{
    struct page *copy = allocate_page(PROT_READ | PROT_WRITE);
    if (!copy) return NULL;
    if (mprotect(source->address, page_size, PROT_READ) != 0) {
        release_page(copy);
        return NULL;
    }
    memcpy(copy->address, source->address, page_size);
    protect(source);
    protect(copy);
    return copy;
}

int tls_write(unsigned int offset, unsigned int length, char *buffer)
{
    int old_state, slot;
    if (enter(&old_state) != 0) return -1;
    slot = find_thread(pthread_self());
    if (slot == -1 || !valid_range(tls_array[slot], offset, length, buffer))
        return leave(old_state, -1);
    TLS *tls = tls_array[slot];
    while (length) {
        unsigned int index = offset / page_size;
        size_t within = offset % page_size;
        size_t count = page_size - within;
        if (count > length) count = length;
        struct page *page = tls->pages[index];
        if (page->ref_count > 1) {
            struct page *copy = copy_page(page);
            if (!copy) return leave(old_state, -1);
            page->ref_count--;
            tls->pages[index] = page = copy;
        }
        if (mprotect(page->address, page_size, PROT_READ | PROT_WRITE) != 0)
            return leave(old_state, -1);
        memcpy((char *)page->address + within, buffer, count);
        protect(page);
        buffer += count;
        offset += (unsigned int)count;
        length -= (unsigned int)count;
    }
    return leave(old_state, 0);
}

int tls_read(unsigned int offset, unsigned int length, char *buffer)
{
    int old_state, slot;
    if (enter(&old_state) != 0) return -1;
    slot = find_thread(pthread_self());
    if (slot == -1 || !valid_range(tls_array[slot], offset, length, buffer))
        return leave(old_state, -1);
    TLS *tls = tls_array[slot];
    while (length) {
        unsigned int index = offset / page_size;
        size_t within = offset % page_size;
        size_t count = page_size - within;
        if (count > length) count = length;
        struct page *page = tls->pages[index];
        if (mprotect(page->address, page_size, PROT_READ) != 0)
            return leave(old_state, -1);
        memcpy(buffer, (char *)page->address + within, count);
        protect(page);
        buffer += count;
        offset += (unsigned int)count;
        length -= (unsigned int)count;
    }
    return leave(old_state, 0);
}

int tls_clone(pthread_t tid)
{
    int old_state, source_slot, slot;
    if (enter(&old_state) != 0) return -1;
    if (find_thread(pthread_self()) != -1 ||
        (source_slot = find_thread(tid)) == -1 || (slot = find_free()) == -1)
        return leave(old_state, -1);
    TLS *source = tls_array[source_slot];
    TLS *tls = allocate_tls(source->size, source->page_num);
    if (!tls) return leave(old_state, -1);
    if (pthread_setspecific(tls_key, tls) != 0) {
        release_tls(tls);
        return leave(old_state, -1);
    }
    for (unsigned int i = 0; i < tls->page_num; i++) {
        tls->pages[i] = source->pages[i];
        tls->pages[i]->ref_count++;
    }
    tls_array[slot] = tls;
    return leave(old_state, 0);
}

int tls_destroy(void)
{
    int old_state, slot;
    if (enter(&old_state) != 0) return -1;
    if ((slot = find_thread(pthread_self())) == -1 ||
        pthread_setspecific(tls_key, NULL) != 0)
        return leave(old_state, -1);
    TLS *tls = tls_array[slot];
    tls_array[slot] = NULL;
    release_tls(tls);
    return leave(old_state, 0);
}
