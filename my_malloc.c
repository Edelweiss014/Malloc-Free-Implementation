#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "my_malloc.h"

void rangeErr() {
    fprintf(stderr, "Pointer out of heap\n");
    exit(EXIT_FAILURE);
}

void pointerNotAllowedErr() {
    fprintf(stderr, "Not the start of a block\n");
    exit(EXIT_FAILURE);
}

void failureErr() {
    fprintf(stderr, "Malloc fails\n");
    exit(EXIT_FAILURE);
}

// check whether a pointer is available for
//      freeing; if not, print message to stderr
// Parameter(s): the pointer to be freed
void checkPointer(void * ptr) {
    if (ptr > sbrk(0) || ptr < heapStart) {
        rangeErr();
    }
    meta_t * thisMetaAddr = get_meta_from_true_addr(ptr);
    if (thisMetaAddr->isFree != 0 && thisMetaAddr->isFree != 1) {
        pointerNotAllowedErr();
    }
    return;
}

// add the new freed block address into the linked
//      list
// Parameter(s): the node to be added
void add_node(meta_t * ptr) {
    if (headFreeMeta == NULL) {
        headFreeMeta = ptr;
    }
    else if (ptr <= headFreeMeta) {
        ptr->nextFreeMeta = headFreeMeta;
        headFreeMeta = ptr;
    }
    else {
        meta_t * curr = headFreeMeta;
        while (curr->nextFreeMeta != NULL && ptr > curr->nextFreeMeta) {
            curr = curr->nextFreeMeta;
        }
        ptr->nextFreeMeta = curr->nextFreeMeta;
        curr->nextFreeMeta = ptr;
    }
    return;
}

// delete the node from the linked list of the free
//      block list
// Parameter(s): the node to be deleted
// Return(s): 0 for success, -1 for failure
int delete_node(meta_t * ptr) {
    if (headFreeMeta == NULL) {
        return -1;
    }
    if (headFreeMeta == ptr) {
        headFreeMeta = headFreeMeta->nextFreeMeta;
        return 0;
    }
    else {
        meta_t * curr = headFreeMeta;
        while (curr->nextFreeMeta != NULL) {
            if (curr->nextFreeMeta == ptr) {
                // finds the node to delete
                meta_t * newNext = curr->nextFreeMeta->nextFreeMeta;
                curr->nextFreeMeta = newNext;
                return 0;
            }
            else {
                curr = curr->nextFreeMeta;
            }
        }
    }
    return -1;
}

// calculate the true size of the block, 
//      with the size of metadata and bound
//      tag added
// Parameter(s): the size user needs
// Return(s): the size of the whole allocated
//      block with meta and bound added
size_t calculate_true_size(size_t inSize) {
    size_t overhead = ALIGNED_META_SIZE + ALIGNED_BOUND_SIZE;
    size_t trueSize = ALIGN(inSize + overhead);
    return trueSize;
}

// given the curr meta address, get the curr bound
//      address
// Parameter(s): meta address of curr block
// Return(s): bound address of curr block
bound_t * get_curr_bound_addr(meta_t * metaAddr) {
    char * metaStart = (char *)metaAddr;
    return (bound_t *) (metaStart + ALIGNED_META_SIZE + metaAddr->size);
}

// renew the information of the meta and bound of a
//      newly allocated block except the size and
//      bound (size will be updated in get_ff_fit 
//      or get_bf_fit if succeeds; on failure, it 
//      will be updated after sbrk); bound will also
//      be updated there
// Parameter(s): the metadata address
void renew_info_malloc(meta_t * metaAddr) {
    metaAddr->isFree = 0;
    metaAddr->nextFreeMeta = NULL;
    return;
}

// if the memory is large enough to hold a new block
//      after this malloc action, split it and mark a
//      new block here; otherwise, just keep it a single
//      block; mark the size in both meta and bound; will
//      update the linked list when needed
// Parameter(s): the current meta address
// Return(s): the final free meta address
meta_t * split_block(meta_t * thisMetaAddr, size_t sizeNeed) {
    size_t oldSize = thisMetaAddr->size;
    // update the allocated part
    size_t sizeBlock1 = ALIGN(sizeNeed);
    // stop splitting on illegal input
    if (sizeBlock1 + ALIGNED_BOUND_SIZE + ALIGNED_META_SIZE > oldSize) {
        return thisMetaAddr;
    }
    // update the remaining part
    size_t sizeBlock2 = oldSize - ALIGNED_META_SIZE 
                    - ALIGNED_BOUND_SIZE - sizeBlock1;
    // make the first part free and allocate the
    thisMetaAddr->size = sizeBlock2;
    bound_t * thisBoundAddr = get_curr_bound_addr(thisMetaAddr);
    thisBoundAddr->thisMeta = thisMetaAddr;
    meta_t * nextMetaAddr = get_next_block_meta(thisMetaAddr);
    renew_info_malloc(nextMetaAddr);
    nextMetaAddr->size = sizeBlock1;
    bound_t * nextBoundAddr = get_curr_bound_addr(nextMetaAddr);
    nextBoundAddr->thisMeta = nextMetaAddr;
    return nextMetaAddr;
}

