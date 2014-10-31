#include "reconstruction.h"
//#include "pagemap.h"
//#include "ssd.h"
//#include <crtdefs.h> 
//#include <stdio.h>
//#include <stdlib.h>
#include "raid_controller.h"
extern struct rec_info *rec_map;


int check_in_visit_table(struct raid *raid4,unsigned int dev_no,struct sub_request *sub)
{
	unsigned int id, bro_dev_no;
	int i,j,k,threshold,tmp; 
	int complete_flag;//visit table中是否已有指定项目

	complete_flag = 0;
	bro_dev_no=calc_brother_dev_no(raid4,dev_no);
	threshold = 10;	//visit count超过6即标识为热点数据
	id = sub->lpn/raid4->ssd[0]->node_size;
	for (i=0; i<raid4->ssd[dev_no]->visit_table_size; i++)
	{
		if (raid4->ssd[dev_no]->visit_table[i].count==-1)
		{
			continue;
		}
		if (raid4->ssd[dev_no]->visit_table[i].id == id)
		{
			raid4->ssd[dev_no]->visit_table[i].count++;
			if (raid4->ssd[dev_no]->visit_table[i].count >= threshold)
			{
				restore(raid4,bro_dev_no,id); //此函数先检查是否已经转存，否则转存到备份盘
				raid4->ssd[dev_no]->visit_table[i].id=0;
				raid4->ssd[dev_no]->visit_table[i].count=-1;
			}
			complete_flag = 1;
			return i;
		}
	} 

	if (complete_flag == 0 ) //在visit table中没有找到该id，寻找visit次数较小的换出
	{
		for (j=0; j<threshold; j++)
		{
			tmp=j;
			if (tmp==0)
			{
				tmp=-1;
			}
			for (k=1; k<raid4->ssd[0]->visit_table_size; k++)
			{
				raid4->ssd[dev_no]->visit_index = (raid4->ssd[dev_no]->visit_index+1)%512;
				if(raid4->ssd[dev_no]->visit_table[raid4->ssd[dev_no]->visit_index].count == tmp)
				{
					raid4->ssd[dev_no]->visit_table[raid4->ssd[dev_no]->visit_index].id = id;
					raid4->ssd[dev_no]->visit_table[raid4->ssd[dev_no]->visit_index].count = 1;
					return raid4->ssd[dev_no]->visit_index;
				}

			}
		}

	}
}


struct restore_node *restore(struct raid *raid4,unsigned int dev_no,unsigned int id)	//dev_no为restore目标盘
{
	struct restore_node *node,*new_node;
	struct sub_request* sub1,*sub2;
	unsigned int lpn;
	unsigned int orig_dev_no;
	int sub_size,sub_state;
	int flag;

	flag = 0;	//指示该stripe是否在restore list中
	node = raid4->ssd[dev_no]->restore_list;
	orig_dev_no=calc_dev_no(raid4,dev_no);


	while (node != NULL)	//从前往后检索list中是否存在该node，若存在flag=1;
	{
		if (node->state==1&&node->id==id)
		{
			//放到队首,先删除，再插入，头尾的情况需要特殊处理
			if (node==raid4->ssd[dev_no]->restore_list)	//头
			{

			}
			else if (node==raid4->ssd[dev_no]->restore_list->pre_node)	//尾
			{
				raid4->ssd[dev_no]->restore_list=node;				
			}
			else
			{
				node->pre_node->next_node = node->next_node;				
				node->next_node->pre_node = node->pre_node;

				node->pre_node = raid4->ssd[dev_no]->restore_list->pre_node;
				node->next_node = raid4->ssd[dev_no]->restore_list;

				node->pre_node->next_node= node;
				node->next_node->pre_node = node;

				raid4->ssd[dev_no]->restore_list = node;
			}

			flag = 1;
			return node;
		}
		else
			if (node->next_node!=raid4->ssd[dev_no]->restore_list)
			{
				node = node->next_node;
			}
			else
				break;

	}
	if (flag==0)	//restore list中没有找到，把该请求所在的zone的数据全部restore到brother的数据盘上
	{
		for (lpn=id*raid4->data_disks+orig_dev_no; lpn<id*raid4->data_disks+raid4->ssd[0]->node_size; lpn=lpn+raid4->data_disks)
		{
			if (raid4->ssd[orig_dev_no]->dram->map->map_entry[lpn].state>0)
			{
				sub_state=raid4->ssd[orig_dev_no]->dram->map->map_entry[lpn].state;
				sub_size=size(sub_state);
				sub1=creat_sub_request(raid4->ssd[orig_dev_no],lpn,sub_size,sub_state,NULL,READ);
				sub2=creat_sub_request(raid4->ssd[dev_no],lpn,sub_size,sub_state,NULL,WRITE);
				sub2->next_pre_read=sub1;

				//把请求放入ssd的restore_req队列
				if (raid4->ssd[dev_no]->restore_req==NULL)
				{
					raid4->ssd[dev_no]->restore_req=sub2;
				}
				else	//在这里，为了简化起见，共用了next_rec指针用于指向restore req链表中下一个请求
				{
					sub2->next_rec=raid4->ssd[dev_no]->restore_req;
					raid4->ssd[dev_no]->restore_req=sub2;
				}
			}
		}
	}

