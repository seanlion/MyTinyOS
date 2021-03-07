#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* A directory. */
struct dir {
	struct inode *inode;                /* Backing store. */
	off_t pos;                          /* Current position. */
};

/* A single directory entry. */
struct dir_entry {
	disk_sector_t inode_sector;         /* Sector number of header. */

	/* 이름 길이 제한!!  */
	char name[NAME_MAX + 1];            /* Null terminated file name. */
	bool in_use;                        /* In use or free? */
};

/* Creates a directory with space for ENTRY_CNT entries in the
 * given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (disk_sector_t sector, size_t entry_cnt) {

									/* 
									* entry개수를 미리 넣어준다. 그리고 이만큼 뒤진다.
									* 이 친구들이 false인지 다 살펴보면 될 듯?
									 */
	return inode_create (sector, entry_cnt * sizeof (struct dir_entry), true);
}

/* Opens and returns the directory for the given INODE, of which
 * it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) {
	struct dir *dir = calloc (1, sizeof *dir);
	if (inode != NULL && dir != NULL) {
		dir->inode = inode;	/* inode 갖다 붙이기 */
		dir->pos = 0;
		return dir;
	} else {
		inode_close (inode);
		free (dir);
		return NULL;
	}
}

/* Opens the root directory and returns a directory for it.
 * Return true if successful, false on failure. */
struct dir *
dir_open_root (void) {

	/* open directory root -- INODE_OPEN(SECTOR)*/		
	return dir_open (inode_open (cluster_to_sector(ROOT_DIR_SECTOR)));

}

/* Opens and returns a new directory for the same inode as DIR.
 * Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) {
	return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) {
	if (dir != NULL) {
		inode_close (dir->inode);
		free (dir);
	}
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) {
	return dir->inode;
}

/* Searches DIR for a file with the given NAME.
 * If successful, returns true, sets *EP to the directory entry
 * if EP is non-null, and sets *OFSP to the byte offset of the
 * directory entry if OFSP is non-null.
 * otherwise, returns false and ignores EP and OFSP. */

 /* 인자로 주어진 ep에 "" Entry의 주소 "" 를 반환시킨다. */
static bool
lookup (const struct dir *dir, const char *name,
		struct dir_entry *ep, off_t *ofsp) {
	struct dir_entry e;
	size_t ofs;

	ASSERT (dir != NULL);
	ASSERT (name != NULL);

	for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
			ofs += sizeof e)
		if (e.in_use && !strcmp (name, e.name)) {
			if (ep != NULL)
				*ep = e;
			if (ofsp != NULL)
				*ofsp = ofs;
			return true;
		}
	return false;
}

bool
dir_empty( struct inode* inode){

	struct dir_entry e;
	off_t ofs;

	int cnt = inode_length(inode) / sizeof (e) ;
					/* entry 개수만큼 돌면서 사용중인 entry가 있는지 살펴본다. */
	
	for(ofs = 0 ; cnt != 0 ; ofs+= sizeof(e)){
		if( inode_read_at(inode, &e, sizeof e, ofs) == sizeof(e) ){
			/* . 과 ..은 포함하지 않는다!!!!  */
			if((strcmp(e.name,".") != NULL && strcmp(e.name,"..") !=NULL) && e.in_use == true){
				// printf("THRER IS SOME FILE **IN USE** IN THIS DIRECTORY\n");
					return false;
				}
		}
		else
		{
			// printf("DIRECTORY READING FAIL!\n");
		}
		cnt --;
	}

}

/* Searches DIR for a file with the given NAME
 * and returns true if one exists, false otherwise.
 * On success, sets *INODE to an inode for the file, otherwise to
 * a null pointer.  The caller must close *INODE. */

 /* 인자로 주어진 inode에 "" inode의 주소 "" 를 반환시킨다. */

