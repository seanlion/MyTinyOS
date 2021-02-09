#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
/*-------------------------- project.2-System Call -----------------------------*/
#include "threads/synch.h"
#include <stdbool.h>
#include <debug.h>
#include <stddef.h>
/*-------------------------- project.2-System Call -----------------------------*/


void syscall_init (void);

/*-------------------------- project.2-System Call -----------------------------*/
struct lock filesys_lock;
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
int write(int, void *, unsigned );
/*-------------------------- project.2-System call -----------------------------*/


#endif /* userprog/syscall.h */
