# Page-protected thread-local storage

A small C library that gives each POSIX thread its own byte storage using anonymous
`mmap` pages. A thread can clone another thread's storage without copying its pages;
a write then copies only the shared pages it touches. This is an operating-systems
exercise in page protection, reference counting, and copy-on-write.

## Build and run

Requires a C11 compiler, POSIX threads, and `mmap`/`mprotect` with anonymous mappings.
The Makefile supports Linux and macOS. Linux CI builds with GCC and Clang and runs
AddressSanitizer/UndefinedBehaviorSanitizer with Clang.

```sh
make                 # build/libtls.a
make test            # API regressions and resource/protection checks
make sanitize        # AddressSanitizer + UndefinedBehaviorSanitizer
make thread-sanitize # ThreadSanitizer, where supported by the host
```

Include `TLS.h` with the same capitalization, then link `build/libtls.a` with
`-pthread`. The implementation queries the actual system page size, including
hosts with 16 KiB pages. The old `PAGESIZE` macro remains for source compatibility;
it isn't the runtime page size.

```c
#include "TLS.h"

int main(void)
{
    char input[] = "hello", output[sizeof(input)];
    if (tls_create(32) != 0) return 1;
    if (tls_write(3, sizeof(input), input) != 0 ||
        tls_read(3, sizeof(output), output) != 0) {
        tls_destroy();
        return 1;
    }
    int matches = memcmp(input, output, sizeof(input)) == 0;
    return tls_destroy() != 0 || !matches;
}
```

## API and lifecycle

Every function returns `0` on success and `-1` on invalid input or recoverable
resource failure. The registry supports up to 128 threads with live storage.

| Function | Behavior |
| --- | --- |
| `tls_create(size)` | Creates zero-filled storage for the caller. Rejects zero size and an existing TLS. |
| `tls_read(offset, length, buffer)` | Copies exactly `length` bytes to the caller's buffer. |
| `tls_write(offset, length, buffer)` | Copies exactly `length` bytes into the caller's TLS, detaching shared pages first. |
| `tls_clone(tid)` | Creates the caller's TLS from a live thread's existing storage. Rejects an existing caller TLS or missing source. |
| `tls_destroy()` | Removes the caller's TLS and releases its page references and registry slot. |

Ranges must fit entirely within the declared byte size; overflow is rejected
before copying. A zero-length transfer accepts `NULL` when `offset <= size`.
Nonzero transfers require a valid caller-owned buffer of at least `length` bytes.
The library detects `NULL`, but cannot validate arbitrary pointers or buffer sizes.

Use a valid, live `pthread_t` obtained from the pthread APIs when cloning, and
coordinate the source's lifetime if the clone must succeed. Cloning and destroying
are serialized: a clone either takes its references before destruction, or returns
`-1` after the source is removed. Clones remain valid after the source is destroyed.
Thread exit also releases storage through a pthread key destructor, so omitted
explicit destruction doesn't leave stale thread IDs, slots, or mappings behind.

Failed creation or cloning rolls back its allocations. A write that fails while
allocating a later copy-on-write page may have already changed earlier pages; writes
are not transactional. A failure to restore `PROT_NONE`, or to unmap a library-owned
page during cleanup, calls `abort()` rather than silently violating the resource or
protection contract.

## Protection and concurrency limits

Pages remain `PROT_NONE` between API accesses. Reads temporarily enable read access;
writes temporarily enable read/write access. One mutex covers the registry, page
reference counts, and each entire API operation, including temporary permission
changes. Cooperative threads using the API can create, clone, read, write, and
destroy concurrently without corrupting those structures. Cancellation is disabled
inside API operations so it cannot strand the mutex or leave permissions enabled.
This simple design serializes transfers rather than promising parallel throughput.

`mprotect` permissions apply to the whole process. Another thread that knows a page
address can access it while an API operation has temporarily enabled permissions,
or change permissions itself. This library therefore provides accidental-access
protection between calls, not a security boundary against hostile threads.

No signal handlers are installed. A direct access to a protected page follows the
process's existing `SIGSEGV`/`SIGBUS` disposition, normally terminating the process;
it does not kill only the offending thread. The API isn't async-signal-safe, must
not be called recursively, and doesn't support use in the child of a multithreaded
`fork()` before `exec()`. Invalid caller buffers have ordinary C fault behavior.

## What the tests cover

The assertion-based suite replaces the original print-only demos, including their
out-of-bounds fixture accesses and unterminated `%s` output. It exercises one-byte
unaligned transfers, exact and partial pages, cross-page spans, range overflow,
missing storage, zero-length operations, repeated destruction/recreation, clone
isolation after source writes/destruction, concurrent lifecycle operations,
thread-exit cleanup, and registry exhaustion/reuse.

A separate test translation unit injects allocation, mapping, and permission
failures without production test hooks. It accounts for heap objects and mappings,
checks copy-on-write rollback, and probes protected mappings in isolated child
processes. The tests use the host's page size instead of assuming 4096 bytes.
