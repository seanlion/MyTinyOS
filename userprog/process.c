#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <list.h>
#include <stdbool.h>
#include "userprog/gdt.h"
#include "userprog/tss.h"
#include "userprog/syscall.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

#include "filesys/fat.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "intrinsic.h"

#include "userprog/exception.h"


#ifdef VM
#include "vm/vm.h"
#include "hash.h"
#endif
static void process_cleanup (void);
static bool load (const char *file_name, struct intr_frame *if_);
static void initd (void *f_name);
static void __do_fork (void *);
/*-------------------------- project.2-System Call -----------------------------*/
// static struct lock process_lock;
/*-------------------------- project.2-System Call -----------------------------*/
/* We load ELF binaries.  The following definitions are taken
 * from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
#define EI_NIDENT 16

#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
 * This appears at the very beginning of an ELF binary. */
struct ELF64_hdr {
	unsigned char e_ident[EI_NIDENT];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

struct ELF64_PHDR {
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};

/* Abbreviations */
#define ELF ELF64_hdr
#define Phdr ELF64_PHDR

static bool setup_stack (struct intr_frame *if_);
static bool validate_segment (const struct Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes,
		bool writable);

/* General process initializer for initd and other process. */
static void
process_init (void) {
	struct thread *current = thread_current ();
}

/* Starts the first userland program, called "initd", loaded from FILE_NAME.
 * The new thread may be scheduled (and may even exit)
 * before process_create_initd() returns. Returns the initd's
 * thread id, or TID_ERROR if the thread cannot be created.
 * Notice that THIS SHOULD BE CALLED ONCE. */
tid_t
process_create_initd (const char *file_name) {
	char *fn_copy;
	tid_t tid;

	/* Make a copy of FILE_NAME.
	 * Otherwise there's a race between the caller and load(). */
	fn_copy = palloc_get_page (0);
	if (fn_copy == NULL)
		return TID_ERROR;
	strlcpy (fn_copy, file_name, PGSIZE);

	/*-------------------------- project.2-Parsing -----------------------------*/
	// 스레드  이름  파싱
	char del[] = " ";
	char *save_ptr = NULL;
	char *program_name = strtok_r(file_name, del, &save_ptr);
	/*-------------------------- project.2-Parsing -----------------------------*/


	/* Create a new thread to execute FILE_NAME. */
	tid = thread_create (file_name, PRI_DEFAULT, initd, fn_copy);
	if (tid == TID_ERROR)
		palloc_free_page (fn_copy);
	return tid;
}

/* A thread function that launches first user process. */
static void
initd (void *f_name) {
#ifdef VM
	supplemental_page_table_init (&thread_current ()->spt);
#endif

	process_init ();

	if (process_exec (f_name) < 0)
		PANIC("Fail to launch initd\n");
	NOT_REACHED ();
}

/* Clones the current process as `name`. Returns the new process's thread id, or
 * TID_ERROR if the thread cannot be created. */
tid_t
process_fork (const char *name, struct intr_frame *if_ UNUSED) {
	/* Clone current thread to new thread.*/
	// return thread_create (name,
	// 		PRI_DEFAULT, __do_fork, thread_current ());
    /*-------------------------- project.2-Process  -----------------------------*/
    struct thread* cur_t = thread_current();
    int child_pid = thread_create (name, cur_t->priority, __do_fork, cur_t);
    if (!(child_pid == TID_ERROR))
    {
        struct thread* child_t = get_child_process(child_pid);
        sema_down(&cur_t->sema_child_load);
        if (child_t->fork_fail) {
            child_pid = -1;
        }
    }
    // 자식이면 return 0, 부모이면 return child_pid
    return child_pid;
    /*-------------------------- project.2-Process  -----------------------------*/
}

#ifndef VM
/* Duplicate the parent's address space by passing this function to the
 * pml4_for_each. This is only for the project 2. */
// aux = &parrent thread
static bool
duplicate_pte (uint64_t *pte, void *va, void *aux) {
	struct thread *current = thread_current ();
	struct thread *parent = (struct thread *) aux;
	void *parent_page;
	void *newpage;
	bool writable;
	/* 1. TODO: If the parent_page is kernel page, then return immediately. */
    /*-------------------------- project.2-Process  -----------------------------*/
    if (is_kernel_vaddr(va)) return true;
    /*-------------------------- project.2-Process  -----------------------------*/
	/* 2. Resolve VA from the parent's page map level 4. */
    // 부모의 pml4를 자식의 pml4로 복사 하기위해서, 부모pml4의 va를 가져온다.
	parent_page = pml4_get_page (parent->pml4, va);


	/* 3. TODO: Allocate new PAL_USER page for the child and set result to
	 *    TODO: NEWPAGE. */
    /*-------------------------- project.2-Process  -----------------------------*/
    newpage = palloc_get_page(PAL_USER);
    /*-------------------------- project.2-Process  -----------------------------*/

	/* 4. TODO: Duplicate parent's page to the new page and
	 *    TODO: check whether parent's page is writable or not (set WRITABLE
	 *    TODO: according to the result). */
    /*-------------------------- project.2-Process  -----------------------------*/
    memcpy(newpage, parent_page, PGSIZE);
    writable = is_writable(pte);
    /*-------------------------- project.2-Process  -----------------------------*/

	/* 5. Add new page to child's page table at address VA with WRITABLE
	 *    permission. */
	if (!pml4_set_page (current->pml4, va, newpage, writable)) {
        palloc_free_page(newpage);
		/* 6. TODO: if fail to insert page, do error handling. */
        return false;
	}
	return true;
}
#endif

/* A thread function that copies parent's execution context.
 * Hint) parent->tf does not hold the userland context of the process.
 *       That is, you are required to pass second argument of process_fork to
 *       this function. */
static void
__do_fork (void *aux) {
	struct intr_frame if_;
	struct thread *parent = (struct thread *) aux;
	struct thread *current = thread_current ();
	/* TODO: somehow pass the parent_if. (i.e. process_fork()'s if_) */
    /*-------------------------- project.2-Process  -----------------------------*/
    struct intr_frame *parent_if = &parent->fork_tf;
    /*-------------------------- project.2-Process  -----------------------------*/
	bool succ = true;
	/* 1. Read the cpu context to local stack. */
	memcpy (&if_, parent_if, sizeof (struct intr_frame));
	/* 2. Duplicate PT */
	current->pml4 = pml4_create();
	if (current->pml4 == NULL)
		goto error;
    // activate는 virtual address와 physical address를 연결시킨다.
	process_activate (current);
#ifdef VM
	supplemental_page_table_init (&current->spt);
	current->rsp = parent->rsp;
	if (!supplemental_page_table_copy (&current->spt, &parent->spt))
		goto error;
#else
	if (!pml4_for_each (parent->pml4, duplicate_pte, parent))
		goto error;
#endif

	/* TODO: Your code goes here.
	 * TODO: Hint) To duplicate the file object, use `file_duplicate`
	 * TODO:       in include/filesys/file.h. Note that parent should not return
	 * TODO:       from the fork() until this function successfully duplicates
	 * TODO:       the resources of parent.*/
    /*-------------------------- project.2-Process  -----------------------------*/
    for (int i = 2 ; i < parent->next_fd ; i++) {
        if (parent->fd_table[i] == NULL) continue;
        // current->fd_table[i] = file_duplicate(parent->fd_table[i]);
		struct file* reopen = file_duplicate(parent->fd_table[i]);
        // current->fd_table[i] = reopen;
		process_add_file(reopen);
    }
    current->next_fd = parent->next_fd;
    sema_up(&parent->sema_child_load);
    /*-------------------------- project.2-Process  -----------------------------*/

	process_init ();
	/* Finally, switch to the newly created process. */
	if (succ)
        /*-------------------------- project.2-Process  -----------------------------*/
        if_.R.rax = 0;
        /*-------------------------- project.2-Process  -----------------------------*/
		do_iret (&if_);
error:
    /*-------------------------- project.2-Process  -----------------------------*/
    sema_up(&parent->sema_child_load);
    /*-------------------------- project.2-oom  -----------------------------*/
    current->fork_fail = true;
    /*-------------------------- project.2-oom  -----------------------------*/
    /*-------------------------- project.2-Process  -----------------------------*/
	thread_exit ();
}

/* Switch the current execution context to the f_name.
 * Returns -1 on fail. */
int
process_exec (void *f_name) {
    /*-------------------------- project.2-Parsing -----------------------------*/
	char *file_name = f_name;
    char *file_static_name[46];
    // char *file_static_name[64];
    memcpy(file_static_name, file_name, strlen(file_name)+1);
	if (file_static_name == NULL)
		return -1;

    /*-------------------------- project.2-Parsing -----------------------------*/
	bool success;
	/* We cannot use the intr_frame in the thread structure.
	 * This is because when current thread rescheduled,
	 * it stores the execution information to the member. */
	struct intr_frame _if;
	_if.ds = _if.es = _if.ss = SEL_UDSEG;
	_if.cs = SEL_UCSEG;
	_if.eflags = FLAG_IF | FLAG_MBS;

	/*-------------------------- project.2-Parsing -----------------------------*/
	// 인자들을 띄어쓰기 기준으로 토큰화 및 토큰의 개수 계산
	// strtok_r() 함수 이용
	// /* Initialize interrupt frame and load executable. */
	/*-------------------------- project.2-Parsing -----------------------------*/

	/* We first kill the current context */
	/*cleanup에서 pt kill을 안하기 위한 방식 */
#ifdef VM
	supplemental_page_table_kill (&thread_current()->spt);
#endif
	process_cleanup ();
#ifdef VM // table_kill에서 hash_destroy를 쓰기 때문에 exec시 init을 다시 해줘야 함.
	supplemental_page_table_init(&thread_current()->spt);
#endif

    /*-------------------------- project.2-Parsing -----------------------------*/
	char *token, *ptr, *last;
	int token_count = 0;
	char* arg_list[64];
	token = strtok_r(file_static_name, " ", &last);
	char *tmp_save = token;
	arg_list[token_count] = token;
	while ( token != NULL)
	{
		token = strtok_r(NULL, " ", &last);
		token_count ++;
		arg_list[token_count] = token;
	}
	/* And then load the binary */
    success = load (tmp_save, &_if);
    if(!success) return-1;
    argument_stack(arg_list, token_count , &_if);
	/*-------------------------- project.2-Parsing -----------------------------*/
	// printf("process_exec에서 load를 진행했다!!!!\n");
	/* If load failed, quit. */
    /*-------------------------- project.2-Parsing -----------------------------*/

    // if (is_kernel_vaddr(file_name)) {
	//     palloc_free_page (file_name);
    // }
    /*-------------------------- project.2-Parsing -----------------------------*/
	// struct thread* t = thread_current();
    // if (!success) {

    //     t->is_load = 0;
    //     return -1;
    // }
    // else t->is_load = 1;
	// PANIC("process exec의 do_iret 전 까지\n");
	/* Start switched process. */
	do_iret (&_if);
	NOT_REACHED ();
}


/* Waits for thread TID to die and returns its exit status.  If
 * it was terminated by the kernel (i.e. killed due to an
 * exception), returns -1.  If TID is invalid or if it was not a
 * child of the calling process, or if process_wait() has already
 * been successfully called for the given TID, returns -1
 * immediately, without waiting.
 *
 * This function will be implemented in problem 2-2.  For now, it
 * does nothing. */
int
process_wait (tid_t child_tid UNUSED) {
	/* XXX: Hint) The pintos exit if process_wait (initd), we recommend you
	 * XXX:       to add infinite loop here before
	 * XXX:       implementing the process_wait. */

    /* ----------------------------------- Project2.Process --------------------------------*/
    struct thread *child_t = get_child_process(child_tid);
    // printf('t_name : %d ', child_tid);
    // printf("process_wait 1\n");
    if (child_t == NULL) return -1;
    // printf("process_wait 2\n");

   	sema_down(&child_t->sema_exit);
    // printf("process_wait 3\n");

    if (child_t->is_exit) {
        int rtn_status = child_t->exit_status;
        remove_child_process(child_t);
        // printf("process_wait 4\n");

        return rtn_status;
    }
    // printf("process_wait 5\n");

    remove_child_process(child_t);
    // printf("process_wait 6\n");

    return -1;
    /* ----------------------------------- Project2.Process --------------------------------*/
}


/* Free the current process's resources. */
static void
process_cleanup (void) {
	struct thread *curr = thread_current ();

// #ifdef VM
// 	supplemental_page_table_kill (&curr->spt);
// #endif

	uint64_t *pml4;
	/* Destroy the current process's page directory and switch back
	 * to the kernel-only page directory. */
	pml4 = curr->pml4;
	if (pml4 != NULL) {
		/* Correct ordering here is crucial.  We must set
		 * cur->pagedir to NULL before switching page directories,
		 * so that a timer interrupt can't switch back to the
		 * process page directory.  We must activate the base page
		 * directory before destroying the process's page
		 * directory, or our active page directory will be one
		 * that's been freed (and cleared). */
		curr->pml4 = NULL;
		pml4_activate (NULL);
		pml4_destroy (pml4);
	}
}

/* Sets up the CPU for running user code in the nest thread.
 * This function is called on every context switch. */
void
process_activate (struct thread *next) {
	/* Activate thread's page tables. */
	pml4_activate (next->pml4);

	/* Set thread's kernel stack for use in processing interrupts. */
	tss_update (next);
}



/* Loads an ELF executable from FILE_NAME into the current thread.
 * Stores the executable's entry point into *RIP
 * and its initial stack pointer into *RSP.
 * Returns true if successful, false otherwise. */
static bool
load (const char *file_name, struct intr_frame *if_) {
	struct thread *t = thread_current ();
	struct ELF ehdr;
	struct file *file = NULL;
	off_t file_ofs;
	bool success = false;
	int i;

	/* Allocate and activate page directory. */
	t->pml4 = pml4_create ();
	if (t->pml4 == NULL)
		goto done;
	process_activate (thread_current ());

	/* Open executable file. */
	// 	프로그램의 파일을 open 할 때 file_deny_write() 함수를 호출
	// 실행중인 파일 구조체를 thread 구조체에 추가
	lock_acquire(&filesys_lock);
	/*-------------------------- project.2-Denying write -----------------------------*/
	// printf("load에서 file_name은??? %s\n", file_name);
	file = filesys_open (file_name);
	// printf("load에서 file은? %p \n", file);
	if (file == NULL) {
		// lock_release(&filesys_lock);
        printf ("load: %s: open failed\n", file_name);
		/*-------------------------- project.2-Denying write -----------------------------*/
		goto done;
	}

	/*-------------------------- project.2-Denying write -----------------------------*/
	t->running_file = file;
	// file_deny_write(t->running_file);
	// lock_release(&filesys_lock);
	/*-------------------------- project.2-Denying write -----------------------------*/

/* Read and verify executable header. */
	if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
			|| memcmp (ehdr.e_ident, "\177ELF\2\1\1", 7)
			|| ehdr.e_type != 2
			|| ehdr.e_machine != 0x3E // amd64
			|| ehdr.e_version != 1
			|| ehdr.e_phentsize != sizeof (struct Phdr)
			|| ehdr.e_phnum > 1024) {
		printf ("load: %s: error loading executable\n", file_name);
		goto done;
	}

