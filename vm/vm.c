/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

/* ---------------------- >> Project.3 Anony >> -----------------------  */
struct load_info{
    struct file *file;
    off_t ofs;
    uint8_t *upage;
    uint32_t read_bytes;
    uint32_t zero_bytes;
    bool writable;
};
/* ---------------------- << Project.3 Anony << -----------------------  */


/* ---------------------- Project.3 ----------------------------  */
unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED);
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
struct page *page_lookup (const void *address);
void page_destroy (const struct hash_elem *hash_elem, void *aux);
/* ---------------------- Project.3 ----------------------------  */


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
// 이니셜라이저
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {

		/* ---------------------- >> Project.3 MEM Management >> ---------------------------- */

		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		 
		 // 페이지를 선언하고 메모리를 할당받는다.
		 struct page *p = (struct page*) malloc(sizeof(struct page));


		// type에 따라 initializer가 달라진다. 
		switch(VM_TYPE(type)){
			case VM_ANON:
				uninit_new(p, upage, init, type, aux, anon_initializer);
				break;
			case VM_FILE:
				uninit_new(p, upage, init, type, aux, file_backed_initializer);
				break;
			default:
				break;
		}
		p->type = VM_UNINIT; // page의 첫 타입은 uninit이 되어야한다.
		p->is_loaded = 0; // 현재 할당받지 않은 상태이다.
		p->frame = NULL; // claim_frame 전에는 page는 frame과 이어지지 않는다.
		p->writable = writable;
		// printf("---debug// vm_alloc_page // writable : %d \n", p->writable);
		/* TODO: Insert the page into the spt. */
		// page를 spt(hash table)에 삽입한다. 
        return spt_insert_page(spt, p);

		/* ---------------------- << Project.3 MEM Management << ---------------------------- */

	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;

	/* ---------------------- >> Project.3 MEM Management >> ---------------------------- */

	/* TODO: Fill this function. */
	// page_lookup()을 통해 va에 해당하는 page가 있는지 찾는다. 
	page = page_lookup(pg_round_down(va));

	/* ---------------------- << Project.3 MEM Management << ---------------------------- */

	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;

	/* ---------------------- >> Project.3 MEM Management >> ---------------------------- */

	/* TODO: Fill this function. */
	// page의 hash_elem을 hash table(vm)에 삽입한다. 
	if (hash_insert(&spt->vm, &page->hash_elem) == NULL){
		succ = true;
	}

	/* ---------------------- << Project.3 MEM Management << ---------------------------- */

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


	/* ---------------------- >> Project.3 Anony >> ---------------------------- */

	/* TODO: The policy for eviction is up to you. */
	// frame에 메모리를 할당한다.  
	frame = (struct frame*) malloc(sizeof(struct frame));
	
	// kva를 통해 물리메모리 할당을 요청한다.
	frame->kva = palloc_get_page(PAL_USER | PAL_ZERO);

	// 할당 실패시, null 반환
	if (frame->kva == NULL){
        free(frame);
		return NULL;
	}

	frame->page = NULL;

	/* ---------------------- << Project.3 Anony << ---------------------------- */

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
// 스택
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
//핸들폴트
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	// printf("---debug//\n");
	// printf("---debug// try_handle // entry\n");
	// printf("---debug// try_handle // address : %p\n", addr);
	// printf("---debug// try_handle // user : %d\n", user);
	// printf("---debug// try_handle // write : %d\n", write);
	// printf("---debug// try_handle // not_present : %d\n", not_present);
	struct page *cur_p = spt_find_page(&thread_current()->spt, addr);
	// printf("---debug// try_handle // cur_p_va : %p\n", cur_p->va);
	// printf("---debug// try_handle // cur_p_writable : %d\n", cur_p->writable);
	// printf("---debug//\n");
	/* ---------------------- >> Project.3 Anony >> ---------------------------- */
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	// printf("---debug// try_handle // address : %p\n", addr);


	if(addr == NULL || is_kernel_vaddr(addr)){
		// printf("---debug// try_handle // kernel?\n");
        exit(-1);
	}

	if(!not_present){
		// printf("---debug// try_handle // !not_present\n");
		exit(-1);
		return false;
    }

    page = spt_find_page(spt, addr);
    
    if(page == NULL){
		// printf("---debug// try_handle // !page_null\n");

		exit(-1);

        return false;
    }

	if(page->writable == false && write == true){
		exit(-1);
		return false;
	}

	/* ---------------------- << Project.3 Anony << ---------------------------- */


	// printf("---debug// try_handle // before_return\n");
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
//클레임페이지
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;

    /* ---------------------- >> Project.3 Anony >> ---------------------------- */
	/* TODO: Fill this function */
	// va에 해당하는 page가 있는지 찾는다.
	// printf("---debug// claim_page // old_p->va : %p \n", va );
	page = spt_find_page(&thread_current()->spt, va);
    if(page == NULL){
		// printf("---debug// claim_page // false?\n");
		// printf("---debug// claim_page // old_p->va : %p \n", va );
        return false;
    }
	/* ---------------------- << Project.3 Anony << ---------------------------- */

	return vm_do_claim_page (page);
}

