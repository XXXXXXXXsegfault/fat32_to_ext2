#ifndef _EXT2_C_
#define _EXT2_C_
struct ext2_superblock
{
	unsigned int inodes;
	unsigned int blocks;
	unsigned int r_blocks;
	unsigned int free_blocks;
	unsigned int free_inodes;
	unsigned int first_data_block;
	unsigned int block_size;
	unsigned int frag_size;
	unsigned int blocks_per_group;
	unsigned int frags_per_group;
	unsigned int inodes_per_group;
	unsigned int mtime;
	unsigned int wtime;
	unsigned short int mount_count;
	unsigned short int max_mounts;
	unsigned short int magic;
	unsigned short int state;
	unsigned short int errors;
	unsigned short int minor_rev;
	unsigned int lastcheck;
	unsigned int checkinterval;
	unsigned int creator_os;
	unsigned int rev;
	unsigned short int def_resuid;
	unsigned short int def_resgid;
	unsigned int first_ino;
	unsigned short int inode_size;
	unsigned short int block_group_nr;
	unsigned int feature_compat;
	unsigned int feature_incompat;
	unsigned int feature_ro_compat;
	unsigned char uuid[16];
	unsigned char volume_name[16];
	unsigned char last_mount[64];
	unsigned int algo_bitmap;
	unsigned char prealloc_blocks;
	unsigned char prealloc_dir_blocks;
	unsigned short int pad;
	unsigned char journal_uuid[16];
	unsigned int journal_ino;
	unsigned int journal_dev;
	unsigned int last_orphan;
	unsigned int hash_seed[4];
	unsigned char hash_version;
	unsigned char pad2[3];
	unsigned int mount_opt;
	unsigned int first_meta_bg;
	unsigned int rsv[190];
};
struct ext2_bgdt
{
	unsigned int block_bitmap;
	unsigned int inode_bitmap;
	unsigned int inode_table;
	unsigned short int free_blocks;
	unsigned short int free_inodes;
	unsigned short int used_dirs;
	unsigned short int pad;
	unsigned char rsv[12];
};
struct ext2_inode
{
	unsigned short int mode;
	unsigned short int uid;
	unsigned int size;
	unsigned int atime;
	unsigned int ctime;
	unsigned int mtime;
	unsigned int dtime;
	unsigned short int gid;
	unsigned short int links;
	unsigned int blocks;
	unsigned int flags;
	unsigned int osd1;
	unsigned int block[15];
	unsigned int generation;
	unsigned int file_acl;
	unsigned int dir_acl;
	unsigned int faddr;
	unsigned int osd2[3];
	unsigned int rsv1;
	unsigned int ctime_extra;
	unsigned int mtime_extra;
	unsigned int atime_extra;
	unsigned int crtime;
	unsigned int crtime_extra;
};
struct ext2_directory
{
	unsigned int inode;
	unsigned short int rec_len;
	unsigned char name_len;
	unsigned char file_type;
	char file_name[1];
};
#endif