	/* Read program headers. */
	file_ofs = ehdr.e_phoff;
	for (i = 0; i < ehdr.e_phnum; i++) {
		struct Phdr phdr;

		if (file_ofs < 0 || file_ofs > file_length (file))
			goto done;
		file_seek (file, file_ofs);

		if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
			goto done;
		file_ofs += sizeof phdr;
		switch (phdr.p_type) {
			case PT_NULL:
			case PT_NOTE:
			case PT_PHDR:
			case PT_STACK:
			default:
				/* Ignore this segment. */
				break;
			case PT_DYNAMIC:
			case PT_INTERP:
			case PT_SHLIB:
				goto done;
			case PT_LOAD:
				if (validate_segment (&phdr, file)) {
					bool writable = (phdr.p_flags & PF_W) != 0;
					uint64_t file_page = phdr.p_offset & ~PGMASK;
					uint64_t mem_page = phdr.p_vaddr & ~PGMASK;
					uint64_t page_offset = phdr.p_vaddr & PGMASK;
					uint32_t read_bytes, zero_bytes;
					if (phdr.p_filesz > 0) {
						/* Normal segment.
						 * Read initial part from disk and zero the rest. */
						read_bytes = page_offset + phdr.p_filesz;
						zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
								- read_bytes);
					} else {
						/* Entirely zero.
						 * Don't read anything from disk. */
						read_bytes = 0;
						zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
					}
					// printf("mem_page address in load: %p\n", mem_page);
					// printf("thread in load: %p\n", t);
					if (!load_segment (file, file_page, (void *) mem_page,
								read_bytes, zero_bytes, writable))
						goto done;
				}
				else
					goto done;
				break;
		}
	}

	/* Set up stack. */
	if (!setup_stack (if_))
		goto done;


	/* Start address. */
	if_->rip = ehdr.e_entry;
	
	/* TODO: Your code goes here.
	 * TODO: Implement argument passing (see project2/argument_passing.html). */

	success = true;