// check the existing part of the heap according
//      to the first-fit or best-fit policy; will
//      update the size of the block if splitting
//      needed; will update the linked list
// Parameter(s): the true size of the block
// Return(s): if there is a fit, returns the
//      address of the block; otherwise, returns
//      NULL
void * get_ff_fit(size_t inSize) {
    if (headFreeMeta == NULL) {
        return NULL;
    }
    meta_t * currMeta = (meta_t *) headFreeMeta;
    while (currMeta != NULL) {
        if (currMeta->isFree == 1 && currMeta->size >= ALIGN(inSize)) {
            if (currMeta->size > ALIGN(inSize) + ALIGNED_META_SIZE + ALIGNED_BOUND_SIZE) {
                currMeta = split_block(currMeta, ALIGN(inSize));
            }
            else {
                delete_node((void *) currMeta);
            }
            return (void *) currMeta;
        }
        currMeta = currMeta->nextFreeMeta;
    }
    return NULL;
}

void * get_bf_fit(size_t inSize) {
    if (headFreeMeta == NULL) {
        return NULL;
    }
    meta_t * currMeta = (meta_t *) headFreeMeta;
    int sizeDiff = -1;
    meta_t * finalMeta = NULL;
    while (currMeta != NULL) {
        if (currMeta->isFree == 1 && currMeta->size == ALIGN(inSize)) {
            finalMeta = currMeta;
            break;
        }
        if (currMeta->isFree == 1 && currMeta->size >= ALIGN(inSize)) {
            if (sizeDiff == -1) {
                finalMeta = currMeta;
                sizeDiff = currMeta->size - ALIGN(inSize);
            }
            else {
                if (currMeta->size - ALIGN(inSize) < sizeDiff) {
                    finalMeta = currMeta;
                    sizeDiff = currMeta->size - ALIGN(inSize);
                }
            }
        }
        currMeta = currMeta->nextFreeMeta;
    }
    if (finalMeta != NULL && 
                finalMeta->size > ALIGN(inSize) + ALIGNED_META_SIZE + ALIGNED_BOUND_SIZE) {
        finalMeta = split_block(finalMeta, ALIGN(inSize));
    }
    else if (finalMeta != NULL) {
        delete_node((void *) finalMeta);
    }
    return finalMeta;
}

// given the current metadata address, return
//      the address of next block's metadata
// Parameter(s): the address of this meta address
// Return(s): addr for available, NULL for not available
meta_t * get_next_block_meta(meta_t * thisMetaAddr) {
    // convert the type for pointer arithmetric
    char * thisMetaStart = (char *) thisMetaAddr;
    meta_t * nextMetaAddr = (meta_t *)(thisMetaStart + ALIGNED_META_SIZE 
                + thisMetaAddr->size + ALIGNED_BOUND_SIZE);
    if ((void *)nextMetaAddr >= sbrk(0)) {
        return NULL;
    }
    else {
        return nextMetaAddr;
    }
}

// given the current metadata address, return
//      the address of previous block's metadata
// Parameter(s): the address of this meta address
// Return(s): if available, return the address;
//      otherwise, return NULL
meta_t * get_prev_block_meta(meta_t * thisMetaAddr) {
    if ((void *)thisMetaAddr <= heapStart) {
        return NULL;
    }
    // convert the type for pointer arithmetric
    char * thisMetaStart = (char *) thisMetaAddr;
    bound_t * prevBoundAddr = (bound_t *)(thisMetaStart - ALIGNED_BOUND_SIZE);
    if ((void *)prevBoundAddr < heapStart) {
        return NULL;
    }
    return prevBoundAddr->thisMeta;
}

// when freeing, check whether the current block can merge
//      with a block next to it; if so, merge it, delete
//      the next block from the free block linked list; 
//      for this block, it will be decided in other functions
// Parameter(s): the current meta address
// Return(s): 1 for merged, 0 for not merged
int merge_back(meta_t * thisMetaAddr) {
    if (thisMetaAddr == NULL) {
        return 0;
    }
    meta_t * nextMetaAddr = get_next_block_meta(thisMetaAddr);
    if (nextMetaAddr == NULL) {
        return 0;
    }
    if (nextMetaAddr->isFree == 1) {
        bound_t * nextBound  = get_curr_bound_addr(nextMetaAddr);
        nextBound->thisMeta = thisMetaAddr;
        thisMetaAddr->size = thisMetaAddr->size + ALIGNED_BOUND_SIZE
                     + ALIGNED_META_SIZE + nextMetaAddr->size;
        delete_node(nextMetaAddr);
        return 1;
    }
    else {
        return 0;
    }
}

