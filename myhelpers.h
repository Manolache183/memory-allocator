#pragma once

#include "helpers.h"

#define MMAP_THRESHOLD 128 * 1024
#define PREALLOC_SIZE 128 * 1024
#define STATUS_HEAD 3
#define ALLOC_BASE 8
#define META_SIZE sizeof(struct block_meta)
#define MALLOC_METHOD 0
#define CALLOC_METHOD 1
#define REALLOC_METHOD 2
#define PAGE_SIZE sysconf(_SC_PAGE_SIZE)

extern struct block_meta mem_list_head;
extern int pre;
extern int method;

void mem_list_init(void);
void block_split(struct block_meta *block, size_t size);
struct block_meta *mem_list_add(size_t size);
struct block_meta *find_best_fit(size_t size);
size_t mem_list_size();
struct block_meta *find_node(void *ptr);
void delete_node(struct block_meta *block);
struct block_meta *realloc_logic(struct block_meta *block, size_t size);