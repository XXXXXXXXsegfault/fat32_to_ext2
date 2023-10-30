struct fat32_bpb bpb;
long long int fat32_sectors,fat32_sector_size,fat32_cluster_size;
long long int fat32_fat_off,fat32_data_off,fat32_fat_size,fat32_data_size;
int fat32_move_status;
int fat32_move_status_old;
int fat32_move_stage;
char *fat_blocks;
unsigned long long fat_blocks_size,fat_blocks_off;
int load_bpb(void)
{
	block_read(&bpb,0,512);
	if(bpb.jump[2]!=0x90||bpb.fat_size!=0||bpb.bootflag!=0xaa55||memcmp(bpb.fstype,"FAT32   ",8))
	{
		return 1;
	}
	fat32_sectors=bpb.sectors;
	if(bpb.sectors_large)
	{
		fat32_sectors=bpb.sectors_large;
	}
	fat32_sector_size=bpb.sector_size;
	fat32_cluster_size=fat32_sector_size*bpb.sectors_per_cluster;
	fat32_fat_off=(long long)bpb.reserved_sectors*fat32_sector_size;
	fat32_data_off=fat32_fat_off+fat32_sector_size*bpb.fat_size_large*bpb.fats;
	fat32_fat_size=fat32_sector_size*bpb.fat_size_large;
	fat32_data_size=(fat32_sectors*fat32_sector_size-fat32_data_off)/fat32_cluster_size;
	return 0;
}
int fat_cache[128];
int fat_cache_off;
int fat32_fat_next(int cluster)
{
	int block_off,ret;
	cluster&=0xfffffff;
	block_off=cluster>>7<<7;
	if(block_off!=fat_cache_off)
	{
		if(fat_cache_off!=-1&&!stage)
		{
			block_write(fat_cache,fat32_fat_off+fat_cache_off*4,512);
			block_write(fat_cache,fat32_fat_off+fat32_fat_size+fat_cache_off*4,512);
		}
		block_read(fat_cache,fat32_fat_off+block_off*4,512);
		fat_cache_off=block_off;
	}
	ret=fat_cache[cluster&0x7f]&0xfffffff;
	return ret;
}
void fat32_fat_write(int cluster,int val)
{
	int block_off,ret;
	cluster&=0xfffffff;
	block_off=cluster>>7<<7;
	if(block_off!=fat_cache_off)
	{
		if(fat_cache_off!=-1&&!stage)
		{
			block_write(fat_cache,fat32_fat_off+fat_cache_off*4,512);
			block_write(fat_cache,fat32_fat_off+fat32_fat_size+fat_cache_off*4,512);
		}
		block_read(fat_cache,fat32_fat_off+block_off*4,512);
		fat_cache_off=block_off;
	}
	fat_cache[cluster&0x7f]=val&0xfffffff;
}
void fat32_fat_sync(void)
{
	if(fat_cache_off!=-1)
	{
		block_write(fat_cache,fat32_fat_off+fat_cache_off*4,512);
		block_write(fat_cache,fat32_fat_off+fat32_fat_size+fat_cache_off*4,512);
		fat_cache_off=-1;
	}
	block_write(&bpb,0,512);
}
long long fat_block_off(int cluster)
{
	long long ret;
	cluster&=0xfffffff;
	if(cluster>0xffffff6)
	{
		return -1;
	}
	ret=fat32_data_off+(cluster-2)*fat32_cluster_size;
	if(ret/fat32_sector_size>=fat32_sectors)
	{
		return -1;
	}
	return ret;
}
int fat32_alloc_start;
int fat32_alloc(void)
{
	long long x,off,n;
	n=0;
	do
	{
		x=fat32_alloc_start;
		while(x<fat32_data_size)
		{
			off=fat_block_off(x+2);
			if(!if_block_used_by_ext2(off,fat32_cluster_size))
			{
				if(fat32_fat_next(x+2)==0)
				{
					return x+2;
				}
			}
			++x;
			if(x>fat32_alloc_start)
			{
				fat32_alloc_start=x;
			}
		}
		fat32_alloc_start=0;
		++n;
	}
	while(n<2);
	return -1;
}
int fat32_alloc_align(int align,int index)
{
	long long x,off,n;
	n=0;
	if(align==1)
	{
		return fat32_alloc();
	}
	do
	{
		x=fat32_alloc_start;
		while(x<fat32_data_size)
		{
			if((x+fat32_data_off/fat32_cluster_size)%align==index)
			{
				off=fat_block_off(x+2);
				if(!if_block_used_by_ext2(off-4096,8192))
				{
					if(fat32_fat_next(x+2)==0)
					{
						return x+2;
					}
				}
			}
			++x;
			if(x>fat32_alloc_start)
			{
				fat32_alloc_start=x;
			}
		}
		fat32_alloc_start=0;
		++n;
	}
	while(n<2);
	return -1;
}
int fat32_fat_find(int val)
{
	long long x,off;
	x=0;
	while(x<fat32_data_size)
	{
		off=fat_block_off(x+2);
		if(!if_block_used_by_ext2(off,fat32_cluster_size))
		{
			if(fat32_fat_next(x+2)==val)
			{
				return x+2;
			}
		}
		++x;
	}
	return -1;
}
int fat32_alloc_block(int align)
{
	long long x,off,x1;
	x=0;
	while(x<fat32_data_size)
	{
		if((x+fat32_data_off/fat32_cluster_size)%align)
		{
			off=fat_block_off(x+2);
			x1=0;
			while(x1<align)
			{
				if(!if_block_used_by_ext2(off,fat32_cluster_size))
				{
					if(fat32_fat_next(x+2)!=0)
					{
						break;
					}
				}
				else
				{
					break;
				}
				++x1;
				off+=fat32_cluster_size;
			}
			if(x1==align)
			{
				return x+2;
			}
		}
		++x;
	}
	return -1;
}
struct fat32_pointer
{
	unsigned int off;
	int current_cluster;
	unsigned int size;
};
#define fat32_pointer_init(cluster,sz,ptr) ((ptr)->off=0,(ptr)->size=(sz),(ptr)->current_cluster=(cluster))
int fat32_read(struct fat32_pointer *ptr,void *buf,int size)
{
	int size1,ret,s;
	long long off;
	ret=0;
	s=0;
	if(ptr->off>=ptr->size)
	{
		return 0;
	}
	while(!s&&size)
	{
		size1=fat32_cluster_size-ptr->off%fat32_cluster_size;
		if(size<size1)
		{
			size1=size;
			s=1;
		}
		if(ptr->off+size1>ptr->size)
		{
			size1=ptr->size-ptr->off;
			s=1;
		}
		off=fat_block_off(ptr->current_cluster);
		if(off==-1)
		{
			break;
		}
		block_read(buf,off+(ptr->off%fat32_cluster_size),size1);
		buf=(char *)buf+size1;
		ret+=size1;
		ptr->off+=size1;
		size-=size1;
		if(ptr->off%fat32_cluster_size==0&&size1)
		{
			ptr->current_cluster=fat32_fat_next(ptr->current_cluster);
		}
	}
	return ret;
}
int fat32_readdir(struct fat32_pointer *ptr,char *name,unsigned char *dir)
{
	unsigned char dirent[32];
	char buf[14];
	int x;
	char name_buf2[512];
	int cluster;
	cluster=ptr->current_cluster;
	while(fat32_read(ptr,dirent,32)==32)
	{
		if(!(dirent[0]==0||dirent[0]==5||dirent[0]==0xe5||dirent[0]==0x20))
		{
			goto X1;
		}
		cluster=ptr->current_cluster;
	}
	return 0;
X1:
	name[0]=0;
	buf[13]=0;
	while(1)
	{
		if(dirent[11]==0xf) // long entry
		{
			buf[0]=dirent[1];
			buf[1]=dirent[3];
			buf[2]=dirent[5];
			buf[3]=dirent[7];
			buf[4]=dirent[9];
			buf[5]=dirent[14];
			buf[6]=dirent[16];
			buf[7]=dirent[18];
			buf[8]=dirent[20];
			buf[9]=dirent[22];
			buf[10]=dirent[24];
			buf[11]=dirent[28];
			buf[12]=dirent[30];
			x=0;
			while(x<13)
			{
				if((buf[x]>=0x7f||buf[x]<0x20)&&buf[x]!=0)
				{
					buf[x]="0123456789ABCDEFGHIJKLMNOPQRSTUV"[(buf[x]&31)];
				}
				++x;
			}
			strcpy(name_buf2,buf);
			strcat(name_buf2,name);
			strcpy(name,name_buf2);
			cluster=ptr->current_cluster;
			if(fat32_read(ptr,dirent,32)!=32)
			{
				return 0;
			}
		}
		else
		{
			if(name[0]==0)
			{
				x=0;
				while(x<8)
				{
					if(dirent[x]==0x20)
					{
						break;
					}
					name[x]=dirent[x];
					++x;
				}
				if(dirent[8]!=0x20)
				{
					name[x]='.';
					name[x+1]=0;
					x=8;
					while(x<11)
					{
						if(dirent[x]==0x20)
						{
							break;
						}
						buf[x-8]=dirent[x];
						++x;
					}
					buf[x-8]=0;
					strcat(name,buf);
				}
				else
				{
					name[x]=0;
				}
			}
			memcpy(dir,dirent,32);
			break;
		}
	}
	return cluster;
}
unsigned long long int required_blocks;
unsigned long long int required_inodes;
int fat32_scan(struct fat32_pointer *ptr,int is_root,int depth)
{
	char name[512];
	unsigned char dirent[32];
	int cluster;
	unsigned int size,fat32_dir_size;
	struct fat32_pointer new_ptr;
	unsigned int dir_size;
	int l;
	fat32_dir_size=0;
	dir_size=24;
	++required_inodes;
	if(depth>128)
	{
		msg(2,"maximum directory depth exceeded,\nfilesystem not changed.\n");
		return 1;
	}
	while(fat32_readdir(ptr,name,dirent))
	{
		if(strcmp(name,".")&&strcmp(name,".."))
		{
			if(dirent[11]&0x10) //directory
			{
				memcpy(&cluster,dirent+20,2);
				cluster<<=16;
				memcpy(&cluster,dirent+26,2);
				memcpy(&size,dirent+28,4);
				fat32_pointer_init(cluster,0xffffffff,&new_ptr);
				if(fat32_scan(&new_ptr,0,depth+1))
				{
					return 1;
				}
			}
			else //file
			{
				memcpy(&size,dirent+28,4);
				required_blocks+=ext2_blocks_required(size);
				++required_inodes;
			}
			l=strlen(name);
			l=(l-1>>2)+1<<2;
			l+=8;
			if(dir_size>>12!=dir_size+l>>12)
			{
				dir_size=(dir_size>>12)+1<<12;
			}
			dir_size+=l;
		}
		fat32_dir_size+=fat32_cluster_size;
	}
	required_blocks+=(dir_size-1>>12)+1;
	if(is_root)
	{
		if(sb.free_inodes<required_inodes||sb.free_blocks<required_blocks*107/100)
		{
			msg(2,"insufficiant space on device,\nfilesystem not changed.\n");
			return 1;
		}
	}
	return 0;
}
int fat32_move_file(int cluster)
{
	int p,c,new_c,n;
	long long int off,off2;
	int align,index,base;

	int bp,bn;
	p=-1;
	c=cluster&0xfffffff;
	index=0;
	if(fat32_move_stage==0)
	{
		while(c<=0xffffff6)
		{
			off=fat_block_off(c);
			n=fat32_fat_next(c);
			if(if_block_used_by_ext2(off,fat32_cluster_size))
			{
				new_c=fat32_alloc();
				if(new_c==-1)
				{
					return -1;
				}
				off2=fat_block_off(new_c);
				block_swap(off,off2,fat32_cluster_size);
				if(p==-1)
				{
					cluster=new_c;
				}
				else
				{
					fat32_fat_write(p,new_c);
				}
				fat32_fat_write(c,0);
				fat32_fat_write(new_c,n);
				c=new_c;
			}
			p=c;
			c=n;
		}
	}
	else if(fat32_move_stage==1)
	{
		if(fat32_cluster_size<4096)
		{
			align=4096/fat32_cluster_size;
			while(c<=0xffffff6)
			{
				off=fat_block_off(c);
				n=fat32_fat_next(c);
				if((c-2+fat32_data_off/fat32_cluster_size)%align!=index)
				{
					new_c=fat32_alloc_align(align,index);
					if(new_c==-1)
					{
						new_c=fat32_alloc();
						++fat32_move_status;
					}
					if(new_c!=-1)
					{
						off2=fat_block_off(new_c);
						block_swap(off,off2,fat32_cluster_size);
						if(p==-1)
						{
							cluster=new_c;
						}
						else
						{
							fat32_fat_write(p,new_c);
						}
						fat32_fat_write(c,0);
						fat32_fat_write(new_c,n);
						c=new_c;
					}
					else
					{
						++fat32_move_status;
					}
				}
				p=c;
				c=n;
				++index;
				if(index==align)
				{
					index=0;
				}
			}
		}
	}
	else if(fat32_move_stage==2)
	{
		if(fat32_cluster_size<4096)
		{
			align=4096/fat32_cluster_size;
			while(c<=0xffffff6)
			{
				off=fat_block_off(c);
				n=fat32_fat_next(c);
				if(index==0)
				{
					base=c;
				}
				else if(base+index!=c)
				{
					if(fat32_fat_next(base+index)==0)
					{
						new_c=base+index;
						off2=fat_block_off(new_c);
						block_swap(off,off2,fat32_cluster_size);
						fat32_fat_write(p,new_c);
						fat32_fat_write(c,0);
						fat32_fat_write(new_c,n);
						c=new_c;
					}
					else
					{
						bp=fat32_fat_find(base+index);
						bn=fat32_fat_next(base+index);
						off2=fat_block_off(base+index);
						block_swap(off,off2,fat32_cluster_size);
						fat32_fat_write(p,base+index);
						fat32_fat_write(base+index,n);
						fat32_fat_write(bp,c);
						fat32_fat_write(c,bn);
						c=base+index;
					}
					++fat32_move_status;
				}
				p=c;
				c=n;
				++index;
				if(index==align)
				{
					index=0;
				}
			}
		}
	}
	return cluster;
}
int fat32_move_dir(int cluster,int is_root,int prev)
{
	int cl,cl1;
	if((cl=fat32_move_file(cluster))<0)
	{
		return -1;
	}
	if(is_root)
	{
		bpb.root_cluster=cl;
		block_write(&bpb,0,512);
		block_write(&bpb,fat32_sector_size*bpb.backup_bootsec,512);
	}
	struct fat32_pointer ptr;
	char name[512];
	unsigned char dirent[32];
	int cluster_prev;
	long long int off;
	fat32_pointer_init(cl,0xffffffff,&ptr);
Retry:
	while(cluster_prev=fat32_readdir(&ptr,name,dirent))
	{
		if(dirent[11]&0x08)
		{
			goto Retry;
		}
		if(!strcmp(name,"."))
		{
			cl1=cl;
			memcpy(dirent+26,&cl1,2);
			cl1>>=16;
			memcpy(dirent+20,&cl1,2);
			off=fat_block_off(cluster_prev)+(ptr.off-32)%fat32_cluster_size;
			block_write(dirent,off,32);
		}
		else if(!strcmp(name,".."))
		{
			if(prev)
			{
				cl1=prev;
				memcpy(dirent+26,&cl1,2);
				cl1>>=16;
				memcpy(dirent+20,&cl1,2);
				off=fat_block_off(cluster_prev)+(ptr.off-32)%fat32_cluster_size;
				block_write(dirent,off,32);
			}
		}
		else
		{
			memcpy(&cl1,dirent+20,2);
			cl1<<=16;
			memcpy(&cl1,dirent+26,2);
			if(dirent[11]&0x10) //directory
			{
				if(is_root)
				{
					if((cl1=fat32_move_dir(cl1,0,0))<0)
					{
						return -1;
					}
				}
				else
				{
					if((cl1=fat32_move_dir(cl1,0,cl))<0)
					{
						return -1;
					}
				}
			}
			else if(!(dirent[11]&0x08)) //file
			{
				if((cl1=fat32_move_file(cl1))<0)
				{
					return -1;
				}
			}
			memcpy(dirent+26,&cl1,2);
			cl1>>=16;
			memcpy(dirent+20,&cl1,2);
			off=fat_block_off(cluster_prev)+(ptr.off-32)%fat32_cluster_size;
			block_write(dirent,off,32);
		}
	}
	return cl;
}
