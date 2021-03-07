#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* On-disk inode.
 * Must be exactly DISK_SECTOR_SIZE bytes long. */
struct inode_disk {
	disk_sector_t start;                /* First data sector. */
	// cluster_t satrt_clst;
	off_t length;                       /* File size in bytes. */
	unsigned magic;                     /* Magic number. */
	uint32_t is_dir;					/* Directory ? */
	uint32_t unused[124];               /* Not used. */
};

/* Returns the number of sectors to allocate for an inode SIZE
 * bytes long. */
static inline size_t
bytes_to_sectors (off_t size) {
	return DIV_ROUND_UP (size, DISK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode {
	struct list_elem elem;              /* Element in inode list. */
	disk_sector_t sector;               /* Sector number of disk location. */
	int open_cnt;                       /* Number of openers. */
	bool removed;                       /* True if deleted, false otherwise. */
	int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
	struct inode_disk data;             /* Inode content. */
};

/* Returns the disk sector that contains byte offset POS within
 * INODE.
 * Returns -1 if INODE does not contain data for a byte at offset
 * POS. */
static disk_sector_t
byte_to_sector (const struct inode *inode, off_t pos) {
	ASSERT (inode != NULL);
	if (pos < inode->data.length){

		disk_sector_t ret = inode->data.start;

		cluster_t clst = sector_to_cluster(ret);

		/* pos / DISK_SECTOR_SIZE 번 만큼의 clst 이동이 필요하다. */
		for(int i = 0 ; i < pos / DISK_SECTOR_SIZE; i++){
			clst = fat_get(clst);

		}

		ret = cluster_to_sector(clst);

		return ret;
	}
	/* 기존 length 보다 큰 position이 들어왔다. */
	else
		return -1;
}

/* List of open inodes, so that opening a single inode twice
 * returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) {
	list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
 * writes the new inode to sector SECTOR on the file system
 * disk.
 * Returns true if successful.
 * Returns false if memory or disk allocation fails. */
bool
inode_create (disk_sector_t sector, off_t length, bool is_dir) {
	struct inode_disk *disk_inode = NULL;
	bool success = false;

	ASSERT (length >= 0);

	/* If this assertion fails, the inode structure is not exactly
	 * one sector in size, and you should fix that. */
	ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);

	disk_inode = calloc (1, sizeof *disk_inode);

	if (disk_inode != NULL) {
		size_t sectors = bytes_to_sectors (length); // 총 몇 sector를 할당할래 ?

		/* 진짜 정보들 넣기 */
		disk_inode->length = length;
		disk_inode->magic = INODE_MAGIC;
		disk_inode->is_dir = is_dir;	/* directory or not */
		/* 파일이 생성 될 clst( != inode clust)를 생성 */		
		cluster_t start_clst = fat_create_chain(0);
		/* 파일이 생성 될 sector를 저장 */		
		disk_inode->start = cluster_to_sector(start_clst);
		/* file에 대한 inode(disk_inode)를 disk에 넣는 행위 */
		disk_write(filesys_disk, sector, disk_inode);


		/* 할당할 만큼을 미리 disk에 써놓는다? */
		if(sectors > 0){
			static char zeros[DISK_SECTOR_SIZE];
			size_t i;

			/* 미리 할당 ?*/
			cluster_t clst;
			disk_sector_t write_sector;

			for (i = 0; i < sectors; i++) {
				
				if (i ==0)
					clst = start_clst;
				else
					clst = fat_create_chain(clst);

				write_sector = cluster_to_sector(clst);

				disk_write (filesys_disk, write_sector, zeros); /* 비워 놓기, 위에 얹을 것이다. */

				// clst = fat_create_chain(clst);
				// write_sector = cluster_to_sector(clst);
			}
		}
		success = true; 
		free(disk_inode);
	}
	return success;
}


/* Reads an inode from SECTOR
 * and returns a `struct inode' that contains it.
 * Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (disk_sector_t sector) {
	struct list_elem *e;
	struct inode *inode;

	/* Check whether this inode is already open. */
	for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
			e = list_next (e)) {
		inode = list_entry (e, struct inode, elem);
		if (inode->sector == sector) {
			inode_reopen (inode);
			return inode; 
		}
	}

	/* Allocate memory. */
	inode = malloc (sizeof *inode);
	if (inode == NULL)
		return NULL;

	/* Initialize. */
	list_push_front (&open_inodes, &inode->elem);
	inode->sector = sector;
	inode->open_cnt = 1;
	inode->deny_write_cnt = 0;
	inode->removed = false;
	disk_read (filesys_disk, inode->sector, &inode->data);
	return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode) {
	if (inode != NULL)
		inode->open_cnt++;
	return inode;
}

/* Returns INODE's inode number. */
disk_sector_t
inode_get_inumber (const struct inode *inode) {
	return inode->sector;
}

/* Closes INODE and writes it to disk.
 * If this was the last reference to INODE, frees its memory.
 * If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) {
	/* Ignore null pointer. */
	if (inode == NULL)
		return;

	/* Release resources if this was the last opener. */
	if (--inode->open_cnt == 0) {
		/* Remove from inode list and release lock. */
		list_remove (&inode->elem);

		/* Deallocate blocks if removed. */
		if (inode->removed) {
			// free_map_release (inode->sector, 1);
			// free_map_release (inode->data.start,
			// 		bytes_to_sectors (inode->data.length)); 

			// fat_remove_chain(inode->sector);
			// fat_remove_chain(inode->data.sector);
		}

		free (inode); 
	}
}

