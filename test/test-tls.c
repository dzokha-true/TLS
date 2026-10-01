#include "TLS.h"

#include <limits.h>
#include <sys/wait.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)

static unsigned int page_size(void) { return (unsigned int)sysconf(_SC_PAGESIZE); }

/* These cases catch missing-TLS dereferences and wrapping range arithmetic. */
static void errors(void)
{
    char c = 'x';
    CHECK(tls_read(0, 1, &c) == -1);
    CHECK(tls_write(0, 1, &c) == -1);
    CHECK(tls_destroy() == -1);
    CHECK(tls_clone(pthread_self()) == -1);
    CHECK(tls_create(0) == -1);
    CHECK(tls_create(17) == 0);
    CHECK(tls_create(17) == -1);
    CHECK(tls_clone(pthread_self()) == -1);
    CHECK(tls_read(UINT_MAX, 2, &c) == -1);
    CHECK(tls_write(1, UINT_MAX, &c) == -1);
    CHECK(tls_write(17, 1, &c) == -1);
    CHECK(tls_read(18, 0, NULL) == -1);
    CHECK(tls_read(0, 1, NULL) == -1);
    CHECK(tls_write(0, 1, NULL) == -1);
    CHECK(tls_read(17, 0, NULL) == 0);
    CHECK(tls_write(17, 0, NULL) == 0);
    CHECK(c == 'x');
    CHECK(tls_destroy() == 0);
}

/* A one-byte buffer must never be copied to the end of a page. */
static void short_spans(void)
{
    char c = 'x', result[3] = {'!', '!', '!'};
    CHECK(tls_create(19) == 0);
    CHECK(tls_write(3, 1, &c) == 0);
    CHECK(tls_read(3, 1, result + 1) == 0);
    CHECK(memcmp(result, "!x!", 3) == 0);
    CHECK(tls_read(2, 3, result) == 0);
    CHECK(result[0] == 0 && result[1] == 'x' && result[2] == 0);
    CHECK(tls_destroy() == 0);
}

/* Full pages followed by a partial page used to skip the final page. */
static void boundaries(void)
{
    unsigned int p = page_size(), size = 3 * p + 11;
    char *input = malloc(size), *output = malloc(size), *expected = calloc(size, 1);
    const unsigned int offsets[] = {0, 1, p - 1, p, p + 1, 3 * p + 10};
    const unsigned int lengths[] = {1, p - 1, p, p + 1, 2 * p + 7};
    CHECK(input && output && expected);
    for (unsigned int i = 0; i < size; i++) input[i] = (char)(i % 251 + 1);
    CHECK(tls_create(size) == 0);
    CHECK(tls_read(0, size, output) == 0);
    CHECK(memcmp(output, expected, size) == 0);
    for (size_t o = 0; o < sizeof(offsets) / sizeof(offsets[0]); o++) {
        for (size_t l = 0; l < sizeof(lengths) / sizeof(lengths[0]); l++) {
            unsigned int offset = offsets[o], length = lengths[l];
            if (length > size - offset) continue;
            CHECK(tls_write(offset, length, input) == 0);
            memcpy(expected + offset, input, length);
            CHECK(tls_read(0, size, output) == 0);
            CHECK(memcmp(output, expected, size) == 0);
            memset(output, '!', size);
            CHECK(tls_read(offset, length, output + 1) == 0);
            CHECK(output[0] == '!' && output[length + 1] == '!');
            CHECK(memcmp(output + 1, input, length) == 0);
        }
    }
    CHECK(tls_destroy() == 0);
    free(input); free(output); free(expected);
}

static void lifecycle(void)
{
    for (int i = 0; i < 300; i++) {
        char c = '!', result = '!';
        CHECK(tls_create(1) == 0);
        CHECK(tls_read(0, 1, &result) == 0 && result == 0);
        CHECK(tls_write(0, 1, &c) == 0);
        CHECK(tls_destroy() == 0);
        CHECK(tls_destroy() == -1);
    }
}

static pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static int arrived, released;
static pthread_t owner;

static void rendezvous(void)
{
    CHECK(pthread_mutex_lock(&gate) == 0);
    arrived++;
    CHECK(pthread_cond_broadcast(&changed) == 0);
    while (!released) CHECK(pthread_cond_wait(&changed, &gate) == 0);
    CHECK(pthread_mutex_unlock(&gate) == 0);
}

static void wait_arrivals(int count)
{
    CHECK(pthread_mutex_lock(&gate) == 0);
    while (arrived < count) CHECK(pthread_cond_wait(&changed, &gate) == 0);
    CHECK(pthread_mutex_unlock(&gate) == 0);
}

static void release_all(void)
{
    CHECK(pthread_mutex_lock(&gate) == 0);
    released = 1;
    CHECK(pthread_cond_broadcast(&changed) == 0);
    CHECK(pthread_mutex_unlock(&gate) == 0);
}

