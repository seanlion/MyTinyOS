/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
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
	// printf("upage address %p\n", (uint64_t*) upage);
	/* Check wheter the upage is already occupied or not. */
	// printf("thread in load_segment & alloc_page: %p\n", &thread_current()->magic);
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *new_page = malloc(sizeof(struct page));
		if (VM_TYPE(type) == VM_ANON)
			uninit_new(new_page, upage, init, type, aux, &anon_initializer);
		else if (VM_TYPE(type) == VM_FILE)
			uninit_new(new_page, upage, init, type, aux, &file_backed_initializer);
		new_page->writable = writable;

		/* TODO: Insert the page into the spt. */
		if (spt_insert_page(spt, new_page))
			return true;
		else
			{
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

	// /* TODO: Fill this function. */
	// struct hash_iterator i;
	// hash_first(&i, &spt->vm);
	// while (hash_next(&i)){
	// 	struct page *cur_page = hash_entry(hash_cur(&i),struct page, hash_elem);
	// 	if (cur_page->va == va){
	// 		return cur_page;
	// 	}
	// }
	// return NULL;
	page.va = pg_round_down(va);
	e = hash_find(&spt->vm, &page.hash_elem);
	return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {
	int succ = false;
	/* TODO: Fill this function. */
	if (hash_find(&spt->vm, &page->hash_elem) == NULL) {
		hash_insert(&spt->vm, &page->hash_elem);
		succ = true;
	}
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	/* TODO: Fill this function. */
	struct frame *frame = malloc(sizeof(struct frame));
	if (frame == NULL)
		free(frame);
	ASSERT (frame != NULL);

	frame->kva = palloc_get_page(PAL_USER | PAL_ZERO);
	if (frame->kva == NULL) {
		free(frame);
	}
	frame->page = NULL;

	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
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
    if (addr == NULL) {
        exit(-1);
    }
    if (addr == 0) {
        exit(-1);
    }
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	page = spt_find_page(spt, addr);

	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va) {
	struct page *page = NULL;
	
	/* TODO: Fill this function */
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

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	struct thread *t = thread_current();
	if (pml4_set_page(t->pml4, page->va, frame->kva, page->writable))
		return swap_in (page, frame->kva);
	// pml4_set_page(t->pml4, page->va, frame->kva, true);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->vm, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
			// void *old_aux = src->vm.aux;
			// src->vm.aux = &dst->vm;
			// hash_apply(&src->vm, page_insert);
			// src->vm.aux = old_aux;

	// iterator 돌리기
	bool result = false;
	struct load_aux *aux_child;
	struct hash_iterator i;
	hash_first(&i, &src->vm);
	while (hash_next(&i)){
		// child hash에 들어갈 페이지를 부모 페이지를 가져옴.
		struct page* parent_page = hash_entry(hash_cur(&i), struct page, hash_elem);
		switch(parent_page->operations->type){
			case VM_UNINIT: // UNINIT인 페이지는 할당해야 함.
				aux_child = malloc(sizeof(struct load_aux));
				memcpy(aux_child, parent_page->uninit.aux,sizeof(struct load_aux));
				result = vm_alloc_page_with_initializer(
					parent_page->uninit.type, \ 
					parent_page->va, \
					parent_page->writable, \
					parent_page->uninit.init, \
					aux_child);
				break;
			default:
				result = vm_alloc_page(parent_page->operations->type,parent_page->va, parent_page->writable);
				if (result){
					struct page *child_page = spt_find_page(&thread_current()->spt, parent_page->va );
					if (vm_do_claim_page(child_page) == 0){
						return false;
						}
					memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE); 
					}
				break;
		}
	}

	return result;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	if (hash_empty(&spt->vm)){ // 예외처리
		return;
	}
	// hash_clear(&spt->vm, page_delete);
	hash_destroy(&spt->vm, page_delete);
	// free(&spt->vm); spt는 free 할 필요 없을듯.

}

/* Returns a hash value for page p. */
uint64_t
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
  const struct page *p = hash_entry (p_, struct page, hash_elem);
  return hash_bytes (&p->va, sizeof p->va);
}

/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED) {
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);
  return a->va < b->va;
}

void page_delete(const struct hash_elem *e, void *aux){
	struct page* page_for_deletion = hash_entry(e, struct page, hash_elem);
	vm_dealloc_page(page_for_deletion);
}