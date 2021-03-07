#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/fat.h"
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"
#include "threads/thread.h"
/* The disk that contains the file system. */
struct disk *filesys_disk;
static void do_format (void);
/* Initializes the file system module.
 * If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) {
	filesys_disk = disk_get (0, 1);
	if (filesys_disk == NULL)
		PANIC ("hd0:1 (hdb) not present, file system initialization failed");
	inode_init ();
#ifdef EFILESYS
	// printf("filesys init 1111\n");
	fat_init ();
	if (format)
		do_format ();
	// printf("filesys init 222\n");
	fat_open ();
	thread_current()->curr_dir = dir_open_root(); // dir_open_root가 dir 리턴함.
	// printf("filesys init 3333\n");
#else
	/* Original FS */
	free_map_init ();
	if (format)
		do_format ();
	free_map_open ();
#endif
}
/* Shuts down the file system module, writing any unwritten data
 * to disk. */
void
filesys_done (void) {
	/* Original FS */
#ifdef EFILESYS
	fat_close ();
#else
	free_map_close ();
#endif
}
/* Creates a file named NAME with the given INITIAL_SIZE.
 * Returns true if successful, false otherwise.
 * Fails if a file named NAME already exists,
 * or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) {
	// file_sys - subdir | name의 파일 경로 cp_name에 복사해서 경로 파싱
	char file_name[NAME_MAX+1];
	char cp_name[128];
	memcpy(cp_name,name, strlen(name)+1);
	struct dir *dir = parse_path(cp_name,file_name);

	if (*file_name == NULL){
		return false;
	}
	
	cluster_t inode_clst = fat_create_chain(0);
	disk_sector_t inode_sector = cluster_to_sector(inode_clst);
	// printf("filesys_create :: inode_sector :: %d\n", inode_sector);


	// struct dir *dir = dir_open_root (); /* subdirectory 구현 후 parse_path하면 제거*/
	bool success = (dir != NULL
			&& inode_clst
			// file만드는거라 is_dir 0으로 설정
			&& inode_create (inode_sector, initial_size,false)
			// name을 file_name으로 교체
			&& dir_add (dir, file_name, inode_sector));
	if (!success && inode_sector != 0) {
		fat_put(inode_sector, 0);
	}
	dir_close (dir);
	return success;
}

/* file_sys - subdir | mkdir을 위한 함수*/
bool filesys_create_dir(const char* dir){
	char dir_name[NAME_MAX+1]; /* 2장 FAQ 참조 & disk 구조체 */

	// ASSERT(strlen(dir) < 32);
	/* const char name 방지 copy */
	char cp_name[128];
	memcpy(cp_name, dir, strlen(dir)+1);

	struct dir *parent_dir = parse_path(cp_name, dir_name);
	
	if(dir_name == NULL){
		return false;
	}
	/* inode를 하나의 sector에 저장한다. */
	cluster_t inode_cluster;
	disk_sector_t inode_sector;
	
	inode_cluster = fat_create_chain(0);
	inode_sector = cluster_to_sector(inode_cluster);
	bool success = (parent_dir != NULL
			// && free_map_allocate (1, &inode_sector)
			&& inode_sector
			&& inode_create (inode_sector, 16, true)	/* 이 컨텐츠에 대한 inode를 inode_sector에 저장 */
			&& dir_add (parent_dir, dir_name, inode_sector));	/* entry를 넣는과정 */
	/* entry의 inode_sector : inode의 inode_sector*/

	if (!success && inode_sector != 0)
		fat_remove_chain(inode_cluster, 0);
	else{
		// printf(" ADD SUCCESS \n");
		struct inode* create_inode = inode_open(inode_sector);
		struct inode* parent_inode = dir_get_inode(parent_dir);
		disk_sector_t parent_sector = inode_get_inumber(parent_inode);

		struct dir* create_dir = dir_open(create_inode);	/* inode 갖다 붙이기 */

		dir_add(create_dir, ".",inode_sector);
		dir_add(create_dir, "..", parent_sector);
		dir_close(create_dir);
	}

	dir_close (parent_dir);

	return success;
}

/* Opens the file with the given NAME.
 * Returns the new file if successful or a null pointer
 * otherwise.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name) {
	// file_sys - subdir | 경로 파싱하면 dir_open_root바로 하는거 제거 해야 함.
	// struct dir *dir = dir_open_root ();
	
	char file_name[NAME_MAX+1]; /* 2장 FAQ 참조 & disk 구조체 */
	/* const char name 방지 copy */
	char cp_name[128];
	memcpy(cp_name, name, strlen(name)+1);

	struct dir *dir = parse_path(cp_name, file_name);
	// printf("file_name : %d, %s\n",file_name,file_name);
	if(*file_name == NULL){
		/*	하나의 폴더만 열라고 한 경우, 그 폴더는 dir에 담겨있다. */
		
		/* 지워 졌는지 확인해야한다. 
		----> current 아니면, root 다. 지워질 수가 없다.
		*/
		return dir;
	}

	struct inode *inode = NULL;

	if (dir != NULL)
		dir_lookup (dir, file_name, &inode);
	dir_close (dir);

	/* 얘는 inode 닫으면 안됨 -- file에 inode 갖다 붙이는 구조  */
	return file_open (inode);
}
/* Deletes the file named NAME.
 * Returns true if successful, false on failure.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) {
	// file_sys - subdir | 경로 파싱해야 함.
	char file_name[NAME_MAX+1];
	ASSERT(strlen(name)<32); // 체크 용도로 넣은듯.
	char cp_name[32];
	memcpy(cp_name,name, strlen(name)+1);
	// printf("filesys_remove 1111 \n");
	struct dir* dir = parse_path(cp_name, file_name);
	if (*file_name == NULL){
		dir_close(dir);
		return false;
	}
	// printf("filesys_remove 2222 \n");
	// struct dir *dir = dir_open_root ();
	struct inode *inode = NULL;
	if (dir!= NULL){
		dir_lookup(dir, file_name, &inode);
	}
	// printf("filesys_remove 3333 \n");
	// 해당 디렉토리에서 file name이 없으면
	ASSERT(inode!=NULL);

	if (inode_get_open_cnt(inode)>1){
		inode_close(inode);
		return false;
	}
	// printf("filesys_remove 4444 \n");
	if (inode && inode_is_dir(inode)){
	// printf("filesys_remove 5555 \n");
		if(dir_empty(inode) == false){
			// 디렉토리가 비어있지않은 경우
	// printf("filesys_remove 5555--1 \n");
			inode_close(inode);
	// printf("filesys_remove 5555--2 \n");
			return false;
		}
	}
	// else{
	// 	printf("empty !!\n");
	// }

	inode_close(inode);
	// printf("filesys_remove 66666 \n");

	bool success = dir != NULL && dir_remove (dir, file_name);
	dir_close (dir);
	// printf("filesys_remove 7777  success : %d\n", success);
	return success;
}
/* Formats the file system. */
static void
do_format (void) {
	printf ("Formatting file system...");
#ifdef EFILESYS
	/* Create FAT and save it to the disk. */
	fat_create ();
	if (!dir_create (cluster_to_sector(ROOT_DIR_CLUSTER), 16))
		PANIC ("root directory creation failed");
	fat_close ();
#else
	free_map_create ();
	if (!dir_create (ROOT_DIR_SECTOR, 16))
		PANIC ("root directory creation failed");
	free_map_close ();
#endif
	printf ("done.\n");
}