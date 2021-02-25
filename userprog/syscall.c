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
#include "userprog/process.h"
#include "devices/input.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "lib/kernel/console.h"
/*-------------------------- project.2-System call -----------------------------*/
void syscall_entry (void);
void syscall_handler (struct intr_frame *);
/*-------------------------- project.2-System call -----------------------------*/

/*-------------------------- project.2-System Call -----------------------------*/
typedef int pid_t;
/*-------------------------- project.2-System Call -----------------------------*/
struct lock filesys_lock;
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
void *mmap (void *addr, size_t length, int writable, int fd, off_t offset);
void munmap (void *addr);
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
	thread_current()->rsp = f->rsp; // 커널 스택의 rsp 쓰기전에 user stack의 rsp 저장.
    uint64_t number = f->R.rax;
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
            check_address(f->R.rdi);
            // printf("create\n");
			// get_argument(f->rsp, arg, 2);
			f->R.rax = create(f->R.rdi, f->R.rsi);
			break;
        }
		case SYS_OPEN:
            check_address(f->R.rdi);
			f->R.rax = open(f->R.rdi);
			break;
        case SYS_REMOVE: 
            // printf("remove\n");
            // get_argument(f->rsp, arg, 1);
            f->R.rax = remove(f->R.rdi);
            break;
        case SYS_FORK:
            memcpy(&thread_current()->fork_tf, f,sizeof(struct intr_frame));
            f->R.rax = fork(f->R.rdi);
			break;
        case SYS_EXEC:
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
            check_address(f->R.rdi);
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
        case SYS_MMAP:
            f->R.rax = mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
            break;

        case SYS_MUNMAP:
            munmap(f->R.rdi);
            break;
		default:
            // printf("default\n");
			thread_exit ();
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
    // printf("hi\n");
	struct thread *t = thread_current();
    t->exit_status = status;
	printf("%s: exit(%d)\n", t->name, status);
	thread_exit();
}
/*-------------------------- project.2-System call -----------------------------*/

/*-------------------------- project.2-System call -----------------------------*/
bool create(const char *file , unsigned initial_size) {
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
    else {
        lock_release(&filesys_lock);
        return -1;
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
            int result = process_add_file(open_file);
            return result;
        }
        else
        {
            return -1;
        }
    }
    else
    {
        return -1;
    }

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
    if (file == NULL || *file == "") {
        exit(-1);
    }
    int result = process_exec(file);
    return result;
}


/*-------------------------- project.2-System call -----------------------------*/
int read (int fd, void*buffer, unsigned size) {
    // printf("readfd=%d\n", fd);
    lock_acquire(&filesys_lock);
    struct file *f = process_get_file(fd);
    int cur_size = -1;
    if (f){
        if (fd == 0) {
            cur_size = input_getc();
        }
        else {
            cur_size = file_read(f, buffer, size);
        }
    }
    else {
        lock_release(&filesys_lock);
        return -1;
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
    unsigned result = file_tell(target);
    return result;
}




void close(int fd){
    process_close_file(fd);
}

/*-------------------------- project.2-Process -----------------------------*/


/*-------------------------- project.2-Process -----------------------------*/
int wait(pid_t pid) {
    int result = process_wait(pid);
    return result;
}

/*-------------------------- project.2-Process -----------------------------*/


/*-------------------------- project.2-Process -----------------------------*/
pid_t fork (const char *thread_name) {
    struct intr_frame *cur_if = &thread_current()->fork_tf;
    pid_t result = process_fork (thread_name, cur_if);
    return result;
}

/*-------------------------- project.2-Process -----------------------------*/


/*-------------------------- project.3-map,unmap -----------------------------*/

void *mmap (void *addr, size_t length, int writable, int fd, off_t offset){
    struct file* file_mmap = process_get_file(fd);
    if (!is_user_vaddr(addr))
			return NULL;

    if (file_mmap == NULL)
        return NULL;
    // 파일이 0바이트이면
    if (file_length(file_mmap) == 0){
        return NULL;
    }
    // addr이 0이면, page aligned 안 되어 있으면
    if (addr == NULL || addr == 0 || pg_round_down(addr) != addr)
        return NULL;

    if (pg_ofs(offset) !=0){
        return NULL;
    }
    // 기존에 매핑 된 페이지를 덮어쓰려 하면
    // length가 0이면
    if ((long long)length <= 0LL){
        return NULL;
    }
    // printf("여기 들어와야 함 mmap 전\n");
    lock_acquire(&filesys_lock);
    void* addr_mmap = do_mmap (addr,length, writable, file_mmap, offset);
    lock_release(&filesys_lock);
	// printf("여기 들어와야 함? mmap 후\n");
    // printf("------");
    return addr_mmap;
}

void munmap (void *addr){
    if (addr == NULL || addr == 0)
        return NULL;
    // lock_acquire(&filesys_lock);
    do_munmap(addr);
    // lock_release(&filesys_lock);
}

/*-------------------------- project.3-map,unmap -----------------------------*/
