#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/synch.h"

void syscall_init (void);
// void syscall_handler (struct intr_frame *);


/*-------------------------- project.2-System Call -----------------------------*/
struct lock filesys_lock;
typedef int pid_t;

/*-------------------------- project.2-System Call -----------------------------*/


#endif /* userprog/syscall.h */
