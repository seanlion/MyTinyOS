#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/init.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "lib/kernel/console.h"
#include "userprog/process.h"
#include "filesys/directory.h"
#include <string.h>

/* ADD header for page fault */
// #include "userprog/exception.h"
// #include "userprog/process.h"
// #include "threads/init.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* 헤더에 넣으면 오류가 나요 */
void check_address(void *addr);
// void get_frame_argument(void *rsp, int *arg);


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

    /* rsp 유효성 검증 */
    /* 우리는 상한선에서 출발해서 밑으로 쌓았다. */
    check_address(f->rsp);
    // intr_dump(f);
    

    /*rsp 부터 인자들을 arg에 저장하기 */
    // get_frame_argument(f,arg);
    // get_argument()

    uint64_t number = f->R.rax;
    switch(number){

    case SYS_HALT :

        halt();
        break;

    case SYS_EXIT:

        exit(f->R.rdi);
        break;
    case SYS_FORK :
        // process_create_initd(thread_name);
        // char* thread_name = f->R.rdi;

        memcpy(&thread_current()->fork_tf, f,sizeof(struct intr_frame));
        f->R.rax = fork(f->R.rdi);
        break;
    case SYS_EXEC :
        f->R.rax = exec(f->R.rdi);
        break;
    case SYS_WAIT:  
        f->R.rax = wait(f->R.rdi);
        break;
    case SYS_CREATE:
        f->R.rax = create(f->R.rdi, f->R.rsi) ;
        break;
    case SYS_REMOVE:
        f->R.rax = remove(f->R.rdi) ;
        break;
    case SYS_OPEN :
        // intr_dump(f);

        f->R.rax = open(f->R.rdi);
        // printf("OPEN FINISH f->R.rax : %d\n",f->R.rax);
        // intr_dump(f);

        break;
    case SYS_FILESIZE:
        f->R.rax = filesize(f->R.rdi);
        break;
    case SYS_READ:
        f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
        break;
    case SYS_WRITE:
        f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
        break;
    case SYS_SEEK :
        seek(f->R.rdi,f->R.rsi);
        break;
    case SYS_TELL:
        f->R.rax = tell(f->R.rdi);
        break;
    case SYS_CLOSE:
        close(f->R.rdi);
        break;

    /* Project 3 and optionally project 4. */
	case SYS_MMAP:                   /* Map a file into memory. */
		f->R.rax = mmap((void*)f->R.rdi,(size_t)f->R.rsi,(int)f->R.rdx, (int)f->R.r10, (off_t)f->R.r8);
		break;
	case SYS_MUNMAP:                 /* Remove a memory mapping. */
		munmap((void*)f->R.rdi);
		break;
        // thread_exit ();

    /* Project 4 */    
    case SYS_CHDIR:
        f->R.rax = chdir((const char*)f->R.rdi);
        break;
    case SYS_MKDIR:
        f->R.rax = mkdir((const char*)f->R.rdi);
        break;
    case SYS_READDIR:
        f->R.rax = readdir((int)f->R.rdi,(char*)f->R.rsi);
        break;
    case SYS_ISDIR:
        f->R.rax = isdir((int)f->R.rdi);
        break;
    case SYS_INUMBER:
        f->R.rax = inumber((int)f->R.rdi);
        break;
    case SYS_SYMLINK:
        f->R.rax = symlink((const char *)f->R.rdi, (const char*)f->R.rsi);
        break;
    }
    

}

/*** It could be dangerous ***/
void check_address(void *addr){
    if (is_kernel_vaddr(addr))
    {
        // printf("CHECK ADDR\n");
        exit(-1);
    }
    thread_current()->rsp = addr;
}

void check_valid_buffer(void *buffer, unsigned size){
	
	struct page* check ;

	// if( size > PGSIZE){
    //     printf("PGSIZE\n");
	// 	exit(-1);
    // }

	if(!(check = spt_find_page(&thread_current()->spt, buffer)) || !check->writable )
    {
        // printf("NON WRITABLE\n");
		exit(-1);
    }

}
void halt(void){
    power_off();
}
void exit(int status) {
    struct thread *curr = thread_current();
    curr->exit_status = status;
    curr->is_exit = 1; // be dead by exit syscall!
    printf("%s: exit(%d)\n", thread_current()->name, thread_current()->exit_status);
    thread_exit();
}

tid_t fork(const char *thread_name){
    // printf("I'm %d, syscall-fork\n", thread_current()->tid);
    // printf("I'm %d, syscall-fork - fd_table[2] : %p\n", thread_current()->tid, thread_current()->fd_table[2]);
    // printf("I'm %d, syscall-fork - fd_table[3] : %p\n", thread_current()->tid, thread_current()->fd_table[3]);

    return process_fork(thread_name,&thread_current()->fork_tf);
}

int exec(const char *file){
    if(process_exec(file))
        return -1;
}

