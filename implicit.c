/*
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "jungle_1_team",
    /* First member's full name */
    "sehoon",
    /* First member's email address */
    "gsh723@naver.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* -- Constants --*/
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* -- Macros -- */
/* Pack a size and allocated bit */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and Write a word at address*/
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr : bp, compute address of its header and footer*/
#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/*Given block ptr : bp, compute address of next and previous blocks*/
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

/*-- PROTOTYPES -- */
int mm_init(void);
void *mm_malloc(size_t size);
void mm_free(void *ptr);
void *mm_realloc(void *ptr, size_t size);
static void *coalesce(void *ptr);
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

/*pointer of heap*/
static char *heap_listp; /* points to the start of the heap */

/*
 * coalesce - connect blocks if free block is adjacent to current freed block
 */
static void *coalesce(void *ptr)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(ptr)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t size = GET_SIZE(HDRP(ptr));

    /*case 1. adjacent blocks are all allocated */
    if (prev_alloc && next_alloc)
    {
        return ptr;
    }
    /*case 2. next block of adjacent blocks is free */
    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));
    }
    /*case 3. previous block of adjacent blocks is free */
    else if (!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(ptr)));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));
        ptr = PREV_BLKP(ptr);
    }
    /*case 4. all adjacent block are free */
    else
    {
        size += GET_SIZE(HDRP(PREV_BLKP(ptr))) + GET_SIZE(FTRP(NEXT_BLKP(ptr)));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(size, 0));
        ptr = PREV_BLKP(ptr);
    }
    return ptr;
}

/*
* exted_heap - extend heap memory space
(case 1) initialize heap
(case 2) need to extend heap when mm_malloc function calls
*/
static void *extend_heap(size_t words)
{
    char *ptr;
    size_t size;

    /*1. apply alignment rule(Dubble Words align)*/
    size = (words % 2) ? (words + 1) * (WSIZE) : (words)*WSIZE;

    /*2. set memory space as 'size' if the memory space exists in memory system*/
    if ((long)(ptr = mem_sbrk(size)) == -1)
    {
        return NULL;
    }
    /*3. allocate, header of empty available block*/
    PUT(HDRP(ptr), PACK(size, 0));
    /*4. allocate footer of empty available block*/
    PUT(FTRP(ptr), PACK(size, 0));
    /*5. allocate epilogue of heap*/
    PUT(HDRP(NEXT_BLKP(ptr)), PACK(0, 1));

    /*6. connect blocks if needed*/
    return coalesce(ptr);
}

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /*1. set 4words for initiate*/
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
    {
        return -1;
    }
    /*2. allocate unused padding word as start of heap*/
    PUT(heap_listp, 0);
    /*3. allocate prologue header(block size : 8, allocated : 1)*/
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    /*4. allocate prologue footer(block size : 8, allocated : 1)*/
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    /*5. allocate epilogue header(block size : 0, allocated : 1)*/
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));

    /*6. move block pointer for setting payload data*/
    heap_listp += (2 * WSIZE);
    /*7. extend empty heap by CHUNKSIZE size*/
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
    {
        return -1;
    }
    return 0;
}
/*
 * find_fit - find available block in free list
 */
static void *find_fit(size_t asize)
{
    void *bp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
        {
            return bp;
        }
    }
    return NULL;
}

/*
 * place - memory of adjusted size is allocated in available block
 */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    /* minimum bytes we can allocate is 16 bytes. */
    if ((csize - asize) >= (2 * DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    else
    {
        // after allocate, if left available block size is lower than 16 bytes(minimum bytes), total block size is allocated
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 * Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    /*Ignore spurious requests*/
    if (size == 0)
    {
        return NULL;
    }
    /*Adjust block size*/
    if (size <= DSIZE)
    {
        asize = 2 * DSIZE;
    }
    else
    {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }

    /*Search the free list for a fit*/
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block*/
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
    {
        return NULL;
    }
    place(bp, asize);
    return bp;
}
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    /*1. get block size*/
    size_t size = GET_SIZE(HDRP(ptr));

    /*2. data in header and footer initialize*/
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    /*3. connect avaliable blocks if free block is adjacent to current freed black*/
    coalesce(ptr);
}
/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    /*[exception 1] free ptr and return zero if requested 'size' is zero or under zero*/
    if (size <= 0)
    {
        mm_free(ptr);
        return 0;
    }
    /*[exception 2] call mm_malloc if 'ptr' is NULL */
    if (ptr == NULL)
    {
        return mm_malloc(size);
    }

    void *newp = mm_malloc(size);
    if (newp == NULL)
    {
        return 0;
    }

    /*compare new size and block size of origin block pointer*/
    size_t oldsize = GET_SIZE(HDRP(ptr));
    if (size < oldsize)
    {
        /*cut oldsize as size if new size is smaller than oldsize */
        oldsize = size;
    }

    /*change newpointer's address to origin ptr's address*/
    memcpy(newp, ptr, oldsize);
    /*free origin block pointer */
    mm_free(ptr);
    return newp;
}
