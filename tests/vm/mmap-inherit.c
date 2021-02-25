/* Maps a file into memory and runs child-inherit to verify that
   mappings are not inherited. */

#include <string.h>
#include <syscall.h>
#include "tests/vm/sample.inc"
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  char *actual = (char *) 0x54321000;
  int handle;
  pid_t child;

  /* Open file, map, verify data. */
  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");
  CHECK (mmap (actual, 4096, 0, handle, 0) != MAP_FAILED, "mmap \"sample.txt\"");

  // printf("111111111111111111111111111\n");

  if (memcmp (actual, sample, strlen (sample)))
    fail ("read of mmap'd file reported bad data");

  // printf("2222222222222222222222222222\n");

	/* Spawn child and wait. */
	child = fork("child-inherit");
  // printf("33333333333333333333333333333333\n");
	if (child == 0) {
		CHECK (exec ("child-inherit") != -1, "exec \"child-inherit\"");
    // printf("44444444444444444444\n");
	}	else {
		quiet = true;
		CHECK (wait (child) == -1, "wait for child (should return -1)");
    // printf("555555555555555555555\n");
		quiet = false;
	}
  // printf("666666666666666666666\n");

  /* Verify data again. */
  CHECK (!memcmp (actual, sample, strlen (sample)),
         "checking that mmap'd file still has same data");
}
