#include "syscall.c"
#include "signal.c"
#include "fat32.c"
#include "ext2.c"

int fd;
int stage;
void raw_block_read(void *buf,long long off,int size)
{
	lseek(fd,off,0);
	read(fd,buf,size);
}
void raw_block_write(void *buf,long long off,int size)
{
	lseek(fd,off,0);
	write(fd,buf,size);
}
#define CACHE_PAGES 32
#define CACHE_PAGE_SIZE 4096

long long int cache_off[CACHE_PAGES];
long long int cache_reads[CACHE_PAGES];
int cache_write[CACHE_PAGES];
unsigned char cache_data[CACHE_PAGES*CACHE_PAGE_SIZE];
void cache_init(void)
{
	int i;
	i=0;
	while(i<CACHE_PAGES)
	{
		cache_off[i]=-1;
		cache_reads[i]=0;
		cache_write[i]=0;
		++i;
	}
}
void cache_flush(void)
{
	int i;
	i=0;
	while(i<CACHE_PAGES)
	{
		if(cache_off[i]!=-1&&cache_write[i]!=0)
		{
			raw_block_write(cache_data+i*CACHE_PAGE_SIZE,cache_off[i],CACHE_PAGE_SIZE);
		}
		cache_off[i]=-1;
		cache_reads[i]=0;
		cache_write[i]=0;
		++i;
	}
}
int cache_load(long long off)
{
	int i,i1;
	long long int reads;
	off=off/CACHE_PAGE_SIZE*CACHE_PAGE_SIZE;
	i=0;
	while(i<CACHE_PAGES)
	{
		if(cache_off[i]==off)
		{
			return i;
		}
		++i;
	}
	i=0;
	while(i<CACHE_PAGES)
	{
		if(cache_off[i]==-1)
		{
			cache_off[i]=off;
			cache_reads[i]=0;
			cache_write[i]=0;
			raw_block_read(cache_data+i*CACHE_PAGE_SIZE,cache_off[i],CACHE_PAGE_SIZE);
			return i;
		}
		++i;
	}
	reads=cache_reads[0];
	i1=0;
	if(cache_write[0]==0)
	{
		reads=-1;
	}
	i=0;
	while(i<CACHE_PAGES)
	{
		if(cache_reads[i]<reads)
		{
			i1=i;
			reads=cache_reads[i];
		}
		if(cache_write[i]==0)
		{
			i1=i;
			reads=-1;
			break;
		}
		++i;
	}
	if(reads!=-1)
	{
		raw_block_write(cache_data+i1*CACHE_PAGE_SIZE,cache_off[i1],CACHE_PAGE_SIZE);
	}
	cache_off[i1]=off;
	cache_reads[i1]=0;
	cache_write[i1]=0;
	raw_block_read(cache_data+i1*CACHE_PAGE_SIZE,cache_off[i1],CACHE_PAGE_SIZE);
	return i1;
}
void block_read(void *buf,long long off,int size)
{
	int i;
	int size1;
	long long off1;
	while(size>0)
	{
		i=cache_load(off);
		size1=cache_off[i]+CACHE_PAGE_SIZE-off;
		if(size1>size)
		{
			size1=size;
		}
		off1=off-cache_off[i];
		memcpy(buf,cache_data+i*CACHE_PAGE_SIZE+off1,size1);
		size-=size1;
		off+=size1;
		buf=(char *)buf+size1;
		++cache_reads[i];
	}
}
void block_write(void *buf,long long off,int size)
{
	int i;
	int size1;
	long long off1;
	while(size>0)
	{
		i=cache_load(off);
		size1=cache_off[i]+CACHE_PAGE_SIZE-off;
		if(size1>size)
		{
			size1=size;
		}
		off1=off-cache_off[i];
		memcpy(cache_data+i*CACHE_PAGE_SIZE+off1,buf,size1);
		size-=size1;
		off+=size1;
		buf=(char *)buf+size1;
		cache_write[i]=1;
	}
}

