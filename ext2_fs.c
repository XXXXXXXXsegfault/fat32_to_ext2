
struct ext2_bgdt *gdt;
unsigned long long groups,gt_blocks;
unsigned long long size,blocks;
unsigned int last_group_size;

int ext2_init_fs_info(void)
{
	unsigned long x;
	unsigned int nb[3];
	int has_super;
	sb.block_size=2;
	sb.frag_size=2;
	sb.blocks_per_group=32768;
	sb.frags_per_group=32768;
	sb.inodes_per_group=8192;
	sb.max_mounts=0xffff;
	sb.magic=0xef53;
	sb.state=1;
	sb.errors=1;
	sb.rev=1;
	sb.first_ino=11;
	sb.inode_size=256;
	sb.feature_incompat=2;
	sb.feature_ro_compat=3;
	getrandom(sb.uuid,16,1);
	size=lseek(fd,0,2);
	if(!valid(size))
	{
		return 1;
	}
	blocks=size>>12;
	if(blocks==0)
	{
		return 1;
	}
	last_group_size=blocks&32767;
	if(last_group_size==0)
	{
		last_group_size=32768;
	}
	groups=blocks+32767>>15;
	if(groups==0)
	{
		return 1;
	}
	gt_blocks=groups+127>>7;
	x=1;
	nb[0]=1;
	nb[1]=5;
	nb[2]=7;
	has_super=1;
	while(x<groups)
	{
		if(x==nb[0])
		{
			has_super=1;
			nb[0]*=3;
		}
		else if(x==nb[1])
		{
			has_super=1;
			nb[1]*=5;
		}
		else if(x==nb[2])
		{
			has_super=1;
			nb[2]*=7;
		}
		else
		{
			has_super=0;
		}
		++x;
	}
	if(has_super)
	{
		if(gt_blocks+(1+1+1+512+1)>last_group_size)
		{
			blocks-=last_group_size;
			last_group_size=32768;
			--groups;
		}
	}
	else
	{
		if(1+1+512+1>last_group_size)
		{
			blocks-=last_group_size;
			last_group_size=32768;
			--groups;
		}
	}
	if(groups==0||blocks>0xffffffff)
	{
		return 1;
	}
	gt_blocks=groups+127>>7;
	gdt=mmap(0,gt_blocks<<12,3,0x22,-1,0);
	if(!valid(gdt))
	{
		return 1;
	}

	sb.blocks=blocks;
	sb.inodes=groups*8192;
	sb.free_inodes=sb.inodes-10;

	gdt[0].free_inodes=8192-10;
	gdt[0].block_bitmap=1+gt_blocks;
	gdt[0].inode_bitmap=1+gt_blocks+1;
	gdt[0].inode_table=1+gt_blocks+1+1;
	gdt[0].used_dirs=1;

	if(groups==1)
	{
		gdt[0].free_blocks=last_group_size-(1+gt_blocks+1+1+512);
	}
	else
	{
		gdt[0].free_blocks=32768-(1+gt_blocks+1+1+512);
	}
	sb.free_blocks=gdt[0].free_blocks;

	x=1;
	nb[0]=1;
	nb[1]=5;
	nb[2]=7;
	while(x<groups)
	{
		if(x==nb[0])
		{
			has_super=1;
			nb[0]*=3;
		}
		else if(x==nb[1])
		{
			has_super=1;
			nb[1]*=5;
		}
		else if(x==nb[2])
		{
			has_super=1;
			nb[2]*=7;
		}
		else
		{
			has_super=0;
		}
		gdt[x].free_inodes=8192;
		if(has_super)
		{
			gdt[x].block_bitmap=(x<<15)+1+gt_blocks;
			if(x==groups-1)
			{
				gdt[x].free_blocks=last_group_size-(1+gt_blocks+1+1+512);
			}
			else
			{
				gdt[x].free_blocks=32768-(1+gt_blocks+1+1+512);
			}
		}
		else
		{
			gdt[x].block_bitmap=x<<15;
			if(x==groups-1)
			{
				gdt[x].free_blocks=last_group_size-(1+1+512);
			}
			else
			{
				gdt[x].free_blocks=32768-(1+1+512);
			}
		}
		gdt[x].inode_bitmap=gdt[x].block_bitmap+1;
		gdt[x].inode_table=gdt[x].inode_bitmap+1;
		sb.free_blocks+=gdt[x].free_blocks;
		++x;
	}
	sb.r_blocks=sb.free_blocks/128;
	return 0;
}
int if_block_used_by_ext2(long long off,long long size)
{
	long long int x;
	struct ext2_bgdt *gdt_ent;
	long long a;
	unsigned int sb_block[3];
	int has_super;
	sb_block[0]=1;
	sb_block[1]=5;
	sb_block[2]=7;
	x=0;
	while(x<groups)
	{
		has_super=0;
		if(x==0)
		{
			has_super=1;
		}
		else if(x==sb_block[0])
		{
			sb_block[0]*=3;
			has_super=1;
		}
		else if(x==sb_block[1])
		{
			sb_block[1]*=5;
			has_super=1;
		}
		else if(x==sb_block[2])
		{
			sb_block[2]*=7;
			has_super=1;
		}
		gdt_ent=gdt+x;
		a=gdt_ent->block_bitmap;
		if(if_in_range(off,off+size,a<<12,a+1<<12))
		{
			return 1;
		}
		a=gdt_ent->inode_bitmap;
		if(if_in_range(off,off+size,a<<12,a+1<<12))
		{
			return 1;
		}
		a=gdt_ent->inode_table;
		if(if_in_range(off,off+size,a<<12,a+512<<12))
		{
			return 1;
		}
		if(has_super)
		{
			a=x*32768;
			if(if_in_range(off,off+size,a<<12,a+gt_blocks+1<<12))
			{
				return 1;
			}
		}
		++x;
	}
	return 0;
}
unsigned int ext2_blocks_required(unsigned long long int size)
{
	unsigned int blocks;
	unsigned int ret;
	unsigned int iblocks;
	blocks=(size-1>>12)+1;
	ret=0;
	if(size==0)
	{
		return 0;
	}
	if(blocks<=12)
	{
		ret=blocks;
	}
	else if(blocks<=12+1024)
	{
		ret=blocks+1;
	}
	else if(blocks<=12+1024+1024*1024)
	{
		ret=blocks+1+1+(blocks-(12+1024)-1)/1024+1;
	}
	else
	{
		iblocks=(blocks-(12+1024+1024*1024)-1)/1024+1;
		ret=blocks+1+1+1024+1+iblocks+(iblocks-1)/1024+1;
	}
	return ret;
}
void ext2_block_read(void *buf,unsigned int index)
{
	long long off;
	off=index;
	off<<=12;
	if(off>=fat_blocks_off&&off<fat_blocks_off+fat_blocks_size)
	{
		memcpy(buf,fat_blocks+off-fat_blocks_off,4096);
		return;
	}
	block_read(buf,off,4096);
}
void ext2_block_write(void *buf,unsigned int index)
{
	long long off;
	off=index;
	off<<=12;
	if(off>=fat_blocks_off&&off<fat_blocks_off+fat_blocks_size)
	{
		memcpy(fat_blocks+off-fat_blocks_off,buf,4096);
		return;
	}
	block_write(buf,off,4096);
}
void bitmap_set(unsigned char *bitmap,int n)
{
	int x;
	x=0;
	while(n>=8)
	{
		bitmap[x]=0xff;
		n-=8;
		++x;
	}
	bitmap[x]|=(1<<n)-1;
}
void bitmap_set2(unsigned char *bitmap,int n)
{
	int x;
	x=4096;
	while(n>=8)
	{
		--x;
		bitmap[x]=0xff;
		n-=8;
	}
	--x;
	bitmap[x]|=0x100-(1<<8-n);
}
void ext2_init_bitmap(void)
{
	int x,y;
	unsigned int index;
	static unsigned char buf[4096];
	static unsigned char buf2[4096];
	static unsigned char buf3[4096];
	x=0;
	memset(buf+1024,0xff,3072);
	while(x<groups)
	{
		index=gdt[x].inode_bitmap;
		if(x==0)
		{
			buf[0]=0xff;
			buf[1]=0x03;
		}
		else
		{
			buf[0]=0x00;
			buf[1]=0x00;
		}
		ext2_block_write(buf,index);
		index=gdt[x].block_bitmap;
		memset(buf2,0,4096);
		if(x==groups-1)
		{
			bitmap_set(buf2,last_group_size-gdt[x].free_blocks);
			bitmap_set2(buf2,32768-last_group_size);
		}
		else
		{
			bitmap_set(buf2,32768-gdt[x].free_blocks);
		}
		ext2_block_write(buf2,index);
		index=gdt[x].inode_table;
		y=0;
		while(y<512)
		{
			ext2_block_write(buf3,index+y);
			++y;
		}
		++x;
	}
}
unsigned char bbitmap_cache[4096],ibitmap_cache[4096];
unsigned int bbitmap_group,ibitmap_group;
void ext2_block_bitmap_set(int block)
{
	unsigned int group;
	unsigned int index;
	group=block/32768;
	block%=32768;
	if(group!=bbitmap_group)
	{
		if(bbitmap_group!=0xffffffff)
		{
			index=gdt[bbitmap_group].block_bitmap;
			ext2_block_write(bbitmap_cache,index);
		}
		index=gdt[group].block_bitmap;
		ext2_block_read(bbitmap_cache,index);
		bbitmap_group=group;
	}
	--gdt[group].free_blocks;
	--sb.free_blocks;
	bbitmap_cache[block>>3]|=1<<(block&7);
}
void ext2_block_bitmap_clr(int block)
{
	unsigned int group;
	unsigned int index;
	group=block/32768;
	block%=32768;
	if(group!=bbitmap_group)
	{
		if(bbitmap_group!=0xffffffff)
		{
			index=gdt[bbitmap_group].block_bitmap;
			ext2_block_write(bbitmap_cache,index);
		}
		index=gdt[group].block_bitmap;
		ext2_block_read(bbitmap_cache,index);
		bbitmap_group=group;
	}
	++gdt[group].free_blocks;
	++sb.free_blocks;
	bbitmap_cache[block>>3]&=~(1<<(block&7));
}
int ext2_block_bitmap_get(int block)
{
	unsigned int group;
	unsigned int index;
	group=block/32768;
	block%=32768;
	if(group!=bbitmap_group)
	{
		if(bbitmap_group!=0xffffffff)
		{
			index=gdt[bbitmap_group].block_bitmap;
			ext2_block_write(bbitmap_cache,index);
		}
		index=gdt[group].block_bitmap;
		ext2_block_read(bbitmap_cache,index);
		bbitmap_group=group;
	}
	return bbitmap_cache[block>>3]&1<<(block&7);
}
void ext2_inode_bitmap_set(int inode)
{
	unsigned int group;
	unsigned int index;
	group=(inode-1)/8192;
	inode=(inode-1)%8192;
	if(group!=ibitmap_group)
	{
		if(ibitmap_group!=0xffffffff)
		{
			index=gdt[ibitmap_group].inode_bitmap;
			ext2_block_write(ibitmap_cache,index);
		}
		index=gdt[group].inode_bitmap;
		ext2_block_read(ibitmap_cache,index);
		ibitmap_group=group;
	}
	--gdt[group].free_inodes;
	--sb.free_inodes;
	ibitmap_cache[inode>>3]|=1<<(inode&7);
}
int ext2_inode_bitmap_get(int inode)
{
	unsigned int group;
	unsigned int index;
	group=(inode-1)/8192;
	inode=(inode-1)%8192;
	if(group!=ibitmap_group)
	{
		if(ibitmap_group!=0xffffffff)
		{
			index=gdt[ibitmap_group].inode_bitmap;
			ext2_block_write(ibitmap_cache,index);
		}
		index=gdt[group].inode_bitmap;
		ext2_block_read(ibitmap_cache,index);
		ibitmap_group=group;
	}
	return ibitmap_cache[inode>>3]&1<<(inode&7);
}
void ext2_mask_used_blocks_file(int cluster,unsigned int size)
{
	long long off;
	int x;
	unsigned int s;
	int align,blocks_per_cluster;
	s=0;
	if(fat32_cluster_size<4096)
	{
		align=4096/fat32_cluster_size;
		while(cluster<=0xffffff6&&s<size)
		{
			off=fat_block_off(cluster);
			ext2_block_bitmap_set(off>>12);
			x=0;
			while(cluster<=0xffffff6&&x<align)
			{
				cluster=fat32_fat_next(cluster);
				++x;
			}
			s+=4096;
		}
	}
	else
	{
		blocks_per_cluster=fat32_cluster_size/4096;
		while(cluster<=0xffffff6&&s<size)
		{
			off=fat_block_off(cluster);
			x=0;
			while(x<blocks_per_cluster&&s<size)
			{
				ext2_block_bitmap_set((off>>12)+x);
				++x;
				s+=4096;
			}
			cluster=fat32_fat_next(cluster);
		}
	}
}
void ext2_mask_used_blocks(int cluster)
{
	struct fat32_pointer ptr;
	char name[512];
	unsigned char dirent[32];
	int cl;
	unsigned int size;
	ext2_mask_used_blocks_file(cluster,0xffffffff);
	fat32_pointer_init(cluster,0xffffffff,&ptr);
Retry:
	while(fat32_readdir(&ptr,name,dirent))
	{
		if(dirent[11]&0x08)
		{
			goto Retry;
		}
		if(strcmp(name,".")&&strcmp(name,".."))
		{
			memcpy(&cl,dirent+20,2);
			cl<<=16;
			memcpy(&cl,dirent+26,2);
			if(dirent[11]&0x10) //directory
			{
				ext2_mask_used_blocks(cl);
			}
			else if(!(dirent[11]&0x08)) //file
			{
				memcpy(&size,dirent+28,4);
				ext2_mask_used_blocks_file(cl,size);
			}
		}
	}
}
unsigned int ext2_block_alloc(int group)
{
	unsigned int x,x1;
	x=group*32768;
	x1=x;
	do
	{
		if(!ext2_block_bitmap_get(x1))
		{
			ext2_block_bitmap_set(x1);
			return x1;
		}
		++x1;
		if(x1>=sb.blocks)
		{
			x1=0;
		}
	}
	while(x1!=x);
	return 0;
}
unsigned int ext2_inode_alloc(int group)
{
	unsigned int x,x1;
	x=group*8192+1;
	x1=x;
	do
	{
		if(!ext2_inode_bitmap_get(x1))
		{
			ext2_inode_bitmap_set(x1);
			return x1;
		}
		++x1;
		if(x1>sb.inodes)
		{
			x1=1;
		}
	}
	while(x1!=x);
	return 0;
}
int ext2_map_block(int group,struct ext2_inode *inode,unsigned int index,unsigned int block)
{
	int i1,i2;
	unsigned int map[1024];
	unsigned int map2[1024];
	if(block>=sb.blocks)
	{
		return -1;
	}
	if(index<12)
	{
		inode->block[index]=block;
		return 0;
	}
	index-=12;
	if(index<1024)
	{
		if(inode->block[12]==0)
		{
			i1=ext2_block_alloc(group);
			if(i1==0)
			{
				return -1;
			}
			inode->block[12]=i1;
			memset(map,0,4096);
		}
		else
		{
			i1=inode->block[12];
			ext2_block_read(map,i1);
		}
		map[index]=block;
		ext2_block_write(map,i1);
		return 0;
	}
	// In FAT32, a single file cannot be larger than 4GB, so the triply indirect block pointer is never used.
	index-=1024;
	if(inode->block[13]==0)
	{
		i1=ext2_block_alloc(group);
		if(i1==0)
		{
			return -1;
		}
		inode->block[13]=i1;
		memset(map,0,4096);
	}
	else
	{
		i1=inode->block[13];
		ext2_block_read(map,i1);
	}
	if(map[index>>10]==0)
	{
		i2=ext2_block_alloc(group);
		if(i2==0)
		{
			ext2_block_bitmap_clr(i1);
			return -1;
		}
		map[index>>10]=i2;
		memset(map2,0,4096);
	}
	else
	{
		i2=map[index>>10];
		ext2_block_read(map2,i2);
	}
	map2[index&1023]=block;
	ext2_block_write(map2,i2);
	ext2_block_write(map,i1);
	return 0;
}
int ext2_map_file(int cluster,int group,struct ext2_inode *inode)
{
	long long off;
	unsigned int index;
	int x;
	unsigned int size;
	int align,blocks_per_cluster;
	index=0;
	size=0;
	if(fat32_cluster_size<4096)
	{
		align=4096/fat32_cluster_size;
		while(cluster<=0xffffff6&&size<inode->size)
		{
			off=fat_block_off(cluster);
			if(ext2_map_block(group,inode,index,off>>12))
			{
				return -1;
			}
			size+=4096;
			++index;
			x=0;
			while(cluster<=0xffffff6&&x<align)
			{
				cluster=fat32_fat_next(cluster);
				++x;
			}
		}
	}
	else
	{
		blocks_per_cluster=fat32_cluster_size/4096;
		while(cluster<=0xffffff6&&size<inode->size)
		{
			off=fat_block_off(cluster);
			x=0;
			while(x<blocks_per_cluster&&size<inode->size)
			{
				if(ext2_map_block(group,inode,index,(off>>12)+x))
				{
					return -1;
				}
				size+=4096;
				++index;
				++x;
			}
			cluster=fat32_fat_next(cluster);
		}
	}
	return 0;
}
int ext2_push_dir(unsigned char *buf,char *name,int inode,int filetype,int init)
{
	int l,total,l1,l2;
	struct ext2_directory *dir;
	int x;
	l=strlen(name);
	x=0;
	total=(l-1>>2)+3<<2;
	if(!init)
	{
		while(x<4096)
		{
			dir=(void *)(buf+x);
			l1=(dir->name_len-1>>2)+3<<2;
			if(total+l1<=dir->rec_len)
			{
				l2=dir->rec_len;
				dir->rec_len=l1;
				x+=l1;
				l1=l2-l1;
				goto X1;
			}
			x+=dir->rec_len;
		}
		return -1;
	}
	else
	{
		l1=4096;
	}
X1:
	dir=(void *)(buf+x);
	dir->rec_len=l1;
	dir->name_len=l;
	dir->file_type=filetype;
	dir->inode=inode;
	memcpy(dir->file_name,name,l);
	return 0;
}
void ext2_write_inode(unsigned int ino,struct ext2_inode *inode)
{
	int group;
	long long off,block;
	static char buf[4096];
	group=(ino-1)/8192;
	off=gdt[group].inode_table;
	off<<=12;
	off+=(ino-1)%8192*256;
	block=off>>12;
	off&=4095;
	ext2_block_read(buf,block);
	memcpy(buf+off,inode,sizeof(*inode));
	ext2_block_write(buf,block);
}
long long fat32_date_to_unix(unsigned short *ptr)
{
	long long int year,month,day,val,result;
	int days_in_month[12];
	result=0;
	year=(*ptr>>9)+1980;
	month=(*ptr>>5&0xf)-1;
	day=(*ptr&0x1f)-1;
	val=1970;
	days_in_month[0]=31;
	days_in_month[1]=28;
	days_in_month[2]=31;
	days_in_month[3]=30;
	days_in_month[4]=31;
	days_in_month[5]=30;
	days_in_month[6]=31;
	days_in_month[7]=31;
	days_in_month[8]=30;
	days_in_month[9]=31;
	days_in_month[10]=30;
	days_in_month[11]=31;
	while(val<year)
	{
		if(val%400==0||val%4==0&&val%100!=0)
		{
			result+=366;
		}
		else
		{
			result+=365;
		}
		++val;
	}
	if(year%400==0||val%4==0&&val%100!=0)
	{
		days_in_month[1]=29;
	}
	val=0;
	while(val<month)
	{
		result+=days_in_month[val];
		++val;
	}
	result+=day;
	return result*86400;
}
long long fat32_time_to_unix(unsigned short *ptr)
{
	long long int hour,minute,second;
	hour=*ptr>>11;
	minute=*ptr>>5&0x3f;
	second=(*ptr&0x1f)*2;
	return hour*3600+minute*60+second;
}
int ext2_map_dir(int cluster,unsigned int ino,unsigned int prev_ino,struct ext2_inode *inode)
{
	struct fat32_pointer ptr;
	char name[512];
	unsigned char dirent[32];
	int cl;
	int group;
	unsigned int size,dir_size;
	struct ext2_inode new_inode;
	unsigned int dir_block;
	unsigned char dir_data[4096];
	unsigned int new_ino;
	unsigned long long crtime,atime,ctime;
	group=(ino-1)/8192;
	dir_size=4096;
	memset(dir_data,0,4096);
	dir_block=ext2_block_alloc(group);
	if(dir_block==0)
	{
		return 0;
	}
	if(ext2_map_block(group,inode,0,dir_block))
	{
		return 0;
	}
	ext2_push_dir(dir_data,".",ino,2,1);
	ext2_push_dir(dir_data,"..",prev_ino,2,0);
	fat32_pointer_init(cluster,0xffffffff,&ptr);
Retry:
	while(fat32_readdir(&ptr,name,dirent))
	{
		if(dirent[11]&0x08)
		{
			goto Retry;
		}
		if(strcmp(name,".")&&strcmp(name,".."))
		{
			memcpy(&cl,dirent+20,2);
			cl<<=16;
			memcpy(&cl,dirent+26,2);
			memset(&new_inode,0,128);
			atime=fat32_date_to_unix((unsigned short *)(dirent+18));
			ctime=fat32_date_to_unix((unsigned short *)(dirent+24))+fat32_time_to_unix((unsigned short *)(dirent+22));
			crtime=fat32_date_to_unix((unsigned short *)(dirent+16))+fat32_time_to_unix((unsigned short *)(dirent+14));
			new_inode.atime=atime;
			new_inode.ctime=ctime;
			new_inode.mtime=crtime;
			new_inode.crtime=crtime;
			new_inode.atime_extra=atime>>32&3;
			new_inode.ctime_extra=ctime>>32&3;
			new_inode.mtime_extra=crtime>>32&3;
			new_inode.crtime_extra=crtime>>32&3;
			new_ino=ext2_inode_alloc(group);
			if(new_ino==0)
			{
				return 0;
			}
			if(dirent[11]&0x10) //directory
			{
				new_inode.mode=040755;
				new_inode.links=2;
				++gdt[(new_ino-1)/8192].used_dirs;
				if((new_inode.size=ext2_map_dir(cl&0xfffffff,new_ino,ino,&new_inode))==0)
				{
					return 0;
				}
				if(ext2_push_dir(dir_data,name,new_ino,2,0))
				{
					ext2_block_write(dir_data,dir_block);
					dir_block=ext2_block_alloc(group);
					if(dir_block==0)
					{
						return 0;
					}
					if(ext2_map_block(group,inode,dir_size>>12,dir_block))
					{
						return 0;
					}
					dir_size+=4096;
					memset(dir_data,0,4096);
					ext2_push_dir(dir_data,name,new_ino,2,1);
				}
				++inode->links;
			}
			else if(!(dirent[11]&0x08)) //file
			{
				new_inode.mode=0100644;
				new_inode.links=1;
				memcpy(&size,dirent+28,4);
				new_inode.size=size;
				if(ext2_map_file(cl&0xfffffff,(new_ino-1)/8192,&new_inode))
				{
					return 0;
				}
				if(ext2_push_dir(dir_data,name,new_ino,1,0))
				{
					ext2_block_write(dir_data,dir_block);
					dir_block=ext2_block_alloc(group);
					if(dir_block==0)
					{
						return 0;
					}
					if(ext2_map_block(group,inode,dir_size>>12,dir_block))
					{
						return 0;
					}
					dir_size+=4096;
					memset(dir_data,0,4096);
					ext2_push_dir(dir_data,name,new_ino,1,1);
				}
			}
			new_inode.blocks=ext2_blocks_required(new_inode.size)*8;
			ext2_write_inode(new_ino,&new_inode);
		}
	}
	ext2_block_write(dir_data,dir_block);
	return dir_size;
}
int ext2_map_root(void)
{
	struct ext2_inode inode;
	memset(&inode,0,128);
	inode.mode=040755;
	inode.links=2;
	inode.size=ext2_map_dir(bpb.root_cluster,2,2,&inode);
	if(inode.size==0)
	{
		return -1;
	}
	inode.blocks=ext2_blocks_required(inode.size)*8;
	ext2_write_inode(2,&inode);
	return 0;
}
void ext2_unmask_fat32_file(int cluster)
{
	long long off;
	int x;
	int align,blocks_per_cluster;
	if(fat32_cluster_size<4096)
	{
		align=4096/fat32_cluster_size;
		while(cluster<=0xffffff6)
		{
			off=fat_block_off(cluster);
			ext2_block_bitmap_clr(off>>12);
			x=0;
			while(cluster<=0xffffff6&&x<align)
			{
				cluster=fat32_fat_next(cluster);
				++x;
			}
		}
	}
	else
	{
		blocks_per_cluster=fat32_cluster_size/4096;
		while(cluster<=0xffffff6)
		{
			off=fat_block_off(cluster);
			x=0;
			while(x<blocks_per_cluster)
			{
				ext2_block_bitmap_clr((off>>12)+x);
				++x;
			}
			cluster=fat32_fat_next(cluster);
		}
	}
}
void ext2_unmask_fat32_dir(int cluster)
{
	struct fat32_pointer ptr;
	char name[512];
	unsigned char dirent[32];
	int cl;
	ext2_unmask_fat32_file(cluster);
	fat32_pointer_init(cluster,0xffffffff,&ptr);
Retry:
	while(fat32_readdir(&ptr,name,dirent))
	{
		if(dirent[11]&0x08)
		{
			goto Retry;
		}
		if(strcmp(name,".")&&strcmp(name,".."))
		{
			memcpy(&cl,dirent+20,2);
			cl<<=16;
			memcpy(&cl,dirent+26,2);
			if(dirent[11]&0x10) //directory
			{
				ext2_unmask_fat32_dir(cl);
			}
		}
	}
}
void ext2_sync(void)
{
	unsigned int index;
	unsigned int sb_block[3];
	int has_super;
	unsigned int x;
	long long off;
	static unsigned char buf[4096];
	sb_block[0]=1;
	sb_block[1]=5;
	sb_block[2]=7;
	if(bbitmap_group!=0xffffffff)
	{
		index=gdt[bbitmap_group].block_bitmap;
		ext2_block_write(bbitmap_cache,index);
	}
	if(ibitmap_group!=0xffffffff)
	{
		index=gdt[ibitmap_group].inode_bitmap;
		ext2_block_write(ibitmap_cache,index);
	}
	x=0;
	while(x<groups)
	{
		has_super=0;
		if(x==0)
		{
			has_super=1;
		}
		else if(x==sb_block[0])
		{
			sb_block[0]*=3;
			has_super=1;
		}
		else if(x==sb_block[1])
		{
			sb_block[1]*=5;
			has_super=1;
		}
		else if(x==sb_block[2])
		{
			sb_block[2]*=7;
			has_super=1;
		}
		if(has_super)
		{
			off=x;
			off*=32768;
			memset(buf,0,4096);
			if(x==0)
			{
				memcpy(buf+1024,&sb,1024);
			}
			else
			{
				memcpy(buf,&sb,1024);
			}
			ext2_block_write(buf,off);
			index=0;
			while(index<gt_blocks)
			{
				ext2_block_write(gdt+index*128,off+1+index);
				++index;
			}
		}
		++x;
	}
	block_write(fat_blocks,fat_blocks_off,fat_blocks_size);
}
