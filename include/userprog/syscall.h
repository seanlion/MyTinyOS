#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/synch.h"

/*-------------------------- project.2-System Call -----------------------------*/
// #include "threads/synch.h"
// #include <stdbool.h>
// #include <debug.h>
// #include <stddef.h>
/*-------------------------- project.2-System Call -----------------------------*/

// static struct lock filesys_lock;
void syscall_init (void);


/*-------------------------- project.2-System call -----------------------------*/
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
/*-------------------------- project.2-System call -----------------------------*/
#endif /* userprog/syscall.h */