static void *clone_worker(void *arg)
{
    unsigned int p = page_size(), size = 2 * p + 19;
    char *readback = malloc(size), c = (char)(uintptr_t)arg;
    CHECK(readback);
    CHECK(tls_clone(owner) == 0);
    rendezvous(); /* Parent destroys its copy before the clones write. */
    CHECK(tls_read(0, size, readback) == 0);
    for (unsigned int i = 0; i < size; i++) CHECK(readback[i] == 'A');
    CHECK(tls_write(3, 1, &c) == 0);
    char *replacement = malloc(p + 7);
    CHECK(replacement);
    memset(replacement, c, p + 7);
    CHECK(tls_write(p - 2, p + 7, replacement) == 0);
    CHECK(tls_write(size - 1, 1, &c) == 0);
    CHECK(tls_read(0, size, readback) == 0);
    for (unsigned int i = 0; i < size; i++) {
        char want = (i == 3 || (i >= p - 2 && i < 2 * p + 5) || i == size - 1) ? c : 'A';
        CHECK(readback[i] == want);
    }
    CHECK(tls_destroy() == 0);
    free(replacement); free(readback);
    return NULL;
}

static void clones(void)
{
    enum { THREADS = 12 };
    pthread_t threads[THREADS];
    unsigned int size = 2 * page_size() + 19;
    char *input = malloc(size), *readback = malloc(size);
    CHECK(input && readback);
    memset(input, 'A', size);
    owner = pthread_self();
    CHECK(tls_create(size) == 0);
    CHECK(tls_write(0, size, input) == 0);
    for (int i = 0; i < THREADS; i++) CHECK(pthread_create(threads + i, NULL, clone_worker, (void *)(uintptr_t)('a' + i)) == 0);
    wait_arrivals(THREADS);
    char c = 'B';
    CHECK(tls_write(3, 1, &c) == 0); /* Owner's COW must leave each clone unchanged. */
    CHECK(tls_read(3, 1, readback) == 0 && readback[0] == 'B');
    CHECK(tls_destroy() == 0);
    release_all();
    for (int i = 0; i < THREADS; i++) CHECK(pthread_join(threads[i], NULL) == 0);
    free(input); free(readback);
}

static void *independent_worker(void *arg)
{
    char input[31], output[31];
    memset(input, (int)(uintptr_t)arg, sizeof(input));
    for (int i = 0; i < 60; i++) {
        CHECK(tls_create(page_size() + 3) == 0);
        CHECK(tls_write(page_size() - 28, sizeof(input), input) == 0);
        CHECK(tls_read(page_size() - 28, sizeof(output), output) == 0);
        CHECK(memcmp(input, output, sizeof(input)) == 0);
        CHECK(tls_destroy() == 0);
    }
    return NULL;
}

static void concurrency(void)
{
    pthread_t threads[12];
    for (int i = 0; i < 12; i++) CHECK(pthread_create(threads + i, NULL, independent_worker, (void *)(uintptr_t)(i + 1)) == 0);
    for (int i = 0; i < 12; i++) CHECK(pthread_join(threads[i], NULL) == 0);
}

static void *exit_worker(void *arg)
{
    (void)arg;
    CHECK(tls_create(1) == 0);
    return NULL; /* A thread exit must release its registry slot and mappings. */
}

static void thread_exit(void)
{
    for (int i = 0; i < 300; i++) {
        pthread_t thread;
        CHECK(pthread_create(&thread, NULL, exit_worker, NULL) == 0);
        CHECK(pthread_join(thread, NULL) == 0);
    }
}

static void *capacity_worker(void *arg)
{
    (void)arg;
    CHECK(tls_create(1) == 0);
    rendezvous();
    CHECK(tls_destroy() == 0);
    return NULL;
}

static void capacity(void)
{
    pthread_t threads[128];
    for (int i = 0; i < 128; i++) CHECK(pthread_create(threads + i, NULL, capacity_worker, NULL) == 0);
    wait_arrivals(128);
    CHECK(tls_create(1) == -1);
    CHECK(tls_clone(threads[0]) == -1);
    release_all();
    for (int i = 0; i < 128; i++) CHECK(pthread_join(threads[i], NULL) == 0);
    CHECK(tls_create(1) == 0);
    CHECK(tls_destroy() == 0);
}

struct test_case { const char *name; void (*run)(void); };

int main(int argc, char **argv)
{
    const struct test_case cases[] = {
        {"errors", errors}, {"short-spans", short_spans}, {"boundaries", boundaries},
        {"lifecycle", lifecycle}, {"clones", clones}, {"concurrency", concurrency},
        {"thread-exit", thread_exit}, {"capacity", capacity}
    };
    int failures = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        if (argc > 1 && strcmp(argv[1], cases[i].name) != 0) continue;
        pid_t child = fork();
        CHECK(child >= 0);
        if (child == 0) { alarm(20); cases[i].run(); exit(0); }
        int status;
        CHECK(waitpid(child, &status, 0) == child);
        if (!WIFEXITED(status) || WEXITSTATUS(status)) {
            fprintf(stderr, "FAIL %s (status %d)\n", cases[i].name, status);
            failures++;
        } else printf("PASS %s\n", cases[i].name);
        fflush(stdout);
    }
    return failures ? 1 : 0;
}
