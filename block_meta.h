/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <errno.h>
#include <stdio.h>

#define ALIGN(size) (((size) + (8 - 1)) & ~(8 - 1))
#define MMAP_THRESHOLD (128 * 1024)

#define DIE(assertion, call_description)									\
	do {													\
		if (assertion) {										\
			fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__);					\
			perror(call_description);								\
			exit(errno);										\
		}												\
	} while (0)

/* Structure to hold memory block metadata */
struct block_meta {
	size_t size;
	int status;
	struct block_meta *prev;
	struct block_meta *next;
};

void insert_block_in_list(struct block_meta **head, struct block_meta *curr, int STAT);
void remove_block_from_list(struct block_meta **head, struct block_meta *curr);
void *get_address(struct block_meta *curr, size_t blk_size);
struct block_meta *get_block(void *ptr, size_t blk_size);
struct block_meta *find_good_size_block(struct block_meta *head, size_t size, size_t blk_size);
struct block_meta *split_block(struct block_meta *init, size_t size, size_t blk_size);
struct block_meta *expand(struct block_meta *head, struct block_meta *curr, size_t size, size_t blk_size);

/* Block metadata status values */
#define STATUS_FREE   0
#define STATUS_ALLOC  1
#define STATUS_MAPPED 2
  