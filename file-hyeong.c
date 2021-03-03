/* file.c: Implementation of memory backed file object (mmaped object). */
#include "vm/vm.h"
static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);
/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};
/* The initializer of file vm */
void
vm_file_init (void) {
}
/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;
	struct file_page *file_page = &page->file;
    return true;
}
/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in(struct page *page, void *kva) {
    struct file_page *file_page UNUSED = &page->file;
    file_seek(page->file.vafile, page->file.file_info->ofs);
    file_read(page->file.vafile, kva, page->file.file_info->page_read_bytes); 
    return true;
}
/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out(struct page *page) {
    struct file_page *file_page UNUSED = &page->file;
    if (pml4_is_dirty(thread_current()->pml4, page->va)) {  
        file_write_at(page->file.vafile, page->va, PGSIZE, page->file.offset);
    }
    pml4_clear_page(thread_current()->pml4, page->va);
    // palloc_free_page(page->va);
    return true;
}
/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy(struct page *page) {
    struct file_page *file_page UNUSED = &page->file;
    list_remove(&page->victim); 
    // }
    if (page_get_type(page) == VM_FILE) {
        do_munmap(page->va);
    }
    free(file_page->file_info);
}
/* Do the mmap */
void *
do_mmap(void *addr, size_t length, int writable, struct file *file, off_t offset) {
    struct file *newfile = file_reopen(file);
    void *upage = addr;
    off_t ofs = offset;
    uint32_t read_bytes = length;
    // hex_dump((uint64_t)file + (uint64_t)offset, (void *)((uint64_t)file + (uint64_t)offset), PGSIZE, 1);
    // uint32_t read_bytes = file_length(file);
    while (read_bytes > 0) {
        struct file_info *file_info = (struct file_info *)malloc(sizeof(struct file_info));
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;
        void *aux = NULL;
        file_info->file = newfile;
        // file_info->file = file;
        file_info->ofs = ofs;
        file_info->page_read_bytes = page_read_bytes;
        file_info->page_zero_bytes = page_zero_bytes;
        file_info->writable = writable;
        aux = (void *)file_info;
        if (!vm_alloc_page_with_initializer(VM_FILE, upage, writable, lazy_load_file, file_info)) {
            return NULL;
        }
        read_bytes -= page_read_bytes;
        upage += PGSIZE;
        ofs += page_read_bytes;  //! 얘는 ppt에 처리되어있었는데 코드상에 없어서 일단 추가후 주석해둠
    }
    return addr;
}
/* Do the munmap */
// void
// do_munmap (void *addr) {
//     struct page *page = spt_find_page(&thread_current()->spt, addr);
//     if(pml4_is_dirty(thread_current()->pml4, addr)){
//         file_write_at(page->file.vafile, page->va, PGSIZE, page->file.offset);
//         //! 추가 : 
//         spt_remove_page(&thread_current()->spt, page);
//         pml4_clear_page(thread_current()->pml4,addr);
//     }
// }
void do_munmap(void *addr) {
    struct page *page = spt_find_page(&thread_current()->spt, addr);
    if (pml4_is_dirty(thread_current()->pml4, addr)) {  
        file_write_at(page->file.vafile, addr, PGSIZE, page->file.offset);  
    }
}
static bool
lazy_load_file(struct page *page, void *aux) {
    /* TODO: Load the segment from the file */
    /* TODO: This called when the first page fault occurs on address VA. */
    /* TODO: VA is available when calling this function. */
    /* Load this page. */
    struct frame *frame = page->frame;  //! 여기서 page는 user page(이미 vm alloc에서 할당되어있음)
    //! 단지 lazy load 는 page fault일때만 들어온다.
    struct file_info *file_info = (struct file_info *)aux;
    struct file *file = file_info->file;
    page->file.file_info = file_info; //@ 이거를 직접 입력해보자
    page->file.file_info->file = file_info->file;
    page->file.file_info->ofs = file_info->ofs;
    page->file.file_info->page_read_bytes = file_info->page_read_bytes;
    page->file.file_info->page_zero_bytes = file_info->page_zero_bytes;
    page->file.file_info->writable = file_info->writable;
    page->file.vafile = file_info->file; //! 얘가 disk와 연결될 수 있는 주소임
    page->file.offset = file_info->ofs; //! munmap때 file write back 해줄때 offset 부터 써줘야 하므로 정보 저장
    size_t page_read_bytes = file_info->page_read_bytes;
    size_t page_zero_bytes = file_info->page_zero_bytes;
    bool writable = file_info->writable;
    off_t ofs = file_info->ofs;
    file_seek(file, ofs);
    //! file_read, 즉 file을 읽어서 frame->kva에 복사한다.
    if (file_read(file, frame->kva, page_read_bytes) != (int)page_read_bytes) {
        return false;
    }
    memset(frame->kva + page_read_bytes, 0, page_zero_bytes); //! 보안
    return true;
}