done:
	lock_release(&filesys_lock);
	/* We arrive here whether the load is successful or not. */
    /*-------------------------- project.2-Denying write -----------------------------*/
	// file_close (file);
    /*-------------------------- project.2-Denying write -----------------------------*/
	// PANIC("load의 done까지\n");
	return success;

}


/* Checks whether PHDR describes a valid, loadable segment in
 * FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Phdr *phdr, struct file *file) {
	/* p_offset and p_vaddr must have the same page offset. */
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
		return false;

	/* p_offset must point within FILE. */
	if (phdr->p_offset > (uint64_t) file_length (file))
		return false;

	/* p_memsz must be at least as big as p_filesz. */
	if (phdr->p_memsz < phdr->p_filesz)
		return false;

	/* The segment must not be empty. */
	if (phdr->p_memsz == 0)
		return false;

	/* The virtual memory region must both start and end within the
	   user address space range. */
	if (!is_user_vaddr ((void *) phdr->p_vaddr))
		return false;
	if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
		return false;

	/* The region cannot "wrap around" across the kernel virtual
	   address space. */
	if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
		return false;

	/* Disallow mapping page 0.
	   Not only is it a bad idea to map page 0, but if we allowed
	   it then user code that passed a null pointer to system calls
	   could quite likely panic the kernel by way of null pointer
	   assertions in memcpy(), etc. */
	if (phdr->p_vaddr < PGSIZE)
		return false;

	/* It's okay. */
	return true;
}

