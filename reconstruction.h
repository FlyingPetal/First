#pragma once


struct visit_count        
{
	unsigned int id;
	int count;
};

struct restore_node
{
	unsigned int id;
	int state;//0:��Ч 1����Ч 
	struct restore_node *pre_node,*next_node;
};

int check_in_visit_table(struct raid *raid4,unsigned int dev_no,struct sub_request *sub);
struct restore_node *restore(struct raid *raid4,unsigned int dev_no,unsigned int id);
int start_GC(struct raid* raid4,unsigned int dev_no);//ɾ�����һ���ڵ㣬������Ӧssd�аѶ�Ӧ�����ݱ��Ϊ��Ч;
unsigned int check_in_brother_ssd(struct raid *raid4,unsigned int dev_no,unsigned int lpn);
unsigned int get_rec_lpn_from_restore_list(struct raid* raid4);
void remove_restore_req(struct raid *raid4);
unsigned int calc_brother_dev_no(struct raid *raid4,int dev_no);
unsigned int calc_dev_no(struct raid *raid4,int bro_dev_no);