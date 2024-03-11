// SPDX-License-Identifier: BSD-3-Clause

#include "osmem.h"
#include "block_meta.h"

struct block_meta *head;

int sbrk_allocs;

size_t blk_size = ALIGN(sizeof(struct block_meta));

/*i have to use this function because malloc and calloc compare with differtent tholds*/
void *os_alloc(size_t data_size, size_t thold)
{
	void *ret;

	if (data_size + blk_size >= thold) {
		ret = mmap(NULL, data_size + blk_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

		DIE(ret == (void *) -1, "Mmap failed!");

		struct block_meta *new = (struct block_meta *) ret;

		new->next = NULL;
		new->prev = NULL;
		new->size = data_size;
		new->status = STATUS_MAPPED;

		insert_block_in_list(&head, new, STATUS_MAPPED);

		return get_address(new, blk_size);
	}

	/* first heap allocation => preallocation*/
	if (sbrk_allocs == 0) {
		sbrk_allocs = 1;

		ret = sbrk(thold);

		DIE(ret == (void *) -1, "Sbrk failed!");

		struct block_meta *new = (struct block_meta *) ret;

		new->next = NULL;
		new->prev = NULL;
		new->size = thold - blk_size;
		new->status = STATUS_ALLOC;

		insert_block_in_list(&head, new, STATUS_ALLOC);

		return get_address(new, blk_size);
	}

	struct block_meta *new = find_good_size_block(head, data_size, blk_size);

	if (!new) {
		ret = sbrk(data_size + blk_size);

		DIE(ret == (void *) -1, "Sbrk failed!");

		new = (struct block_meta *) ret;

		new->next = NULL;
		new->prev = NULL;
		new->size = data_size;
		new->status = STATUS_ALLOC;

		insert_block_in_list(&head, new, STATUS_ALLOC);
	}

	return get_address(new, blk_size);
}

void *os_malloc(size_t size)
{
	if (size == 0)
		return NULL;

	size_t data_size = ALIGN(size);

	return os_alloc(data_size, MMAP_THRESHOLD);
}

void os_free(void *ptr)
{
	if (!ptr)
		return;

	struct block_meta *curr = get_block(ptr, blk_size);

	/* if it is on the heap i just mark it as freed so maybe */
	/* i can use it later on */
	if (curr->status == STATUS_ALLOC) {
		curr->status = STATUS_FREE;
	} else if (curr->status == STATUS_MAPPED) {
		remove_block_from_list(&head, curr);

		int ret = munmap((void *) curr, curr->size + blk_size);

		DIE(ret == -1, "munmap failed!\n");
	}
}

void *os_calloc(size_t nmemb, size_t size)
{
	if (nmemb == 0 || size == 0)
		return NULL;

	int data_size = ALIGN(size * nmemb);

	void *ret = os_alloc(data_size, getpagesize());

	memset(ret, 0, data_size);

	return ret;
}

void *os_realloc(void *ptr, size_t size)
{
	if (!ptr)
		return os_malloc(size);

	/* ig if we set the size to 0 we just free the memory*/
	if (size == 0) {
		os_free(ptr);
		return NULL;
	}

	size_t data_size = ALIGN(size);
	struct block_meta *iterator = get_block(ptr, blk_size);

	if (iterator->status == STATUS_FREE)
		return NULL;

	/* if i have to reallocate with mmap or it was already allocated with mmap */
	/* i dont have to check for any free blocks so i can just call malloc */
	if (data_size + blk_size >= MMAP_THRESHOLD || iterator->status == STATUS_MAPPED) {
		void *ret = os_malloc(data_size);

		if (data_size < iterator->size)
			memcpy(ret, ptr, data_size);
		else
			memcpy(ret, ptr, iterator->size);
		os_free(ptr);

		return ret;
	}

	/* if i allocate the same memory i dont do anything*/
	if (data_size == iterator->size)
		return ptr;

	struct block_meta *new = NULL;

	/* if i reallocate less memory i just split the block*/
	if (data_size < iterator->size) {
		new = split_block(iterator, data_size, blk_size);
		return get_address(new, blk_size);
	}

	/* if i reallocate more memory but less than MMAPTHRESHOLD */
	/* i have to try to expand the block and if i cant i simply */
	/* allocate a new one */

	if (data_size > iterator->size) {
		/* now i try to expand the block */
		new = expand(head, iterator, data_size, blk_size);

		/* if expansion worked we return the expanded adress */
		if (new)
			return get_address(new, blk_size);

		/* when i cant expand i just allocate a new chunk of memory and insert it in the list*/
		void *ret = os_malloc(data_size);

		memcpy(ret, ptr, iterator->size);

		os_free(ptr);
		return ret;
	}

	return NULL;
}
