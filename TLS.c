// Created by Zhakhangir on 12.11.2024.
#include "tls.h"

typedef struct thread_local_storage
{
    pthread_t tid;
    unsigned int size; /* size in bytes */
    unsigned int page_num; /* number of pages */
    struct page **pages; /* array of pointers to pages */
} TLS;

struct page {
    void* address; /* start address of page */
    int ref_count; /* counter for shared pages */
};

struct thread_local_storage* TLS_array[128];


/////////////////////////////////////
// helper functions section start //
///////////////////////////////////

void tls_protect(struct page *p)
{
    if (mprotect((char*) p->address, 4096, 0)) {
        /*fprintf(stderr, "tls_protect: could not protect page\n");*/
        perror("tls_protect: could not protect page");
        exit(1);
    }
}

void tls_unprotect(struct page *p)
{
    if (mprotect((void*) p->address, 4096, PROT_READ | PROT_WRITE)) {
        /*fprintf(stderr, "tls_unprotect: could not unprotect page\n");*/
        perror("tls_unprotect: could not protect page");
        exit(1);
    }
}

int calc_page_num(unsigned int size) {
    if ( (size % 4096) != 0) { //if the size of bytes does not align with the page size, we have to add an extra page...
        return ((size / 4096) + 1); // ... for the trailing bits
    }
    return (size / 4096);
}

int find_free_tls() {
    int tls_id;
    for (tls_id = 0; tls_id < 128; tls_id++) {
        if (!(TLS_array[tls_id])) { //see if the tls is not allocated
            return tls_id;
        }
    }
    return -1; //if none was found, return an error;
}

int find_threads_tls(pthread_t current_thread) {
    int tls_index;
    for (tls_index = 0; tls_index < 128; tls_index++) {
        if (TLS_array[tls_index]) {
            if (TLS_array[tls_index]->tid == current_thread) {
                return tls_index; // found the tls associated with the tls
            }
        }
    }
    //printf("Could not find the TLS associated with the current thread\n");
    return -1; // could not find a tls associated with the current tid
}

struct page* create_page_copy(struct page *shared_page) {
    tls_unprotect(shared_page);
    struct page *new_page = calloc(1, sizeof(struct page));

