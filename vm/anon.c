/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include <bitmap.h>
#include "vm/vm.h"
#include "devices/disk.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

static struct bitmap *swap_table;

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1);
	// printf("anon init :: swap_disk :: %p\n", swap_disk);
	/* disk_size: SECTOR 단위로 반환
	 * bitmap_create: PG 단위로 비트맵을 생성
	 * 한 PG 당 8개의 SECTOR이기 때문에 8로 나눠줌 */
	swap_table = bitmap_create (disk_size (swap_disk) / 8);
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	// printf("anon initializer 들어오나??\n");
	page->operations = &anon_ops;
	struct anon_page *anon_page = &page->anon;
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	// printf("anon_swap_in이 작동?\n");
	// lock_acquire(&clock_list_lock);
	struct anon_page *anon_page = &page->anon;
	size_t number = anon_page->st_number;
	// printf("swap in의 number는?? %d\n",number);
	// 물리 메모리 페이지에 디스크의 데이터 적기
	for (int i = 0; i < SECTOR_PER_PAGE; i++) {
		disk_read(swap_disk, (number * SECTOR_PER_PAGE) + i, kva + (DISK_SECTOR_SIZE * i));
	}

	// page table에 page와 frame 매핑정보 설정
	pml4_set_page(thread_current()->pml4, page->va, kva, page->writable);
	page->frame->kva = kva;
	page->frame->thread = thread_current();
	add_frame_to_clock_list(page->frame);

	// swap_table에 해당 number 공간이 들어있다고 적기
	bitmap_set(swap_table, number, false);

	// lock_release(&clock_list_lock);
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	// printf("anon_swap_out이 작동?\n");
	// lock_acquire(&clock_list_lock);
	
	// false는 비어있음, true는 들어있음
	size_t number = bitmap_scan(swap_table, 0, 1, false);
	anon_page->st_number = number;

	// 디스크에 물리메모리 정보 적기
	for (int i = 0; i < SECTOR_PER_PAGE; i++) {
		disk_write(swap_disk, (number * SECTOR_PER_PAGE) + i, page->frame->kva + (DISK_SECTOR_SIZE * i));
	}

	// page table에 page와 frame 매핑정보 삭제
	pml4_clear_page(thread_current()->pml4, page->va);
	
	// swap_table에 해당 number 공간을 비웠다고 적기
	bitmap_set(swap_table, number, true);
	page->frame = NULL;
 	/**/
	// lock_release(&clock_list_lock);

	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	// printf("anon_destroy이 작동?\n");
	// printf("anon destroy 여기서 터지나1111???\n");
	struct anon_page *anon_page = &page->anon;
	// printf("anon destroy 여기서 터지나222???\n");
	// printf("frame kva?? %p\n", page->frame->kva);
	// if (page->frame != NULL)
	// 	free_frame(page->frame->kva);
	// palloc_free_page(page->frame->kva); /*vm_get_frame에서 get page 하고 안 해주는 것 같은데?*/
	free(page->frame);
	// printf("anon destroy 여기서 터지나3333???\n");

}
