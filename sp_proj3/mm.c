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
 * provide your information in the following struct.
 ********************************************************/
team_t team = {
	/* Your student ID */
	"20181589",
	/* Your full name*/
	"Minseok Kang",
	/* Your email address */
	"richkang715@gmail.com",
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/*Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val) | GET_TAG(p))
#define PUT_NOTAG(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_TAG(p) (GET(p) & 0x2)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))

/* Address of free block's predecessor and successor entries */
#define PRED_PTR(bp) ((char *)(bp))
#define SUCC_PTR(bp) ((char *)(bp) + WSIZE)

/* Address of free block's predecessor and successor on the seg list */
#define PRED(bp) (*(char **)(bp))
#define SUCC(bp) (*(char **)(SUCC_PTR(bp)))

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Global Variables */
void *seg_list0;	//block size 1
void *seg_list1;	//block size 2 ~ 2^2-1
void *seg_list2;	//block size 2^2 ~ 2^3-1
void *seg_list3;	//block size 2^3 ~ 2^4-1
void *seg_list4;	//block size 2^4 ~ 2^5-1
void *seg_list5;	//block size 2^5 ~ 2^6-1
void *seg_list6;	//block size 2^6 ~ 2^7-1
void *seg_list7;	//block size 2^7 ~ 2^8-1
void *seg_list8;	//block size 2^8 ~ 2^9-1
void *seg_list9;	//block size 2^9 ~ 2^10-1
void *seg_list10;	//block size 2^10 ~ 2^11-1
void *seg_list11;	//block size 2^11 ~ 2^12-1
void *seg_list12;	//block size 2^12 ~ 2^13-1
void *seg_list13;	//block size 2^13 ~ 2^14-1
void *seg_list14;	//block size 2^14 ~ 2^15-1
void *seg_list15;	//block size 2^15 ~ 2^16-1
void *seg_list16;	//block size 2^16 ~	2^17-1
void *seg_list17;	//block size 2^17 ~ 2^18-1
void *seg_list18;	//block size 2^18 ~ 2^19-1
void *seg_list19;	//block size 2^19 ~ 2^20-1
static char *heap_listp = 0;

/*
   insert_node - Insert the free block node into appropriate segregated list
*/
static void insert_node(void *bp, size_t size)
{
	void *searchp;
	void *insertp=NULL;
	int tmp=0;
	while((tmp < 19) && (size > 1)){
		size = size>>1;
		tmp++;
	}
	switch (tmp)
	{
		case 0: searchp = seg_list0; break;
		case 1: searchp = seg_list1; break;
		case 2: searchp = seg_list2; break;
		case 3: searchp = seg_list3; break;
		case 4: searchp = seg_list4; break;
		case 5: searchp = seg_list5; break;
		case 6: searchp = seg_list6; break;
		case 7: searchp = seg_list7; break;
		case 8: searchp = seg_list8; break;
		case 9: searchp = seg_list9; break;
		case 10: searchp = seg_list10; break;
		case 11: searchp = seg_list11; break;
		case 12: searchp = seg_list12; break;
		case 13: searchp = seg_list13; break;
		case 14: searchp = seg_list14; break;
		case 15: searchp = seg_list15; break;
		case 16: searchp = seg_list16; break;
		case 17: searchp = seg_list17; break;
		case 18: searchp = seg_list18; break;
		case 19: searchp = seg_list19; break;
		default: printf("Size error!!\n");
	}

	while((searchp != NULL) && (size < GET_SIZE(HDRP(searchp)))){
		insertp=searchp;
		searchp=PRED(searchp);
	}

	if(searchp != NULL) {
		if(insertp != NULL){	//In the middle of the list
			*(unsigned int*)(PRED_PTR(bp)) = (unsigned int)searchp;
			*(unsigned int*)(SUCC_PTR(searchp)) = (unsigned int)bp;
			*(unsigned int*)(SUCC_PTR(bp)) = (unsigned int)insertp;
			*(unsigned int*)(PRED_PTR(insertp)) = (unsigned int)bp;
		}
		else {  //At the last of the list
			*(unsigned int*)(PRED_PTR(bp)) = (unsigned int)searchp;
			*(unsigned int*)(SUCC_PTR(searchp)) = (unsigned int)bp;
			*(unsigned int*)(SUCC_PTR(bp)) = (unsigned int)NULL;
			switch (tmp) //seg_list points the last block of the list
			{
				case 0: seg_list0 = bp; break;
				case 1: seg_list1 = bp; break;
				case 2: seg_list2 = bp; break;
				case 3: seg_list3 = bp; break;
				case 4: seg_list4 = bp; break;
				case 5: seg_list5 = bp; break;
				case 6: seg_list6 = bp; break;
				case 7: seg_list7 = bp; break;
				case 8: seg_list8 = bp; break;
				case 9: seg_list9 = bp; break;
				case 10: seg_list10 = bp; break;
				case 11: seg_list11 = bp; break;
				case 12: seg_list12 = bp; break;
				case 13: seg_list13 = bp; break;
				case 14: seg_list14 = bp; break;
				case 15: seg_list15 = bp; break;
				case 16: seg_list16 = bp; break;
				case 17: seg_list17 = bp; break;
				case 18: seg_list18 = bp; break;
				case 19: seg_list19 = bp; break;
				default: printf("Size error!!\n");
			}
		}
	}
	else {
		if(insertp != NULL) { // In front of the list
			*(unsigned int*)(PRED_PTR(bp)) = (unsigned int)NULL;
			*(unsigned int*)(SUCC_PTR(bp)) = (unsigned int)insertp;
			*(unsigned int*)(PRED_PTR(insertp)) = (unsigned int)bp;
		} else { //Putting into the blank list (make bp the first in list)
			*(unsigned int*)(PRED_PTR(bp)) = (unsigned int)NULL;
			*(unsigned int*)(SUCC_PTR(bp)) = (unsigned int)NULL;
			switch (tmp)
			{
				case 0: seg_list0 = bp; break;
				case 1: seg_list1 = bp; break;
				case 2: seg_list2 = bp; break;
				case 3: seg_list3 = bp; break;
				case 4: seg_list4 = bp; break;
				case 5: seg_list5 = bp; break;
				case 6: seg_list6 = bp; break;
				case 7: seg_list7 = bp; break;
				case 8: seg_list8 = bp; break;
				case 9: seg_list9 = bp; break;
				case 10: seg_list10 = bp; break;
				case 11: seg_list11 = bp; break;
				case 12: seg_list12 = bp; break;
				case 13: seg_list13 = bp; break;
				case 14: seg_list14 = bp; break;
				case 15: seg_list15 = bp; break;
				case 16: seg_list16 = bp; break;
				case 17: seg_list17 = bp; break;
				case 18: seg_list18 = bp; break;
				case 19: seg_list19 = bp; break;
				default: printf("Size error!!\n");
			}
		}
	}
	return;
}

/*
   delete_node - Delete the free block node from appropriate segregated list
*/
static void delete_node(void *bp)
{
	int tmp=0;
	size_t size = GET_SIZE(HDRP(bp));

	while((tmp < 19) && (size > 1)){
		size = size>>1;
		tmp++;
	}

	if(PRED(bp) != NULL){
		if(SUCC(bp) != NULL){ //If bp in the middle of the list
			*(unsigned int*)(SUCC_PTR(PRED(bp))) = (unsigned int)(SUCC(bp));
			*(unsigned int*)(PRED_PTR(SUCC(bp))) = (unsigned int)(PRED(bp));
		}
		else {	//If bp in the last of the list
			*(unsigned int*)(SUCC_PTR(PRED(bp))) = (unsigned int)NULL;
			switch (tmp)
			{
				case 0: seg_list0 = PRED(bp); break;
				case 1: seg_list1 = PRED(bp); break;
				case 2: seg_list2 = PRED(bp); break;
				case 3: seg_list3 = PRED(bp); break;
				case 4: seg_list4 = PRED(bp); break;
				case 5: seg_list5 = PRED(bp); break;
				case 6: seg_list6 = PRED(bp); break;
				case 7: seg_list7 = PRED(bp); break;
				case 8: seg_list8 = PRED(bp); break;
				case 9: seg_list9 = PRED(bp); break;
				case 10: seg_list10 = PRED(bp); break;
				case 11: seg_list11 = PRED(bp); break;
				case 12: seg_list12 = PRED(bp); break;
				case 13: seg_list13 = PRED(bp); break;
				case 14: seg_list14 = PRED(bp); break;
				case 15: seg_list15 = PRED(bp); break;
				case 16: seg_list16 = PRED(bp); break;
				case 17: seg_list17 = PRED(bp); break;
				case 18: seg_list18 = PRED(bp); break;
				case 19: seg_list19 = PRED(bp); break;
				default: printf("Size error!!\n");
			}
		}
	}
	else {
		if(SUCC(bp) != NULL) { //If bp is the first of the list
			*(unsigned int*)(PRED_PTR(SUCC(bp))) = (unsigned int)NULL;
		}
		else {	//If bp is the only block in the list
			switch (tmp)
			{
				case 0: seg_list0 = NULL; break;
				case 1: seg_list1 = NULL; break;
				case 2: seg_list2 = NULL; break;
				case 3: seg_list3 = NULL; break;
				case 4: seg_list4 = NULL; break;
				case 5: seg_list5 = NULL; break;
				case 6: seg_list6 = NULL; break;
				case 7: seg_list7 = NULL; break;
				case 8: seg_list8 = NULL; break;
				case 9: seg_list9 = NULL; break;
				case 10: seg_list10 = NULL; break;
				case 11: seg_list11 = NULL; break;
				case 12: seg_list12 = NULL; break;
				case 13: seg_list13 = NULL; break;
				case 14: seg_list14 = NULL; break;
				case 15: seg_list15 = NULL; break;
				case 16: seg_list16 = NULL; break;
				case 17: seg_list17 = NULL; break;
				case 18: seg_list18 = NULL; break;
				case 19: seg_list19 = NULL; break;
				default: printf("Size error!!\n");
			}
		}
	}
	return;
}

/*
   coalesce - Boundary tag coalescing. Return ptr to coalesced block
*/
static void *coalesce(void *bp)
{
	size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));

	if(GET_TAG(HDRP(PREV_BLKP(bp))))
		prev_alloc = 1;

	if(prev_alloc && next_alloc){			/* Case 1 */
		return bp;
	}

	else if(prev_alloc && !next_alloc){		/* Case 2 */
		delete_node(bp);
		delete_node(NEXT_BLKP(bp));
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
	}

	else if(!prev_alloc && next_alloc){		/* Case 3 */
		delete_node(bp);
		delete_node(PREV_BLKP(bp));
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		PUT(FTRP(bp), PACK(size, 0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}

	else {									/* Case 4 */
		delete_node(bp);
		delete_node(PREV_BLKP(bp));
		delete_node(NEXT_BLKP(bp));
		size += GET_SIZE(HDRP(PREV_BLKP(bp)))+GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}
	insert_node(bp, size);
	return bp;
}

/*
	extend_heap - Extend heap with free block and return its block pointer
*/
static void *extend_heap(size_t words)
{
	void *bp;
	size_t size;

	size = ALIGN(words);
	if((long)(bp = mem_sbrk(size))==-1)
		return NULL;

	PUT_NOTAG(HDRP(bp), PACK(size, 0));
	PUT_NOTAG(FTRP(bp), PACK(size, 0));
	PUT_NOTAG(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
	insert_node(bp, size);

	return coalesce(bp);
}

/*
    place - Place block of asize bytes at start of free block bp
			and split if remainder would be at least minimum block size
*/
static void *place(void *bp, size_t asize)
{
	size_t csize = GET_SIZE(HDRP(bp));
	size_t rem = csize - asize;	//remainder size

	delete_node(bp);

	if(rem <= 2*DSIZE){			//Don't split the block
		PUT(HDRP(bp), PACK(csize, 1));
		PUT(FTRP(bp), PACK(csize, 1));
	}
	else if(asize >= 100) {		//Split the block and return next block
		PUT(HDRP(bp), PACK(rem, 0));
		PUT(FTRP(bp), PACK(rem, 0));
		PUT_NOTAG(HDRP(NEXT_BLKP(bp)), PACK(asize,1));
		PUT_NOTAG(FTRP(NEXT_BLKP(bp)), PACK(asize,1));
		insert_node(bp, rem);
		return NEXT_BLKP(bp);
	}
	else {						//Split the block and reutnr current block
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
		PUT_NOTAG(HDRP(NEXT_BLKP(bp)), PACK(rem, 0));
		PUT_NOTAG(FTRP(NEXT_BLKP(bp)), PACK(rem, 0));
		insert_node(NEXT_BLKP(bp), rem);
	}
	return bp;
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{

	seg_list0 = NULL;
	seg_list1 = NULL;
	seg_list2 = NULL;
	seg_list3 = NULL;
	seg_list4 = NULL;
	seg_list5 = NULL;
	seg_list6 = NULL;
	seg_list7 = NULL;
	seg_list8 = NULL;
	seg_list9 = NULL;
	seg_list10 = NULL;
	seg_list11 = NULL;
	seg_list12 = NULL;
	seg_list13 = NULL;
	seg_list14 = NULL;
	seg_list15 = NULL;
	seg_list16 = NULL;
	seg_list17 = NULL;
	seg_list18 = NULL;
	seg_list19 = NULL;

	/* Create the initial empty heap */
	if((long)(heap_listp = mem_sbrk(4 * WSIZE))==-1)
		return -1;

	PUT_NOTAG(heap_listp, 0);						/* Alignment padding */
	PUT_NOTAG(heap_listp+(1*WSIZE), PACK(DSIZE, 1));/* Prologue header */
	PUT_NOTAG(heap_listp+(2*WSIZE), PACK(DSIZE, 1));/* Prologue footer */
	PUT_NOTAG(heap_listp+(3*WSIZE), PACK(0, 1));	/* Epilogue header */
	heap_listp += (2*WSIZE);

	/* Extend the empty heap with a free block of CHUNKSIZE bytes */
	if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
		return -1;

	return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
	size_t asize;		/* Adjusted block size */
	size_t extendsize;	/* Amount to extend heap if no fit */
	void *ptr=NULL;
	void *which_list;

	if(heap_listp == 0){
		mm_init();
	}

	/* Ignore spurious requests */
	if(size==0)
		return NULL;

	/* Adjust block size to include overhead and alignment reqs. */
	if(size<=DSIZE)
		asize = 2*DSIZE;
	else
		asize = ALIGN(size + SIZE_T_SIZE);

	int tmp=0;
	size_t search = asize;

	while(tmp < 20) {
		switch (tmp)
		{
			case 0: which_list = seg_list0; break;
			case 1: which_list = seg_list1; break;
			case 2: which_list = seg_list2; break;
			case 3: which_list = seg_list3; break;
			case 4: which_list = seg_list4; break;
			case 5: which_list = seg_list5; break;
			case 6: which_list = seg_list6; break;
			case 7: which_list = seg_list7; break;
			case 8: which_list = seg_list8; break;
			case 9: which_list = seg_list9; break;
			case 10: which_list = seg_list10; break;
			case 11: which_list = seg_list11; break;
			case 12: which_list = seg_list12; break;
			case 13: which_list = seg_list13; break;
			case 14: which_list = seg_list14; break;
			case 15: which_list = seg_list15; break;
			case 16: which_list = seg_list16; break;
			case 17: which_list = seg_list17; break;
			case 18: which_list = seg_list18; break;
			case 19: which_list = seg_list19; break;
			default: printf("Size error!!\n");
		}
		if((tmp == 19) || ((which_list != NULL) && (search <= 1))){
			ptr = which_list;
			while((ptr!=NULL) && ((GET_SIZE(HDRP(ptr))<asize) || (GET_TAG(HDRP(ptr)))))
			{
				ptr=PRED(ptr);
			}
			if(ptr!=NULL) break;
		}
		search = search >> 1;
		tmp++;
	}

	if(ptr != NULL){
		ptr=place(ptr, asize);
	}
	/* No fit found. Get more memory and place the block */
	else if(ptr == NULL){
		extendsize = MAX(asize, CHUNKSIZE);
		if((ptr = extend_heap(extendsize)) == NULL)
			return NULL;

		ptr=place(ptr, asize);
	}
	return ptr;
}

/*
 * mm_free - Freeing a block
 */
void mm_free(void *ptr)
{
	if(ptr == 0)
		return;

	size_t size = GET_SIZE(HDRP(ptr));
	
	if(heap_listp == 0)
		mm_init();

	PUT(HDRP(ptr), PACK(size, 0));
	PUT(FTRP(ptr), PACK(size, 0));
	insert_node(ptr, size);
	coalesce(ptr);

	return;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */

void *mm_realloc(void *ptr, size_t size)
{
	void *newptr = ptr;
	int rem;
	size_t copySize;
	if(size==0){
		mm_free(ptr);
		return 0;
	}

	if(size <= DSIZE) size = 2*DSIZE;
	else size = ALIGN(size+DSIZE);

	if(GET_SIZE(HDRP(ptr)) < size) {
		/* If next block is free block or Epilogue block */
		if(GET_ALLOC(HDRP(NEXT_BLKP(ptr)))==0 || GET_SIZE(HDRP(NEXT_BLKP(ptr)))==0){
			rem = GET_SIZE(HDRP(ptr)) + GET_SIZE(HDRP(NEXT_BLKP(ptr))) - size;
			if(rem < 0) {
				if(extend_heap(CHUNKSIZE) == NULL)
					return NULL;
				rem += CHUNKSIZE;
			}
			delete_node(NEXT_BLKP(ptr));

			/* No splitting */
			PUT_NOTAG(HDRP(ptr), PACK(size + rem, 1));
			PUT_NOTAG(FTRP(ptr), PACK(size + rem, 1));
		}
		else {
			newptr = mm_malloc(size);
			if(newptr == NULL)
				return NULL;
			copySize = *(size_t *)((char *)ptr - SIZE_T_SIZE);
			if(size < copySize)
				copySize = size;
			memcpy(newptr, ptr, copySize);
			mm_free(ptr);
		}
	}
	return newptr;
}

