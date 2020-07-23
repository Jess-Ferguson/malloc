#ifndef _MALLOC_H
#define _MALLOC_H

#include <stdbool.h>
#include <unistd.h>
#include <stddef.h>

#ifdef PAGESIZE
	#define PAGE_SIZE PAGESIZE
#elif defined PAGE_SHIFT
	#define PAGE_SIZE (1 << PAGE_SHIFT)
#else
	#define PAGE_SIZE 4096
#endif

#define ALIGNMENT_WIDTH 16

typedef struct mem_header mem_header;

struct mem_header {
	size_t size;
	bool free;
	mem_header * prev;
	mem_header * next;
};

void * _malloc(size_t size);
void _free(void * ptr);

#endif