#ifndef VM
/* Codes of this block will be ONLY USED DURING project 2.
 * If you want to implement the function for whole project 2, implement it
 * outside of #ifndef macro. */

/* load() helpers. */
static bool install_page (void *upage, void *kpage, bool writable);

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	file_seek (file, ofs);
	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* Get a page of memory. */
		uint8_t *kpage = palloc_get_page (PAL_USER);
		if (kpage == NULL)
			return false;

		/* Load this page. */
		if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes) {
			palloc_free_page (kpage);
			return false;
		}
		memset (kpage + page_read_bytes, 0, page_zero_bytes);

		/* Add the page to the process's address space. */
		if (!install_page (upage, kpage, writable)) {
			printf("fail\n");
			palloc_free_page (kpage);
			return false;
		}

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a minimal stack by mapping a zeroed page at the USER_STACK */
static bool
setup_stack (struct intr_frame *if_) {
	uint8_t *kpage;
	bool success = false;

	kpage = palloc_get_page (PAL_USER | PAL_ZERO);
	if (kpage != NULL) {
		success = install_page (((uint8_t *) USER_STACK) - PGSIZE, kpage, true);
		if (success)
			if_->rsp = USER_STACK;
		else
			palloc_free_page (kpage);
	}
	return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
 * virtual address KPAGE to the page table.
 * If WRITABLE is true, the user process may modify the page;
 * otherwise, it is read-only.
 * UPAGE must not already be mapped.
 * KPAGE should probably be a page obtained from the user pool
 * with palloc_get_page().
 * Returns true on success, false if UPAGE is already mapped or
 * if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable) {
	struct thread *t = thread_current ();

	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
	return (pml4_get_page (t->pml4, upage) == NULL
			&& pml4_set_page (t->pml4, upage, kpage, writable));
}
#else
/* From here, codes will be used after project 3.
 * If you want to implement the function for only project 2, implement it on the
 * upper block. */

