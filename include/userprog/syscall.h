#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
/*-------------------------- project.2-System Call -----------------------------*/
#include "threads/synch.h"
/*-------------------------- project.2-System Call -----------------------------*/


void syscall_init (void);

/*-------------------------- project.2-System Call -----------------------------*/
struct lock filesys_lock;
typedef int pid_t;
/*-------------------------- project.2-System Call -----------------------------*/

#endif /* userprog/syscall.h */
