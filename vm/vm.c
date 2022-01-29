/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include <list.h>

struct load_info{
    struct file *file;
    off_t ofs;
    uint8_t *upage;
    uint32_t read_bytes;
    uint32_t zero_bytes;
    bool writable;
};
unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED);
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
struct page *page_lookup (const void *address);
void page_destroy (const struct hash_elem *hash_elem, void *aux);

#define STACK_LIMIT 0x47380000
/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	list_init(&clock_list);
	lock_init(&clock_list_lock);
	clock_ptr = NULL;
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current()->spt;
	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {

		struct page *new_page = malloc(sizeof(struct page));
		switch (VM_TYPE(type))
		{
		case VM_ANON:
			uninit_new(new_page, upage, init, type, aux, &anon_initializer);
			new_page->mapping_id = -1;
			break;

		case VM_FILE:
			uninit_new(new_page, upage, init, type, aux, &file_backed_initializer);
			struct file_aux * tmp_aux = (struct file_aux *)aux;
			if (aux != NULL)
				new_page->mapping_id = tmp_aux->mapping_id;
			break;
		
		case VM_PAGE_CACHE:
			/* code */
			break;
		
		default:
			break;
		}

		new_page->writable = writable;
		if (spt_insert_page(spt, new_page)) {
			return true;
		}
		else {
			/* setup stack 함수가 false가 날 수 있는 상황 */
			free (new_page);
			goto err;
		}

	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	struct page page;
	struct hash_elem *e;
	page.va = pg_round_down(va);
	e = hash_find(&spt->vm, &page.hash_elem);
	return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;

}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {
	int succ = false;
	if (hash_find(&spt->vm, &page->hash_elem) == NULL) {
		hash_insert(&spt->vm, &page->hash_elem);
		succ = true;
	}
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	if (hash_delete(&spt->vm, &page->hash_elem)) {
		
		pml4_clear_page(thread_current()->pml4, page->va);
		vm_dealloc_page (page);
		return true;
	}
	return false;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	struct frame *curr_frame;
	void *va;

	if (clock_ptr == NULL) {
		clock_ptr = list_begin(&clock_list);
	}

	while (true) {
		curr_frame = list_entry(clock_ptr, struct frame, list_elem);
		va = curr_frame->page->va;
		if (!pml4_is_accessed(thread_current()->pml4, va)) {
			victim = curr_frame;
			break;
		}
		pml4_set_accessed(thread_current()->pml4, va, false);
		clock_ptr = get_next_clock();
		if (clock_ptr == NULL) {
			clock_ptr = list_begin(&clock_list);
		}
	}
	
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim = vm_get_victim ();
	swap_out(victim->page);
	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = malloc(sizeof(struct frame));
	if (frame == NULL)
		free(frame);
	ASSERT (frame != NULL);
	frame->kva = palloc_get_page(PAL_USER);
	if (frame->kva == NULL) {
		free(frame);
		lock_acquire(&clock_list_lock);
		frame = vm_evict_frame();
		lock_release(&clock_list_lock);
	}
	frame->page = NULL;
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr) {
	if (!is_user_vaddr(addr)){ /* bad read 방지 */
		exit(-1);
	}
	addr = pg_round_down(addr);
	while (vm_alloc_page(VM_MARKER_0 | VM_ANON, addr, true)) {
		vm_claim_page(addr);
		addr += PGSIZE;
	}
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f, void *addr,
		bool user, bool write, bool not_present) {
	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *page = NULL;
    if (write && !not_present) {
        exit(-1);
    }
    if (addr == NULL || addr == 0) {
        exit(-1);
    }
	uintptr_t t_rsp = NULL;
	if (!user){ /* kernel에서 넘어오는거 */
		t_rsp = thread_current()->rsp; 
	}
	else{ /* user에서 넘어오는거 */
		t_rsp = f->rsp;
	}
	
	page = spt_find_page(spt, addr);
	if(page!=NULL){
			if(!not_present&&is_user_vaddr(addr))
				exit(-1);
			if(((page->writable) == 0) && write)
				exit(-1);
			return vm_do_claim_page (page);
		}
	else {
		if ((user && write)){
			if (addr > (USER_STACK - (1 << 20)) && addr >= f->rsp - 8 && addr < USER_STACK)
				{
					vm_stack_growth(addr);
					return true;
				}
		}
		return false;
	}
}