// when freeing, check whether the current block can merge
//      with a block next to it; if so, merge it; if 
//      not, just add the current block to the linked list
// Parameter(s): the current meta address
void merge_front(meta_t * thisMetaAddr) {
    if (thisMetaAddr == NULL) {
        return;
    }
    meta_t * prevMetaAddr = get_prev_block_meta(thisMetaAddr);
    if (prevMetaAddr == NULL) {
        add_node(thisMetaAddr);
        return;
    }
    if (prevMetaAddr->isFree == 1) {
        bound_t * currBound = get_curr_bound_addr(thisMetaAddr);
        currBound->thisMeta = prevMetaAddr;
        prevMetaAddr->size = prevMetaAddr->size + ALIGNED_BOUND_SIZE
                        + ALIGNED_META_SIZE + thisMetaAddr->size;
    }
    else {
        add_node(thisMetaAddr);
    }
    return;
}

// from the address of metadata, calculate the
//      starting address of the inner data block
// Parameter(s): the address of the meta data of
//      a block
// Return(s): the starting address of the inner 
//      data block
void * get_true_addr_from_meta(void * metaAddr) {
    // convert the type for pointer arithmetric
    char * metaStart = (char *) metaAddr;
    return (void *)(metaStart + ALIGNED_META_SIZE);
}

// from the address of program break, calculate the
//      starting address of the inner data block
// Parameter(s): the address of the program break
// Return(s): the starting address of the inner 
//      data block
void * get_true_addr_from_break(void * programBreak, size_t inSize) {
    // convert the type for pointer arithmetric
    char * programBreakC = (char *)programBreak;
    return (void *)(programBreakC - ALIGNED_BOUND_SIZE - ALIGN(inSize));
}

// from the address of true data block, get the
//      current meta address
// Parameter(s): the true address of the data block
// Return(s): the address of the current meta
meta_t * get_meta_from_true_addr(void * trueAddr) {
    // convert the type for pointer arithmetric
    char * trueAddrC = (char *)trueAddr;
    return (meta_t *) (trueAddrC - ALIGNED_META_SIZE);
}

// calls either ff_malloc or bf_malloc to work
// Parameter(s): the size that user needs; the
//      funtion pointer to fit a fit block within
//      the heap (either get_ff_fit or get_bf_fit)
// Return(s): the allocated true address
void * perform_malloc(size_t inSize, void * (*get_fit)(size_t)) {
    if (isFirstMalloc == 1) {
        heapStart = sbrk(0);
        isFirstMalloc = 0;
    }
    void * resultAddr = NULL;
    // get a freed address according to the policy

    void * tempAddr = get_fit(inSize);
    if (tempAddr != NULL) {
        // if found, use it directly
        renew_info_malloc((meta_t *) tempAddr);
        resultAddr = get_true_addr_from_meta(tempAddr);
    }
    else {
        // if failed, call sbrk
        size_t trueSize = calculate_true_size(inSize);
        tempAddr = sbrk(trueSize);
        if (tempAddr == (void *)(-1)) {
            // malloc fails
            return NULL;
        }
        resultAddr = get_true_addr_from_meta(tempAddr);
        // convert the type for pointer arithmetric
        char * resultStart = (char *) resultAddr;
        meta_t * thisMetaAddr = (meta_t *)(resultStart - ALIGNED_META_SIZE);
        renew_info_malloc(thisMetaAddr);
        thisMetaAddr->size = ALIGN(inSize);
        get_curr_bound_addr(thisMetaAddr)->thisMeta = thisMetaAddr;
    }
    // return the address of the true data block
    return resultAddr;
}

// performs malloc; can be called by either ff_malloc
//      and bf_malloc
// Parameter(s): the pointer to be freed
void perform_free(void * ptr) {
    if (ptr == NULL) return;
    checkPointer(ptr);
    // get the meta and renew
    meta_t * metaAddr = get_meta_from_true_addr(ptr);
    metaAddr->isFree = 1;
    merge_back(metaAddr);
    merge_front(metaAddr);
    return;
}


// malloc provides an available address for a
//      newly allocated block; free marks a block
//      as available again after malloc

// implementation of malloc in first-fit policy
void * ff_malloc(size_t size) {
    return perform_malloc(size, get_ff_fit);
}

// implementation of malloc in best-fit policy
void * bf_malloc(size_t size) {
    return perform_malloc(size, get_bf_fit);
}

// implementation of free in first-fit policy
void ff_free(void * ptr) {
    perform_free(ptr);
    return;
}

// implementation of free in best-fit policy
void bf_free(void * ptr) {
    perform_free(ptr);
    return;
}

// functions that helps with testing
unsigned long get_largest_free_data_segment_size() {
    unsigned long largest = 0;
    meta_t * curr = headFreeMeta;
    while (curr != NULL) {
        if (curr->size > largest) {
            largest = curr->size;
        }
        curr = curr->nextFreeMeta;
    }
    return largest;
}
unsigned long get_total_free_size() {
    unsigned sum = 0;
    meta_t * curr = headFreeMeta;
    while (curr != NULL) {
        sum += curr->size;
        curr = curr->nextFreeMeta;
    }
    return sum;
}
