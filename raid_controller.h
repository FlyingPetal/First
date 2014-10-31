#define SSD_NUMBER 13

#include "initialize.h"
#include "pagemap.h"
#include "ssd.h"
#include "flash.h"
#include <crtdefs.h> 
#include <stdio.h>
#include <stdlib.h>

struct rec_buf_node 
{
	unsigned int stripe_no;
	int involve_dev;	//该条目是那几个dev上的数据的异或值
	int is_used;	//表明该条目是否被使用
	struct rec_buf_node* next_node;//指向链表中下一个节点
};

struct rec_info
{
	int need_reconstruct_flag; //大于1表示需要重建（用四位表示需要重建的有效sector），0表示已经重建，-1表示无需重建
};


/************************************************************************/
/* 定义raid的结构，包含输入的trace文件，磁盘个数，sh，请求队列，请求平均大小
等信息 ,原来放在ssd中的request的各类统计信息也放入这里*/
/************************************************************************/
struct raid
{
	int mode; //raid mode:0,1,4,5
	char parameterfilename1[42];
	char parameterfilename2[42];
	char tracefilename[42];
	char outputfilename[42];
	char statisticfilename[50];
	char recfilename[50];
	FILE * tracefile;
	FILE * outputfile;
	FILE * statisticfile;
	FILE * recfile;
	//FILE *out_put_file1,*out_put_file2,*out_put_file3,*out_put_file4;
	unsigned int STRIPE_SECTORS,data_disks,par_disks,sectors_per_chunk,chunk_size;
	int pd_idx;


	struct rec_info* rec_map;
	struct rec_buf_node* rec_buf;
	unsigned int rec_buf_num;
	int used_rec_buf_num;
	int broken_device_num;
	struct sub_request* rec_list;
	int rec_list_length;
	int rec_complete_flag;
	unsigned int current_rec_lpn;
	__int64	rec_complete_time;
	struct request *request_queue,*request_tail;
	int request_queue_length,request_queue_length_max;
	unsigned int min_lsn,max_lsn;
	float ave_read_size,ave_write_size;
	unsigned int read_request_count,write_request_count;
	unsigned int  read_request_count_in_rec, write_request_count_in_rec;
	__int64 read_avg,write_avg;
	__int64 total_read_time_in_rec,total_write_time_in_rec;
	__int64 read_avg_in_rec,write_avg_in_rec;
	__int64 read_avg_in_normal,write_avg_in_normal;
	__int64 response_time_in_normal,response_time_in_reconstruction;
	struct ssd_info *ssd[SSD_NUMBER];
	__int64 current_time,next_request_time;
	unsigned long pre_read_count;

	//add for rec_exchange_buf
	struct read_node *read_lru;
	int read_lru_length,read_lru_mex_length;
	int node_size;	//标识每个bode包含多少个page。
	unsigned int raid_state; //
	unsigned int restore_rec_lpn;
	__int64 broken_time;

};





void main();
struct raid *simulate(struct raid *raid4);
struct raid *init(struct raid *raid4);
void distribute(struct raid *raid4,int flag);
int distribute_request(struct raid *raid4,struct request *request1,int is_in_delay_queue);
__int64 find_nearest_time(struct raid *raid4);

int get_request(struct raid *raid4);
void trace_out_put(struct raid *raid4);

struct raid *process(struct raid *raid4);
struct raid *pre_process(struct raid *raid4);
int get_pre_read_flag(struct raid *raid4,struct sub_request *sub);
void statistic_output(struct raid *raid4);
void set_raid4(struct raid *raid4);

struct ssd_info *ssd_process(struct raid *,struct ssd_info *,int );
unsigned int get_rec_lpn(struct raid* raid4);
struct sub_request* create_rec_request(struct raid* raid4,unsigned int lpn,struct sub_request* sub_read,unsigned int sub_size,unsigned int sub_state,int priority);
struct sub_request * init_rec_read(struct ssd_info * ssd,unsigned int lpn,unsigned int sub_size,unsigned int sub_state,struct request * req);
void check_rec_done(struct raid* raid4);
void add_to_recbuf(struct raid *raid4,struct sub_request *sub);
void start_rec(struct raid* raid4);
int check_in_rec_buf(struct raid * raid4,unsigned int lpn);
void init_rec_map(struct raid* raid4);