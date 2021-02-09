#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

/*-------------------------- project.2-System call -----------------------------*/
#include "threads/synch.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/process.h"
/*-------------------------- project.2-System call -----------------------------*/




void syscall_entry (void);
void syscall_handler (struct intr_frame *);



/*-------------------------- project.2-System call -----------------------------*/




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



    /*-------------------------- project.2-System Call -----------------------------*/
    lock_init (&filesys_lock);
    /*-------------------------- project.2-System Call -----------------------------*/
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	// printf ("system call!\n");
    // printf("f->Rsp:%p\n", f->rsp);
    // hex_dump(f->rsp, f->rsp, USER_STACK - f->rsp, true);
	uint64_t *arg[6];
	check_address(f->rsp);
	switch (f->R.rax) {
		case SYS_HALT:
            // printf("halt\n");
			halt();
			break;
		case SYS_EXIT:
            // printf("hi\n");
            // printf("exit:%d\n", f->R.rdi);
			// get_argument(f->rsp, arg, 1);
			exit(f->R.rdi);
			break;
		case SYS_CREATE:
            // printf("create\n");
			// get_argument(f->rsp, arg, 2);
			create(f->R.rdi, f->R.rsi);
			break;
		case SYS_OPEN2: {

            printf("handler open : %s\n", f->R.rdi);
			// get_argument(f->R.rdi, arg, 1);
            char * open_arg = (char *)f->R.rdi;
			open2(open_arg);
            // printf("rtn_fd:%d\n", rtn_fd);
        }
			break;
        case SYS_REMOVE:
            // printf("remove\n");
            // get_argument(f->rsp, arg, 1);
            // remove(f->R.rdi);
            break;
        case SYS_FORK:
            // printf("fork\n");
			break;
        case SYS_EXEC:
            // check_address(f->rsp);
            // f->R.rax = exec(f->R.rdi);  // 왜 rax로 받는지?
            break;
        case SYS_WAIT:
            // printf("wait\n");
            break;
        case SYS_FILESIZE:
            // printf("filesize\n");
            filesize(f->R.rdi);
            break;
        case SYS_READ:
            // printf("read\n");
            read (f->R.rdi, f->R.rsi, f->R.rdx);
            break;
        case SYS_WRITE:
            // get_argument(f->R.rsi, arg, 3);
            // printf("syscall-write\n");
            // printf("rdi:%d\n", f->R.rdi);
            write(f->R.rdi, f->R.rsi, f->R.rdx);
            // printf("write\n");
            break;
        case SYS_SEEK:
            // seek (f->R.rdi, f->R.rsi);
            break;
        case SYS_TELL:
            // tell (f->R.rdi);
            break;
        case SYS_CLOSE:
            // close(f->R.rdi);
            break;
		default:
            // printf("default\n");
			thread_exit ();
	}
}


/*-------------------------- project.2-System call -----------------------------*/
void
get_argument(void *rsp, uint64_t *arg, int count) { 
	// 처음에는 rsp가 return address를 가리키고 있기때문에 uint64_t 한 칸을 올려준다.
    
	for (int i = 0 ; i < count ; i++)
	{
		// arg[i]에 인자값을 순서대로 넣어준다.
		arg[i] = *(uint64_t*) rsp;
		rsp += sizeof(uint64_t);
	}
}
/*-------------------------- project.2-System call -----------------------------*/

void
check_address(void *addr) {
	/*-------------------------- project.2-System call -----------------------------*/
	if ((uint64_t) addr == 0x0 || !(is_user_vaddr(addr)) || (uint64_t) addr <= 0x400000) {
		// page_fault();
        // printf("check_addr\n");
		exit(-1);
	}
	/*-------------------------- project.2-System call -----------------------------*/
}

/*-------------------------- project.2-System call -----------------------------*/
void
halt (void) {
	// printf("power_off\n");
	power_off();
}
/*-------------------------- project.2-System call -----------------------------*/

/*-------------------------- project.2-System call -----------------------------*/
void
exit (int status) {
    // printf("hi\n");
	struct thread *t = thread_current();
    // printf("status:%d\n", status);
	printf("%s: exit(%d)\n", t->name, status);
	thread_exit();
}
/*-------------------------- project.2-System call -----------------------------*/

/*-------------------------- project.2-System call -----------------------------*/
bool
create(const char *file , unsigned initial_size) {
	// printf("create\n");
    lock_acquire(&filesys_lock);
    bool result = (filesys_create (file, initial_size));
    lock_release(&filesys_lock);
    return result;
}
/*-------------------------- project.2-System call -----------------------------*/

/*-------------------------- project.2-System call -----------------------------*/
bool remove(const char *file) {
	// printf("remove\n");
	return (filesys_remove (file));
}
/*-------------------------- project.2-System call -----------------------------*/



/*-------------------------- project.2-System call -----------------------------*/
int write(int fd, void *buffer, unsigned size) {
    // printf("writefd=%d\n", fd);
    lock_acquire(&filesys_lock);
    struct file *f = process_get_file(fd);
    int cur_size;
    if (fd == 1) {
        cur_size = size;
        putbuf((const char*) buffer, size);
    }
    else {
        cur_size = file_write(f, buffer, size);
    }
    lock_release(&filesys_lock);
    return cur_size;

}
/*-------------------------- project.2-System call -----------------------------*/


/*-------------------------- project.2-System call -----------------------------*/
int open2 (const char *file_name) {
    printf("open2 file name : %s\n", *file_name);
	lock_acquire(&filesys_lock);
	struct file *file = filesys_open(file_name);
    if (file) {
        int fd = process_add_file(file);
        lock_release(&filesys_lock);
        // printf("openfd=%d\n", fd);
        return fd;
    }
    else{
        lock_release(&filesys_lock);
        return -1;
    }
}
/*-------------------------- project.2-System call -----------------------------*/


/*-------------------------- project.2-System call -----------------------------*/
int filesize(int fd) {
    lock_acquire(&filesys_lock);
    struct file *f = process_get_file(fd);
    if (f) {
        int file_size = file_length(f);
        lock_release(&filesys_lock);
        return file_size;
    }
    else {
        lock_release(&filesys_lock);
        return -1;
    }
}
/*-------------------------- project.2-System call -----------------------------*/



/*-------------------------- project.2-System call -----------------------------*/
int read (int fd, void*buffer, unsigned size) {
    int cur_size;
    char * rd_buf = (char *) buffer;
    // printf("readfd=%d\n", fd);
    lock_acquire(&filesys_lock);
    if (fd == 0){
        cur_size = 0;
        rd_buf[cur_size] = input_getc();
        while (cur_size < size && rd_buf[cur_size] != '\n') {
            cur_size ++;
            rd_buf[cur_size] = input_getc();
        }
        cur_size ++;
        rd_buf[cur_size] = '\0';
    }
    else {
        struct file *f = process_get_file(fd);
        cur_size = file_read(f, buffer, size);
    }
    lock_release(&filesys_lock);
    return cur_size;
}
/*-------------------------- project.2-System call -----------------------------*/