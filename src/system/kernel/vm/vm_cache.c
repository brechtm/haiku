/*
 * Copyright 2002-2007, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT License.
 *
 * Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
 * Distributed under the terms of the NewOS License.
 */


#include <kernel.h>
#include <vm.h>
#include <vm_priv.h>
#include <vm_cache.h>
#include <vm_page.h>
#include <int.h>
#include <util/khash.h>
#include <lock.h>
#include <debug.h>
#include <lock.h>
#include <smp.h>
#include <arch/cpu.h>

#include <stddef.h>
#include <stdlib.h>

//#define TRACE_VM_CACHE
#ifdef TRACE_VM_CACHE
#	define TRACE(x) dprintf x
#else
#	define TRACE(x) ;
#endif


/* hash table of pages keyed by cache they're in and offset */
#define PAGE_TABLE_SIZE 1024 /* TODO: make this dynamic */

static void *sPageCacheTable;
static spinlock sPageCacheTableLock;

struct page_lookup_key {
	uint32	offset;
	vm_cache *cache;
};


static int
page_compare_func(void *_p, const void *_key)
{
	vm_page *page = _p;
	const struct page_lookup_key *key = _key;

	TRACE(("page_compare_func: page %p, key %p\n", page, key));

	if (page->cache == key->cache && page->cache_offset == key->offset)
		return 0;

	return -1;
}


static uint32
page_hash_func(void *_p, const void *_key, uint32 range)
{
	vm_page *page = _p;
	const struct page_lookup_key *key = _key;

#define HASH(offset, ref) ((offset) ^ ((uint32)(ref) >> 4))

	if (page)
		return HASH(page->cache_offset, page->cache) % range;

	return HASH(key->offset, key->cache) % range;
}


status_t
vm_cache_init(kernel_args *args)
{
	sPageCacheTable = hash_init(PAGE_TABLE_SIZE, offsetof(vm_page, hash_next),
		&page_compare_func, &page_hash_func);
	if (sPageCacheTable == NULL)
		panic("vm_cache_init: no memory\n");

	return B_OK;
}


vm_cache *
vm_cache_create(vm_store *store)
{
	vm_cache *cache;

	if (store == NULL) {
		panic("vm_cache created with NULL store!");
		return NULL;
	}

	cache = malloc(sizeof(vm_cache));
	if (cache == NULL)
		return NULL;

	list_init(&cache->consumers);
	cache->page_list = NULL;
	cache->ref = NULL;
	cache->source = NULL;
	cache->virtual_base = 0;
	cache->virtual_size = 0;
	cache->temporary = 0;
	cache->scan_skip = 0;
	cache->page_count = 0;
	cache->busy = false;

	// connect the store to its cache
	cache->store = store;
	store->cache = cache;

	return cache;
}


status_t
vm_cache_ref_create(vm_cache *cache)
{
	vm_cache_ref *ref;
	status_t status;

	ref = malloc(sizeof(vm_cache_ref));
	if (ref == NULL)
		return B_NO_MEMORY;

	status = mutex_init(&ref->lock, "cache_ref_mutex");
	if (status < B_OK && (!kernel_startup || status != B_NO_MORE_SEMS)) {
		// During early boot, we cannot create semaphores - they are
		// created later in vm_init_post_sem()
		free(ref);
		return status;
	}

	ref->areas = NULL;
	ref->ref_count = 1;

	// connect the cache to its ref
	ref->cache = cache;
	cache->ref = ref;

	return B_OK;
}


void
vm_cache_acquire_ref(vm_cache_ref *cacheRef)
{
	TRACE(("vm_cache_acquire_ref: cacheRef %p, ref will be %ld\n",
		cacheRef, cacheRef->ref_count + 1));

	if (cacheRef == NULL)
		panic("vm_cache_acquire_ref: passed NULL\n");

	if (cacheRef->cache->store->ops->acquire_ref != NULL)
		cacheRef->cache->store->ops->acquire_ref(cacheRef->cache->store);

	atomic_add(&cacheRef->ref_count, 1);
}


