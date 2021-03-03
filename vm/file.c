/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "userprog/syscall.h"
#include "userprog/process.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);
static void munmap_action (struct hash_elem *e, void* aux);


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
	// printf("file_backed_initializer이 작동?\n");
	page->operations = &file_ops;
	struct file_page *file_page = &page->file;
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	// printf("file_backed_swap in \n");
	struct file_page *file_page = &page->file;
	// printf("file_backed swap in이 작동?\n");
	// lock_acquire(&filesys_lock);
	/*file seek를 안해서 그런거 아닐까?*/
	uint32_t read_bytes = file_read_at(file_page->file, kva, file_page->read_bytes, file_page->offset);
	uint32_t zero_bytes = PGSIZE - read_bytes;
	memset(kva + read_bytes, 0, zero_bytes);

	// lock_release(&filesys_lock);
	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	// printf("file_backed_swap out \n");
	struct file_page *file_page = &page->file;
	struct thread *t = thread_current();
	// printf("file_backed swap out이 작동?\n");
	// lock_acquire(&filesys_lock);
	
	if (pml4_is_dirty(t->pml4, page->va) && page->frame != NULL) {
		// 디스크에 있는 파일에 변경사항 있으면 반영
		file_write_at(page->file.file, page->frame->kva, page->file.read_bytes, page->file.offset);
		pml4_set_dirty(t->pml4, page->va, false);
	}
	pml4_clear_page(thread_current()->pml4, page->va);

	// lock_release(&filesys_lock);

	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page = &page->file;
	struct thread *t = thread_current();

	// // if(page->frame == NULL)
	// // 	return;

	// if (pml4_is_dirty(t->pml4, page->va)) {
	// 	// 디스크에 있는 파일에 변경사항 있으면 반영
	// 	file_write_at(page->file.file, page->frame->kva, page->file.read_bytes, page->file.offset);
	// }
	// // palloc_free_page(page->frame->kva); /*vm_get_frame에서 get page 하고 안 해주는 것 같은데?*/
	// free(page->frame);

}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	// fail 처리되는 것들 처리
	if (length <= 0){
        return NULL;
	}
	// printf("mmap 부분 1111\n");
	// mmap overlap 예외처리
	void* overlap_addr = addr+length;
	void* std_addr = addr;
	void* mmap_addr = addr;
	while (std_addr < overlap_addr){ // 전체 길이보다 작아야 루프 끝남.
		if (!is_user_vaddr(std_addr))
			return NULL;
		struct page* page = spt_find_page(&thread_current()->spt, std_addr);
		if( page != NULL){
			return NULL;
		}
		std_addr +=PGSIZE;
	}
	// printf("여기 들어와야 함?\n");
	uint32_t read_bytes = (uint32_t) length;
	uint32_t zero_bytes = 0;
	uint32_t remain_bytes = (PGSIZE - read_bytes) % PGSIZE; // read_bytes가 PGSIZE보다 크면 0으로 남김.
	if (remain_bytes !=0){
		zero_bytes = remain_bytes;
	}
	// 처음 addr을 리턴해줘야 하기 때문에 저장

	// mapping id를 넣어주기 위해 파일 테이블에서 파일 위치를 id로 넣음.(fd_table에서 파일을 찾음.) -> 이거는 일단 보류.

	// struct thread * t = thread_current();
	// for (int i = 2; i < t->next_fd; i++) {
	// 	if (t->fd_table[i] == file) {
	// 		map_id = i;
	// 		// break;
	// 	}

	// }
	// printf("mmap 부분 file reopen 전111 \n");
	struct file* reopen_file = file_reopen(file);
	// map_id = process_add_file(reopen_file); /* seugnmin's advice */
	int map_id = process_add_file(reopen_file); /* seugnmin's advice */
	while (read_bytes > 0 || zero_bytes > 0) {

		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;
		struct file_aux *tmp_aux = malloc(sizeof(struct file_aux));
		tmp_aux->file = reopen_file;
		tmp_aux->offset = offset;
		tmp_aux->read_bytes = page_read_bytes;
		tmp_aux->zero_bytes = page_zero_bytes;
		tmp_aux->writable = writable;
		tmp_aux->mapping_id = map_id;
		if (!vm_alloc_page_with_initializer (VM_FILE, addr, writable, lazy_map, tmp_aux) )
			{
				free(tmp_aux);
				return NULL;
			}

		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		addr += PGSIZE;
		offset += PGSIZE; 
		// printf("mmap의 read_bytes %d\n",read_bytes);
		// printf("mmap의 zero_bytes %d\n",zero_bytes);

	}
	// printf("여기 들어옴 do map222\n");
	// printf("mmap 부분 22222\n");
	// printf("mmap 부분 마지막쪽 \n");

	return mmap_addr;
}

