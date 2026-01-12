#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "dam/dam.h"
#include "dam/dam_config.h"
#include "dam/internal/dam_internal.h"

void  dam_direct_init(void) {}

void* dam_direct_malloc(size_t size) {
    size_t total = align_up(sizeof(pool_header_t) +sizeof(block_header_t) + size, PAGE_SIZE);

    void* memory = mmap(
        NULL,
        total,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );

    if (memory == MAP_FAILED)
        return NULL;

    pool_header_t* pool_header = memory;
    pool_header->type = DAM_POOL_DIRECT;
    pool_header->size = total;
    pool_header->memory = memory;

    dam_register_pool(pool_header);

    block_header_t* block_header = (block_header_t*)(pool_header + 1);
    block_header->size = size;
    block_header->magic = BLOCK_MAGIC;

    return block_header + 1;
}

void* dam_direct_realloc(void* ptr, size_t size) {
    block_header_t* block_header = direct_block_from_ptr(ptr);
    size_t old_size = block_header->size;


    // Case 1 Shrink to lower layer
    void* new_ptr;
    if (size <= DAM_GENERAL_MAX) {
        new_ptr = dam_malloc_internal(size);

        memcpy(new_ptr, ptr, old_size < size ? old_size : size);
        dam_direct_free(ptr);

        return new_ptr;
    }

    // Case 2/3 stay inside direct
    if (size *  100 <= old_size * DAM_DIRECT_SHRINK_PERCENTAGE || size > block_header->size) {
        new_ptr = dam_direct_malloc(size);
        if (!new_ptr) return NULL;

        memcpy(new_ptr, ptr, old_size < size ? old_size : size);
        dam_direct_free(ptr);

        return new_ptr;
    }

    return ptr;
}


void  dam_direct_free(void* ptr) {
    if (!ptr) return;

    pool_header_t* pool_header = dam_pool_from_ptr(ptr);

    dam_unregister_pool(pool_header);
    munmap(pool_header, pool_header->size);
}

pool_header_t* direct_pool_from_ptr(void* ptr) {
    return (pool_header_t*)((char*)ptr - sizeof(block_header_t) - sizeof(pool_header_t));
}

block_header_t* direct_block_from_ptr(void* ptr) {
    return (block_header_t*)((char*)ptr - sizeof(block_header_t));
}