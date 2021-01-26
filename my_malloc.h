#ifndef MY_MALLOC_H
#define MY_MALLOC_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>



/* 
 * Error handling functions
 * Exit with a failure state and print error
 * message when needed
 */
void rangeErr();
void pointerNotAllowedErr();
void failureErr();

// check whether a pointer is available for
//      freeing; if not, print message to stderr
// Parameter(s): the pointer to be freed
void checkPointer(void * ptr);

/* 
 * Related data structures
 * meta_t: the structure for metadata
 * bound_t: the structure for bound tag
 */

// the size stored is aligned: it includes
//      some extra space even if the user
//      does not need it

typedef struct meta_tag meta_t;
struct meta_tag {
    size_t size; // in byte
    int isFree;
    meta_t * nextFreeMeta;
};

// bound is used to get the meta from
//      the back of a block
struct bound_tag {
    meta_t * thisMeta;
};
typedef struct bound_tag bound_t;

static meta_t * headFreeMeta = NULL;
static void * heapStart = NULL;
static int isFirstMalloc = 1;


#define ALIGNMENT 16
#define ALIGN(inSize) (((inSize) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))
#define ALIGNED_META_SIZE ALIGN(sizeof(meta_t))
#define ALIGNED_BOUND_SIZE ALIGN(sizeof(bound_t))

/* 
 * Helper function declaration:
 * 
 */

// calculate the true size of the block, 
//      with the size of metadata and bound
//      tag added
// Parameter(s): the size user needs
// Return(s): the size of the whole allocated
//      block with meta and bound added
size_t calculate_true_size(size_t inSize);

// given the curr meta address, get the curr bound
//      address
// Parameter(s): meta address of curr block
// Return(s): bound address of curr block
bound_t * get_curr_bound_addr(meta_t * metaAddr);

// renew the information of the meta and bound of a
//      newly allocated block except the size (size
//      will be updated in get_ff_fit or get_bf_fit
//      if succeeds; on failure, it will be updated
//      after sbrk)
// Parameter(s): the metadata address
void renew_info_malloc(meta_t * metaAddr);

// given the current metadata address, return
//      the address of next block's metadata
// Parameter(s): the address of this meta address
meta_t * get_next_block_meta(meta_t * thisMetaAddr);

// given the current metadata address, return
//      the address of previous block's metadata
// Parameter(s): the address of this meta address
meta_t * get_prev_block_meta(meta_t * thisMetaAddr);

// check the existing part of the heap
// Parameter(s): the size of the block that user
//      needs; the function will compare it to
//      the existing block
// Return(s): if there is a fit, returns the
//      address of the block; otherwise, returns
//      NULL
void * get_ff_fit(size_t inSize);
void * get_bf_fit(size_t inSize);

// from the address of metadata, calculate the
//      starting address of the inner data block
// Parameter(s): the address of the meta data of
//      a block
// Return(s): the starting address of the inner 
//      data block
void * get_true_addr_from_meta(void * metaAddr);

// from the address of program break, calculate the
//      starting address of the inner data block
// Parameter(s): the address of the program break
// Return(s): the starting address of the inner 
//      data block
void * get_true_addr_from_break(void * programBreak, size_t inSize);

// from the address of true data block, get the
//      current meta address
// Parameter(s): the true address of the data block
// Return(s): the address of the current meta
meta_t * get_meta_from_true_addr(void * trueAddr);

// if the memory is large enough to hold a new block
//      after this malloc action, split it and mark a
//      new block here; otherwise, just keep it a single
//      block
// Parameter(s): the current meta address
// Return(s): the final free meta address
meta_t * split_block(meta_t * thisMetaAddr, size_t sizeNeed);

// performs malloc; can be called by either ff_malloc
//      and bf_malloc
// Parameter(s): the size that user needs; the
//      funtion pointer to fit a fit block within
//      the heap (either get_ff_fit or get_bf_fit)
// Return(s): the allocated true address
void * perform_malloc(size_t inSize, void * (*get_fit)(size_t));

// add the new freed block address into the linked
//      list
// Parameter(s): the node to be added
void add_node(meta_t * ptr);

// delete the node from the linked list of the free
//      block list
// Parameter(s): the node to be deleted
// Return(s): 0 for success, -1 for failure
int delete_node(meta_t * ptr);

// when freeing, check whether the current block can merge
//      with a block next to it; if so, merge it, delete
//      the next block from the free block linked list; 
//      for this block, it will be decided in other functions
// Parameter(s): the current meta address
// Return(s): 1 for merged, 0 for not merged
int merge_back(meta_t * thisMetaAddr);

// when freeing, check whether the current block can merge
//      with a block next to it; if so, merge it and delete
//      the next block from the free block linked list; if 
//      not, just add the current block to the linked list
// Parameter(s): the current meta address
void merge_front(meta_t * thisMetaAddr);

// performs malloc; can be called by either ff_malloc
//      and bf_malloc
// Parameter(s): the pointer to be freed
void perform_free(void * ptr);

/* 
 * Function declaration:
 * malloc and free for both
 * first-fit and best-fit
 * policy
 */

// malloc provides an available address for a
//      newly allocated block; free marks a block
//      as available again after malloc

// implementation of malloc in first-fit policy
void * ff_malloc(size_t size);

// implementation of free in first-fit policy
void ff_free(void * ptr);

// implementation of malloc in best-fit policy
void * bf_malloc(size_t size);

// implementation of free in best-fit policy
void bf_free(void * ptr);

// functions that helps with testing
unsigned long get_largest_free_data_segment_size();
unsigned long get_total_free_size();



#endif