static bool
lazy_load_segment (struct page *page, void *aux) {
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */

	struct load_aux *tmp_aux = (struct load_aux *)aux;

	if(page->frame == NULL){
		return false;
	}

	uint8_t *kva = page->frame->kva;
	// struct file * reopen_file = file_reopen(tmp_aux->file);
	if (file_read_at(tmp_aux->file, kva, tmp_aux->read_bytes, tmp_aux->offset) != (int) tmp_aux->read_bytes)
	// if (file_read_at(reopen_file, kva, tmp_aux->read_bytes, tmp_aux->offset) != (int) tmp_aux->read_bytes)
	{
		free(tmp_aux);
		return false;
	}
	memset(kva + (tmp_aux->read_bytes), 0, tmp_aux->zero_bytes);
	free(tmp_aux);
	return true;
}

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	struct file* reopen_file = file_reopen(file); /* swap fork 안되서 테스트 */
	process_add_file(reopen_file); /* swap fork 안되서 테스트 */
	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		struct load_aux *tmp_aux = malloc(sizeof(struct load_aux));
		tmp_aux->file = reopen_file;
		tmp_aux->offset = ofs;
		tmp_aux->read_bytes = page_read_bytes;
		tmp_aux->zero_bytes = page_zero_bytes;
		tmp_aux->writable = writable;
		// printf("upage address in load_segment: %p\n", upage);
		if (!vm_alloc_page_with_initializer (VM_ANON, upage,
					writable, lazy_load_segment, tmp_aux))
			{
				free(tmp_aux);
				return false;
			}

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		ofs += PGSIZE;  //! read_byte? 비교 필요
		upage += PGSIZE;
	}
	return true;
}

