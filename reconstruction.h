#pragma once


struct visit_count        
{
	unsigned int id;
	int count;
};

struct restore_node
{
	unsigned int id;
	int state;//0:无效 1：有效 
	struct restore_node *pre_node,*next_node;
};

int check_in_visit_table(struct raid *raid4,unsigned int dev_no,struct sub_request *sub);
struct restore_node *restore(struct raid *raid4,unsigned int dev_no,unsigned int id);
int start_GC(struct raid* raid4,unsigned int dev_no);//删除最后一个节点，并在相应ssd中把对应的数据标记为无效;
unsigned int check_in_brother_ssd(struct raid *raid4,unsigned int dev_no,unsigned int lpn);
unsigned int get_rec_lpn_from_restore_list(struct raid* raid4);
void remove_restore_req(struct raid *raid4);
unsigned int calc_brother_dev_no(struct raid *raid4,int dev_no);
unsigned int calc_dev_no(struct raid *raid4,int bro_dev_no);