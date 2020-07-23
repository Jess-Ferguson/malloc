/* 
 *	Filename:	malloc.c
 *	Author:		Jess Turner
 *	Date:		29/09/19
 *	Licence:	GNU GPL V3
 *
 *	Linux heap memory allocator implementing the malloc() and free() functions using system calls
 *
 *	Return/exit codes:
 *
 *		EXIT_SUCCESS	- No error
 *		MEM_ERROR		- Memory allocation error
 *		INPUT_ERROR		- No input given
 *
 *	User Functions:
 *
 *		_malloc()	- Allocate a memory block on the heap of a user specified size
 *		_free()		- Return a previously allocated memory block to the system if possible, otherwise return it to the unallocated memory pool
 *
 *	Helper Functions:
 *
 *		eat_next_block()	- Combine two adjacent free blocks of memory into a single block 
 *
 *	Memory Block Format:
 *
 *		Memory Header:
 *
 *			- Length of the current block
 *			- Whether or not the current block is free
 *			- A pointer to the previous memory header
 *			- A pointer to the next memory header
 *
 *		User Space Memory:
 *
 *			- The allocated memory requested by the user program
 *			- The extra padding required to align the memory to chunks of ALIGNMENT_WIDTH bytes
 *
 *	Todo:
 *	
 *		- Implement realloc and calloc
 *			- void *calloc(size_t nmemb, size_t size);
 *				- The  calloc() function allocates memory for an array of nmemb elements of size bytes each and returns a pointer to the allocated memory.  The memory is set to zero.  If nmemb or size is 0, then calloc() returns either NULL, or a unique pointer value that can later be successfully passed to free().
 *			- void *realloc(void *ptr, size_t size);
 *				- The realloc() function changes the size of the memory block pointed to by ptr to size bytes.  The contents will be unchanged in the range from the start of the region up to the minimum  of  the  old and  new  sizes.  If the new size is larger than the old size, the added memory will not be initialized.  If ptr is NULL, then the call is equivalent to malloc(size), for all values of size; if size is equal to zero, and ptr is not NULL, then the call is equivalent to free(ptr).  Unless ptr is NULL, it must have been returned by an earlier call to malloc(), calloc() or realloc().  If  the  area pointed to was moved, a free(ptr) is done.
 */

#include <unistd.h>
#include <stdbool.h>
#include "malloc.h"

static inline void eat_next_block(mem_header * header_ptr);

mem_header * head_ptr = NULL;
mem_header * tail_ptr = NULL;

void * _malloc(size_t size)
{
	if(!size)
		return NULL;

	bool heap_empty = false;
	size_t additional_space = 0;

	size += (ALIGNMENT_WIDTH - (size + sizeof(mem_header))) % ALIGNMENT_WIDTH; /* Pad the block size to fit the alignment of the system */

	if(!head_ptr) {
		/* Try and get the base pointer for the heap, as it is defined sbrk(0) could suprisingly fail */
		if((head_ptr = tail_ptr = sbrk(0)) == (void *) -1)
			return NULL;

		heap_empty = true;
	} else {
		/* Try and find enough free space to give the user */
		for(mem_header * heap_ptr = head_ptr; heap_ptr; heap_ptr = heap_ptr->next) {
			if(heap_ptr->free && heap_ptr->size >= size + sizeof(mem_header)) {
				/* Set up free block, if there is space for one */
				if(heap_ptr->size > size + 2 * sizeof(mem_header)) {
					mem_header * next_block = (mem_header *)((char *) heap_ptr + size) + 1;
					next_block->size = heap_ptr->size - (size + sizeof(mem_header));
					next_block->free = true;
					next_block->prev = heap_ptr;
					next_block->next = heap_ptr->next;
					heap_ptr->next = next_block;
				} else {
					size = heap_ptr->size;
				}

				/* Configure newly allocated block */

				heap_ptr->size = size;
				heap_ptr->free = false;

				if((tail_ptr == heap_ptr) && heap_ptr->next)
					tail_ptr = heap_ptr->next;

				return heap_ptr + 1;
			}
		}

		/* If we have a free block at the end that isn't large enough we can subtract its size from our allocation requirement */
		if(tail_ptr->free)
			additional_space = tail_ptr->size + sizeof(mem_header);
	}

	/* Determine how much we need to grow the heap by, alligned to a multiple of PAGE_SIZE bytes */

	size_t block_size = size + sizeof(mem_header) - additional_space;

	if(block_size % PAGE_SIZE != 0)
		block_size += PAGE_SIZE - (block_size % PAGE_SIZE);

	/* Grow the heap */

	if(sbrk((ptrdiff_t)block_size) == (void *) -1)
		return NULL;

	/* Configure the memory block to be returned */

	if(heap_empty) {
		tail_ptr->prev = NULL;
	} else if(!tail_ptr->free) {
		tail_ptr->next = (mem_header *)((char *) tail_ptr + tail_ptr->size) + 1;
		tail_ptr->next->prev = tail_ptr;
		tail_ptr = tail_ptr->next;
	}

	tail_ptr->next = NULL;
	tail_ptr->free = false;
	tail_ptr->size = size;

	/* Configure any free space at the top of the heap */

	void * return_ptr = tail_ptr + 1;

	size_t leftover = block_size + additional_space - (size + sizeof(mem_header));
	
	if(leftover > sizeof(mem_header)) {
		tail_ptr->next = (mem_header *)((char *) tail_ptr + size) + 1;
		tail_ptr->next->prev = tail_ptr;
		tail_ptr = tail_ptr->next;
		tail_ptr->free = true;
		tail_ptr->next = NULL;
		tail_ptr->size = leftover - sizeof(mem_header);
	} else {
		tail_ptr->size += leftover;
	}

	return return_ptr;
}

void _free(void * ptr)
{
	if(!ptr)
		return;

	mem_header * header_ptr = (mem_header *) ptr - 1;

	if(header_ptr->free)
		return;

	header_ptr->free = true;

	/* If there is a free block in front then consume it */

	if(header_ptr->next && header_ptr->next->free)
		eat_next_block(header_ptr);

	/* If there is a free block directly behind then jump to it and consume the block infront */

	if(header_ptr->prev && header_ptr->prev->free) {
		header_ptr = header_ptr->prev;
		eat_next_block(header_ptr);
	}

	/* If there is a sufficient amount of memory at the end of the heap, return it to the kernel */

	if(!header_ptr->next && header_ptr->size + sizeof(mem_header) >= PAGE_SIZE) {
		size_t leftover = (header_ptr->size + sizeof(mem_header)) % PAGE_SIZE;
		size_t excess = header_ptr->size + sizeof(mem_header) - leftover;

		if(!header_ptr->prev) {
			head_ptr = tail_ptr = NULL;
		} else {
			header_ptr->prev->size += leftover;
			header_ptr->prev->next = NULL;
		}

		sbrk(-(ptrdiff_t)excess);
	}

	return;
}

static inline void eat_next_block(mem_header * header_ptr)
{
	header_ptr->size += header_ptr->next->size + sizeof(mem_header);
	header_ptr->next = header_ptr->next->next;

	if(header_ptr->next)
		header_ptr->next->prev = header_ptr;

	return;
}
