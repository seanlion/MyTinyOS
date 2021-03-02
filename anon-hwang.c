/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */
#include <string.h>
#include "vm/vm.h"
#include "threads/mmu.h"
#include "threads/palloc.h"
#include "lib/kernel/bitmap.h"
/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
struct bitmap *swap_table;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);
uint32_t next_sector;
/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};
/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1,1);
	swap_table = bitmap_create(disk_size(swap_disk));
	next_sector = 0;
}
/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;
	struct anon_page *anon_page = &page->anon;
}
/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	bool succ = false;
	printf("anon_swap_in!\n");
	if(!page->is_in_disk)
		return false;
	for (int i = 0; i< 8; i ++){
		disk_read(swap_disk, anon_page->start_sector + i, kva);
		kva += 512; 
	}
	page->is_in_disk = 0;
	bitmap_set_multiple(swap_table, anon_page->start_sector, 8, 0);
	anon_page->start_sector = NULL;
	// succ = pml4_set_page(page->thread->pml4, page->va, kva, page->writable);
	pml4_set_accessed (thread_current()->pml4, page->va, 1);
	// printf("va: %p, kva: %p\n", page->va, kva-PGSIZE);
	succ = true;
	return succ;
}
/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	void *kva = page->frame->kva;
	bool succ = false;
	printf("anon_swap_out!\n");
	anon_page->start_sector = bitmap_scan_and_flip(swap_table, next_sector, 8, 0);
	if (anon_page->start_sector == BITMAP_ERROR){
		anon_page->start_sector = bitmap_scan_and_flip(swap_table, 0, 8, 0);
	}
	if (anon_page->start_sector == BITMAP_ERROR){
		return succ;
	}
	next_sector = anon_page->start_sector + 8;
	if (!page->is_in_disk){
		for (int i = 0; i< 8; i ++){
			disk_write (swap_disk, anon_page->start_sector + i, kva);
			kva += 512; 
		}
	}
	page->is_in_disk = 1;
	palloc_free_page(kva - 4096);
	pml4_clear_page(page->thread->pml4, page->va);
	return succ;
}
/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}