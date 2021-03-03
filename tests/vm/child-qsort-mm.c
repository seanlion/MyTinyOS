/* Mmaps a 128 kB file "sorts" the bytes in it, using quick sort,
   a multi-pass divide and conquer algorithm.  */

#include <debug.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"
#include "tests/vm/qsort.h"

const char *test_name = "child-qsort-mm";

int
main (int argc UNUSED, char *argv[]) 
{
  int handle;
  unsigned char *p = (unsigned char *) 0x10000000;

  quiet = true;
  // printf("child-qsort-mm open 하려는 것? %s\n", argv[1]);
  CHECK ((handle = open (argv[1])) > 1, "open \"%s\"", argv[1]);
  // printf("child-qsort-mm open 후\n");
  CHECK (mmap (p, 4096*33, 1, handle, 0) != MAP_FAILED, "mmap \"%s\"", argv[1]);
  // printf("child-qsort-mm mmap 후\n");
  qsort_bytes (p, 1024 * 128);
  
  return 80;
}
