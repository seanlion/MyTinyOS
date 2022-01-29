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
#include "lib/kernel/console.h"
void syscall_entry (void);
void syscall_handler (struct intr_frame *);

typedef int pid_t;

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
void *mmap (void *addr, size_t length, int writable, int fd, off_t offset);
void munmap (void *addr);
void check_user(void* addr,struct intr_frame *f );
void check_user_write(void* addr,struct intr_frame *f );

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
	thread_current()->rsp = f->rsp; /* 커널 스택의 rsp 쓰기전에 user stack의 rsp 저장. */
    uint64_t number = f->R.rax;
	switch (number) {
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			exit(f->R.rdi);
			break;
		case SYS_CREATE: {
            check_address(f->R.rdi);
			f->R.rax = create(f->R.rdi, f->R.rsi);
			break;
        }
		case SYS_OPEN:
            check_address(f->R.rdi);
			f->R.rax = open(f->R.rdi);
			break;
        case SYS_REMOVE: 
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
            f->R.rax = filesize(f->R.rdi);
            break;

        }
        case SYS_READ: {
            check_address(f->R.rdi);
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
			thread_exit ();
	}

	
}

void
check_address(void *addr) {
	if (is_kernel_vaddr(addr) 
        || 
        (uint64_t)addr ==0x0 
        || 
        addr ==NULL
        )
    {
        exit(-1);
    }
}

void
halt (void) {
	power_off();
}

void
exit (int status) {
	struct thread *t = thread_current();
    if(lock_held_by_current_thread(&filesys_lock))
		lock_release(&filesys_lock);
    t->exit_status = status;
	printf("%s: exit(%d)\n", t->name, status);
	thread_exit();
}

bool create(const char *file , unsigned initial_size) {
    if (file == NULL) 
        exit(-1);
    lock_acquire(&filesys_lock);
    bool result = filesys_create (file, initial_size);
    lock_release(&filesys_lock);
    return result;
}

bool remove(const char *file) {
    if (file)
        return filesys_remove(file);
    else
        exit(-1);
}

int write(int fd, const void *buffer, unsigned size) {
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

int open (const char *file) {
   if (file)
    {   
        lock_acquire(&filesys_lock);
        struct file * open_file = filesys_open(file);

        if (open_file)
        {
            if(!strcmp(file,thread_current()->name))
		        {
                    file_deny_write(open_file); 
                }
            int result = process_add_file(open_file);
            lock_release(&filesys_lock);
            return result;
        }
        else
        {
            lock_release(&filesys_lock);
            return -1;
        }
    }
    else
    {
        return -1;
    }

}

int filesize(int fd) {
    lock_acquire(&filesys_lock);
    struct file *want_length_file = process_get_file(fd);
    int ret =-1;
    if (want_length_file)
    {
        ret = file_length(want_length_file);
        lock_release(&filesys_lock);
        return ret; /* ASSERT (NULL), so we need to branch out */
    }
    else
    {
        lock_release(&filesys_lock);
        return ret;
    }
}

int exec(const char *file){
    if (file == NULL || *file == "") {
        exit(-1);
    }
    int result = process_exec(file);
    return result;
}


int read (int fd, void*buffer, unsigned size) {
    char* rd_buf = (char *)buffer;
    struct file *f = process_get_file(fd);
    int cur_size = 0;
    struct page *page = spt_find_page(&thread_current()->spt, rd_buf);
    if(page->writable == 0){ /* pt-write-code2 예외처리 */
        exit(-1);
    }
    if (fd == 0) {
            rd_buf[cur_size] = input_getc();
            while(cur_size<size && rd_buf[cur_size]!='\n'){
                cur_size +=1;
                rd_buf[cur_size] = input_getc();
		    }
		    rd_buf[cur_size] = '\0';
    }
    else {
        lock_acquire(&filesys_lock);
        if (f){
            cur_size = file_read(f, buffer, size);
        }
        else {
            cur_size =  -1;
        }
    }
    lock_release(&filesys_lock);
    return cur_size;
}

void seek(int fd, unsigned position){
    lock_acquire(&filesys_lock);
    struct file *target = process_get_file(fd);
    file_seek(target, position);
    lock_release(&filesys_lock);
} 



unsigned tell(int fd){
    lock_acquire(&filesys_lock);
    struct file *target = process_get_file(fd);
    unsigned result = file_tell(target);
    lock_release(&filesys_lock);
    return result;
}




void close(int fd){
    lock_acquire(&filesys_lock);
    process_close_file(fd);
    lock_release(&filesys_lock);
}


int wait(pid_t pid) {
    int result = process_wait(pid);
    return result;
}


pid_t fork (const char *thread_name) {
    struct intr_frame *cur_if = &thread_current()->fork_tf;
    pid_t result = process_fork (thread_name, cur_if);
    return result;
}

void *mmap (void *addr, size_t length, int writable, int fd, off_t offset){
    struct file* file_mmap = process_get_file(fd);
    if (!is_user_vaddr(addr))
			return NULL;

    if (file_mmap == NULL)
        return NULL;
    if (file_length(file_mmap) == 0){
        return NULL;
    }
    if (addr == NULL || addr == 0 || pg_round_down(addr) != addr)
        return NULL;

    if (pg_ofs(offset) !=0){
        return NULL;
    }
    /* 기존에 매핑 된 페이지를 덮어쓰려 하면 */
    if ((long long)length <= 0LL){
        return NULL;
    }
    if (!lock_held_by_current_thread(&filesys_lock)) 
    /* if에 안걸리면 lock release만 되길래 주석 처리 */
        lock_acquire(&filesys_lock);
    void* addr_mmap = do_mmap (addr,length, writable, file_mmap, offset);
    lock_release(&filesys_lock);
    return addr_mmap;
}

void munmap (void *addr){
    if (addr == NULL || addr == 0)
        return NULL;
    lock_acquire(&filesys_lock);
    do_munmap(addr);
    lock_release(&filesys_lock);
}

/* pt-bad-read 잡기 위해 테스트 - 정확히 이 함수들로 pass하진 않음. */
void check_user(void* addr,struct intr_frame *f ){
	struct page *p = spt_find_page(&thread_current()->spt, addr);
	if( addr < pg_round_down(f->rsp) && p==NULL ){
		exit(-1);
	}
} 