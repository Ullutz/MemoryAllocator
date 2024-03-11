// SPDX-License-Identifier: BSD-3-Clause

#include "block_meta.h"
#include "osmem.h"

void insert_block_in_list(struct block_meta **head, struct block_meta *curr, int STAT)
{
	if (!(*head)) {
		*head = curr;
		return;
	}

	struct block_meta *iterator = *head;

	/* everything that is mapped is after everything that is sbrk-ed to keep */
	/* the list clean, because we mmpap bigger chunks of memory */
	if ((*head)->status == STATUS_MAPPED) {
		if (STAT == STATUS_MAPPED) {
			while (iterator->next)
				iterator = iterator->next;

			iterator->next = curr;
		} else {
			curr->next = (*head);
			(*head) = curr;
		}

		return;
	}

	iterator = *head;

	if (STAT == STATUS_MAPPED) {
		while (iterator->next)
			iterator = iterator->next;

		iterator->next = curr;
		curr->next = NULL;
	} else if (STAT == STATUS_ALLOC) {
		while (iterator->next && iterator->next->status != STATUS_MAPPED)
			iterator = iterator->next;

		curr->next = iterator->next;
		iterator->next = curr;
	}
}

void remove_block_from_list(struct block_meta **head, struct block_meta *curr)
{
	/* this is inspired from my data structures hw from last year */
	if (!*head)
		return;

	if (!(*head)->next) {
		*head = NULL;
		return;
	}

	struct block_meta *iterator = *head;

	while (iterator->next && iterator->next != curr)
		iterator = iterator->next;

	/* this means we didnt find the block */
	if (!iterator->next)
		return;

	iterator->next = curr->next;
}

void *get_address(struct block_meta *curr, size_t blk_size)
{
	return (void *) (((char *) curr) + blk_size);
}

struct block_meta *get_block(void *ptr, size_t blk_size)
{
	return (struct block_meta *) (((char *) ptr) - blk_size);
}

struct block_meta *split_block(struct block_meta *bl, size_t size, size_t blk_size)
{
	/* i have a block bigger than what i need so i split it in 2 */
	/* one sbkr-ed block and one free block*/
	if (bl->size >= size + ALIGN(1) + blk_size) {
		struct block_meta *new = (struct block_meta *) ((char *) bl + blk_size + size);

		/* i set the parameters for the new block that is basically free */
		new->size = bl->size - size - blk_size;
		new->status = STATUS_FREE;
		new->next = bl->next;

		/* i update the block */
		bl->size = size;
		bl->status = STATUS_ALLOC;
		bl->next = new;
	}

	return bl;
}

void merge_free_blocks(struct block_meta *head, size_t blk_size)
{
	struct block_meta *iterator = head;

	while (iterator->next) {
		if (iterator->status == STATUS_FREE && iterator->next->status == STATUS_FREE) {
			/* i add to the size of the block the size of the next block and the size of a block_meta struct */
			/* because of the struct that is between them */
			iterator->size += iterator->next->size + blk_size;
			iterator->next = iterator->next->next;
		} else {
			iterator = iterator->next;
		}
	}
}

struct block_meta *expand_block(struct block_meta *iterator, size_t size)
{
	void *ret = sbrk(size - iterator->size);

	DIE(ret == (void *) -1, "SBRK faield!");

	iterator->next = NULL;
	iterator->size = size;
	iterator->prev = NULL;
	iterator->status = STATUS_ALLOC;

	return iterator;
}

struct block_meta *find_good_size_block(struct block_meta *head, size_t size, size_t blk_size)
{

	if (!head)
		return NULL;

	merge_free_blocks(head, blk_size);

	struct block_meta *iterator = head;
	struct block_meta *chosen_block = NULL;

	while (iterator) {
		if (iterator->status == STATUS_FREE && iterator->size >= size) {
			/* i see if the current block is good enough */
			if (!chosen_block)
				chosen_block = iterator;
			else if (iterator->size < chosen_block->size)
				chosen_block = iterator;
		}
		iterator = iterator->next;
	}

	/* if i found one i just return it i set its status as alloc, after i split it to exactly */
	/* the size that i need */
	if (chosen_block) {
		chosen_block->status = STATUS_ALLOC;
		return split_block(chosen_block, size, blk_size);
	}

	/* if i didnt find a good block i try to expand the last one on the heap */
	/* so i go to the end of the list where i have the sbrk-ed blocks */
	iterator = head;
	/* i iterate through the list until i find that the next block is a mmaped one so */
	/* i stop and return the current block which is the last sbrk ed one */
	while (iterator->next) {
		if (iterator->next->status == STATUS_MAPPED)
			break;

		iterator = iterator->next;
	}

	/* if it is free i just allocate more memory and expand it */
	if (iterator->status == STATUS_FREE) {
		return expand_block(iterator, size);
	}

	return NULL;
}

struct block_meta *expand(struct block_meta *head, struct block_meta *curr, size_t size, size_t blk_size)
{
	if (!head)
		return NULL;

	struct block_meta *iterator = head;

	while (iterator->next) {
		if (iterator == curr) {
			/* this means i can expand*/
			if (iterator->next->status == STATUS_FREE) {
				iterator->size += iterator->next->size + blk_size;
				iterator->next = iterator->next->next;

				/* maybe i expanded too much*/
				if (iterator->size > size)
					return split_block(iterator, size, blk_size);
			} else {
				/* i cant expand*/
				break;
			}
		} else {
			iterator = iterator->next;
		}
	}

	/* this means i ended on the last block so maybe i can just expand it*/
	if (!iterator->next && iterator == curr) {
		return expand_block(iterator, size);
	}

	return NULL;
}
