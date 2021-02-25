#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "vm/vm.h"

struct page;
enum vm_type;

struct file_page {
	int mapping_id;
	size_t length;
	struct file* file;
	size_t offset;				/* 읽어야 할 파일 오프셋 */
	size_t read_bytes;			/* 가상페이지에 쓰여져 있는 데이터 크기, // unmmap을 위한 파일 길이 */
	size_t zero_bytes;			/* 0으로 채울 남은 페이지의 바이트 */
	bool writable;				/* True일 경우 해당 주소에 write 가능, False일 경우 */
};

void vm_file_init (void);
bool file_backed_initializer (struct page *page, enum vm_type type, void *kva);
void *do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset);
void do_munmap (void *va);
static bool lazy_map (struct page *page, void *aux);

#endif