void
vm_cache_release_ref(vm_cache_ref *cacheRef)
{
	vm_page *page;

	TRACE(("vm_cache_release_ref: cacheRef %p, ref will be %ld\n",
		cacheRef, cacheRef->ref_count - 1));

	if (cacheRef == NULL)
		panic("vm_cache_release_ref: passed NULL\n");

	if (atomic_add(&cacheRef->ref_count, -1) != 1) {
		// the store ref is only released on the "working" refs, not
		// on the initial one (this is vnode specific)
		if (cacheRef->cache->store->ops->release_ref)
			cacheRef->cache->store->ops->release_ref(cacheRef->cache->store);
#if 0
{
	// count min references to see if everything is okay
	struct stack_frame {
		struct stack_frame*	previous;
		void*				return_address;
	};
	int32 min = 0;
	vm_area *a;
	vm_cache *c;
	bool locked = false;
	if (cacheRef->lock.holder != find_thread(NULL)) {
		mutex_lock(&cacheRef->lock);
		locked = true;
	}
	for (a = cacheRef->areas; a != NULL; a = a->cache_next)
		min++;
	for (c = NULL; (c = list_get_next_item(&cacheRef->cache->consumers, c)) != NULL; )
		min++;
	dprintf("! %ld release cache_ref %p, ref_count is now %ld (min %ld, called from %p)\n",
		find_thread(NULL), cacheRef, cacheRef->ref_count,
		min, ((struct stack_frame *)x86_read_ebp())->return_address);
	if (cacheRef->ref_count < min)
		panic("cache_ref %p has too little ref_count!!!!", cacheRef);
	if (locked)
		mutex_unlock(&cacheRef->lock);
}
#endif
		return;
	}

	// delete this cache

	if (cacheRef->areas != NULL)
		panic("cache_ref %p to be deleted still has areas", cacheRef);
	if (!list_is_empty(&cacheRef->cache->consumers))
		panic("cache %p to be deleted still has consumers", cacheRef->cache);

	// delete the cache's backing store
	cacheRef->cache->store->ops->destroy(cacheRef->cache->store);

	// free all of the pages in the cache
	page = cacheRef->cache->page_list;
	while (page) {
		vm_page *oldPage = page;
		int state;

		page = page->cache_next;

		if (oldPage->mappings != NULL || oldPage->wired_count != 0) {
			panic("remove page %p from cache %p: page still has mappings!\n",
				oldPage, cacheRef->cache);
		}

		// remove it from the hash table
		state = disable_interrupts();
		acquire_spinlock(&sPageCacheTableLock);

		hash_remove(sPageCacheTable, oldPage);
		oldPage->cache = NULL;
		// TODO: we also need to remove all of the page's mappings!

		release_spinlock(&sPageCacheTableLock);
		restore_interrupts(state);

		TRACE(("vm_cache_release_ref: freeing page 0x%lx\n",
			oldPage->physical_page_number));
		vm_page_set_state(oldPage, PAGE_STATE_FREE);
	}

	// remove the ref to the source
	if (cacheRef->cache->source)
		vm_cache_remove_consumer(cacheRef->cache->source->ref, cacheRef->cache);

	mutex_destroy(&cacheRef->lock);
	free(cacheRef->cache);
	free(cacheRef);
}


vm_page *
vm_cache_lookup_page(vm_cache_ref *cacheRef, off_t offset)
{
	struct page_lookup_key key;
	cpu_status state;
	vm_page *page;

	ASSERT_LOCKED_MUTEX(&cacheRef->lock);

	key.offset = (uint32)(offset >> PAGE_SHIFT);
	key.cache = cacheRef->cache;

	state = disable_interrupts();
	acquire_spinlock(&sPageCacheTableLock);

	page = hash_lookup(sPageCacheTable, &key);

	release_spinlock(&sPageCacheTableLock);
	restore_interrupts(state);

	if (page != NULL && cacheRef->cache != page->cache)
		panic("page %p not in cache %p\n", page, cacheRef->cache);

	return page;
}


void
vm_cache_insert_page(vm_cache_ref *cacheRef, vm_page *page, off_t offset)
{
	cpu_status state;

	TRACE(("vm_cache_insert_page: cacheRef %p, page %p, offset %Ld\n",
		cacheRef, page, offset));
	ASSERT_LOCKED_MUTEX(&cacheRef->lock);

	if (page->cache != NULL) {
		panic("insert page %p into cache %p: page cache is set to %p\n",
			page, cacheRef->cache, page->cache);
	}

	page->cache_offset = (uint32)(offset >> PAGE_SHIFT);

	if (cacheRef->cache->page_list != NULL)
		cacheRef->cache->page_list->cache_prev = page;

	page->cache_next = cacheRef->cache->page_list;
	page->cache_prev = NULL;
	cacheRef->cache->page_list = page;
	cacheRef->cache->page_count++;

	page->cache = cacheRef->cache;

	state = disable_interrupts();
	acquire_spinlock(&sPageCacheTableLock);

	hash_insert(sPageCacheTable, page);

	release_spinlock(&sPageCacheTableLock);
	restore_interrupts(state);
}