void block_swap(long long b1,long long b2,int size)
{
	static char buf1[4096],buf2[4096];
	while(size)
	{
		if(size>4096)
		{
			block_read(buf1,b1,4096);
			block_read(buf2,b2,4096);
			block_write(buf1,b2,4096);
			block_write(buf2,b1,4096);
			size-=4096;
		}
		else
		{
			block_read(buf1,b1,size);
			block_read(buf2,b2,size);
			block_write(buf1,b2,size);
			block_write(buf2,b1,size);
			size=0;
		}
		b1+=4096;
		b2+=4096;
	}
}
int if_in_range(long long al,long long ar,long long bl,long long br)
{
	if(bl>=al&&bl<ar)
	{
		return 1;
	}
	if(br>al&&br<=ar)
	{
		return 1;
	}
	if(al>=bl&&al<br)
	{
		return 1;
	}
	return 0;
}
int if_block_used_by_ext2(long long off,long long size);
unsigned int ext2_blocks_required(unsigned long long int size);
struct ext2_superblock sb;
void msg(int fd,char *str)
{
	write(fd,str,strlen(str));
}
#include "fat32_fs.c"
#include "ext2_fs.c"
int main(int argc,char **argv)
{
	int sig;
	int cluster;
	// in fat32_fs.c
	fat_cache_off=-1;
	// in ext2_fs.c
	bbitmap_group=0xffffffff;
	ibitmap_group=0xffffffff;
	if(argc<2)
	{
		msg(1,"Usage: fat32_to_ext2 <Partition>\n");
		return 1;
	}
	fd=open(argv[1],2,0);
	if(fd<0)
	{
		msg(2,"cannot open device\n");
		return 1;
	}
	if(ext2_init_fs_info())
	{
		return 1;
	}
	cache_init();
	if(load_bpb())
	{
		msg(2,"device does not contain FAT32 filesystem,\nfilesystem not changed.\n");
		cache_flush();
		return 1;
	}
	fat_blocks_size=(fat32_data_off-fat32_fat_off-1>>12)+5<<12;
	fat_blocks=mmap(0,fat_blocks_size,3,0x22,-1,0);
	if(!valid(fat_blocks))
	{
		msg(2,"cannot allocate memory,\nfilesystem not changed.\n");
		cache_flush();
		return 1;
	}
	fat_blocks_off=fat32_fat_off-4096&0xfffffffffffff000;
	struct fat32_pointer ptr;
	fat32_pointer_init(bpb.root_cluster,0xffffffff,&ptr);
	if(fat32_scan(&ptr,1,0))
	{
		cache_flush();
		return 1;
	}
	sig=1;
	while(sig<64)
	{
		signal(sig,SIG_IGN);
		++sig;
	}
	msg(1,"Start changing filesystem\n");
	fat32_move_stage=0;
	if((cluster=fat32_move_dir(bpb.root_cluster,1,0))<0)
	{
		fat32_fat_sync();
		msg(2,"cannot allocate free block,\nfilesystem CHANGED.\n");
		cache_flush();
		return 1;
	}
	bpb.root_cluster=cluster;
	fat32_move_stage=1;
	do
	{
		fat32_move_status_old=fat32_move_status;
		fat32_move_status=0;
		cluster=fat32_move_dir(bpb.root_cluster,1,0);
		bpb.root_cluster=cluster;
		if(fat32_move_status_old<=fat32_move_status&&fat32_move_status_old)
		{
			fat32_fat_sync();
			msg(2,"cannot allocate free block,\nfilesystem CHANGED.\n");
			cache_flush();
			return 1;
		}
	}
	while(fat32_move_status);
	fat32_move_stage=2;
	do
	{
		fat32_move_status_old=fat32_move_status;
		fat32_move_status=0;
		cluster=fat32_move_dir(bpb.root_cluster,1,0);
		if(fat32_move_status_old<=fat32_move_status&&fat32_move_status_old)
		{
			fat32_fat_sync();
			msg(2,"cannot allocate free block,\nfilesystem CHANGED.\n");
			cache_flush();
			return 1;
		}
		bpb.root_cluster=cluster;
	}
	while(fat32_move_status);
	fat32_fat_sync();
	msg(1,"Start converting\n");
	block_read(fat_blocks,fat_blocks_off,fat_blocks_size);
	stage=1;
	ext2_init_bitmap();
	ext2_mask_used_blocks(bpb.root_cluster);
	if(ext2_map_root())
	{
		msg(2,"failed to allocate block or inode,\nfilesystem CHANGED.\n");
		cache_flush();
		return 1;
	}
	ext2_unmask_fat32_dir(bpb.root_cluster);
	ext2_sync();
	cache_flush();
	close(fd);
	msg(1,"Complete!\n");
	return 0;
}