// 두클레임
/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	// printf("---debug// do_claim_page // page : %p \n", page );
	/* Set links */
	frame->page = page;
	page->frame = frame;
	// printf("---debug// do_claim_page // frame : %p \n", frame );
	// printf("---debug// do_claim_page // frame->page : %p \n", frame->page );
	// printf("---debug// do_claim_page // page->frame : %p \n", page->frame );

	/* ---------------------- >> Project.3 MEM Management >> ---------------------------- */

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
    struct thread *t = thread_current();

	// page를 plm4 리스트에 넣는다. 
	if(!(pml4_get_page(thread_current()->pml4, page->va) == NULL && pml4_set_page (thread_current()->pml4, page->va, frame->kva, page->writable)))
	{
        printf("install_page false\n");
        page->frame = NULL;
        palloc_free_page(frame->kva);
		free(frame);
		printf("---debug// do_claim_page // false? \n");
		return false;
	}
	/* ---------------------- << Project.3 MEM Management << ---------------------------- */


	// frame 할당에 성공했다면 swap_in 실행 
	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {

    /* ---------------------- >> Project.3 MEM Management >> ---------------------------- */
	// spt를 초기화할 때, hash_table (vm) 를 초기화한다.
	hash_init(&spt->vm, page_hash, page_less, NULL);
	/* ---------------------- << Project.3 MEM Management << ---------------------------- */


}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	/* ---------------------- >> Project.3 MEM Management >> ---------------------------- */
	struct hash *old_h = &src->vm;
	struct hash_iterator i;

	hash_first (&i, old_h);
	while (hash_next (&i))
	{
		// page를 복사한다.
		struct page *old_p = hash_entry (hash_cur (&i), struct page, hash_elem);
		struct page *new_p = (struct page*) malloc(sizeof(struct page));
		memcpy (new_p, old_p, sizeof(struct page));



		// printf("---debug//\n");
		// printf("---debug// spt_copy // copy // old_p : %p \n", old_p );
		// printf("---debug// spt_copy // copy // type : %d \n", old_p->type );
		// printf("---debug//\n");



		// 복사로받은 frame은 해제한다.
		new_p->frame = NULL;
		spt_insert_page(dst, new_p);
		// switch로 타입을 나눈다. 
		switch(VM_TYPE(new_p->type)){
			// uninit인 경우, info도 할당한다.
			// uninit이면서 info가 없는 경우가 있을까? 
			case VM_UNINIT:
			{
				// printf("---debug//\n");
				// printf("---debug// spt_copy // uninit : \n" );
				// printf("---debug// spt_copy // uninit // old_p : %p \n", old_p );
				// printf("---debug// spt_copy // uninit // old_p->va : %p \n", old_p->va );
				// printf("---debug//\n");
				struct load_info *n_info = (struct load_info*) malloc(sizeof(struct load_info));
				memcpy(n_info, old_p->uninit.aux, sizeof(struct load_info));
				new_p->uninit.aux = n_info;
				break;
			}
			
			// anon인 경우는 물리 메모리도 복사한다.
			case VM_ANON:
			{
				// printf("---debug//\n");
				// printf("---debug// spt_copy // anon_va : \n" );
				// printf("---debug// spt_copy // anon_va // old_p : %p \n", old_p );
				// printf("---debug// spt_copy // anon_va // old_p->va : %p \n", old_p->va );
				// printf("---debug//\n");
				if (vm_claim_page(new_p->va))
				{
					memcpy(new_p->frame->kva, old_p->frame->kva, PGSIZE);
				}
				else 
				{
					// printf("---debug//\n");
					// printf("---debug// spt_copy // anon_claim_error\n" );
					// printf("---debug//\n");
					return false;
				}
				break;
			}
			// file인 경우는 물리 메모리도 복사한다. 
			case VM_FILE:
			{
				if (vm_claim_page(new_p->va))
				{
					memcpy(new_p->frame->kva, old_p->frame->kva, PGSIZE);
				}
				else
				{
					// printf("---debug//\n");
					// printf("---debug// spt_copy // file_claim_error\n" );
					// printf("---debug//\n");
					return false;
				}
				break;
			}
			default:
				break;
		}
		// spt_insert_page(dst, new_p);
	}

	return true;


	/* ---------------------- << Project.3 MEM Management << ---------------------------- */
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {

	/* ---------------------- >> Project.3 MEM Management >> ---------------------------- */

	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */

	struct hash *h = &spt->vm;
	// hash_clear(h, page_destroy);
	// free(h->buckets);

	/* ---------------------- << Project.3 MEM Management << ---------------------------- */
}