bool
dir_lookup (const struct dir *dir, const char *name,
		struct inode **inode) {
	struct dir_entry e;

	ASSERT (dir != NULL);
	ASSERT (name != NULL);

	if (lookup (dir, name, &e, NULL)){
		// printf("look up SUCCESS \n");
		*inode = inode_open (e.inode_sector);	/* 저장된 inode를 채워준다. */
	}
	else{
		// printf("look up FAIL \n");
		*inode = NULL;
	}

	return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
 * file by that name.  The file's inode is in sector
 * INODE_SECTOR.
 * Returns true if successful, false on failure.
 * Fails if NAME is invalid (i.e. too long) or a disk or memory
 * error occurs. */
bool
dir_add (struct dir *dir, const char *name, disk_sector_t inode_sector) {
	struct dir_entry e;
	off_t ofs;
	bool success = false;

	ASSERT (dir != NULL);
	ASSERT (name != NULL);

	/* Check NAME for validity. */
	if (*name == '\0' || strlen (name) > NAME_MAX)
		return false;

	// printf("dir : %p, name : %s\n",dir,name);
	/* Check that NAME is not in use. */
	if (lookup (dir, name, NULL, NULL))
		goto done;

	/* Set OFS to offset of free slot.
	 * If there are no free slots, then it will be set to the
	 * current end-of-file.

	 * inode_read_at() will only return a short read at end of file.
	 * Otherwise, we'd need to verify that we didn't get a short
	 * read due to something intermittent such as low memory. */

	 /* 20만큼 계속 읽으면서 entry를 살펴본다. --> 이전에 할당시켜놨음 */
	for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
			ofs += sizeof e)
		if (!e.in_use)	/* entry 중에서 사용중이 아닌 것을 찾을 때까지*/
			break;

	/* Write slot. */
	e.in_use = true;
	strlcpy (e.name, name, sizeof e.name);
	e.inode_sector = inode_sector; /* entry의 inode_sector : 컨텐츠의 inode의 sector */
	success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

done:
	return success;
}

/* Removes any entry for NAME in DIR.
 * Returns true if successful, false on failure,
 * which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) {
	struct dir_entry e;
	struct inode *inode = NULL;
	bool success = false;
	off_t ofs;

	ASSERT (dir != NULL);
	ASSERT (name != NULL);

	/* Find directory entry. */
	if (!lookup (dir, name, &e, &ofs))
		goto done;

	/* Open inode. */
	inode = inode_open (e.inode_sector);
	if (inode == NULL)
		goto done;

	/* Erase directory entry. */
	e.in_use = false;
					/* inode, buffer, size, offset	-> e에 대한 정보를 적는다. */
	if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e)
		goto done;

	/* Remove inode. */
	inode_remove (inode);
	success = true;

done:
	inode_close (inode);
	return success;
}


/* 하나 씩 읽는 거구나...!!!! */
/* Reads the next directory entry in DIR and stores the name in
 * NAME.  Returns true if successful, false if the directory
 * contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1]) {
	struct dir_entry e;

	while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) {
		dir->pos += sizeof e;
						/*.은 안적기로 설정 */
		if (e.in_use &&  (strcmp(e.name,".") != NULL && strcmp(e.name,"..") != NULL)) {
			strlcpy (name, e.name, NAME_MAX + 1);

			return true;
		}
	}
	return false;
}