/* Free the page. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va) {
	struct page *page = NULL;
	struct thread *t = thread_current();
	page = spt_find_page(&t->spt, va);
	if (page == NULL)
		return false;

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	if (page == NULL)
		return false;

	struct frame *frame = vm_get_frame ();
	frame->page = page;
	page->frame = frame;
	struct thread *t = thread_current();
	if (!pml4_set_page(t->pml4, page->va, frame->kva, page->writable)) {
		return false;
	}
	
	add_frame_to_clock_list(frame);

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->vm, page_hash, page_less, NULL);
	lock_init(&spt_lock);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	bool result = false;
	struct load_aux *aux_child;
	struct hash_iterator i;
	lock_acquire(&spt_lock);
	hash_first(&i, &src->vm);
	while (hash_next(&i)){
		struct page *child_page;
		struct page* parent_page = hash_entry(hash_cur(&i), struct page, hash_elem);
		switch(parent_page->operations->type){
			case VM_UNINIT: /* UNINIT인 페이지는 할당해야 함. */
				aux_child = malloc(sizeof(struct load_aux));
				memcpy(aux_child, parent_page->uninit.aux,sizeof(struct load_aux));
				result = vm_alloc_page_with_initializer(
					parent_page->uninit.type, \ 
					parent_page->va, \
					parent_page->writable, \
					parent_page->uninit.init, \
					aux_child);
				break;

			case VM_ANON:
				result = vm_alloc_page(parent_page->operations->type,parent_page->va, parent_page->writable);
				child_page->mapping_id = parent_page->mapping_id;
				child_page->anon.st_number = parent_page->anon.st_number;

				if (result){
					child_page = spt_find_page(&thread_current()->spt, parent_page->va );
					if (vm_do_claim_page(child_page) == 0){
						return false ;
						}
					memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE); 
					}
				break;

			case VM_FILE:
				result = vm_alloc_page(parent_page->operations->type,parent_page->va, parent_page->writable);
				child_page->mapping_id = parent_page->mapping_id;
				child_page->file.file = parent_page->file.file;
				child_page->file.offset = parent_page->file.offset;
				child_page->file.read_bytes = parent_page->file.read_bytes;
				child_page->file.zero_bytes = parent_page->file.zero_bytes;
				child_page->file.writable = parent_page->file.writable;
				if (result){
					struct page *child_page = spt_find_page(&thread_current()->spt, parent_page->va );
					if (vm_do_claim_page(child_page) == 0){
						return false ;
						}
					memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE); 
					}
				break;

			default:
				break;
		}
	}
	lock_release(&spt_lock);

	return result;

}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {

	if (hash_empty(&spt->vm)){ /* 예외처리 */
		return;
	}
	hash_destroy(&spt->vm, page_delete);

}

/* Returns a hash value for page p. */
uint64_t
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
  const struct page *p = hash_entry (p_, struct page, hash_elem);
  return hash_bytes (&p->va, sizeof p->va);
}

/* page 간 va를 비교 (a가 크면 false, b가 크면 true 반환) */
bool page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED) {
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);
  return a->va < b->va;
}


void 
page_delete(const struct hash_elem *e, void *aux){
	struct page* page_for_deletion = hash_entry(e, struct page, hash_elem);
	vm_dealloc_page(page_for_deletion);
}

void
add_frame_to_clock_list(struct frame *frame) {
	list_push_back(&clock_list, &frame->list_elem);
}

void
del_frame_to_clock_list(struct frame *frame) {
	if (!list_empty(&clock_list))
		list_remove(&frame->list_elem);
}

struct frame*
alloc_frame(void) {
	struct frame *frame = vm_get_frame();
	add_frame_to_clock_list(frame);
	return frame;
}
 
void 
free_frame(void *kva) {
	struct list_elem *e;
	struct frame *frame;
	for (e = list_begin(&clock_list); e != list_end(&clock_list);) {
		frame = list_entry(e, struct frame, list_elem);
		if (frame->kva == kva) {
			__free_page(frame);
			break;
		}
		e = list_next(e);
	}
	return;
}

void 
__free_page(struct frame *frame) {
	del_frame_to_clock_list(frame);
	free(frame);
}

struct list_elem*
get_next_clock() {
	clock_ptr = list_next(clock_ptr);
	if (clock_ptr == list_end(&clock_list))
		return NULL;
	return clock_ptr;
}



