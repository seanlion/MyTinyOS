/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */
#include "vm/vm.h"
#include "devices/disk.h"
#include <bitmap.h>
static struct bitmap *swap_table;
int swap_slot_start;
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
#define SWAP_SLOC_CNT PGSIZE/DISK_SECTOR_SIZE
// static struct lock anon_lock;
/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1);
    if(swap_disk == NULL){
        printf("vm_anon_init : swap_disk == NULL\n");
    }
    // lock_init(&anon_lock);
    // lock_acquire(&anon_lock);
    swap_table = bitmap_create(disk_size(swap_disk)/(SWAP_SLOC_CNT));
    // lock_release(&anon_lock);
    if(swap_table == NULL){
        printf("vm_anon_init : swap_table == NULL\n");;
    }
}
/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;
	page->type = page->operations->type;
	struct anon_page *anon_page = &page->anon;
	anon_page->aux = page->uninit.aux;
    swap_slot_start = 0;
	return true;
}
/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
    // printf("\nanon_swap_in start\n");
    // printf("page->va : %p, kva : %p, page->frame->kva %p\n", page->va, kva, page->frame->kva);
	struct anon_page *anon_page = &page->anon;
    if(page->swap_slot == -1){
        // printf("page->swap_slot == -1");
        return true;
    }
    // lock_acquire(&anon_lock);
    for(int i = 0; i < SWAP_SLOC_CNT; i++){
        disk_read(swap_disk, (page->swap_slot) + i, kva + (DISK_SECTOR_SIZE * i));
        // printf("%d ,", (page->swap_slot) + i);
    }
    // lock_release(&anon_lock);
    // printf("\n");
    bitmap_reset (swap_table, (page->swap_slot)/8);
    // printf("page->swap_slot : %d\n", page->swap_slot);
    // printf("(page->swap_slot)/8 : %d\n", page->swap_slot/8);
    page->swap_slot = -1;
    page->is_loaded = true;
    // printf("anon_swap_in end\n");
    return true;
}
/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
    // printf("\nanon_swap_out start\n");
    // printf("page->va : %p, page->frame->kva : %p\n", page->va, page->frame->kva);
	struct anon_page *anon_page = &page->anon;
    struct thread *t = page->frame->thread;
    struct page_aux *aux = anon_page->aux;
    page->swap_slot = bitmap_scan(swap_table, 0, 1, 0) * SWAP_SLOC_CNT;
    if(page->swap_slot == BITMAP_ERROR){
        return false;
    }
    // lock_acquire(&anon_lock);
    for(int i = 0; i < SWAP_SLOC_CNT; i++){
        disk_write(swap_disk, (page->swap_slot) + i, page->frame->kva + (DISK_SECTOR_SIZE * i));
        // printf("%d ,", (page->swap_slot) + i);
    }
    // lock_release(&anon_lock);
    // printf("\n");
    bitmap_mark(swap_table, (page->swap_slot)/8);
    // printf("page->swap_slot : %d\n", page->swap_slot);
    // printf("(page->swap_slot)/8 : %d\n", page->swap_slot/8);
    // lock_acquire(&anon_lock);
    list_remove(&page->frame->clock_elem);
    pml4_clear_page(t->pml4, page->va);
    memset (page->frame->kva, 0, PGSIZE);
    palloc_free_page(page->frame->kva);
    free(page->frame);
    page->frame = NULL;
    page->is_loaded = false;
    // lock_release(&anon_lock);
    // printf("anon_swap_out end\n");
    return true;
}
/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
    // free(page->uninit.aux);
}