	//如果list中没找到且list未满，则需要为该stripe新建一个node并放入队首；
	if (flag == 0)
	{
		//restore list满了，则需要腾出一个空间
		if (raid4->ssd[dev_no]->list_count == raid4->ssd[dev_no]->max_list_count)
		{
			start_GC(raid4,dev_no);	
		}

		new_node = (struct restore_node*)malloc(sizeof(struct restore_node));
		alloc_assert(new_node,"new_node");
		memset(new_node,0,sizeof(struct restore_node));

		new_node->next_node =NULL;
		new_node->state = 1;
		new_node->id= id;

		raid4->ssd[dev_no]->list_count++;

		if (raid4->ssd[dev_no]->restore_list==NULL)	//list为空
		{
			new_node->next_node=new_node;
			new_node->pre_node=new_node;
			raid4->ssd[dev_no]->restore_list=new_node;
		}
		else
		{
			new_node->next_node = raid4->ssd[dev_no]->restore_list;	
			new_node->pre_node = raid4->ssd[dev_no]->restore_list->pre_node;
			new_node->next_node->pre_node = new_node;
			new_node->pre_node->next_node = new_node;
			raid4->ssd[dev_no]->restore_list = new_node;
		}
	}


}



int start_GC(struct raid* raid4,unsigned int dev_no)	//删除最后一个节点，并在相应ssd中把对应的数据标记为无效
{
	struct restore_node * tail,*node;
	struct local *location;
	struct direct_erase * new_direct_erase;
	unsigned int start_lpn,lpn,ppn;
	struct direct_erase *direct_erase_node;
	struct gc_operation *gc_node;
	unsigned int channel,chip,die,plane;
	unsigned int orig_dev_no;

	tail = raid4->ssd[dev_no]->restore_list->pre_node;
	node = tail;
	orig_dev_no=calc_dev_no(raid4,dev_no);

	start_lpn = node->id*raid4->ssd[0]->node_size + orig_dev_no ; 
	for (lpn=start_lpn; lpn<=start_lpn+raid4->ssd[0]->node_size; lpn=lpn+raid4->data_disks)
	{
		if (raid4->ssd[dev_no]->dram->map->map_entry[lpn].state!=0)
		{
			ppn = raid4->ssd[0]->dram->map->map_entry[lpn].pn;
			location = find_location(raid4->ssd[dev_no],ppn);

			raid4->ssd[dev_no]->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state=0;             /*表示某一页失效，同时标记valid和free状态都为0*/
			raid4->ssd[dev_no]->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].free_state=0;              /*表示某一页失效，同时标记valid和free状态都为0*/
			raid4->ssd[dev_no]->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn=0;
			raid4->ssd[dev_no]->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].invalid_page_num++;

			/*******************************************************************************************
			*该block中全是invalid的页，可以直接删除，就在创建一个可擦除的节点，挂在location下的plane下面
			********************************************************************************************/
			if (raid4->ssd[dev_no]->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].invalid_page_num==raid4->ssd[dev_no]->parameter->page_block)    
			{
				new_direct_erase=(struct direct_erase *)malloc(sizeof(struct direct_erase));
				alloc_assert(new_direct_erase,"new_direct_erase");
				memset(new_direct_erase,0, sizeof(struct direct_erase));

				new_direct_erase->block=location->block;
				new_direct_erase->next_node=NULL;
				direct_erase_node=raid4->ssd[dev_no]->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].erase_node;
				if (direct_erase_node==NULL)
				{
					raid4->ssd[dev_no]->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].erase_node=new_direct_erase;
				} 
				else
				{
					new_direct_erase->next_node=raid4->ssd[dev_no]->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].erase_node;
					raid4->ssd[dev_no]->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].erase_node=new_direct_erase;
				}
			}

			free(location);
			location=NULL;
			raid4->ssd[dev_no]->dram->map->map_entry[lpn].state = 0;
			raid4->ssd[dev_no]->dram->map->map_entry[lpn].pn=0;
		}	
	}

	if (raid4->ssd[dev_no]->parameter->active_write==0)                                            /*如果没有主动策略，只采用gc_hard_threshold，并且无法中断GC过程*/
	{          
		/*如果plane中的free_page的数目少于gc_hard_threshold所设定的阈值就产生gc操作*/
		ppn = raid4->ssd[0]->dram->map->map_entry[lpn].pn;
		location = find_location(raid4->ssd[dev_no],ppn);
		channel=location->channel;
		chip=location->chip;
		die=location->die;
		plane=location->plane;

		if (raid4->ssd[dev_no]->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page<(raid4->ssd[0]->parameter->page_block*raid4->ssd[0]->parameter->block_plane*raid4->ssd[0]->parameter->gc_hard_threshold))
		{
			gc_node=(struct gc_operation *)malloc(sizeof(struct gc_operation));
			alloc_assert(gc_node,"gc_node");
			memset(gc_node,0, sizeof(struct gc_operation));

			gc_node->next_node=NULL;
			gc_node->chip=chip;
			gc_node->die=die;
			gc_node->plane=plane;
			gc_node->block=0xffffffff;
			gc_node->page=0;
			gc_node->state=GC_WAIT;
			gc_node->priority=GC_UNINTERRUPT;
			gc_node->next_node=raid4->ssd[dev_no]->channel_head[channel].gc_command;
			raid4->ssd[dev_no]->channel_head[channel].gc_command=gc_node;
			raid4->ssd[dev_no]->gc_request++;
		}
	} 

	if (node != NULL)
	{
		raid4->ssd[dev_no]->restore_list->pre_node = node->pre_node;
		node->pre_node->next_node = raid4->ssd[dev_no]->restore_list;
		raid4->ssd[dev_no]->list_count--;
	}
}


