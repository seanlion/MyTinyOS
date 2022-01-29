/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include <bitmap.h>
#include "vm/vm.h"
#include "devices/disk.h"

static struct bitmap *swap_table;
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};


/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	swap_disk = disk_get(1, 1);
	/* disk_size: SECTOR 단위로 반환
	 * bitmap_create: PG 단위로 비트맵을 생성
	 * 한 PG 당 8개의 SECTOR이기 때문에 8로 나눠줌 */
	swap_table = bitmap_create (disk_size (swap_disk) / 8);
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;
	struct anon_page *anon_page = &page->anon;
	anon_page->st_number = 0;
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	size_t number = anon_page->st_number;

	if (anon_page->st_number == -1) {
		return true;
	}

	for (int i = 0; i < SECTOR_PER_PAGE; i++) {
		disk_read(swap_disk, (number * SECTOR_PER_PAGE) + i, kva + (DISK_SECTOR_SIZE * i));
	}

	page->frame->kva = kva;
	page->frame->thread = thread_current();
	anon_page->st_number = -1;
	pml4_set_accessed(thread_current()->pml4, page->va, 1);

	/* swap_table에 해당 number 공간이 들어있다고 적기 */
	bitmap_set(swap_table, number, false);
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	size_t number = bitmap_scan(swap_table, 0, 1, false);
	
	anon_page->st_number = number;
	if (number == -1) {
		return false;
	}
		
	/* 디스크에 물리메모리 정보 적기 */
	for (int i = 0; i < SECTOR_PER_PAGE; i++) {
		disk_write(swap_disk, (number * SECTOR_PER_PAGE) + i, page->frame->kva + (DISK_SECTOR_SIZE * i));
	}	
	bitmap_set(swap_table, number, true);
	del_frame_to_clock_list(page->frame);
	page->frame = NULL;
	free(page->frame);
	pml4_clear_page(thread_current()->pml4, page->va);
	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}
