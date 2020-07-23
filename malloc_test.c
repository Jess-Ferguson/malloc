#include <unistd.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "malloc.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))

int main()
{
	char * mem_block[4];
	char * malloc_breaks[6];
	size_t alloc_size[4] = { 312, 4234, 40, 33333 };

	malloc_breaks[0] = sbrk(0);

	for(size_t i = 0; i < ARRAY_SIZE(mem_block); i++) {
		if(!(mem_block[i] = _malloc(alloc_size[i] * sizeof(char)))) {
			fprintf(stderr, "Error: Could not allocate memory!\n");
			return -1;
		}

		malloc_breaks[i + 1] = sbrk(0);
	}

	for(size_t i = 0; i < ARRAY_SIZE(mem_block); i++)
		memset(mem_block[i], 'A', alloc_size[i]);

	_free(mem_block[1]);
	_free(mem_block[0]);
	_free(mem_block[3]);
	_free(mem_block[2]);

	malloc_breaks[5] = sbrk(0);

	ptrdiff_t total_allocated = malloc_breaks[4] - malloc_breaks[0];
	ptrdiff_t excess_pages = (malloc_breaks[5] - malloc_breaks[0]) / PAGE_SIZE;

	printf("\n\tHeap Break Positions\n\nInitial break:\t\t%p\n", (void *) malloc_breaks[0]);

	for(size_t i = 1; i < ARRAY_SIZE(malloc_breaks) - 1; i++)
		printf("Break %zu:\t\t%p\n", i, (void *) malloc_breaks[i]);

	printf("Post-free break:\t%p\n\n", (void *) malloc_breaks[5]);

	for(size_t i = 0; i < ARRAY_SIZE(mem_block); i++)
		printf("Block %zu:\t\t%p\n", i, (void *) mem_block[i]);

	putchar('\n');

	if(excess_pages)
		printf("Error: %ld pages were not free'd\n", excess_pages);
	else
		printf("All allocated pages free'd\n");
	
	printf("Allocated %ld bytes (%ld pages)\n", total_allocated, total_allocated / PAGE_SIZE);

	return 0;
}
