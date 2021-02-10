#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"


tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);
/*-------------------------- project.2-Parsing -----------------------------*/
void argument_stack(char ** ,int  ,struct intr_frame *);
/*-------------------------- project.2-Parsing -----------------------------*/


/*-------------------------- project.2-System call -----------------------------*/
int process_add_file(struct file *);
struct file * process_get_file(int);
void process_close_file(int);
void process_exit(void);
struct thread *get_child_process(int);
void remove_child_process(struct thread *);
/*-------------------------- project.2-System call -----------------------------*/
#endif /* userprog/process.h */
