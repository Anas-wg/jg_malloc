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
    "room301_team_seven",
    /* First member's full name */
    "Anas",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
#define WSIZE 4 // word size
#define DSIZE 8 // double word size
#define CHUNKSIZE (1 << 12) // 힙을 늘릴 사이즈(4KB)


#define MAX(x, y) ((x) > (y) ? (x) : (y))
// 워드 안에 할당된 비트와 size를 묶음
#define PACK(size, alloc) ((size) | (alloc))

// 주소 p 에 워드 읽기/쓰기
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

// 주소 p로부터 사이즈, 할당된 영역 가져오는 매크로
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1) // 할당 비트 가져오기

// 포인터 bp가 가리키는 블록의 헤더와 푸터 주소를 계산
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// 포인터 bp가 가리키는 블록의 다음블록과 이전 블록을 계산
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))


/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))


static char *heap_listp = NULL;
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *first_fit(size_t asize);
static void place(void *bp, size_t asize);
static void *next_fit(size_t asize);


/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{   
    // empty heap 생성
    if((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1){
        return -1;
    }
    // 패딩 워드 가르키는 포인터에 주소값 더해서 각각 구현
    PUT(heap_listp, 0); // 정렬 위한 Padding
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); // Prologue 헤더, DSIZE(8Byte), => 할당상태 1로 변경
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); // Prologue footer
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1)); // Epilouge Header
    heap_listp += (2 * WSIZE);

    // CHUNKSIZE 의 byte 만큼 빈 힙을 연장
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL){
        return -1;
    }

    return 0;
}

static void *extend_heap(size_t words){
    char *bp; // 블록 포인터
    size_t size; // size

    // 정렬 유지 위해 짝수로 변환, 홀수면 1 더해서 활용
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1){
        return NULL;
    }

    // 가용 블록의 헤더/푸터, 에블로그 헤더 초기화
    PUT(HDRP(bp), PACK(size, 0)); // 블록 헤더
    PUT(FTRP(bp), PACK(size, 0)); // 블록 푸터
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // 새로운 에필로그 헤더

    // 이전 블럭도 가용상태라면 병합
    return coalesce(bp);
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    // int newsize = ALIGN(size + SIZE_T_SIZE);
    // void *p = mem_sbrk(newsize);
    // if (p == (void *)-1)
    //     return NULL;
    // else
    // {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }

    size_t asize;
    size_t extendsize;
    char *bp;

    // 예외처리
    if(size == 0){
        return NULL;
    }
    // 요청 크기 조정
    // 헤더(4) + Payload(??) + 푸터(4) 구조
    if(size <= DSIZE){
        // 요청 크기가 8바이트 이하, 최소 블록 크기인 16바이트 활용
        asize = 2 * DSIZE;
    } else {
        // 8바이트 초과시 합 보고 8의 배수로 변경
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE); 
    }

    // 가용 블록 탐색(First Fit)
    if ((bp = first_fit(asize)) != NULL) {
        place(bp, asize);  // 블록 배치 및 분할
        return bp;
    }

    // 만약 적당한 가용 블럭 없다면 커널에게 요청
    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL){
        return NULL;
    }
    place(bp, asize);
    return bp;
}

static void *last_fitp = NULL; // 마지막 탐색 위치 저장용

static void *next_fit(size_t asize){
    void *bp;
    if (last_fitp == NULL) {
        last_fitp = heap_listp;
    }
    
    for (bp = last_fitp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
         if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            last_fitp = bp;
            return bp;
            }
    }
    
    return NULL;
}


static void *first_fit(size_t asize) {
    // heap_listp ~ 에필로그 까지
    void *bp = heap_listp;
    for (bp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
        if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){
            return bp;
        }
    }
    return NULL;
}

static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp)); // 현재 가용 블록의 크기

    // 블록 분할 가능 여부 확인
    if ((csize - asize) >= (2 * DSIZE)) {
        // 1. 앞부분 asize만큼 할당
        PUT(HDRP(bp), PACK(asize, 1));           // 헤더: 할당 표시
        PUT(FTRP(bp), PACK(asize, 1));           // 풋터: 할당 표시

        // 2. 나머지 뒷부분을 새로운 가용 블록으로 설정
        bp = NEXT_BLKP(bp);                          // 다음 블록으로 이동
        PUT(HDRP(bp), PACK(csize - asize, 0));   // 새 가용 블록 헤더
        PUT(FTRP(bp), PACK(csize - asize, 0));   // 새 가용 블록 풋터
    } else {
        // 분할 불가 → 전체 블록 할당 처리
        PUT(HDRP(bp), PACK(csize, 1));           // 헤더: 전체 할당
        PUT(FTRP(bp), PACK(csize, 1));           // 풋터: 전체 할당
    }
}



/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    // 해제하려는 블록 사이즈 알기
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0)); // 헤제하려는 블록 헤더
    PUT(FTRP(bp), PACK(size, 0)); // 헤제하려는 블록 푸터
    coalesce(bp); // 4가지 케이스 병합 체크
}

static void *coalesce(void *bp){

    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));  // 헤제 블럭 이전 블럭 할당 확인
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 블럭
    size_t size = GET_SIZE(HDRP(bp)); // 헤제 하려는 블럭 Size

    // CASE 1, 앞, 뒤 모두 할당 상태
    if(prev_alloc && next_alloc) {
        return bp; // 현재 블록만 가능
    }
    // CASE 2. 앞은 할당, 뒤는 free
    else if(prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
    } 
    // CASE 3. 앞이 free 뒤는 할당
    else if(!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    // CASE 4. 앞 free, 뒤 Free
    else {
        size += (GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    // last_fitp = bp;
    return bp;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