int wait(pid_t pid){
    return process_wait(pid);
}
bool create(const char *file, unsigned initial_size){

    bool check = false;
    if (file && strlen(file)){
        if(strlen(file) < 128)
            check =  filesys_create(file,initial_size);
        else
            check = false;
        // if(check == false){
        //     // printf("CREATE NULL\n");
        //     exit(-1);
        }
    else{
        exit(-1);
    }

    return check; // ASSERT, dir_add (name!=NULL)

}
bool remove(const char *file){
    if (file)
        return filesys_remove(file);
    else
    {
        // printf("CAN'T REMOVE\n");
        exit(-1);
    }
}
int open(const char *file){
    // printf("I'm %d, syscall_open %s\n", thread_current()->tid,file);
    int ret;

    lock_acquire(&filesys_lock);
    /*thanks filesys_open : NULL, if there isn't */
    if (file)
    {
        struct file * open_file = filesys_open(file);
        lock_release(&filesys_lock);
        if (open_file)
        {
            // printf("BEFORE PROCESS_ADD_FILE\n");
            ret = process_add_file(open_file);
           
            // lock_release(&filesys_lock);
            // printf("FINISH PROCESS_ADD_FILE, ret : %d\n",ret);

            return ret;
        }
        else{
            // printf("NO OPEN_FILE!\n");
            // lock_release(&filesys_lock);

            return -1;
        }
    }
    else{
        // printf("NO FILE NAME\n");
        lock_release(&filesys_lock);

        return -1;
    }

}
int filesize(int fd){
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
int read(int fd, void *buffer, unsigned length){

	check_valid_buffer(buffer,length);

    // if(length == 0)
    //     return NULL;

    lock_acquire(&filesys_lock);
    // printf("[syscall] BEFORE PROCEE GET FILE\n");
    struct file *target = process_get_file(fd);
    int ret = -1;
    if(target) /* fd == 0 이 었으면, 0을 return 했을 것이다.*/
    {   
        if (fd == 0)
        {   
            ret = input_getc();
        }
        else
        {
            // printf("[syscall] BEFORE FILE READ\n");
            ret = file_read(target,buffer,length);
            // printf("[syscall] AFTER FILE READ\n");

            // printf("read : %d\n", ret);
        }
    }
    lock_release(&filesys_lock);
    return ret;
}
int write(int fd, const void *buffer, unsigned length){
    lock_acquire(&filesys_lock);
    struct file *target = process_get_file(fd);
    int ret = -1;
    if(target)
    {   
        if (fd == 1)
        {
            putbuf(buffer, length);
            ret = sizeof(buffer);
        }
        /* 폴더에 쓰려는 경우 */
        else if(inode_is_dir(file_get_inode(target))){
            // printf("[syscall write] CAN'T WRITE FOLDER\n");
        }
        else
        {
            /* 실제로는 inode의 writable이 더 중요하다 !!! */

            ret = file_write(target,buffer,(off_t)length);

        }
    }
    lock_release(&filesys_lock);
    return ret;
}
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

void *mmap (void *addr, size_t length, int writable, int fd, int offset){
	struct file* file = file_reopen(process_get_file(fd));
    // process_add_file(file);
								/* file == NULL includes fd (0 , 1) */				
	// printf("addr : %ud , length : %d\n",addr,length);
	if(addr == 0 || (uintptr_t)addr%(uintptr_t)PGSIZE != 0 || 
									/*pg_round_down(offset) : page-aligned를 비교 가능 */
		(int)length <= 0 || file == NULL || pg_round_down(offset) != offset ||
		is_kernel_vaddr(addr) )
		return NULL;
	else{
        // process_add_file(file);
		return do_mmap(addr, length, writable, file, offset);
	}
}

void munmap (void *addr){
	
	do_munmap(addr);

}

bool chdir (const char *dir){
    char cp_name[32];
    char dir_name[NAME_MAX+1];
    struct inode* target_inode;
    struct dir *parent_dir;
    bool check;

	memcpy(cp_name, dir, strlen(dir)+1);
	parent_dir = parse_path(cp_name, dir_name);
    if(*dir_name == NULL){

        thread_current()->curr_dir = parent_dir;
        check = parent_dir != NULL;
    }
    else{
        check = dir_lookup(parent_dir, dir_name, &target_inode);    /* 주소임 !!!!!!!!!!!! ㅠㅠ */
        if(check){
            dir_close(thread_current()->curr_dir);
            thread_current()->curr_dir = dir_open(target_inode);
        }
        else{
            // printf("[syscall chdir] NO DIRECTORY\n");
        }
    }
    return check;
}
bool mkdir (const char *dir){

    return filesys_create_dir(dir);

}
bool readdir (int fd, char *name){
    struct file *target = process_get_file(fd);
    bool ret = false;
    if(target) /* fd == 0 이 었으면, 0을 return 했을 것이다.*/
    {   
        /* if it is directory */
        if(inode_is_dir(file_get_inode(target))){
            /* directory 일 때만 ! */
            // printf("readdir , name :  %s\n",name);
            // struct dir* dir = dir_open(file_get_inode(target));
            ret = dir_readdir((struct dir*)target, name);
            // printf("name : %s\n",name);
        }
    }
    return ret;
}
bool isdir (int fd){

    struct file *target = process_get_file(fd);
    bool ret = false;
    if(target) /* fd == 0 이 었으면, 0을 return 했을 것이다.*/
    {   
        /* if it is directory */
        if(inode_is_dir(file_get_inode(target))){

            ret = true;
        }
    }
    return ret;
}
int inumber (int fd){
    struct file *target = process_get_file(fd);
    int ret = 0;
    if(target) /* fd == 0 이 었으면, 0을 return 했을 것이다.*/
    {   
        ret = inode_get_inumber(file_get_inode(target));
    }
    return ret;
}
int symlink (const char *target, const char *linkpath){
    return 1;
}