#include <string.h>
#include <stdio.h>
#include <unistd.h>

/* The standard allocator interface from stdlib.h.  These are the
 * functions you must implement, more information on each function is
 * found below. They are declared here in case you want to use one
 * function in the implementation of another. */
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

/* When requesting memory from the OS using sbrk(), request it in
 * increments of CHUNK_SIZE. */
#define CHUNK_SIZE (1<<12)

/*
 * This function, defined in bulk.c, allocates a contiguous memory
 * region of at least size bytes.  It MAY NOT BE USED as the allocator
 * for pool-allocated regions.  Memory allocated using bulk_alloc()
 * must be freed by bulk_free().
 *
 * This function will return NULL on failure.
 */
extern void *bulk_alloc(size_t size);

/*
 * This function is also defined in bulk.c, and it frees an allocation
 * created with bulk_alloc().  Note that the pointer passed to this
 * function MUST have been returned by bulk_alloc(), and the size MUST
 * be the same as the size passed to bulk_alloc() when that memory was
 * allocated.  Any other usage is likely to fail, and may crash your
 * program.
 *
 * Passing incorrect arguments to this function will result in an
 * error message notifying you of this mistake.
 */
extern void bulk_free(void *ptr, size_t size);

/*
 * This function computes the log base 2 of the allocation block size
 * for a given allocation.  To find the allocation block size from the
 * result of this function, use 1 << block_index(x).
 *
 * This function ALREADY ACCOUNTS FOR both padding and the size of the
 * header.
 *
 * Note that its results are NOT meaningful for any
 * size > 4088!
 *
 * You do NOT need to understand how this function works.  If you are
 * curious, see the gcc info page and search for __builtin_clz; it
 * basically counts the number of leading binary zeroes in the value
 * passed as its argument.
 */
static inline __attribute__((unused)) int block_index(size_t x) {
    if (x <= 8) {
        return 5;
    } else {
        return 32 - __builtin_clz((unsigned int)x + 7);
    }
}

static void *free_list_table[13] = {NULL};

/*
 * You must implement malloc().  Your implementation of malloc() must be
 * the multi-pool allocator described in the project handout.
 */
void *malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    if (size > 4088){
        void *b = bulk_alloc(size + 8);
        //should be putting in allocation size
        *(size_t*)b = size;
        b += sizeof(size_t);
        return b;
    }
    
    int index = block_index(size);
    if (free_list_table[index] == NULL){
        void *mem = sbrk(CHUNK_SIZE);
        
        size_t allocationSize = (1 << index);
        int partion = CHUNK_SIZE / allocationSize;
        
        for (int i = 0; i < partion; i++){
            //if we are at the final block to add set the next block pointer pointing to NULL
            if (i == (partion - 1)){
                *(size_t*)mem = allocationSize;
                mem += sizeof(size_t);
                *(void**)mem = NULL;
                mem -= sizeof(size_t);
            }
            else{
                //add allocation size in metadata
                *(size_t*)mem = allocationSize;
                mem += sizeof(size_t);
                //add pointer to next block
                void *nextBlock = mem + allocationSize;
                *(void**)mem = nextBlock;
                mem -= sizeof(size_t);
                mem += allocationSize;
            }
        }

        mem += sizeof(size_t);
        free_list_table[index] = mem; 
        
        //return the head of the linked list structure for this index starting after the metadata
        mem -= allocationSize;
        return mem;
    }
    void *head = free_list_table[index];
    void *current = free_list_table[index];
    current = *(void**)head;
    free_list_table[index] = current;
    return head;
}

/*
 * You must also implement calloc().  It should create allocations
 * compatible with those created by malloc().  In particular, any
 * allocations of a total size <= 4088 bytes must be pool allocated,
 * while larger allocations must use the bulk allocator.
 *
 * calloc() (see man 3 calloc) returns a cleared allocation large enough
 * to hold nmemb elements of size size.  It is cleared by setting every
 * byte of the allocation to 0.  You should use the function memset()
 * for this (see man 3 memset).
 */
void *calloc(size_t nmemb, size_t size) {
    if ((nmemb * size) == 0){
        return NULL;
    }
    void *p = malloc(nmemb * size);
    memset(p, 0, nmemb * size);
    return p;
}

/*
 * You must also implement realloc().  It should create allocations
 * compatible with those created by malloc(), honoring the pool
 * alocation and bulk allocation rules.  It must move data from the
 * previously-allocated block to the newly-allocated block if it cannot
 * resize the given block directly.  See man 3 realloc for more
 * information on what this means.
 *
 * It is not possible to implement realloc() using bulk_alloc() without
 * additional metadata, so the given code is NOT a working
 * implementation!
 */
void *realloc(void *ptr, size_t size) {
    if (size == 0){
        return NULL;
    }

    void *newHead = ptr;
    newHead -= sizeof(size_t);
    size_t allocationSize = *(size_t*)newHead;
    newHead += sizeof(size_t);
    
    if (size < allocationSize - 8){
        return ptr;
    }
    if (size > 4088){
        void *newBulkedPtr = bulk_alloc(size + 8);
        *(size_t*)newBulkedPtr = size;
        newBulkedPtr += sizeof(size_t);
        memcpy(newBulkedPtr, ptr, (allocationSize - 8));
        free(ptr);
        return newBulkedPtr;
    }
    //allocate new blocks if null or use head of block already there and then copy data over then free old block
    void *newPtr = malloc(size);
    memcpy(newPtr, ptr, allocationSize - 8);
    free(ptr);
    return newPtr;
}

/*
 * You should implement a free() that can successfully free a region of
 * memory allocated by any of the above allocation routines, whether it
 * is a pool- or bulk-allocated region.
 *
 * The given implementation does nothing.
 */
void free(void *ptr) {
    //pointer to the block we want to free i.e. insert back into the head of our "linked list"
    if (ptr == NULL){
        return;
    }
    
    
    void *head = ptr;
    head -= sizeof(size_t);
    size_t allocationSize = *(size_t*)head;
    head += sizeof(size_t);
    
    if (allocationSize > CHUNK_SIZE){
        head -= sizeof(size_t);
        bulk_free(head, allocationSize);
        return;
    }

    int index = block_index(allocationSize) - 1;
    if (free_list_table[index] == NULL){
        *(void**)head = NULL;
    }
    else{
        *(void**)head = free_list_table[index];
    }
    
    free_list_table[index] = head;
    
    return;
}