//检查是数据否已经转存到brother ssd中，若在的话，返回1，若不在返回0
unsigned int check_in_brother_ssd(struct raid *raid4,unsigned int dev_no,unsigned int lpn)
{
	unsigned int id;
	unsigned int in_brother_ssd_flag;
	struct restore_node* node;

	id = lpn/raid4->ssd[0]->node_size;
	node = raid4->ssd[dev_no]->restore_list;
	in_brother_ssd_flag = 0;

	while(node!= NULL)
	{
		if (node->id != id && node->state ==1&&node->next_node!=raid4->ssd[dev_no]->restore_list)
		{
			node = node->next_node;
		}
		else if (node->id==id&&node->state==1)
		{
			in_brother_ssd_flag = 1;
			break;	
		}
		else if (node->next_node==raid4->ssd[dev_no]->restore_list)
		{
			break;
		}
	}
	return in_brother_ssd_flag;

}


//重建时如果restore list里面已有的数据还没有写入替换盘则先重建restore list上的数据
unsigned int get_rec_lpn_from_restore_list(struct raid* raid4)
{
	struct restore_node* node,*tmp;
	unsigned int lpn,start_lpn,last_lpn,id;

	node=raid4->ssd[1]->restore_list;
	raid4->restore_rec_lpn=raid4->restore_rec_lpn+raid4->data_disks;
	start_lpn=raid4->restore_rec_lpn;

	if (start_lpn>=(node->id+1)*raid4->node_size&&node->next_node==node)
	{
		free(raid4->ssd[1]->restore_list);
		raid4->ssd[1]->restore_list=NULL;

		return 0xffffffff;
	}
	//检测到重建进行到了下一个node；
	if (start_lpn>=(node->id+1)*raid4->node_size&&node->next_node!=node)
	{
		tmp=node;
		node=node->next_node;

		raid4->ssd[1]->restore_list = tmp->next_node;
		tmp->next_node->pre_node = tmp->pre_node;
		tmp->pre_node->next_node = tmp->next_node;

		free(tmp);
		tmp = NULL;
	}
	if (start_lpn<raid4->ssd[1]->restore_list->id*raid4->node_size+raid4->broken_device_num||start_lpn>=(raid4->ssd[1]->restore_list->id+1)*raid4->node_size)
	{
		start_lpn=raid4->ssd[1]->restore_list->id*raid4->node_size+raid4->broken_device_num;
	}
	id=start_lpn/raid4->node_size;
	last_lpn=(id+1)*raid4->node_size;
	while (node!=NULL)
	{
		for (lpn=start_lpn; lpn<last_lpn; lpn+=raid4->data_disks)
		{
			if (rec_map[lpn].need_reconstruct_flag<=0)
			{
				continue;
			}
			raid4->restore_rec_lpn=lpn;
			return lpn;
		}

		if (node->next_node!=node)
		{
			tmp=node;
			node=node->next_node;

			raid4->ssd[1]->restore_list = tmp->next_node;
			tmp->next_node->pre_node = tmp->pre_node;
			tmp->pre_node->next_node = tmp->next_node;
			start_lpn=raid4->ssd[1]->restore_list->id*raid4->node_size;
			last_lpn=start_lpn+raid4->node_size;
			tmp = NULL;
			free(tmp);
		}
		else	//即到达最后一个节点
		{
			raid4->ssd[1]->restore_list=NULL;
			node = NULL;
			return 0xffffffff;		
		}				
	}	
}


