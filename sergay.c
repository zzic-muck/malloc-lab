/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
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
    "ateam",
    /* First member's full name */
    "malloc",
    /* First member's email address */
    "ㄹㅇㅋㅋ",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 8)
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))
#define GET(p) (*(unsigned int *)(p))                            
#define PUT(p, val) (*(unsigned int *)(p) = (unsigned int)(val))
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define HDRP(bp) ((char *)(bp)-WSIZE)                       
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE))) 
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

#define SUCC_P(bp) (*(void **)(bp + WSIZE))
#define PRED_P(bp) (*(void **)(bp))
#define ROOT_P(choice) (*(void **)((char *)(heap_listp) + (WSIZE * choice)))

static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void add_free_block(void *bp);
static void delete_free_block(void *bp);
int get_class(size_t size);


static void *heap_listp;
static void *free_listp;

int mm_init(void)
{
    if ((heap_listp = mem_sbrk(14 * WSIZE)) == (void *)-1)
    {
        return -1;
    }

    PUT(heap_listp, 0);                                // 얼라이먼트 패딩
    PUT(heap_listp + (1 * WSIZE), PACK(12 * WSIZE, 1)); // 프롤로그헤더

    for(int i=0; i<10;i++){
        PUT(heap_listp + ((2+i) * WSIZE), NULL); 
    }

    PUT(heap_listp + (12 * WSIZE), PACK(12 * WSIZE, 1)); // 프롤로그푸터
    PUT(heap_listp + (13 * WSIZE), PACK(0, 1));         // 에필로그 헤더

    heap_listp += (2 * WSIZE); // 힙의 시작점 근본
    free_listp = heap_listp;   // 프리 리스트 포인터


    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
    {
        return -1;
    }
    return 0;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; // 홀짝
    if ((long)(bp = mem_sbrk(size)) == -1)
    {
        return NULL;
    }

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

void *mm_malloc(size_t size)
{

    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0){
        return NULL;
    }

    if (size <= DSIZE){
        asize = 2 * DSIZE;
    }
    else{
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }

    if ((bp = find_fit(asize)) != NULL){
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}



static void add_free_block(void *bp)
{   
    int choice = get_class(GET_SIZE(HDRP(bp)));
    SUCC_P(bp) = ROOT_P(choice);
    if(ROOT_P(choice) != NULL){
        PRED_P(ROOT_P(choice))= bp;
    }
    ROOT_P(choice) = bp;
}


static void delete_free_block(void *bp)
{
    int choice = get_class(GET_SIZE(HDRP(bp)));

    if (bp == ROOT_P(choice))
    {
        ROOT_P(choice) = SUCC_P(ROOT_P(choice));
        return;
    }

    SUCC_P(PRED_P(bp)) = SUCC_P(bp);
    if(SUCC_P(bp) != NULL){
        PRED_P(SUCC_P(bp)) = PRED_P(bp);
    }


}   



static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc)
    {
        add_free_block(bp);
        return bp;
    }

    else if (prev_alloc && !next_alloc)
    {
        delete_free_block(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc)
    {
        delete_free_block(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else
    {
        delete_free_block(NEXT_BLKP(bp));
        delete_free_block(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } 
    
    add_free_block(bp);
    return bp;
}

static void *find_fit(size_t asize)
{
    int choice = get_class(asize);
    void *bp = ROOT_P(choice);

    while (choice < 10) 
    {
        bp = ROOT_P(choice);
        while (bp != NULL)
        {
            if ((asize <= GET_SIZE(HDRP(bp)))) 
                return bp;
            bp = SUCC_P(bp); 
        }
        choice += 1;
    }

    return NULL;
}

static void place(void *bp, size_t asize)
{
    delete_free_block(bp);
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2 * DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);

        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        add_free_block(bp);

    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */

void *mm_realloc(void *bp, size_t size)
{
    void *old_bp = bp;
    void *new_bp;
    size_t copySize;
    size_t oldsize = GET_SIZE(HDRP(bp));
    size_t need_size = size + DSIZE;
    size_t ssiizzee;

    new_bp = mm_malloc(size);
    if (new_bp == NULL)
    {
        return NULL;
    }


    if (GET_ALLOC(HDRP(NEXT_BLKP(bp))) == 0 && need_size <= oldsize + GET_SIZE(HDRP(NEXT_BLKP(bp))))
    {

        ssiizzee = oldsize + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        delete_free_block(NEXT_BLKP(bp));

        PUT(HDRP(bp), PACK(ssiizzee, 1));
        PUT(FTRP(bp), PACK(ssiizzee, 1));
        return bp;
    }


    copySize = GET_SIZE(HDRP(old_bp));
    if (size < copySize)
    {
        copySize = size;
    }
    memcpy(new_bp, old_bp, copySize);
    mm_free(old_bp);

    return new_bp;
    
}



int get_class(size_t size)
{
    int SEGREGATED_SIZE = 10;

    size_t class_sizes[SEGREGATED_SIZE];
    class_sizes[0] = 16;

    for (int i = 0; i < SEGREGATED_SIZE; i++)
    {
        if (i != 0)
            class_sizes[i] = class_sizes[i - 1] << 1;
        if (size <= class_sizes[i])
            return i;
    }

    return SEGREGATED_SIZE - 1;
}