/* Create a PAGE of stack at the USER_STACK. Return true on success. */
static bool
setup_stack (struct intr_frame *if_) {
	bool success = false;
	void *stack_bottom = (void *) (((uint8_t *) USER_STACK) - PGSIZE);
	/* TODO: Map the stack on stack_bottom and claim the page immediately.
	 * TODO: If success, set the rsp accordingly.
	 * TODO: You should mark the page is stack. */
	/* TODO: Your code goes here */
	if (vm_alloc_page(VM_MARKER_0 | VM_ANON, stack_bottom, true)) {
		if (vm_claim_page(stack_bottom)) {
			// rsp에 page를 넣으려고 했으나, 컴파일러가 에러가 발생.
			// if_->rsp = stack_bottom + PGSIZE;
			if_->rsp = stack_bottom + PGSIZE;
			success = true;
		}
	}
	return success;		
}
#endif /* VM */


// 새힘
void argument_stack(char **argv, int argc, struct intr_frame *if_)
{
    /* rsp : rsp 주소를 담고있는 공간이다!! REAL rsp : *rsp   */
    // rsp : stack pointer -- stack을 구분할 수 있는 지점 (== 스택 시작점)
    /* insert arguments' address */
    char* argu_address[128];
    for(int i = argc-1; i >=0; i--)
    {
        int argv_len = strlen(argv[i]);
        /* strlen은 '\0'을 제외한다. */
        if_->rsp = if_->rsp-(argv_len+1);
        /* store adress seperately */
        argu_address[i] = if_->rsp;
        memcpy(if_->rsp, argv[i], argv_len+1);
        //이러면 4바이트가 삽입된다.
        // *(char **)(if_->rsp) = argv[i][j]
    }
    /* insert padding for word-align */
    while(if_->rsp %8 != 0)
    {
        if_->rsp --;
        *(uint8_t *)(if_->rsp) = 0;
    }
    /* insert address of strings including sentinel */
    for (int i = argc; i >= 0; i--)
    {
        if_->rsp = if_->rsp - 8;
        if (i==argc)
            memset(if_->rsp, 0, sizeof(char**));
        else
            memcpy(if_->rsp, &argu_address[i], sizeof(char**));
            // argu_adress[i]값을 복사하려면, argu_address[i]의 주소를 cpy요소로 넣어라!!
    }
    /* fake return address */
    if_->rsp = if_->rsp-8;
    memset(if_->rsp,0,sizeof(void*));
    /*
    Check this out.
    if_->rsp = if_->rsp-8;
    memcpy(if_->rsp, &argc, sizeof(int));
    if_->rsp -= 8;
    memcpy(if_->rsp,&argu_address[0],sizeof(char*))
    */
    /* We need to check again */
    if_->R.rdi = argc;
    if_->R.rsi = if_->rsp+8; /* 이녀석 주의!!! */
}