//回收在restore时产生的请求的空间
void remove_restore_req(struct raid *raid4)	
{
	struct sub_request *sub,*tmp_pre_read,*pre_node;
	__int64 start_time,end_time;
	int flag;
	unsigned int dev_no;

	for (dev_no=0;dev_no<raid4->data_disks+raid4->par_disks;dev_no++)
	{	
		sub=raid4->ssd[dev_no]->restore_req;
		pre_node=NULL;
		flag=0;

		start_time=0;
		end_time=0;


		while(sub != NULL)
		{
			if(start_time == 0)
				start_time = sub->begin_time;
			if(start_time > sub->begin_time)
				start_time = sub->begin_time;
			if(end_time < sub->complete_time)
				end_time = sub->complete_time;
			if((sub->current_state == SR_COMPLETE)||((sub->next_state==SR_COMPLETE)&&(sub->next_state_predict_time<=raid4->current_time)))	
			{			
				flag=1;		
			}
			else
			{
				flag=0;	

			}


			if (flag == 1) //已经完成，从rec_list链上摘除该请求
			{
				//fprintf(raid4->recfile,"%10u %16I64u %16I64u %10I64u\n",sub->lpn,	sub->begin_time,	sub->complete_time,	sub->complete_time - sub->begin_time);

				//先将该请求的pre read链上的请求摘除
				while (sub->next_pre_read!=NULL)
				{
					tmp_pre_read=sub->next_pre_read;
					sub->next_pre_read=tmp_pre_read->next_pre_read;
					free(tmp_pre_read->location);
					tmp_pre_read->location=NULL;
					free(tmp_pre_read);
					tmp_pre_read=NULL;						
				}

				//再将该请求从rec list上摘除
				if (pre_node==NULL)
				{
					if (sub->next_rec==NULL)
					{
						raid4->ssd[dev_no]->restore_req=NULL;
						free(sub->location);
						sub->location=NULL;
						free(sub);
						sub=NULL;
					}
					else
					{	
						raid4->ssd[dev_no]->restore_req=sub->next_rec;
						pre_node=sub;
						free(pre_node->location);
						pre_node->location=NULL;
						free(pre_node);
						pre_node=NULL;
						sub=raid4->ssd[dev_no]->restore_req;
					}

				}
				else		//pre_node!=NULL
				{
					if (sub->next_rec==NULL)
					{
						pre_node->next_rec=NULL;
						free(sub);
						sub=NULL;

					}
					else
					{
						pre_node->next_rec=sub->next_rec;
						free(sub);
						sub=NULL;
						sub=pre_node->next_rec;
					}

				}

			}
			else		//flag!=0,未完成
			{
				pre_node=sub;
				sub=sub->next_rec;
			}


		}
	}
}



//通过device No计算brother device No
unsigned int calc_brother_dev_no(struct raid *raid4,int dev_no)
{
	unsigned int bro_dev_no;

	if (dev_no>=raid4->data_disks-1)
	{
		bro_dev_no=(dev_no-raid4->data_disks+1)%raid4->par_disks+raid4->data_disks;
	}
	else
	{
		bro_dev_no=(dev_no+1)%raid4->data_disks;
	}

	return bro_dev_no;
}

unsigned int calc_dev_no(struct raid *raid4,int bro_dev_no)
{
	unsigned int dev_no;
	if (bro_dev_no<=raid4->data_disks-1)
	{
		dev_no=(bro_dev_no-1)%raid4->data_disks;
	}
	else
	{
		dev_no=(bro_dev_no-raid4->data_disks-1)%raid4->par_disks+raid4->data_disks;
	}
	return dev_no;
}