/* Marks INODE to be deleted when it is closed by the last caller who
 * has it open. */
void
inode_remove (struct inode *inode) {
	ASSERT (inode != NULL);
	inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
 * Returns the number of bytes actually read, which may be less
 * than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) {
	uint8_t *buffer = buffer_;
	off_t bytes_read = 0;
	uint8_t *bounce = NULL;

	// size_t sectors = bytes_to_sectors(inode->data->length)



	while(size > 0){
		

		disk_sector_t sector_idx = byte_to_sector(inode,offset); /* fate_get으로 계속 건너 뛰는 상황은? */
		
		// ASSERT(sector_idx != 157);
		int sector_ofs = offset % DISK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length (inode) - offset;
		int sector_left = DISK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* Number of bytes to actually copy out of this sector. */
		int chunk_size = size < min_left ? size : min_left;
		if (chunk_size <= 0)
			break;

		if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) {
			/* Read full sector directly into caller's buffer. */
			disk_read (filesys_disk, sector_idx, buffer + bytes_read); 
		} else {
			/* Read sector into bounce buffer, then partially copy
			 * into caller's buffer. */
			if (bounce == NULL) {
				bounce = malloc (DISK_SECTOR_SIZE);
				if (bounce == NULL)
					break;
			}
			disk_read (filesys_disk, sector_idx, bounce);
			memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
		}

		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_read += chunk_size;

	}
	free(bounce);

	return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
 * Returns the number of bytes actually written, which may be
 * less than SIZE if end of file is reached or an error occurs.
 * (Normally a write at end of file would extend the inode, but
 * growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
		off_t offset) {
	const uint8_t *buffer = buffer_;
	off_t bytes_written = 0;
	uint8_t *bounce = NULL;

	if (inode->deny_write_cnt)
		return 0;

	off_t old_length = inode->data.length;
	off_t new_length = offset + size;

	if(new_length > old_length){

		cluster_t clst = sector_to_cluster(inode->data.start);
		disk_sector_t old_sectors = bytes_to_sectors(old_length);
		
		disk_sector_t write_sector;
		static char zeros[DISK_SECTOR_SIZE];


		if (old_sectors > 1 ){
			for(int i = 0; i < (old_sectors - 1) ; i++){
				clst = fat_get(clst);
			}
		}
		
		// /* 0으로 fill 하든, 아니든 새로운 chain 생성 */
		// clst = fat_create_chain(clst);
		

		// /* 앞부분 0으로 fill */
		// if (offset > old_length){
		// 	/* offset - old_length 부분을 0으로 채워야한다 */			

		// 	for(int i = 0; i < bytes_to_sectors(offset-old_length); i++){
				
		// 		clst = fat_create_chain(clst);
		// 		write_sector = cluster_to_sector(clst);
		// 		disk_write(filesys_disk, write_sector, zeros);
		// 	}
		// 	/* clst : 마지막 clst, wr_sector : 마지막 sector */
		// }


		for (int i = 0; i < bytes_to_sectors(new_length-old_length); i++){

			clst = fat_create_chain(clst);
			write_sector = cluster_to_sector(clst);
			disk_write(filesys_disk, write_sector, zeros);

		}
		// printf("HERE3\n");

		inode->data.length = new_length;

		/* disk에 inode에 대한 정보를 꼭 다시 써줘야한다. */
		disk_write(filesys_disk, inode->sector, &inode->data);

	}


	while (size > 0) {
		/* Sector to write, starting byte offset within sector. */


		disk_sector_t sector_idx = byte_to_sector(inode,offset); /* 이미 file이 있다고 가정하고 찾는다. */

		int sector_ofs = offset % DISK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length (inode) - offset;
		int sector_left = DISK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* Number of bytes to actually write into this sector. */
		int chunk_size = size < min_left ? size : min_left;
		if (chunk_size <= 0)
			break;

		if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) {
			/* Write full sector directly to disk. */
			disk_write (filesys_disk, sector_idx, buffer + bytes_written); 
		} else {
			/* We need a bounce buffer. */
			if (bounce == NULL) {
				bounce = malloc (DISK_SECTOR_SIZE);
				if (bounce == NULL)
					break;
			}

			/* If the sector contains data before or after the chunk
			   we're writing, then we need to read in the sector
			   first.  Otherwise we start with a sector of all zeros. */
			if (sector_ofs > 0 || chunk_size < sector_left) 
				disk_read (filesys_disk, sector_idx, bounce);
			else
				memset (bounce, 0, DISK_SECTOR_SIZE);


			memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
			disk_write (filesys_disk, sector_idx, bounce); 
		}

		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_written += chunk_size;
	}
	free (bounce);
	
	return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
	void
inode_deny_write (struct inode *inode) 
{
	inode->deny_write_cnt++;
	ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
 * Must be called once by each inode opener who has called
 * inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) {
	ASSERT (inode->deny_write_cnt > 0);
	ASSERT (inode->deny_write_cnt <= inode->open_cnt);
	inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode) {
	return inode->data.length;
}

/* syscall isdir에 활용한다. */
bool 
inode_is_dir(struct inode *inode){

	return inode->data.is_dir;
}

int
inode_get_open_cnt(const struct inode *inode){
	return inode->open_cnt;
}