// #include "userprog/syscall.h"
// #include <stdio.h>
// #include <syscall-nr.h>
// #include "threads/interrupt.h"
// #include "threads/thread.h"
// #include "threads/loader.h"
// #include "userprog/gdt.h"
// #include "threads/flags.h"
// #include "intrinsic.h"

/*-------------------------- project.2-System call -----------------------------*/
#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "userprog/process.h"
#include "devices/input.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
// #include "filesys/filesys.h"
// #include "filesys/file.h"
#include "lib/kernel/console.h"
// #include "userprog/process.h"

#include "threads/synch.h"
/*-------------------------- project.2-System call -----------------------------*/
void syscall_entry (void);
void syscall_handler (struct intr_frame *);
/*-------------------------- project.2-System call -----------------------------*/

void check_address(void *addr);

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
    
    lock_init(&filesys_lock);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	// printf ("system call!\n");
    // printf("f->Rsp:%p\n", f->rsp);
    // hex_dump(f->rsp, f->rsp, USER_STACK - f->rsp, true);
	check_address(f->rsp);
    uint64_t number = f->R.rax;
	// uint64_t *arg[6];
	switch (number) {
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
			f->R.rax = create(f->R.rdi, f->R.rsi);
			break;
		case SYS_OPEN2: 
            // printf("handler open : %s\n", f->R.rdi);
			// get_argument(f->R.rdi, arg, 1);
            // char * open_arg = (char *)f->R.rdi;
			f->R.rax = open(f->R.rdi);
            // printf("rtn_fd:%d\n", rtn_fd);
			break;
        case SYS_REMOVE: 
            // printf("remove\n");
            // get_argument(f->rsp, arg, 1);
            f->R.rax=remove(f->R.rdi);
            break;
        case SYS_FORK:
            // printf("fork\n");
			break;
        case SYS_EXEC:
            // check_address(f->rsp);
            f->R.rax = exec(f->R.rdi);  // 왜 rax로 받는지?
            break;
        case SYS_WAIT:
            // printf("wait\n");
            break;
        case SYS_FILESIZE:
            // printf("filesize\n");
            f->R.rax = filesize(f->R.rdi);
            break;
        case SYS_READ: 

            // printf("read\n");
            f->R.rax = read (f->R.rdi, f->R.rsi, f->R.rdx);
            break;
        case SYS_WRITE:

            // get_argument(f->R.rsi, arg, 3);
            // printf("syscall-write\n");
            // printf("rdi:%d\n", f->R.rdi);
            f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
            // printf("write\n");
            break;
        case SYS_SEEK:
            seek (f->R.rdi, f->R.rsi);
            break;
        case SYS_TELL:
            tell (f->R.rdi);
            break;
        case SYS_CLOSE:
            close(f->R.rdi);
            break;

	}
}


/*-------------------------- project.2-System call -----------------------------*/
// void
// get_argument(void *rsp, uint64_t *arg, int count) { 
// 	// 처음에는 rsp가 return address를 가리키고 있기때문에 uint64_t 한 칸을 올려준다.
    
// 	for (int i = 0 ; i < count ; i++)
// 	{
// 		// arg[i]에 인자값을 순서대로 넣어준다.
// 		arg[i] = *(uint64_t*) rsp;
// 		rsp += sizeof(uint64_t);
// 	}
// }
/*-------------------------- project.2-System call -----------------------------*/

void
check_address(void *addr) {
	/*-------------------------- project.2-System call -----------------------------*/
	if (is_kernel_vaddr(addr))
    {
        exit(-1);
    }
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
    struct thread *curr = thread_current();
    curr->exit_status = status;
    printf("%s: exit(%d)\n", thread_current()->name, thread_current()->exit_status);
    thread_exit();
}
/*-------------------------- project.2-System call -----------------------------*/

/*-------------------------- project.2-System call -----------------------------*/
bool create(const char *file , unsigned initial_size) {
	// printf("create\n");
 if (file)
        return filesys_create(file,initial_size); // ASSERT, dir_add (name!=NULL)
    else
        exit(-1);
}
/*-------------------------- project.2-System call -----------------------------*/

/*-------------------------- project.2-System call -----------------------------*/
bool remove(const char *file) {
	// printf("remove\n");
    if (file)
        return filesys_remove(file);
    else
        exit(-1);
}
/*-------------------------- project.2-System call -----------------------------*/



/*-------------------------- project.2-System call -----------------------------*/
int write(int fd, const void *buffer, unsigned size) {
    // printf("writefd=%d\n", fd);
    lock_acquire(&filesys_lock);
    struct file *f = process_get_file(fd);
    int cur_size = -1;
    if(f) {
        if (fd == 1) {
            putbuf(buffer, size);
            cur_size = sizeof(buffer);
        }
        else {
            cur_size = file_write(f, buffer, size);
        }

    }
    lock_release(&filesys_lock);
    return cur_size;
}
/*-------------------------- project.2-System call -----------------------------*/


/*-------------------------- project.2-System call -----------------------------*/

int open (const char *file) {
   if (file)
    {
        struct file * open_file = filesys_open(file);
        if (open_file)
        {
            return process_add_file(open_file);
        }
        else
            return -1;
    }
    else
        return -1;

}
/*-------------------------- project.2-System call -----------------------------*/


/*-------------------------- project.2-System call -----------------------------*/
int filesize(int fd) {
    struct file *want_length_file = process_get_file(fd);
    int ret =-1;
    if (want_length_file)
    {
        ret = file_length(want_length_file); 
        return ret; /* ASSERT (NULL), so we need to branch out */
    }
    else
    {
        return ret;
    }
}
/*-------------------------- project.2-System call -----------------------------*/

int exec(const char *file){
    if(process_exec(file))
        return -1;
}


/*-------------------------- project.2-System call -----------------------------*/
int read (int fd, void*buffer, unsigned size) {
    // printf("readfd=%d\n", fd);
    lock_acquire(&filesys_lock);
    struct file *f = process_get_file(fd);
    int cur_size = -1;
    if (f){
        if (fd == 0){
        // rd_buf[cur_size] = input_getc();
        // while (cur_size < size && rd_buf[cur_size] != '\n') {
        //     cur_size ++;
            cur_size = input_getc();
        // }
        // cur_size ++;
        // rd_buf[cur_size] = '\0';
        }
        else {
            cur_size = file_read(f, buffer, size);
        }
    }
    lock_release(&filesys_lock);
    return cur_size;
}
/*-------------------------- project.2-System call -----------------------------*/

void seek(int fd, unsigned position){
    struct file *target = process_get_file(fd);
    file_seek(target, position);
}
unsigned tell(int fd){
    struct file *target = process_get_file(fd);
    return file_tell(target);
}
void close(int fd){
    process_close_file(fd);
}