/*-------------------------- project.3-map,unmap -----------------------------*/

static bool
lazy_map (struct page *page, void *aux){
	// printf("lazy_map :: page :: %p\n", page);
	struct file_aux *tmp_aux = (struct file_aux *)aux;
	// printf("lazy map 들어왔다!!\n");
	if(page->frame == NULL){
		return false;
	}
	uint8_t *kva = page->frame->kva;
	// struct file* reopen_file = file_duplicate(tmp_aux->file);
	// reopen을 한 파일은 예전 파일과 달라져있을 수 있어서 read byte를 다시 받는다?
	uint32_t read_bytes = file_read_at(tmp_aux->file, kva, tmp_aux->read_bytes, tmp_aux->offset);
	uint32_t zero_bytes = PGSIZE - read_bytes;
	// printf("lazy map 들어왔다222!!\n");
	memset(kva + read_bytes, 0, zero_bytes);
	// printf("lazy map 들어왔다333!!\n");
	page->file.file = tmp_aux->file; // file 복제해서 썼으니 파일 갱신 
	page->file.offset = tmp_aux->offset;
	page->file.read_bytes = read_bytes;
    page->file.zero_bytes = zero_bytes;
	free(tmp_aux);
	return true;
}



/*-------------------------- project.3-map,unmap -----------------------------*/

/* Do the munmap */
void
do_munmap (void *addr) {
	struct thread *t = thread_current();
	struct page* page = spt_find_page(&t->spt, addr);
	struct file* curr_file = page->file.file;
	t->spt.vm.aux = &page->mapping_id;

	// printf("do_mumap :: page->file.file :: %p\n", page->file.file);
	// printf("do mumap :: hash apply 이전\n");
	lock_acquire(&spt_lock);
	hash_apply(&t->spt.vm, munmap_action);
	// printf("do mumap :: hash apply 이후\n");
	lock_release(&spt_lock);
	
	// printf("do_mumap :: page->file.file :: %p\n", page->file.file);
	// file_close(curr_file);
	// printf("do_mumap :: file_close 이후\n");
};


void 
munmap_action (struct hash_elem *e, void* aux) {
	struct thread *t = thread_current();
	struct page* page = hash_entry(e, struct page, hash_elem);
	int mapping_id = *(int *)aux;
	// printf("munmap_action :: munmap_action 이전\n");
	
	// if (page->operations->type == VM_FILE ) {	
	if (page->mapping_id == mapping_id) {
		if (VM_TYPE(page->operations->type) == VM_FILE  && pml4_is_dirty(&t->pml4, page->va)) {
			if (page->frame != NULL){
				
				file_write_at(page->file.file, page->frame->kva, page->file.read_bytes, page->file.offset);

				for (int i = 2; i < t->next_fd; i++) {
					if (i == page->mapping_id) {
						t->fd_table[i] = file_reopen(page->file.file);
					}
				}
			}
		}
		pml4_clear_page(thread_current()->pml4, page->va); /* seunghyun's advice*/
		spt_remove_page(&t->spt, page);
	}
	// printf("munmap_action :: munmap_action 이후\n");
}