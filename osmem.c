// SPDX-License-Identifier: BSD-3-Clause

#include "osmem.h"
#include "helpers.h"
#include "myhelpers.h"

void *os_malloc(size_t size)
{
	if (size == 0)
		return NULL;

	while ((size + META_SIZE) % ALLOC_BASE != 0)
		size++;

	if (mem_list_size() == 0)
		mem_list_init();

	unsigned int threshold;

	if (method == MALLOC_METHOD)
		threshold = MMAP_THRESHOLD;
	else if (method == CALLOC_METHOD)
		threshold = PAGE_SIZE;

	struct block_meta *block = NULL;

	// Find best fit only for heap-allocated memory
	if (size + META_SIZE <= threshold)
		block = find_best_fit(size);

	if (block == NULL)
		block = mem_list_add(size);


	return ((void *)block + META_SIZE);
}

void os_free(void *ptr)
{
	if (ptr == NULL)
		return;

	struct block_meta *block = find_node(ptr);

	if (block->status == STATUS_MAPPED)
		delete_node(block);
	else
		block->status = STATUS_FREE;
}

void *os_calloc(size_t nmemb, size_t size)
{
	if (size == 0 || nmemb == 0)
		return NULL;

	// To change compare threshold
	method = CALLOC_METHOD;

	void *p = os_malloc(nmemb * size);

	memset(p, 0, nmemb * size);

	method = MALLOC_METHOD;

	return p;
}

void *os_realloc(void *ptr, size_t size)
{
	if (ptr == NULL)
		return os_malloc(size);


	if (size == 0) {
		os_free(ptr);
		return NULL;
	}

	while ((size + META_SIZE) % ALLOC_BASE != 0)
		size++;

	struct block_meta *block = find_node(ptr);

	if (block == NULL || block->status == STATUS_FREE)
		return NULL;

	if (block->status == STATUS_MAPPED || ((size + META_SIZE) > MMAP_THRESHOLD)) {
		void *p = os_malloc(size);

		if (size < block->size)
			memcpy(p, ptr, size);
		else
			memcpy(p, ptr, block->size);

		os_free(ptr);
		return p;

	} else if (block->status == STATUS_ALLOC) {
		// Try to expand block / coalesce with next free blocks
		struct block_meta *new_block = realloc_logic(block, size);

		// Need to create and entirely new block
		if (new_block == NULL) {
			void *p = os_malloc(size);

			memcpy(p, ptr, block->size);
			os_free(ptr);
			return p;
		} else {
			return ((void *)new_block + META_SIZE);
		}
	}

	return NULL;
}