/*-------------------------- project.2-Process -----------------------------*/
struct thread *get_child_process(int pid) {
	struct thread *t = thread_current();
    if (list_empty(&t->my_child)) return NULL;

	struct list_elem *e = list_begin(&t->my_child);
    for (e ; e != list_end(&t->my_child) ; )
    {
        struct thread *cur = list_entry(e, struct thread, child_elem);
        if (cur->tid == pid) {
            return cur;
        }
        e = list_next(e);
    }
    return NULL;
}
/*-------------------------- project.2-Process -----------------------------*/

/*-------------------------- project.2-Process -----------------------------*/
void remove_child_process(struct thread *cp) {
    struct list_elem* remove_elem = &cp->child_elem;
    list_remove(remove_elem);
	list_remove(&cp->elem);
    palloc_free_page(cp);
}

/*-------------------------- project.2-Process -----------------------------*/



/*-------------------------- project.2-System Call -----------------------------*/
int process_add_file(struct file *f) {

	struct thread *curr = thread_current();
    // if (curr->next_fd > 511) { // filesys dir-rm-tree 때문에 변경
    if (curr->next_fd > 63) { 
        file_close(f);
        // return -1;
		return -1; // filesys dir-rm-tree 때문에 해봄
    }
	// printf("process add file next fd? %d\n", curr->next_fd);
	curr->fd_table[curr->next_fd] = f;
	return curr->next_fd++;
}
/*-------------------------- project.2-System Call -----------------------------*/


/*-------------------------- project.2-System Call -----------------------------*/
struct file * process_get_file(int fd) {
    struct thread *t = thread_current();
    // if (fd <=1 || fd >= t->next_fd ) 
	// 	return NULL;
	struct file* fd_file = t->fd_table[fd];
	if(fd_file)
		return fd_file;
	else
		return NULL;
}
/*-------------------------- project.2-System Call -----------------------------*/


/*-------------------------- project.2-System Call -----------------------------*/
void process_close_file(int fd) {
    file_close(process_get_file(fd));
	thread_current() -> fd_table[fd] = NULL;
}
/*-------------------------- project.2-System Call -----------------------------*/


/*-------------------------- project.2-System Call -----------------------------*/
/* Exit the process. This function is called by thread_exit (). */
	/* TODO: Your code goes here.
	 * TODO: Implement process termination message (see
	 * TODO: project2/process_termination.html).
	 * TODO: We recommend you to implement process resource cleanup here. */

void process_exit(void) {
    struct thread *t = thread_current();
    t->is_exit = true;
#ifdef VM
	supplemental_page_table_kill (&t->spt);
#endif
// #ifdef VM - tongky's solution
//     if (!hash_empty(&t->spt.vm)){
//         struct hash_iterator i;
//         hash_first(&i, &t->spt.vm);
//         while (hash_next(&i))
//         {
//             struct page *page = hash_entry(hash_cur(&i), struct page, hash_elem);
//             if (page_get_type(page) == VM_FILE && pml4_is_dirty(t->pml4, page->va) && page->writable)
//             {
//                 file_write_at(page->file.file, page->frame->kva, page->file.read_bytes, page->file.offset);
//             }
//         }
//     }
// #endif
    // for (t->next_fd; t->next_fd >= 2 ; t->next_fd --)
    for (int fd = t->next_fd-1; fd>=2; fd --)
    {
        // process_close_file(t->next_fd);
        process_close_file(fd);
				
    }
    // palloc_free_page(t->fd_table);
    file_close(t->running_file);
    sema_up(&t->sema_exit);
	// file_sys - subdir | 이거 하니까 근데 터진다..
	// free(&t->curr_dir);
    process_cleanup();

}