struct dir* 
parse_path(char* path_name, char* file_name){
	if( path_name == NULL || file_name == NULL)
		return NULL;
	
	if (strlen(path_name) == 0)
		return NULL;

	/* 계속해서 token으로 dir_lookup할 것이다. 
		이 때, nextToken이 NULL이 되는 순간이 token이 filename이 되는 순간이다. */

	/* Parse S into tokens separated by characters in DELIM.
	If S is NULL, the saved pointer in SAVE_PTR is used as
	the next starting point.  For example:
		char s[] = "-abc-=-def";
		char *sp;
		x = strtok_r(s, "-", &sp);  // x = "abc", sp = "=-def"
		x = strtok_r(NULL, "-=", &sp);  // x = "def", sp = NULL
		x = strtok_r(NULL, "=", &sp);   // x = NULL
			// s = "abc\0-def\0"
	*/

	struct dir* dir;
	struct inode *inode;

	/* 시작 dir 정하기 -- token 하면 구분이 불가능하다. */
	if(path_name[0] == '/'){
		dir = dir_open_root();
	}
	else{
		dir = dir_reopen(thread_current()->curr_dir);
	}


	/* 이중 세트로 움직인다. -- 마지막에 file임을 구분하기 위해서 */
	
	char *token, *nextToken, *save_ptr;
	bool check;
	token = strtok_r(path_name, "/", &save_ptr);
	nextToken = strtok_r(NULL, "/", &save_ptr);
	
	/* 주의 !!!!!!!!! sample.txt 도 .을 포함하고 있다 이녀석아 ㅠㅠ*/
	/* '/' 없이 들어온 경우 */
	if(nextToken == NULL){
		/* 인자로 하나 들어온 경우 */
		if(token != NULL){
			/* ..만 들어온 경우 */
			if(strcmp(token,"..") == NULL){
				// printf("ONLY PARENT FOLDER\n");
				if(dir->inode == inode_open (cluster_to_sector(ROOT_DIR_SECTOR))){
					// printf("ROOT's PARENT\n");
					inode_close(dir_get_inode(dir));
				}
				else{
					check = dir_lookup(dir,token,&inode);
					if(check == false){
						// printf("THERE IS NO PARENT FILE\n");
						return NULL;
					}
					// printf(" parent directory ..\n");
					dir_close(dir);
					dir = dir_open(inode);
				}
				*file_name = NULL;
			}
			/* . 만 들어온 경우 */
			else if(strcmp(token,".") == NULL){
				// printf("ONLY CURRENT FOLDER\n");
				/* 위에서 했음 */
				// dir = dir_reopen(thread_current()->curr_dir);
				*file_name = NULL;
			}
			/* 파일 이름만 들어온 경우 */
			else{
				/* 이미 위에서 했음*/
				// dir = dir_reopen(thread_current()->curr_dir);
				memcpy(file_name, token, strlen(token)+1);
				// printf("dir : %p, sector : %d\n",dir,inode_get_inumber(dir_get_inode(dir)));
			}
			// printf("FINAL FILE_NAME : %s\n",token);
			return dir;
		}
		else{
			*file_name = NULL;
			return dir;
		}
	}
	else{
		while(token && nextToken){
			/* 현재 directory */
			// if( strcmp(token,".") == NULL || strchr(token, '.') == NULL){
			if( strcmp(token,".") == NULL ){

				// printf("curr directory \n");
				// printf(" it might be .. / .\n");

				/* .은 .을 꼭 열어야 하나? */
				// dir = dir_reopen(thread_current()->curr_dir);
			}
			/* 부모 directory */
			else if(strcmp(token,"..") == NULL){
				
				if(dir->inode == inode_open (cluster_to_sector(ROOT_DIR_SECTOR))){
					// printf("ROOT's PARENT\n");
					continue;
				}

				check = dir_lookup(dir,token,&inode);
				if(check == false){
					// printf("THERE IS NO PARENT FILE\n");
					return NULL;
				}
				// printf(" parent directory ..\n");
				dir_close(dir);
				dir = dir_open(inode);
				// printf("PARENT FILE SUCCESS\n");
			}
			else{
				/* 해당 이름 폴더 찾기 */
				// printf("FOLDER NAME : %s\n",token);
				check = dir_lookup(dir,token,&inode);	/* inode의 주소임..........ㅠㅠㅠㅠ */
				if(check == false){
					// printf("There is no such File\n");
					return NULL;
				}
				else if (inode_is_dir(inode) == false){
					// printf("Inode is File\n");
					// printf("sector : %d\n",inode_length(inode));
					return NULL;
				}
				dir_close(dir); /* dir 은 calloc받고 있는 껍데기 녀석이다. 이동 시에는 free 시킨다. */
				dir = dir_open(inode);
			}
			token = nextToken;
			nextToken = strtok_r(NULL,"/", &save_ptr);
		}
		// printf("FINAL FILE_NAME : %s\n",token);
		memcpy(file_name, token, strlen(token)+1);
		return dir;
	}	
}