    new_page->address = mmap(0,PAGESIZE, 0, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
    tls_unprotect(new_page);
    memset(new_page->address, 0, 4096);

    memcpy((void*) new_page->address, (void *)shared_page->address, 4096);
    new_page->ref_count = 1;

    tls_protect(new_page);
    tls_protect(shared_page);
    return new_page;
}

////////////////////////////////////
// helper functions section end ///
//////////////////////////////////

int tls_create(unsigned int size) {
    pthread_t current_thread = pthread_self();

    if (find_threads_tls(current_thread) != -1) { //if the current thread already has a TLS associated with it...
        return -1; // return an error (only create if thread does not already have a tls)
    }

    int num_of_pages = calc_page_num(size);

    int free_tls_index = find_free_tls();
    if (free_tls_index == -1) {
        printf("could not find a free tls\n");
        return -1;
    }

    struct thread_local_storage* new_tls = malloc(sizeof(TLS));
    if (new_tls == NULL) { //check if the malloc has failed
        printf("could not malloc tls\n");
        return -1;
    }
    new_tls->size = size;
    new_tls->page_num = num_of_pages;
    new_tls->tid = current_thread;
    new_tls->pages = calloc(num_of_pages, sizeof(struct page));
    if (new_tls->pages == NULL) { //check if the calloc has failed
        printf("could not allocate the array of pages for the tls\n");
        return -1;
    }

    int i;
    for (i = 0; i < num_of_pages; i++) {
        struct page *p = calloc(1, sizeof(struct page));
        p->address = mmap(0,PAGESIZE, 0, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
        tls_unprotect(p);
        memset(p->address, 0, 4096);
        p->ref_count = 1;
        tls_protect(p);
        new_tls->pages[i] = p;
    }


    TLS_array[free_tls_index] = new_tls;
    return 0;
}

int tls_write(unsigned int offset, unsigned int length, char* buffer) {
    char *local_buffer = buffer;

    int tls_index = find_threads_tls(pthread_self());

    if ((length + offset) > TLS_array[tls_index]->size) { //if the read operations is larger than the TLS allows...
        return -1; //return an error
    }

    int page_index = offset/4096;
    int offset_in_page = offset % 4096;

    if (offset_in_page != 0) {
        int bytes_to_write = 4096 - offset_in_page;
        struct page *cur_page = TLS_array[tls_index]->pages[page_index];

        if (cur_page->ref_count == 1) {
            tls_unprotect(TLS_array[tls_index]->pages[page_index]);
            char *dst = ((char *) (TLS_array[tls_index]->pages[page_index]->address) + offset_in_page);
            memcpy((void *) dst, (void *) local_buffer, bytes_to_write);
            tls_protect(TLS_array[tls_index]->pages[page_index]);
        }
        else {
            tls_unprotect(cur_page);
            struct page *new_page = create_page_copy(cur_page); // clone that page into a new page

            cur_page->ref_count--;
            tls_protect(cur_page);
            tls_unprotect(new_page);

            void *dst = ((void *) (new_page->address) + offset_in_page);
            memcpy((void *) dst, local_buffer, 4096);
            TLS_array[tls_index]->pages[page_index] = new_page; //change the shared page to the new page
            tls_protect(new_page);
        }

        local_buffer = local_buffer + bytes_to_write;
        length = length - bytes_to_write;
        page_index++;
    }

    int pages_num = calc_page_num(length) + page_index; //add a starting page num for correct looping
    if (length % 4096 != 0) {
        pages_num--;
    }

    //whole page copy
    bool for_happened = false;
    for (; page_index < pages_num; page_index++) {
        for_happened = true;
        struct page *cur_page = TLS_array[tls_index]->pages[page_index];

        if (cur_page->ref_count == 1) { //if there is no other thread that has access to this page
            tls_unprotect(cur_page);
            memcpy((void *) cur_page->address, local_buffer, 4096);
            tls_protect(cur_page);
        }

        else { //if it is a shared page
            tls_unprotect(cur_page);
            struct page *new_page = create_page_copy(cur_page); // clone that page into a new page

            cur_page->ref_count--; //TODO ask TAs if this is correct
            tls_protect(cur_page);

            tls_unprotect(new_page);
            memcpy((void *) new_page->address, local_buffer, 4096);
            TLS_array[tls_index]->pages[page_index] = new_page; //change the shared page to the new page
            tls_protect(new_page);
        }
        local_buffer = local_buffer + 4096; //increment the buffer we are reading from.
    }

    //in case there are trailing bytes
    if (length % 4096 != 0) { // check whether we should write any trailing bytes
        if (for_happened) { //if there was no for loop allocation, we do not need to increment page_index
            page_index++;              //since it was incremented already before the for loop
        }
        int bytes_to_write = length % 4096;
        struct page *cur_page = TLS_array[tls_index]->pages[page_index];

        if (cur_page->ref_count == 1) { //if the page is not shared
            tls_unprotect(TLS_array[tls_index]->pages[page_index]);
            char* dst = ((char *) TLS_array[tls_index]->pages[page_index]->address);

            memcpy(dst, local_buffer, bytes_to_write);

            tls_protect(TLS_array[tls_index]->pages[page_index]);
        }

        else { //if the page is shared

            struct page *new_page = create_page_copy(TLS_array[tls_index]->pages[page_index]); //create a new page
            tls_unprotect(new_page);

            int bytes_to_write= length % 4096;
            char* dst = ((char *) new_page->address); //declaring the destination byte

            memcpy(dst, local_buffer, bytes_to_write);

            TLS_array[tls_index]->pages[page_index]->ref_count--;
            TLS_array[tls_index]->pages[page_index] = new_page;
            tls_protect(new_page);
        }
    }
    return 0;
}

int tls_read(unsigned int offset, unsigned int length, char *buffer) {
    char *local_buffer = buffer;

    int tls_index = find_threads_tls(pthread_self());
    if(tls_index == -1) { //if there is not tls associated with the thread
        return -1;
    }

    if ((length + offset) > TLS_array[tls_index]->size) { //if the read operations is larger than the TLS allows...
        return -1; //return an error
    }

    int page_index = offset / 4096;
    int offset_in_page = offset % 4096;
    if (offset_in_page != 0) {
        int bytes_to_read = 4096 - offset_in_page;
        struct page *cur_page = TLS_array[tls_index]->pages[page_index];

        tls_unprotect(cur_page);
        char* src = ((char *) (cur_page->address) + offset_in_page);

        memcpy((void*)local_buffer, (void*)src, bytes_to_read);

        tls_protect(TLS_array[tls_index]->pages[page_index]);

        local_buffer = local_buffer + bytes_to_read;
        length = length - bytes_to_read;
        page_index++;
    }

    int pages_num = calc_page_num(length) + page_index;
    if ((length % 4096) != 0) { //decrement for the whole page copy, the last page will be added with byte granularity
        pages_num--;
    }

    //whole page copy
    bool for_happened = false;
    for (; page_index < pages_num; page_index++) {
        for_happened = true;
        struct page *cur_page = TLS_array[tls_index]->pages[page_index];
        tls_unprotect(cur_page);
        memcpy( local_buffer, (void *) cur_page->address, 4096);
        tls_protect(cur_page);
        local_buffer = local_buffer + 4096;
    }

    //in case there are trailing bytes
    if (length % 4096 != 0) { // check whether we should write any trailing bytes
        if (for_happened) {
            page_index++; //move to the next page
        }

        int bytes_to_write = length % 4096;
        struct page *cur_page = TLS_array[tls_index]->pages[page_index];

        tls_unprotect(TLS_array[tls_index]->pages[page_index]);
        char* src = ((char *) cur_page->address);

        memcpy(local_buffer, src,  bytes_to_write);

        tls_protect(TLS_array[tls_index]->pages[page_index]);
    }
    return 0;
}

int tls_clone(pthread_t tid) {
    pthread_t current_thread = pthread_self();

    if (find_threads_tls(current_thread) != -1) { //if the current thread already has a TLS associated with it...
        return -1; // return an error (only create if thread does not already have a tls)
    }

    int target_tls_id = find_threads_tls(tid);
    if (target_tls_id == -1) { //if the target thread does not have a TLS associated with it...
        return -1; // return an error (only create if thread does not already have a tls)
    }

    int free_tls_index = find_free_tls();
    if (free_tls_index == -1) {
        printf("could not find a free tls\n");
        return -1;
    }

    struct thread_local_storage* new_tls = malloc(sizeof(TLS));
    if (new_tls == NULL) { //check if the malloc has failed
        printf("could not malloc tls\n");
        return -1;
    }

    new_tls->size = TLS_array[target_tls_id]->size;
    new_tls->page_num = TLS_array[target_tls_id]->page_num;
    new_tls->tid = current_thread;
    new_tls->pages = malloc(sizeof(TLS_array[target_tls_id]->pages));
    if (new_tls->pages == NULL) { //check if the malloc has failed
        printf("could not allocate the array of pages for the tls\n");
        return -1;
    }

    int pages;
    for (pages = 0; pages < new_tls->page_num; pages++) {
        tls_unprotect(TLS_array[target_tls_id]->pages[pages]);
        tls_unprotect(new_tls->pages[pages]);

        TLS_array[target_tls_id]->pages[pages]->ref_count++;
        new_tls->pages[pages] = TLS_array[target_tls_id]->pages[pages];

        tls_protect(new_tls->pages[pages]);
        tls_protect(TLS_array[target_tls_id]->pages[pages]);
    }

    TLS_array[free_tls_index] = new_tls;
    return 0;
}

int tls_destroy() {
    int tls_index = find_threads_tls(pthread_self());
    if (tls_index == -1) { // if the thread does not exist in the tls
        return -1; //return an error
    }

    int num_pages = TLS_array[tls_index]->page_num;

    int page_index;
    for (page_index = 0; page_index < num_pages; page_index++){
        tls_unprotect(TLS_array[tls_index]->pages[page_index]);

        TLS_array[tls_index]->pages[page_index]->ref_count--;
        if (TLS_array[tls_index]->pages[page_index]->ref_count == 0) {
            free(TLS_array[tls_index]->pages[page_index]);
        } else { tls_protect(TLS_array[tls_index]->pages[page_index]);}

    }
    return 0;
}