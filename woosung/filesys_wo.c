#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"
#include "filesys/fat.h"
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
	fat_init ();

	if (format)
		do_format ();

	fat_open ();

	thread_current()->curr_dir = dir_open_root(); /* loading 을 위해서 필요하다. */
	// printf("%d\n",inode_get_inumber(dir_get_inode(thread_current()->curr_dir)));

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
	/* NAME을 기반으로 PARSING 

		'/' : root 가 있는지 ?
		'/' 가 없으면 현재 경로 기준이다.
		parse_path
		dir_lookup 으로 inode를 받아오자. 
	*/
	// struct dir *dir = dir_open_root (); /* 잘 열고 있는지 확인하라 ! */
	

	char file_name[NAME_MAX+1]; /* 2장 FAQ 참조 & disk 구조체 */

	// ASSERT(strlen(name) < 32);

	/* const char name 방지 copy */
	char cp_name[128];
	memcpy(cp_name, name, strlen(name)+1);
	struct dir *dir = parse_path(cp_name, file_name);
	
	// printf("file_name : %s\n",file_name);
	if(*file_name == NULL){
		// printf("[filesys_create] file_name is NULL !!\n");

		/* .이나 ..을 만들어달라고 요청한거임 ㅡㅡ */
		return false;
	}
	cluster_t inode_cluster;
	disk_sector_t inode_sector;
	/* inode를 하나의 sector에 저장한다. */
	inode_cluster = fat_create_chain(0);
	inode_sector = cluster_to_sector(inode_cluster);

	bool success = (dir != NULL
			// && free_map_allocate (1, &inode_sector)
			&& inode_sector
			&& inode_create (inode_sector, initial_size, false)
			&& dir_add (dir, file_name, inode_sector));	/* entry를 넣는과정 */
				/* file_name 이 NULL 일 때, dir_add 에서 걸렀을 듯?*/
	
	if (!success && inode_sector != 0)
		fat_remove_chain(inode_cluster, 0);
	

	dir_close (dir);

	return success;
}

bool
filesys_create_dir(const char* dir){
	
	char dir_name[NAME_MAX+1]; /* 2장 FAQ 참조 & disk 구조체 */

	// ASSERT(strlen(dir) < 32);
	/* const char name 방지 copy */
	char cp_name[128];
	memcpy(cp_name, dir, strlen(dir)+1);

	struct dir *parent_dir = parse_path(cp_name, dir_name);
	
	if(dir_name == NULL){
		// printf("[filesys_create_dir] file_name is NULL !!\n");

		/* .이나 ..을 만들어달라고 요청한거임 ㅡㅡ */
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

	/* PARSE ! */
	
	char file_name[NAME_MAX+1]; /* 2장 FAQ 참조 & disk 구조체 */

	/* const char name 방지 copy */
	// ASSERT(strlen(name) < 32);

	char cp_name[128];
	memcpy(cp_name, name, strlen(name)+1);

	struct dir *dir = parse_path(cp_name, file_name);
	// printf("file_name : %d, %s\n",file_name,file_name);
	if(*file_name == NULL){
		/*	하나의 폴더만 열라고 한 경우, 그 폴더는 dir에 담겨있다. */
		// printf("[filesys_open] file_name is NULL !!\n");
		
		/* 지워 졌는지 확인해야한다. 
		----> current 아니면, root 다. 
		지워질 수가 없다.
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

	/* PARSE ! */
	char file_name[NAME_MAX+1]; /* 2장 FAQ 참조 & disk 구조체 */
	
	ASSERT(strlen(name) < 32);

	char cp_name[32];
	memcpy(cp_name, name, strlen(name)+1);

	struct dir *dir = parse_path(cp_name, file_name);
	if(*file_name == NULL){
		// printf("[filesys_remove] file_name is NULL !!\n");

		/* .이나 ..은 지우지 못하게 했다. */
		dir_close(dir);
		return false;
	}

	struct inode *inode = NULL;

	/* 해당 file_name을 가진 녀석이 file인지 directory인지 검증 */
	if (dir != NULL)
		dir_lookup (dir, file_name, &inode);	/* 여기서 inode_open이 행해진다. */

	/* 해당 directory에서 file_name이 없을 경우에 ! */
	ASSERT(inode != NULL);

	/* 무조건 1은 있다. dir_lookup 에서 찾으려고 열었으니까 있으니까 */
	if(inode_get_open_cnt(inode) > 1){
		/* 열려 있는 녀석은 지울 수 없다. */

		// printf("inode : %p, cnt : %d\n",inode, inode_get_open_cnt(inode));
		inode_close(inode);
		// printf("inode : %p, cnt : %d\n",inode, inode_get_open_cnt(inode));
		// printf("remove fail!\n");
		return false;
	}
	// else{
	// 	printf("inode cnt : 0 \n");
	// }
	if(inode && inode_is_dir(inode)){
		/* 비어있는지 검증 */
		/* 

		directory entry 사이즈는 inode에 들어있다.
		--> 지워질 때, length가 줄어드는가 ?

		*/
		if(dir_empty(inode) == false){
			printf("DIRECTORY NOT EMPTY\n");

			inode_close(inode);	/* 추가 하고 싶음 */
			return false;
		}
	}
	

	inode_close(inode);	/* 추가 하고 싶음 +1 된 상태임  */
	bool success = dir != NULL && dir_remove (dir, file_name);
	dir_close (dir);

	return success;
}

int
filesys_symlink(const char *target, const char *linkpath){
	
	/* target이 가리키는 inode를 linkpath라는 이름의 inode와 같게 한다. */

	char file_name[NAME_MAX +1];

	char cp_target[128];
	memcpy(cp_target, target, strlen(target)+1);


	/* 얘는 현재 위치에 link를 넣어야한다.!!!!!!!!!!!!!!!!!!!!! 주의 !!!!!!!*/
	// struct dir* parent_dir = parse_path(cp_target, file_name);
	if(file_name == NULL){

		/* .이나 .. 전용 공간*/


		return 0;
	}

	if(dir != NULL)
		dir_lookup(dir,file_name, &inode);
	
	if(inode){
		disk_sector_t link_sector = inode_get_inumber(inode);
		dir_add(parent_dir, linkpath, link_sector);
	}





}

/* Formats the file system. */
static void
do_format (void) {
	printf ("Formatting file system...");

#ifdef EFILESYS
	/* Create FAT and save it to the disk. */
	fat_create();                                                                                                                                                                                                                                                                                                                                         
	fat_close ();


#else
	/* 아래 녀석들은 되지 않는다. */
	free_map_create ();
	if (!dir_create (ROOT_DIR_SECTOR, 16))
		PANIC ("root directory creation failed");
	free_map_close ();
	
#endif

	printf ("done.\n");
}
