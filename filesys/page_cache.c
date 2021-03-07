/* page_cache.c: Implementation of Page Cache (Buffer Cache). */

#include "vm/vm.h"
static bool page_cache_readahead (struct page *page, void *kva);
static bool page_cache_writeback (struct page *page);
static void page_cache_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations page_cache_op = {
	.swap_in = page_cache_readahead,
	.swap_out = page_cache_writeback,
	.destroy = page_cache_destroy,
	.type = VM_PAGE_CACHE,
};

tid_t page_cache_workerd;



/* The initializer of file vm */
void
pagecache_init (void) {
	/* TODO: Create a worker daemon for page cache with page_cache_kworkerd */
}

/* Initialize the page cache */
bool
page_cache_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &page_cache_op;

	return true;
}

/* Utilze the Swap in mechanism to implement readhead */
// 캐쉬에 없으면, 디스크에서 읽어오기./  다음 블록을 page cache 안에 넣기 
static bool
page_cache_readahead (struct page *page, void *kva) {
}

/* Utilze the Swap out mechanism to implement writeback */
// page cache 안에 dirty block(page?) 저장, cache full이면 victim 선정해서 해제(cache full인지 알려면? loopup 함수?)
static bool
page_cache_writeback (struct page *page) {
}

/* Destory the page_cache. */
static void
page_cache_destroy (struct page *page) {
}

/* Worker thread for page cache */
static void
page_cache_kworkerd (void *aux) {
}


/* page cache 꽉 찬지 살피는 look up 함수 만들어야 하나? palloc get page했는데 갖고올 수 있는거 없으면, evict?*/
