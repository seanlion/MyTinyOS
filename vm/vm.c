/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"




/**/

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
/* ---------------------- Project.3 vm_entry ----------------------------------  */
// static bool vm_do_claim_page (struct page *page);
bool vm_do_claim_page (struct page *page);
/* ---------------------- Project.3 vm_entry ----------------------------------  */
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		/* TODO: Insert the page into the spt. */
		/* ---------------------- Project.3 vm_entry ----------------------------------  */
		// page 만큼 할당
		struct page* new_p = (struct page*) malloc(sizeof(struct page));
		enum vm_type new_type = VM_TYPE(type);

		// type에 따른 initailizer 실행
		switch (new_type)
		{
			case VM_UNINIT:
				printf("--------------------error--------------------\n");
				printf("------------- page type : UNINIT-------------\n");
				printf("--------------------error--------------------\n");
				return false;
				break;

			case VM_ANON:
				uninit_new(new_p, upage, init, type, new_p->frame->kva, anon_initializer);
				break;

			case VM_FILE:
				uninit_new(new_p, upage, init, type, new_p->frame->kva, file_backed_initializer);
				break;
			default:
				return false;
				break;
		}

		// page field 초기화
		struct load_info *info = (struct load_info *)aux;

		// new_p->writable = info->writable;
		// new_p->is_loaded = 0;
		// new_p->file = info->file;
		// new_p->offset = info->ofs;
		// new_p->read_bytes = info->read_bytes;
		// new_p->zero_bytes = info->zero_bytes;
		// new_p->va = upage;
		// new_p->frame = NULL;
		// new_p->swap_slot = 0;

		//spt에 새로 만든 page 삽입
		return spt_insert_page(spt, new_p);
		/* ---------------------- Project.3 vm_entry ----------------------------------  */


	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function. */
	/* ---------------------- Project.3 vm_entry ----------------------------------  */
	// address를 가지고 있는 page가 이미 존재하면 해당 page를 반환하고, 없으면 NULL을 반환한다. 
	page = page_lookup(va);
	/* ---------------------- Project.3 vm_entry ----------------------------------  */

	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	/* ---------------------- Project.3 vm_entry ----------------------------------  */
	if (!(hash_insert(&spt->vm, &page->hash_elem)))
	{
		succ = true;
	}
	/* ---------------------- Project.3 vm_entry ----------------------------------  */

	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	/* ---------------------- Project.3 vm_entry ----------------------------------  */
	hash_delete (&spt->vm, &page->hash_elem);
	/* ---------------------- Project.3 vm_entry ----------------------------------  */
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
	struct frame *frame = NULL;
	/* TODO: Fill this function. */

	/* ---------------------- Project.3 vm_entry ----------------------------------  */

	frame = (struct frame*) malloc(sizeof(struct frame));
	frame->kva = palloc_get_page(PAL_USER);

	if(frame->kva == NULL){
		return NULL;
	}

	frame->page = NULL;

	/* ---------------------- Project.3 vm_entry ----------------------------------  */

	ASSERT (frame != NULL);
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
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	/* ---------------------- Project.3 Anonymous Page ----------------------------------  */
	
	/* ---------------------- Project.3 Anonymous Page ----------------------------------  */

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
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	/* ---------------------- Project.3 vm_entry ----------------------------------  */
	page = spt_find_page(&thread_current()->spt, va);
	/* ---------------------- Project.3 vm_entry ----------------------------------  */
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
/* ---------------------- Project.3 Anonymous Page ----------------------------------  */
// static bool
/* ---------------------- Project.3 Anonymous Page ----------------------------------  */
bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	// if(!install_page(page->va, frame->kva, page->writable)){
	// 	return false;
	// }

	if(pml4_get_page(thread_current()->pml4, page->va) == NULL){
		pml4_set_page (thread_current()->pml4, page->va, frame->kva, page->writable);
	}
	else{
		return false;
	}
	
	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	/* ---------------------- Project.3 vm_entry ----------------------------------  */
	hash_init(&spt->vm, page_hash, page_less, NULL);
	/* ---------------------- Project.3 vm_entry ----------------------------------  */
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	 /* ---------------------- Project.3 vm_entry ----------------------------------  */
	hash_destroy(&spt->vm, hash_destructor);
	// writeback 구현할 것
	 /* ---------------------- Project.3 vm_entry ----------------------------------  */
}

/* ---------------------- Project.3 vm_entry ----------------------------------  */
void hash_destructor(struct hash_elem *e, void *aux) {
	struct page *p = hash_entry (e, struct page, hash_elem);
	if (p->frame != NULL) {
		free(p->frame);
	}
	free(p);
}


/* ---------------------- Project.3 vm_entry ----------------------------------  */




/* ---------------------- Project.3 vm_entry ----------------------------------  */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
  const struct page *p = hash_entry (p_, struct page, hash_elem);
  return hash_bytes (&p->va, sizeof p->va);
}
/* ---------------------- Project.3 vm_entry ----------------------------------  */



/* ---------------------- Project.3 vm_entry ----------------------------------  */
bool
page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED) {
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->va < b->va;
}


/* ---------------------- Project.3 vm_entry ----------------------------------  */


/* ---------------------- Project.3 vm_entry ----------------------------------  */
// address를 가지고 있는 page가 이미 존재하면 해당 page를 반환하고, 없으면 NULL을 반환한다. 
struct page * page_lookup (const void *address) {
	struct thread * cur_t = thread_current();
	struct page p;
	struct hash_elem *e;


	p.va = address;
	// va를 가지고 있는 page가 이미 존재하면, 해당을 page 반환하고 없으면 NULL을 반환한다. 
	e = hash_find (&cur_t->spt.vm, &p.hash_elem);

	// 
	return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}
/* ---------------------- Project.3 vm_entry ----------------------------------  */


/* ---------------------- Project.3 vm_entry ----------------------------------  */
// void page_delete(struct hash_elem *e, void *aux) {
// 	struct page *page = hash_entry (e, struct page, hash_elem);
// 	vm_dealloc_page(page);
// }
/* ---------------------- Project.3 vm_entry ----------------------------------  */


/* ---------------------- Project.3 vm_entry ----------------------------------  */
// void vm_destroy (struct hash *vm) {
// 	hash_destroy(vm, page_delete);
// }
/* ---------------------- Project.3 vm_entry ----------------------------------  */