/*!
	Removes the vm_page from this cache. Of course, the page must
	really be in this cache or evil things will happen.
	The vm_cache_ref lock must be held.
*/
void
vm_cache_remove_page(vm_cache_ref *cacheRef, vm_page *page)
{
	cpu_status state;

	TRACE(("vm_cache_remove_page: cache %p, page %p\n", cacheRef, page));
	ASSERT_LOCKED_MUTEX(&cacheRef->lock);

	if (page->cache != cacheRef->cache)
		panic("remove page from %p: page cache is set to %p\n", cacheRef->cache, page->cache);

	state = disable_interrupts();
	acquire_spinlock(&sPageCacheTableLock);

	hash_remove(sPageCacheTable, page);

	release_spinlock(&sPageCacheTableLock);
	restore_interrupts(state);

	if (cacheRef->cache->page_list == page) {
		if (page->cache_next != NULL)
			page->cache_next->cache_prev = NULL;
		cacheRef->cache->page_list = page->cache_next;
	} else {
		if (page->cache_prev != NULL)
			page->cache_prev->cache_next = page->cache_next;
		if (page->cache_next != NULL)
			page->cache_next->cache_prev = page->cache_prev;
	}
	cacheRef->cache->page_count--;
	page->cache = NULL;
}


status_t
vm_cache_write_modified(vm_cache_ref *ref, bool fsReenter)
{
	status_t status;

	TRACE(("vm_cache_write_modified(ref = %p)\n", ref));

	mutex_lock(&ref->lock);
	status = vm_page_write_modified(ref->cache, fsReenter);
	mutex_unlock(&ref->lock);

	return status;
}


/*!
	Commits the memory to the store if the \a commitment is larger than
	what's committed already.
	Assumes you have the \a ref's lock held.
*/
status_t
vm_cache_set_minimal_commitment_locked(vm_cache_ref *ref, off_t commitment)
{
	status_t status = B_OK;
	vm_store *store = ref->cache->store;

	ASSERT_LOCKED_MUTEX(&ref->lock);

	// If we don't have enough committed space to cover through to the new end of region...
	if (store->committed_size < commitment) {
		// ToDo: should we check if the cache's virtual size is large
		//	enough for a commitment of that size?

		// try to commit more memory
		status = store->ops->commit(store, commitment);
	}

	return status;
}


/*!
	This function updates the size field of the vm_cache structure.
	If needed, it will free up all pages that don't belong to the cache anymore.
	The vm_cache_ref lock must be held when you call it.
	Since removed pages don't belong to the cache any longer, they are not
	written back before they will be removed.
*/
status_t
vm_cache_resize(vm_cache_ref *cacheRef, off_t newSize)
{
	vm_cache *cache = cacheRef->cache;
	status_t status;
	uint32 oldPageCount, newPageCount;

	ASSERT_LOCKED_MUTEX(&cacheRef->lock);

	status = cache->store->ops->commit(cache->store, newSize);
	if (status != B_OK)
		return status;

	oldPageCount = (uint32)((cache->virtual_size + B_PAGE_SIZE - 1) >> PAGE_SHIFT);
	newPageCount = (uint32)((newSize + B_PAGE_SIZE - 1) >> PAGE_SHIFT);

	if (newPageCount < oldPageCount) {
		// we need to remove all pages in the cache outside of the new virtual size
		vm_page *page, *next;

		for (page = cache->page_list; page; page = next) {
			next = page->cache_next;

			if (page->cache_offset >= newPageCount) {
				// remove the page and put it into the free queue
				vm_cache_remove_page(cacheRef, page);
				vm_page_set_state(page, PAGE_STATE_FREE);
			}
		}
	}

	cache->virtual_size = newSize;
	return B_OK;
}