/* ---------------------- >> Project.3 MEM Management >> ---------------------------- */

// hash_elem에 해당하는 페이지의 hash값 반환 
unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED) {
	const struct page *p = hash_entry (p_, struct page, hash_elem);
	return hash_bytes (&p->va, sizeof p->va);
}

/* ---------------------- << Project.3 MEM Management << ---------------------------- */





/* ---------------------- >> Project.3 MEM Management >> ---------------------------- */

// page 간 va를 비교 (a가 크면 false, b가 크면 true 반환)
bool page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED) {
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);
  return a->va < b->va;
}
/* ---------------------- << Project.3 MEM Management << ---------------------------- */



/* ---------------------- >> Project.3 MEM Management >> ---------------------------- */

// va에 해당하는 page가 있으면 반환 (없으면 null)
struct page *page_lookup (const void *address) {
    struct page p;
	struct hash_elem *e;

	p.va = address;
	e = hash_find (&thread_current()->spt.vm, &p.hash_elem);
	return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}

/* ---------------------- << Project.3 MEM Management << ---------------------------- */



/* ---------------------- >> Project.3 MEM Management >> ---------------------------- */

// hash_clear()의 destructor 인자에 들어가는 함수.
// 해당 hash에 해당하는 page와 frame의 메모리를 반환한다. 
void page_destroy (const struct hash_elem *hash_elem, void *aux){
    const struct page *p = hash_entry (hash_elem, struct page, hash_elem);
    destroy(p);
	free(p);
}
/* ---------------------- << Project.3 MEM Management << ---------------------------- */



/* ---------------------- >> Project.3 MEM Management >> ---------------------------- */
/* ---------------------- << Project.3 MEM Management << ---------------------------- */



/* ---------------------- >> Project.3 MEM Management >> ---------------------------- */
/* ---------------------- << Project.3 MEM Management << ---------------------------- */



/* ---------------------- >> Project.3 MEM Management >> ---------------------------- */
/* ---------------------- << Project.3 MEM Management << ---------------------------- */



