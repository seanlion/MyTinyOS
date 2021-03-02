/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */
#include "vm/vm.h"
//! 2/27 추가
#include "bitmap.h"         //! 비트맵을 사용하기 위한 헤더 추가
#include "threads/mmu.h"    //! pml4_clear_page를 위한 헤더
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
//! 2/27 추가
struct bitmap *bitmap; //! anon을 위한 swap_disk의 할당 여부를 관리하기 위한 비트맵 구조체
static size_t nextfit = 0;
//! bitmap_mark, bitmap_reset 함수를 쓰면 될거 같다.
/* Initialize the data for anonymous pages */
//_________________________________________________
/*Initialize for anonymous page subsystem. 
In this function, you can setup anything related to the anonymous page.
In this function, you need to set up the swap disk. You will also need 
a data structure to manage free and used areas in the swap disk. 
The swap area will be also managed at the granularity of PGSIZE (4096 bytes)*/
//_________________________________________________
//^~~~~~~~~~~~~~~~~~ON GITBOOK
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
    swap_disk = NULL;
    //! 여기서 부터 구현
    // disk_init(); //! 얘를 쓰면 터짐;
    //_ 뭔가 anon.h의 anon_page에 변수선언후 초기화 해줘야할거같은 느낌?
    //_ 아니면 anon_initializer에서 해주고 여기서는 그냥 disk 세팅만??
    //_ 깃북에서는 스왑디스크에서 사용 가능한 영역과 사용된 영역을 관리할
    //_ 데이터 구조가 필요하다고함. 
    //_ 스왑 영역은 PGSIZE(4096 바이트) 단위로 관리된다.
    swap_disk = disk_get (1,1); //! 1,1이 swap을 위한 디스크 구조체를 받아옴
    bitmap = bitmap_create(disk_size(swap_disk)); //todo bitmap의 크기가 이게맞음?
    //! disk size 만큼 bitmap_create애 던저주고 , 그거에 해당하는 bitmap을 
    //! 0으로 초기화 해서 bitmap 구조체로 반환해준다.
} //! 이 함수는 결국 swap table을 관리하기 위한 초기화 과정이다.
/* Initialize the file mapping */
//_________________________________________________
/*The function first sets up the handlers for the anonymous page 
in page->operations. You might need to update some information in anon_page, 
which is currently an empty struct. This function is used as initializer for 
anonymous pages (i.e. VM_ANON).
This is the initializer for the anonymous page. You will need to add some 
information to the anon_page to support the swapping.*/
//_________________________________________________
//^~~~~~~~~~~~~~~~~~ON GITBOOK
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;
	struct anon_page *anon_page = &page->anon;
    //! 여기서 부터 구현
    //_여기는 anon pag의 이니셜라이저다. anon_page로의 swapping을 해주기 위해
    //_ anon_page 구조체에 정보를 추가할 필요가 있음
    //! 여기에 disk sector 값을 -1로 해주면 좋을것 같다? 라고함
    //! 그냥 disk sector 값이 무작위값이라서 초기화하라는 의미
    //! swap out하면 sector 번호가 정해짐. 쓸 번호를 찾아서 anon page에 할당해줌
    //! swap in할떈 또 그 secotr 번호를 가지고 어따 쓴다. 근데 왜 -1로해주는지? 는모르겟음
    return true;
}
/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
    // list_push_back(&victim_list, );
    // if(page->anon.sector == -1) return true;
    uint32_t sector = page->anon.sector; //! 빼놧던 swap table을 가져온다.
    for(int i=0; i<8; i++){
        disk_read(swap_disk, sector +i, kva + (i * DISK_SECTOR_SIZE));
    }
    bitmap_set_multiple(bitmap, sector, 8, 0); //! swap out 했으니까 disk의 sector에 0으로 해줌  //! 즉, bitmap에 있었던 거를 비워준다. (왜냐면 다시 메모리에 올려놨으니까)
    return true;
}
/* Swap out the page by writing contents to the swap disk. */
static bool 
anon_swap_out (struct page *page){
	struct anon_page *anon_page = &page->anon;
    //! next fit 적용
    if(nextfit >= bitmap_size(bitmap)) nextfit = 0;
    uint32_t sector = bitmap_scan_and_flip(bitmap, nextfit, 8, false); // 8개가 연속으로 빈 비트맵을 찾아줌
    nextfit = sector + 8;
    page->anon.sector = sector; 
    void *buffer = page->frame->kva;
    for(int i=0; i<8; i++) {            
        disk_write(swap_disk, sector+i, buffer +(i*DISK_SECTOR_SIZE));
    }
    pml4_clear_page(page->thread->pml4, page->va); 
    return true;
}
/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	// struct anon_page *anon_page = &page->anon; //! 스켈레톤 코든데 안쓰는거 같아 지워둠
    // // free(page->va); 
    // printf("this is anon_destroy\n");
    // palloc_free_page(page->va);
    //! list가 empty일수가 있나?
    if(page != NULL){
        if(!list_empty(&victim_list)){
            list_remove(&page->victim);
        }
    }
    // if(page->frame){
    //     free(page->frame);
    // }
    //free(page->frame);
    // palloc_free_page(page->frame->kva);
    // free(page->anon.aux); 
    // page->va = NULL; 
    // // free(page);
} 