// SPDX-License-Identifier: BSD-3-Clause

#include "myhelpers.h"

struct block_meta mem_list_head;
int method = MALLOC_METHOD;
int pre;

void mem_list_init(void)
{
	mem_list_head.next = NULL;
	mem_list_head.size = 0;
	mem_list_head.status = STATUS_HEAD;
}

static struct block_meta *prealloc(void)
{
	void *r = sbrk(0);

	DIE(r == (void *) -1, "sbrk");

	struct block_meta *block = sbrk(PREALLOC_SIZE);

	DIE((void *)block == (void *) -1, "sbrk");

	block->size = PREALLOC_SIZE - META_SIZE;
	block->status = STATUS_FREE;
	block->next = NULL;

	return block;
}

void block_split(struct block_meta *block, size_t size)
{
	if (block->size < size + META_SIZE + 8) {
		block->status = STATUS_ALLOC;
		return;
	}

	struct block_meta *new_block = (void *) block + META_SIZE + size;

	new_block->size = block->size - size - META_SIZE;
	new_block->status = STATUS_FREE;
	new_block->next = block->next;

	block->size = size;
	block->status = STATUS_ALLOC;
	block->next = new_block;
}

static struct block_meta *mem_list_alloc(size_t size)
{
	unsigned int threshold;

	if (method == MALLOC_METHOD)
		threshold = MMAP_THRESHOLD;
	else if (method == CALLOC_METHOD)
		threshold = PAGE_SIZE;

	if (size + META_SIZE <= threshold) {
		if (pre == 0) {
			struct block_meta *block = prealloc();

			pre = 1;
			block_split(block, size);
			return block;
		}

		void *r = sbrk(0);

		DIE(r == (void *) -1, "sbrk");

		struct block_meta *block = sbrk(size + META_SIZE);

		DIE((void *)block == (void *) -1, "sbrk");

		block->size = size;
		block->status = STATUS_ALLOC;
		block->next = NULL;

		return block;
	}

	struct block_meta *block = mmap(NULL, size + META_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	DIE(block == MAP_FAILED, "mmap");

	block->size = size;
	block->status = STATUS_MAPPED;
	block->next = NULL;

	return block;
}

struct block_meta *mem_list_add(size_t size)
{
	struct block_meta *block = mem_list_alloc(size);

	struct block_meta *current = &mem_list_head;

	while (current->next != NULL)
		current = current->next;

	current->next = block;
	return block;
}

static void block_coalesce(struct block_meta *block)
{
	struct block_meta *next_block = block->next;

	block->size += next_block->size + META_SIZE;
	block->next = next_block->next;
}

static void mem_list_coalesce_free(struct block_meta *head)
{
	struct block_meta *current = head;
	struct block_meta *prev;

	while (current->next != NULL) {
		prev = current;
		current = current->next;

		if (current->status == STATUS_FREE && prev->status == STATUS_FREE) {
			block_coalesce(prev);
			current = prev;
		}
	}
}

struct block_meta *find_best_fit(size_t size)
{
	mem_list_coalesce_free(&mem_list_head);

	struct block_meta *current = &mem_list_head;

	struct block_meta *best_fit = NULL;

	while (current->next != NULL) {
		current = current->next;

		if (current->status == STATUS_FREE && current->size >= size)
			if (best_fit == NULL || current->size < best_fit->size)
				best_fit = current;
	}

	if (best_fit != NULL)
		block_split(best_fit, size);

	if (best_fit == NULL && current->status == STATUS_FREE) {
		// Expand the current block

		struct block_meta *next = mem_list_add(size - current->size - META_SIZE);

		next->status = STATUS_FREE;
		mem_list_coalesce_free(current);
		current->size = size;
		current->status = STATUS_ALLOC;
		best_fit = current;
	}

	return best_fit;
}

size_t mem_list_size(void)
{
	size_t size = 0;
	struct block_meta *current = &mem_list_head;

	while (current->next != NULL) {
		current = current->next;
		size += current->size;
	}

	return size;
}

struct block_meta *find_node(void *ptr)
{
	struct block_meta *current = &mem_list_head;

	while (current->next != NULL) {
		current = current->next;

		if ((void *)current + META_SIZE == ptr)
			return current;
	}

	return NULL;
}

void delete_node(struct block_meta *block)
{
	// Used only in memory allocated with mmap

	struct block_meta *current = &mem_list_head;

	while (current->next != NULL) {
		if (current->next == block) {
			current->next = block->next;
			munmap(block, block->size + META_SIZE);
			return;
		}

		current = current->next;
	}
}

struct block_meta *realloc_logic(struct block_meta *block, size_t size)
{
	// Try to expand current block, either by coaleascing with the next blocks or just by expanding the end of the list

	if (size <= block->size) {
		block_split(block, size);
		return block;
	}

	while (block->next != NULL && block->next->status == STATUS_FREE && block->size < size)
		block_coalesce(block);

	if (block->size >= size) {
		block_split(block, size);
		return block;
	}

	if (block->next == NULL) {
		mem_list_add(size - block->size - META_SIZE);
		block_coalesce(block);
		block->status = STATUS_ALLOC;
		return block;
	}

	return NULL;
}
