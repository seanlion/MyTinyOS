// #include "userprog/syscall.h"
// #include <stdio.h>
// #include <syscall-nr.h>
// #include "threads/interrupt.h"
// #include "threads/thread.h"
// #include "threads/loader.h"
// #include "userprog/gdt.h"
// #include "threads/flags.h"
// #include "intrinsic.h"
// #include "userprog/process.h"
// #include "devices/input.h"
// #include "filesys/filesys.h"
// #include "filesys/file.h"


// #include "threads/synch.h"
// #include <stdbool.h>
// #include <debug.h>
// #include <stddef.h>

#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <list.h>
#include <syscall-nr.h>
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/loader.h"
#include "threads/palloc.h"
#include "threads/mmu.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "vm/vm.h"
#include "vm/file.h"
#include "filesys/directory.h"
#include "filesys/fat.h"

#include "intrinsic.h"

/*-------------------------- project.2-System call -----------------------------*/
void syscall_entry (void);
void syscall_handler (struct intr_frame *);
/*-------------------------- project.2-System call -----------------------------*/


/*-------------------------- project.2-System Call -----------------------------*/
static struct lock filesys_lock;
typedef int pid_t;
/*-------------------------- project.2-System Call -----------------------------*/

/*-------------------------- project.2-System call -----------------------------*/
void check_address(void *);
void get_argument(void *, uint64_t *, int);
void halt (void);
void exit (int );
int open (const char *);
bool create(const char * , unsigned);
bool remove(const char *);
void seek (int, unsigned);
unsigned tell (int);
void close (int);
int filesize(int);
int read (int , void*, unsigned);
int write(int, const void *, unsigned );
int wait(tid_t);
pid_t fork (const char *);
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
    uint64_t number = f->R.rax;
	// uint64_t *arg[6];
	check_address(f->rsp);
	switch (number) {
		case SYS_HALT:
            // printf("halt\n");
			halt();
			break;
		case SYS_EXIT:
            // printf("--------------exit:%d\n", f->R.rdi);
			// get_argument(f->rsp, arg, 1);
			exit(f->R.rdi);
			break;
		case SYS_CREATE: {

            // printf("create\n");
			// get_argument(f->rsp, arg, 2);
			f->R.rax = create(f->R.rdi, f->R.rsi);
			break;
        }
		case SYS_OPEN: 
			f->R.rax = open(f->R.rdi);

			break;
        case SYS_REMOVE: 
            // printf("remove\n");
            // get_argument(f->rsp, arg, 1);
            f->R.rax = remove(f->R.rdi);
            break;
        case SYS_FORK:
            // memcpy(, )
            f->R.rax = fork(f->R.rdi);
			break;
        case SYS_EXEC:
            // check_address(f->rsp);
            f->R.rax = exec(f->R.rdi);
            break;
        case SYS_WAIT:
            f->R.rax = wait(f->R.rdi);
            break;
        case SYS_FILESIZE: {
            // printf("filesize\n");
            f->R.rax = filesize(f->R.rdi);
            break;

        }
        case SYS_READ: {
            f->R.rax = read (f->R.rdi, f->R.rsi, f->R.rdx);
            break;
        }
        case SYS_WRITE:{
            f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
            break;
        }
        case SYS_SEEK:
            {
            seek (f->R.rdi, f->R.rsi);
            break;
            }
        case SYS_TELL:
            f->R.rax = tell (f->R.rdi);
            break;
        case SYS_CLOSE:
            close(f->R.rdi);
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
    t->exit_status = status;
	printf("%s: exit(%d)\n", t->name, status);
	thread_exit();
}
/*-------------------------- project.2-System call -----------------------------*/

/*-------------------------- project.2-System call -----------------------------*/
bool
create(const char *file , unsigned initial_size) {
	// printf("create\n");
    if (file == NULL) exit(-1);
    // lock_acquire(&filesys_lock);
    bool result = (filesys_create (file, initial_size));
    // lock_release(&filesys_lock);
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
int write(int fd, const void *buffer, unsigned size) {
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
int open (const char *file_name) {
    if (file_name == NULL) {
        return -1;
    }

	struct file *file = filesys_open(file_name);
    if (file) {
        int fd = process_add_file(file);
        return fd;
    }
    else{
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
    int cur_size = 0;
    char * rd_buf = (char *) buffer;
    // printf("readfd=%d\n", fd);
    lock_acquire(&filesys_lock);
    if (fd == STDIN_FILENO){
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
        if (f != NULL) {
            cur_size = file_read(f, buffer, size);
        }
        else {
            cur_size = -1;
        }
    }
    lock_release(&filesys_lock);
    return cur_size;
}

/*-------------------------- project.2-System call -----------------------------*/

/*-------------------------- project.2-System call -----------------------------*/
void seek (int fd, unsigned position) {
    struct file *f;
    lock_acquire(&filesys_lock);
    // struct file *f = process_get_file(fd);
    if((f = process_get_file(fd))!=NULL)
        file_seek(f, position);
    lock_release(&filesys_lock);
}
/*-------------------------- project.2-System call -----------------------------*/


/*-------------------------- project.2-System call -----------------------------*/
unsigned tell (int fd) {
    struct file *f;
    lock_acquire(&filesys_lock);
    // struct file *f = process_get_file(fd);
    unsigned offset = 0;
    if((f = process_get_file(fd))!=NULL)
		offset = file_tell(f);
    lock_release(&filesys_lock);
    return offset;
}
/*-------------------------- project.2-System call -----------------------------*/



/*-------------------------- project.2-System call -----------------------------*/
void close (int fd) {
    process_close_file(fd);
}
/*-------------------------- project.2-System call -----------------------------*/



/*-------------------------- project.2-Process -----------------------------*/
pid_t exec(const char *cmd_line) {
    int error = process_exec(cmd_line);
    if (error == -1) return -1;
    return error;
}
/*-------------------------- project.2-Process -----------------------------*/


/*-------------------------- project.2-Process -----------------------------*/
int wait(tid_t tid) {
    return process_wait(tid);
}

/*-------------------------- project.2-Process -----------------------------*/


/*-------------------------- project.2-Process -----------------------------*/
pid_t fork (const char *thread_name) {
    struct intr_frame *cur_if = &thread_current()->tf;
    return process_fork (thread_name, cur_if);
}

/*-------------------------- project.2-Process -----------------------------*/
