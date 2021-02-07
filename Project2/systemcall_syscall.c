#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/vaddr.h"



void syscall_entry (void);
static void syscall_handler (struct intr_frame *);

/*-------------------------- project.2-System call -----------------------------*/
void check_address(void *);
void get_argument(void *, uint64_t *, int);
void halt (void);
void exit (int );
bool create(const char * , unsigned);
bool remove(const char *);
int open (const char *);
/*-------------------------- project.2-System call -----------------------------*/




/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
static void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	printf ("Entering system call!\n");
	uint64_t *arg[6];
	check_address(f->R.rsi);
	switch (f->R.rax) {
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			get_argument(f->rsp, arg, 1);
			exit(*arg[0]);
			break;
		case SYS_CREATE:
			get_argument(f->rsp, arg, 2);
			create(*(const char*) arg[0], *arg[1]);
			break;
		case SYS_REMOVE:
			get_argument(f->rsp, arg, 1);
			remove(*arg[0]);
			break;
		default:
			thread_exit ();
		
	}


	
}



void
check_address(void *addr) {
	/*-------------------------- project.2-System call -----------------------------*/
	if (addr == NULL || !(is_user_vaddr(addr) || (addr < 0x400000)) {
		// page_fault();
		exit(-1);
	}
	/*-------------------------- project.2-System call -----------------------------*/
}	

/*-------------------------- project.2-System call -----------------------------*/
void
get_argument(void *rsp, uint64_t *arg, int count) { 
	// 처음에는 rsp가 return address를 가리키고 있기때문에 uint64_t 한 칸을 올려준다.
	rsp += sizeof(uint64_t);
	for (int i = 0 ; i < count ; i++)
	{
		// arg[i]에 인자값을 순서대로 넣어준다. 
		arg[i] = *(uint64_t*) rsp;
		rsp += sizeof(uint64_t);
	}
}
/*-------------------------- project.2-System call -----------------------------*/





/*-------------------------- project.2-System call -----------------------------*/
void
halt (void) {
	printf("power_off\n");
	power_off();
	
}
/*-------------------------- project.2-System call -----------------------------*/



/*-------------------------- project.2-System call -----------------------------*/
void
exit (int status) {
	struct thread *t = thread_current();
	printf("%s : exit(%d)\n", t->name, status);
	thread_exit();
}
/*-------------------------- project.2-System call -----------------------------*/


/*-------------------------- project.2-System call -----------------------------*/
bool
create(const char *file , unsigned initial_size) {
	printf("create\n");
	return (filesys_create (file, initial_size));
}
/*-------------------------- project.2-System call -----------------------------*/


/*-------------------------- project.2-System call -----------------------------*/
bool remove(const char *file) {
	printf("remove\n");
	return (filesys_remove (file));
}
/*-------------------------- project.2-System call -----------------------------*/

/*-------------------------- project.2-System call -----------------------------*/
int open (const char *file) {
	printf("remove\n");
	filesys_open(file);
}
/*-------------------------- project.2-System call -----------------------------*/


/*-------------------------- project.2-System call -----------------------------*/

/*-------------------------- project.2-System call -----------------------------*/
