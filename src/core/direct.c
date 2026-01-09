#include <stddef.h>
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
    size_t total_size = align_up(sizeof(pool_header_t) +sizeof(block_header_t) + size, PAGE_SIZE);


    // Case A Shrink to lower layer
    if (size <= DAM_GENERAL_MAX) {
        void* new_ptr = dam_malloc(size);

        memcpy(new_ptr, ptr, size);
        dam_direct_free(ptr);

        return new_ptr;
    }

    // Case B still direct
    if (size / block_header->size <= DAM_DIRECT_SHRINK_RATIO || block_header->size < size) {

        memcpy(new_ptr, ptr, min(block_header->size, size));
        dam_direct_free(ptr);

        return new_ptr;
    }

    return ptr;
}


void  dam_direct_free(void* ptr) {
    pool_header_t* pool_header = dam_pool_from_ptr(ptr);

    dam_unregister_pool(pool_header);
    munmap(ptr, pool_header->size);
}
