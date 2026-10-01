/* Resource accounting and fault injection stay in this test translation unit. */
#include "TLS.h"
#include <signal.h>
#include <sys/wait.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)

static int allocation_fail, mapping_fail, protection_fail;
static int allocations, mappings;

static void *checked_calloc(size_t count, size_t size)
{
    if (allocation_fail && --allocation_fail == 0) return NULL;
    void *result = calloc(count, size);
    if (result) allocations++;
    return result;
}

static void checked_free(void *value)
{
    if (value) allocations--;
    free(value);
}

static void *checked_mmap(void *address, size_t size, int prot, int flags, int fd, off_t offset)
{
    if (mapping_fail && --mapping_fail == 0) { errno = ENOMEM; return MAP_FAILED; }
    void *result = mmap(address, size, prot, flags, fd, offset);
    if (result != MAP_FAILED) mappings++;
    return result;
}

static int checked_munmap(void *address, size_t size)
{
    int result = munmap(address, size);
    if (result == 0) mappings--;
    return result;
}

static int checked_mprotect(void *address, size_t size, int prot)
{
    if (protection_fail && --protection_fail == 0) { errno = EACCES; return -1; }
    return mprotect(address, size, prot);
}

#define calloc checked_calloc
#define free checked_free
#define mmap checked_mmap
#define munmap checked_munmap
#define mprotect checked_mprotect
#include "../TLS.c"
#undef calloc
#undef free
#undef mmap
#undef munmap
#undef mprotect

static void no_resources(void)
{
    CHECK(allocations == 0 && mappings == 0);
    for (int i = 0; i < TLS_CAPACITY; i++) CHECK(tls_array[i] == NULL);
}

/* Check actual protection by probing a mapping in an isolated child. */
static void fault_handler(int signal_number)
{
    (void)signal_number;
    _exit(99);
}

static void protected_page(struct page *page)
{
    pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        signal(SIGSEGV, fault_handler);
        signal(SIGBUS, fault_handler);
        volatile unsigned char byte = *(volatile unsigned char *)page->address;
        (void)byte;
        _exit(1);
    }
    int status;
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 99);
}

static pthread_t owner;

static void *clone_failure_worker(void *arg)
{
    (void)arg;
    int original_allocations = allocations, original_mappings = mappings;
    for (int i = 1; i <= 2; i++) {
        allocation_fail = i;
        CHECK(tls_clone(owner) == -1);
        CHECK(allocations == original_allocations && mappings == original_mappings);
    }
    CHECK(tls_clone(owner) == 0);
    int slot = find_thread(pthread_self());
    struct page *shared = tls_array[slot]->pages[0];
    char byte = 'B', readback;
    allocation_fail = 1;
    CHECK(tls_write(0, 1, &byte) == -1);
    CHECK(tls_array[slot]->pages[0] == shared && shared->ref_count == 2);
    mapping_fail = 1;
    CHECK(tls_write(0, 1, &byte) == -1);
    CHECK(tls_array[slot]->pages[0] == shared && shared->ref_count == 2);
    protection_fail = 1;
    CHECK(tls_write(0, 1, &byte) == -1);
    CHECK(tls_array[slot]->pages[0] == shared && shared->ref_count == 2);
    CHECK(tls_read(0, 1, &readback) == 0 && readback == 'A');
    protected_page(shared);
    CHECK(tls_write(0, 1, &byte) == 0);
    CHECK(tls_array[slot]->pages[0] != shared);
    protected_page(tls_array[slot]->pages[0]);
    CHECK(tls_destroy() == 0);
    CHECK(allocations == original_allocations && mappings == original_mappings);
    return NULL;
}

int main(void)
{
    unsigned int size = (unsigned int)sysconf(_SC_PAGESIZE) * 3;
    /* TLS metadata, pointer array, and three page descriptors. */
    for (int i = 1; i <= 5; i++) {
        allocation_fail = i;
        CHECK(tls_create(size) == -1);
        no_resources();
    }
    for (int i = 1; i <= 3; i++) {
        mapping_fail = i;
        CHECK(tls_create(size) == -1);
        no_resources();
    }
    CHECK(tls_create(size) == 0);
    int slot = find_thread(pthread_self());
    for (unsigned int i = 0; i < 3; i++) protected_page(tls_array[slot]->pages[i]);
    char byte = 'A', readback = '!';
    protection_fail = 1;
    CHECK(tls_write(0, 1, &byte) == -1);
    protection_fail = 1;
    CHECK(tls_read(0, 1, &readback) == -1 && readback == '!');
    CHECK(tls_write(0, 1, &byte) == 0);
    protected_page(tls_array[slot]->pages[0]);
    owner = pthread_self();
    pthread_t clone;
    CHECK(pthread_create(&clone, NULL, clone_failure_worker, NULL) == 0);
    CHECK(pthread_join(clone, NULL) == 0);
    CHECK(tls_read(0, 1, &readback) == 0 && readback == 'A');
    protected_page(tls_array[slot]->pages[0]);
    CHECK(tls_destroy() == 0);
    no_resources();
    puts("PASS resource rollback and page protection");
    return 0;
}
