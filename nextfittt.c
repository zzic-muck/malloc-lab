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
#define CHUNKSIZE (1 << 12)
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))              // 주소 읽기
#define PUT(p, val) (*(unsigned int *)(p) = (val)) // 주소 쓰기

#define GET_SIZE(p) (GET(p) & ~0x7) //
#define GET_ALLOC(p) (GET(p) & 0x1) //

#define HDRP(bp) ((char *)(bp)-WSIZE)                        // 헤더주소
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // 푸터주소

/* [[헤드]bp->[페이로드][푸터]] 워드만큼 빼면 지금블럭 헤드가 나오고 더블워드만큼 빼면 이전블록 푸터가 나옴*/
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE))) // 다음블록 주소
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))   // 이전블록 주소

#define NEXT_FREE(bp) (*(void**)(bp + WSIZE))
#define PREV_FREE(bp) (*(void**)(bp))


#define set_next(bp, addr) *bp = addr
#define set_prev(bp,addr) *(bp+WSIZE) = addr

static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
void add_free_blm(void *bp);
void remove_free_blm(void *bp);

static void *heap_listp;
static void *free_listp;
static void *next_p;
static void *temp;


/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(6 * WSIZE)) == (void *)-1)
    {
        return -1;
    }

    PUT(heap_listp, 0);                            // 얼라이먼트 패딩
    PUT(heap_listp + (1 * WSIZE), PACK(2*DSIZE, 1)); // 프롤로그헤더-----|
//                bp위치    일껄?                                        | 
    PUT(heap_listp + (2 * WSIZE),NULL); // 이전꺼 가 될놈         |
    PUT(heap_listp + (3 * WSIZE),NULL); // 다음꺼 가 될놈         | 한블럭   
//                                                                      |
    PUT(heap_listp + (4 * WSIZE), PACK(2*DSIZE, 1)); // 프롤로그푸터-----|   
    PUT(heap_listp + (5 * WSIZE), PACK(0, 1));     // 에필로그 헤더
    
    heap_listp += (2 * WSIZE); //힙의 시작점 근본
    free_listp = heap_listp; // 프리 리스트 포인터
    next_p = heap_listp; // 넥스핏용 포인터

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
// 


static void *coalesce(void *bp){

    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if(prev_alloc && next_alloc){
    
        return bp;
    }
    else if (prev_alloc && !next_alloc){

        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc){

        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else{
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));    
        bp = PREV_BLKP(bp);
    }

    next_p = bp;
    return bp;
}




//  static void *find_fit(size_t asize){

//     void *bp;

//     for (bp = heap_listp; GET_SIZE(HDRP(bp)) >0; bp = NEXT_BLKP(bp)){
//         if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){
//             return bp;
//         }
//     }
//     return NULL;
// }


static void *find_fit(size_t asize)
{

    void *bp;

    for (bp = next_p; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
        {
            return bp;
        }
    }

    for (bp = heap_listp; bp != next_p; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
        {
            return bp;
        }
    }

    return NULL;
}

static void place(void *bp, size_t asize)
{

    size_t csize = GET_SIZE(HDRP(bp));
    if ((csize - asize) >= (2 * DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        next_p = bp;
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        next_p = bp;
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

    if (size <= DSIZE){
        need_size = 2 * DSIZE;
    }
    else{
        need_size = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }    

    if(need_size <= oldsize){

        return bp;
    }
    
   
    if( GET_ALLOC(HDRP(NEXT_BLKP(bp))) == 0 && need_size <= oldsize + GET_SIZE(HDRP(NEXT_BLKP(bp)))){

       ssiizzee = oldsize + GET_SIZE(HDRP(NEXT_BLKP(bp)));

       PUT(HDRP(bp), PACK(ssiizzee , 1));
       PUT(FTRP(bp), PACK(ssiizzee , 1));

    //    printf("ssiizzee   %d    헤더푸터%d\n", ssiizzee, FTRP(bp) - HDRP(bp));
       next_p = bp;
       return bp;

    }


    else{
    new_bp = mm_malloc(size);
    if (new_bp == NULL)
    {
        return NULL;
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
    
}
