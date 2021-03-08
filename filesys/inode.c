#include "filesys/inode.h"
#include "filesys/fat.h"
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
	cluster_t start;                	/* First data cluster(sector). */
	cluster_t last;                		/* Last data cluster(sector). */
	off_t length;                       /* File size in bytes. */
	uint32_t type;						/* 0: file, 1: dir, 2: symlink */
	unsigned magic;                     /* Magic number. */
	char path[128];						/* Symlink Path */
	uint32_t unused[91];               	/* Not used. */
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
	// file_sys - FAT
	// printf("byte_to_sector 1111 || inode->sector %d\n", inode->sector); 
	// printf("byte_to_sector 1111 || pos %d\n", pos); 
	// printf("byte_to_sector 1111 || inode->data.length %d\n", inode->data.length); 
	if (pos < inode->data.length) {
		cluster_t clst = inode->data.start;
		// printf("byte_to_sector 2222 || inode->data.start %d\n", inode->data.start); 
		for (int i = 0; i < pos / (DISK_SECTOR_SIZE * SECTORS_PER_CLUSTER); i++) {
			clst = fat_get(clst);
		}
		// printf("byte_to_sector 3333 || cluster_to_sector(clst) %d\n", cluster_to_sector(clst)); 
		return cluster_to_sector(clst);
	}
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
// subdirectory 수정
bool
inode_create (disk_sector_t sector, off_t length, uint32_t type, char *path) {
	struct inode_disk *disk_inode = NULL;
	bool success = true;
	ASSERT (length >= 0);
	// printf("\n\ninode_create :: sector :: %d\n", sector);
	// printf("inode_create :: length :: %d\n", length);
	/* If this assertion fails, the inode structure is not exactly
	 * one sector in size, and you should fix that. */
	ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);
	disk_inode = calloc (1, sizeof *disk_inode);
	if (disk_inode != NULL) {
		size_t sectors = bytes_to_sectors (length);
		disk_inode->length = length;
		disk_inode->magic = INODE_MAGIC;
		// file_Sys - subDir, Symlink
		disk_inode->type = type;
		if (type == 2) {
			memcpy (disk_inode->path, path, strlen(path) + 1);
		}

		// file_sys - FAT
		disk_inode->start = fat_create_chain(0);
		// printf("!!!!!!!!!!!!inode_create :: disk_inode->start :: %d\n", disk_inode->start);
		cluster_t origin_clst = disk_inode->start;
		for (int i = 1; i < sectors; i++) {
			origin_clst = fat_create_chain(origin_clst);
			if (!origin_clst) {
				return false;
			}
		}
		disk_inode->last = origin_clst;
		disk_write (filesys_disk, sector, disk_inode);
		free (disk_inode);
	}
	else {
		success = false;
	}
	// printf("inode_create :: success :: %d\n", success);
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
	// printf("\n\ninode_open :: sector :: %d\n", sector);
	// printf("inode_open :: inode->data.length :: %d\n", inode->data.length);
	// printf("inode_open :: inode->data.start :: %d\n", inode->data.start);
	// printf("inode_open :: inode->data.last :: %d\n", inode->data.last);
	// printf("inode_open :: inode->data.magic :: %d\n\n", inode->data.magic);
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
	// printf("inode_close 11111 || inode->sector %d\n", inode->sector);
	/* Release resources if this was the last opener. */
	if (--inode->open_cnt == 0) {
		/* Remove from inode list and release lock. */
		list_remove (&inode->elem);
		/* Deallocate blocks if removed. */
		if (inode->removed) {
			// printf("inode_close 22222 || inode->sector %d\n", inode->sector);
			fat_remove_chain(inode->data.start, 0);
			fat_put(sector_to_cluster(inode->sector), 0);
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
	// printf("inode_read_at 11111 || inode->sector %d\n", inode->sector);
	while (size > 0) {
		/* Disk sector to read, starting byte offset within sector. */
		disk_sector_t sector_idx = byte_to_sector (inode, offset);
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
			/* disk read를 buffer cache 할 때 다른거로 대체 해야하는듯.?*/
			disk_read (filesys_disk, sector_idx, buffer + bytes_read);
			// printf("inode_read_at 22222 || sector_idx %d\n", sector_idx); 
		} 
		else {
			/* Read sector into bounce buffer, then partially copy
			 * into caller's buffer. */
			if (bounce == NULL) {
				bounce = malloc (DISK_SECTOR_SIZE);
				if (bounce == NULL)
					break;
			}
			disk_read (filesys_disk, sector_idx, bounce);
			// printf("inode_read_at 33333 || sector_idx %d\n", sector_idx); 
			memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
		}
		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_read += chunk_size;
	}
	free (bounce);
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
	while (size > 0) {
		/* Sector to write, starting byte offset within sector. */
		disk_sector_t sector_idx = byte_to_sector (inode, offset);
		int sector_ofs = offset % DISK_SECTOR_SIZE;
		// printf("inode_write_at 111111 || sector_idx %d\n", sector_idx); 
		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length (inode) - offset;
		int sector_left = DISK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;
		/* Number of bytes to actually write into this sector. */
		int chunk_size = size < min_left ? size : min_left;
		// printf("inode_write_at :: chunk_size :: %d\n", chunk_size); 
		if (chunk_size <= 0) {
			disk_sector_t original_sectors = bytes_to_sectors(inode_length(inode));
			inode->data.length = offset + size;
			disk_sector_t additional_sectors = bytes_to_sectors(inode_length(inode)) - original_sectors;
			// if (original_sectors == 0) {
			// 	additional_sectors -= 1;
			// }
			if (additional_sectors > 0) {
				cluster_t last_clst = inode->data.last;
				static char zeros[DISK_SECTOR_SIZE];
				// printf("inode_write_at :: 11111111111\n");
				for (size_t i = 0; i < additional_sectors; i++) {
					last_clst = fat_create_chain(last_clst);
					disk_write (filesys_disk, cluster_to_sector(last_clst), zeros);
				}
				// printf("inode_write_at :: 222222222222\n");
				inode->data.last = last_clst;
			}
			continue;
			// break;
		}
		// printf("inode_write_at 22222 || sector_idx %d\n", sector_idx); 
		if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) {
			/* Write full sector directly to disk. */
			disk_write (filesys_disk, sector_idx, buffer + bytes_written); 
				// printf("inode_write_at :: 333333333333\n");
		} 
		else {
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
				// printf("inode_write_at :: 4444444444444\n");
		}
		// printf("inode_write_at 33333 || sector_idx %d\n", sector_idx); 
		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_written += chunk_size;
		// printf("inode_write_at :: chunk_size :: %d\n", chunk_size); 
		// printf("inode_write_at :: bytes_written :: %d\n", bytes_written); 
	}
	free (bounce);
	// printf("inode_write_at :: bytes_written :: %d\n", bytes_written); 
	disk_write (filesys_disk, inode->sector, &inode->data); 
	return bytes_written;
}
/* Disables writes to INODE.
   May be called at most once per inode opener. */
	void
inode_deny_write (struct inode *inode) {
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

// file_sys - subdir | syscall(is_dir)에 활용됨.
bool 
inode_is_dir (const struct inode *inode){
	bool result = false;
	if (inode->data.type == 1) {
		result = true;
	}
	return result;
}

bool 
inode_is_file (const struct inode *inode){
	bool result = false;
	if (inode->data.type == 0) {
		result = true;
	}
	return result;
}

bool 
inode_is_symlink (const struct inode *inode){
	bool result = false;
	if (inode->data.type == 2) {
		result = true;
	}
	return result;
}

int
inode_get_open_cnt(const struct inode *inode){
	return inode->open_cnt;
}

char *
inode_get_path(const struct inode *inode){
	// printf("inode_get_path :: inode_get_path(inode) :: %s\n",  inode->data.path);
	return inode->data.path;
}