/*!
	Removes the \a consumer from the \a cacheRef's cache.
	It will also release the reference to the cacheRef owned by the consumer.
	Assumes you have the consumer's cache_ref lock held.
*/
void
vm_cache_remove_consumer(vm_cache_ref *cacheRef, vm_cache *consumer)
{
	vm_cache *cache;

	TRACE(("remove consumer vm cache %p from cache %p\n", consumer, cacheRef->cache));
	ASSERT_LOCKED_MUTEX(&consumer->ref->lock);

	// remove the consumer from the cache, but keep its reference until later
	mutex_lock(&cacheRef->lock);
	cache = cacheRef->cache;
	list_remove_item(&cache->consumers, consumer);
	consumer->source = NULL;

	if (cacheRef->areas == NULL && cache->source != NULL
		&& !list_is_empty(&cache->consumers)
		&& cache->consumers.link.next == cache->consumers.link.prev) {
		// The cache is not really needed anymore - it can be merged with its only
		// consumer left.
		vm_cache_ref *consumerRef;
		bool merge = false;

		consumer = list_get_first_item(&cache->consumers);

		// Our cache doesn't have a ref to its consumer (only the other way around), 
		// so we cannot just acquire it here; it might be deleted right now
		while (true) {
			int32 count;
			consumerRef = consumer->ref;

			count = consumerRef->ref_count;
			if (count == 0)
				break;

			if (atomic_test_and_set(&consumerRef->ref_count, count + 1, count) == count) {
				// We managed to grab a reference to the consumerRef.
				// Since this doesn't guarantee that we get the cache we wanted
				// to, we need to check if this cache is really the last
				// consumer of the cache we want to merge it with.
				merge = true;
				break;
			}
		}

		if (merge) {
			// But since we need to keep the locking order upper->lower cache, we
			// need to unlock our cache now
			cache->busy = true;
			mutex_unlock(&cacheRef->lock);

			mutex_lock(&consumerRef->lock);
			mutex_lock(&cacheRef->lock);

			// the cache and the situation might have changed
			cache = cacheRef->cache;
			consumer = consumerRef->cache;

			if (cacheRef->areas != NULL || cache->source == NULL
				|| list_is_empty(&cache->consumers)
				|| cache->consumers.link.next != cache->consumers.link.prev
				|| consumer != list_get_first_item(&cache->consumers)) {
				merge = false;
				cache->busy = false;
				mutex_unlock(&consumerRef->lock);
				vm_cache_release_ref(consumerRef);
			}
		}

		if (merge) {
			vm_page *page, *nextPage;
			vm_cache *newSource;

			consumer = list_remove_head_item(&cache->consumers);

			TRACE(("merge vm cache %p (ref == %ld) with vm cache %p\n",
				cache, cacheRef->ref_count, consumer));

			for (page = cache->page_list; page != NULL; page = nextPage) {
				vm_page *consumerPage;
				nextPage = page->cache_next;

				consumerPage = vm_cache_lookup_page(consumerRef,
					(off_t)page->cache_offset << PAGE_SHIFT);
				if (consumerPage == NULL) {
					// the page already is not yet in the consumer cache - move
					// it upwards
					vm_cache_remove_page(cacheRef, page);
					vm_cache_insert_page(consumerRef, page,
						(off_t)page->cache_offset << PAGE_SHIFT);
				} else if (consumerPage->state != PAGE_STATE_BUSY
					&& (page->mappings != 0 || page->wired_count != 0)) {
					panic("page %p has still mappings (consumer cache %p)!",
						page, consumerRef);
				}
			}

			newSource = cache->source;

			// The remaining consumer has gotten a new source
			mutex_lock(&newSource->ref->lock);

			list_remove_item(&newSource->consumers, cache);
			list_add_item(&newSource->consumers, consumer);
			consumer->source = newSource;
			cache->source = NULL;

			mutex_unlock(&newSource->ref->lock);

			// Release the other reference to the cache - we take over
			// its reference of its source cache; we can do this here
			// (with the cacheRef locked) since we own another reference
			// from the first consumer we removed
if (cacheRef->ref_count < 2)
panic("cacheRef %p ref count too low!\n", cacheRef);
			vm_cache_release_ref(cacheRef);

			mutex_unlock(&consumerRef->lock);
			vm_cache_release_ref(consumerRef);
		}
	}

	mutex_unlock(&cacheRef->lock);
	vm_cache_release_ref(cacheRef);
}


/*!
	Marks the \a cacheRef's cache as source of the \a consumer cache,
	and adds the \a consumer to its list.
	This also grabs a reference to the source cache.
	Assumes you have the cache_ref and the consumer's lock held.
*/
void
vm_cache_add_consumer_locked(vm_cache_ref *cacheRef, vm_cache *consumer)
{
	TRACE(("add consumer vm cache %p to cache %p\n", consumer, cacheRef->cache));
	ASSERT_LOCKED_MUTEX(&cacheRef->lock);
	ASSERT_LOCKED_MUTEX(&consumer->ref->lock);

	consumer->source = cacheRef->cache;
	list_add_item(&cacheRef->cache->consumers, consumer);

	vm_cache_acquire_ref(cacheRef);
}


/*!
	Adds the \a area to the \a cacheRef.
	Assumes you have the locked the cache_ref.
*/
status_t
vm_cache_insert_area_locked(vm_cache_ref *cacheRef, vm_area *area)
{
	ASSERT_LOCKED_MUTEX(&cacheRef->lock);

	area->cache_next = cacheRef->areas;
	if (area->cache_next)
		area->cache_next->cache_prev = area;
	area->cache_prev = NULL;
	cacheRef->areas = area;

	return B_OK;
}


status_t
vm_cache_remove_area(vm_cache_ref *cacheRef, vm_area *area)
{
	mutex_lock(&cacheRef->lock);

	if (area->cache_prev)
		area->cache_prev->cache_next = area->cache_next;
	if (area->cache_next)
		area->cache_next->cache_prev = area->cache_prev;
	if (cacheRef->areas == area)
		cacheRef->areas = area->cache_next;

	mutex_unlock(&cacheRef->lock);
	return B_OK;
}
