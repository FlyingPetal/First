

#include "raid_controller.h"

struct rec_info *rec_map;
void main()
{
	struct raid *raid4;
	unsigned int i;

	raid4=(struct raid*)malloc(sizeof(struct raid));
	alloc_assert(raid4,"raid4");
	memset(raid4,0,sizeof(struct raid));

	set_raid4(raid4);
	init_rec_map(raid4);
	for (i=0;i<raid4->data_disks+raid4->par_disks+1;i++)
	{
		raid4->ssd[i]=(struct ssd_info*)malloc(sizeof(struct ssd_info));
		alloc_assert(raid4->ssd[i],"raid4->ssd[i]");
		memset(raid4->ssd[i],0, sizeof(struct ssd_info));	

		raid4->ssd[i]->state=NORMAL_STATE;	
	}

	init(raid4);
	for (i=0;i<raid4->data_disks+raid4->par_disks;i++)
	{
		make_aged(raid4->ssd[i]);		
	}
	pre_process(raid4); 

	simulate(raid4);
	statistic_output(raid4); 
}

struct raid *simulate(struct raid *raid4)
{
	errno_t err;
	int flag=1;
	unsigned int lpn,sub_state,sub_size;
	int rec_complete_flag,rec_in_restore_flag;
	struct sub_request *sub1,*sub2;

	rec_complete_flag=0;
	rec_in_restore_flag=0;

	if((err=fopen_s(&(raid4->tracefile),raid4->tracefilename,"r"))!=0)
	{  
		printf("the trace file can't open\n");
		return 1;
	}
	fprintf(raid4->outputfile,"      arrive           lsn     size ope     begin time    response time    process time\n");	
	fflush(raid4->outputfile);

	while (flag!=100)
	{
		flag=get_request(raid4);
		distribute(raid4,flag);
		/************************************************************************/
		/* added for reconstructed:
		���rec listΪ�գ��������ؽ�
		�����ؽ�ʱ���������ؽ�restore list�е����ݣ����ؽ���������*/
		/************************************************************************/
		while (raid4->raid_state == RECONSTRUCT_STATE && raid4->rec_list_length <= 4 && raid4->rec_complete_flag != 1)
		{
			if (raid4->ssd[1]->restore_list!= NULL) //����restore list�д�д�ص�����
			{
				lpn=get_rec_lpn_from_restore_list(raid4);
				rec_in_restore_flag=1;
				if (lpn==0xffffffff)	//��鵽restore list�Ѿ�û�д�д�ص�����
				{
					lpn=get_rec_lpn(raid4);
					rec_in_restore_flag=0;
				}	
			}
			else
			{
				lpn=get_rec_lpn(raid4);
				rec_in_restore_flag=0;
			}


			sub_state=rec_map[lpn].need_reconstruct_flag;
			sub_size=size(sub_state);
			if (rec_in_restore_flag==0)
			{
				create_rec_request(raid4,lpn,NULL,sub_size,sub_state,-1);
			}
			else	//restore list�����У�ֱ�Ӵ��ж�ȡ�ؽ�
			{
				sub1=creat_sub_request(raid4->ssd[1],lpn,sub_size,sub_state,NULL,READ);
				sub2=creat_sub_request(raid4->ssd[raid4->data_disks+raid4->par_disks],lpn,sub_size,sub_state,NULL,WRITE);
				sub2->next_pre_read = sub1;

				//��д��������ؽ�����֮��
				if (raid4->rec_list == NULL)
				{
					raid4->rec_list=sub2;
					sub2->next_rec=NULL;
				}
				else
				{
					sub2->next_rec=raid4->rec_list;
					raid4->rec_list=sub2;
				}
				raid4->rec_list_length++;

			}

		}


		process(raid4);
		trace_out_put(raid4);
		if (raid4->raid_state==NORMAL_STATE)
		{
			remove_restore_req(raid4);
			//��¼����normal����µ�ƽ����Ӧʱ��
			if (raid4->read_request_count>0&&raid4->write_request_count>0)
			{
				raid4->read_avg_in_normal=raid4->read_avg/raid4->read_request_count;
				raid4->write_avg_in_normal=raid4->write_avg/raid4->write_request_count;
				raid4->response_time_in_normal=(raid4->read_avg+raid4->write_avg)/(raid4->read_request_count+raid4->write_request_count);
			}
			
		}
		if (raid4->raid_state==RECONSTRUCT_STATE)
		{
			check_rec_done(raid4);
		}

		if(flag == 0 && raid4->request_queue == NULL)
			flag = 100;

	}
	return raid4;

}

struct raid *init(struct raid *raid4)
{
	unsigned int x=0,y=0,i=0,j=0,k=0,l=0,m=0,n=0;
	errno_t err;
	char buffer[300];
	struct parameter_value *parameters;
	FILE *fp=NULL;



	//printf("input parameter file name:");
	//scanf("%s",&raid4->parameterfilename1);
	strcpy_s(raid4->parameterfilename1,13,"f:\\traces\\p1");

	//printf("input parameter file name:");
	//scanf("%s",&raid4->parameterfilename2);
	strcpy_s(raid4->parameterfilename2,13,"f:\\traces\\p2");

	//printf("\ninput trace file name:");
	//scanf("%s",&raid4->tracefilename);
	strcpy_s(raid4->tracefilename,15,"f:\\traces\\web1");

	//printf("\ninput output file name:");
	//scanf("%s",raid4->outputfilename);
	strcpy_s(raid4->outputfilename,5,"f:\\1");

	strcpy_s(raid4->recfilename,5,"f:\\2");

	//printf("\ninput statistic file name:");
	//scanf("%s",raid4->statisticfilename);
	strcpy_s(raid4->statisticfilename,9,"f:\\adv_3");

	//printf("\ninput reconstrution output file name:");
	//scanf("%s",raid4->recfilename);
	strcpy_s(raid4->recfilename,9,"f:\\adv_4");



	//����ssd�������ļ�
	parameters=load_parameters(raid4->parameterfilename1);
	raid4->min_lsn=0x7fffffff;
	for (i=0;i<raid4->data_disks+raid4->par_disks+1;i++)	//raid4->data_disks+raid4->par_disks+1,+1����Ϊ��Ҫ��ʼ��һ���������滻��
	{
		if(i>=raid4->data_disks)
		{
			parameters=load_parameters(raid4->parameterfilename2);
		}
		if (i==raid4->data_disks+raid4->par_disks)
		{
			parameters=load_parameters(raid4->parameterfilename1);
		}
		raid4->ssd[i]->parameter=parameters;
		raid4->ssd[i]->page=raid4->ssd[i]->parameter->chip_num*raid4->ssd[i]->parameter->die_chip*raid4->ssd[i]->parameter->plane_die*raid4->ssd[i]->parameter->block_plane*raid4->ssd[i]->parameter->page_block;



		//��ʼ�� dram
		raid4->ssd[i]->dram = (struct dram_info *)malloc(sizeof(struct dram_info));
		alloc_assert(raid4->ssd[i]->dram,"raid4->ssd[i]->dram");
		memset(raid4->ssd[i]->dram,0,sizeof(struct dram_info));
		initialize_dram(raid4->ssd[i]);

		//��ʼ��ͨ��
		raid4->ssd[i]->channel_head=(struct channel_info*)malloc(raid4->ssd[i]->parameter->channel_number * sizeof(struct channel_info));
		alloc_assert(raid4->ssd[i]->channel_head,"raid4->ssd[i]->channel_head");
		memset(raid4->ssd[i]->channel_head,0,raid4->ssd[i]->parameter->channel_number * sizeof(struct channel_info));
		initialize_channels(raid4->ssd[i]);
		initialize_restore_list(raid4->ssd[i]);
		initialize_visit_table(raid4->ssd[i]);
	}

	printf("\n");
	if((err=fopen_s(&raid4->outputfile,raid4->outputfilename,"w")) != 0)
	{
		printf("the output file can't open\n");
		return NULL;
	}

	printf("\n");
	if((err=fopen_s(&raid4->statisticfile,raid4->statisticfilename,"w"))!=0)
	{
		printf("the statistic file can't open\n");
		return NULL;
	}

	printf("\n");
	if ((err=fopen_s(&raid4->recfile,raid4->recfilename,"w")) != 0)
	{
		printf("\nthe rec output file can't open");
		return NULL;
	}
	// 	if((err=fopen_s(&ssd->statisticfile2,ssd->statisticfilename2,"w"))!=0)
	// 	{
	// 		printf("the second statistic file can't open\n");
	// 		return NULL;
	// 	}

	fprintf(raid4->outputfile,"parameter file: %s\n",raid4->parameterfilename1); 
	fprintf(raid4->outputfile,"trace file: %s\n",raid4->tracefilename);
	fprintf(raid4->statisticfile,"parameter file: %s\n",raid4->parameterfilename1); 
	fprintf(raid4->statisticfile,"trace file: %s\n",raid4->tracefilename);

	fflush(raid4->outputfile);
	fflush(raid4->statisticfile);

	if((err=fopen_s(&fp,raid4->parameterfilename1,"r"))!=0)
	{
		printf("\nthe parameter file can't open!\n");
		return NULL;
	}

	//fp=fopen(ssd->parameterfilename,"r");

	fprintf(raid4->outputfile,"-----------------------parameter file----------------------\n");
	fprintf(raid4->statisticfile,"-----------------------parameter file----------------------\n");
	while(fgets(buffer,300,fp))
	{
		fprintf(raid4->outputfile,"%s",buffer);
		fflush(raid4->outputfile);
		fprintf(raid4->statisticfile,"%s",buffer);
		fflush(raid4->statisticfile);
	}

	fprintf(raid4->outputfile,"\n");
	fprintf(raid4->outputfile,"-----------------------simulation output----------------------\n");
	fflush(raid4->outputfile);

	fprintf(raid4->statisticfile,"\n");
	fprintf(raid4->statisticfile,"-----------------------simulation output----------------------\n");
	fflush(raid4->statisticfile);

	fclose(fp);
	printf("initiation is completed!\n");

	return raid4;
}



void distribute(struct raid *raid4,int flag)
{
	struct request *request1;

	if (flag == 1)
	{
		request1=raid4->request_tail;
		distribute_request(raid4,request1,-1);//-1��ʾ����delay queue�е�����

	}	

}


/************************************************************************/
/*           */
/************************************************************************/
int distribute_request(struct raid *raid4,struct request *request1,int is_in_delay_queue)
{
	unsigned int lsn,lpn,rec_lpn,id,tmp_lpn,first_lpn,last_lpn,last_lpn_in_stripe,state,largest_lsn;
	struct sub_request *sub,*sub1,*sub2,*sub_read,*pre_read=NULL,*parity_write,*rec_write;
	unsigned int stripe_num,dev_no,bro_dev_no,first_stripe_num,last_stripe_num;
	unsigned int in_brother_ssd;	//��־λ����ʾ���ؽ��������Ƿ����ֵ��������л���
	unsigned int mask=0; 
	unsigned int offset1=0, offset2=0;
	unsigned int sub_size=0;
	unsigned int sub_state=0;
	unsigned int sub_in_stripe,rcw_pre_read_count,rmw_pre_read_count;
	unsigned int M_C_flag;//��ʾ��ʹ��RMW����RCW��1--RMW��0--RCW
	unsigned in_stripe_flag;
	struct local *location;
	unsigned int pd_idx;
	int k;
	//largest_lsn=(unsigned int )(((raid4->ssd[raid4->pd_idx]->parameter->chip_num*raid4->ssd[raid4->pd_idx]->parameter->die_chip*raid4->ssd[raid4->pd_idx]->parameter->plane_die*raid4->ssd[raid4->pd_idx]->parameter->block_plane*raid4->ssd[raid4->pd_idx]->parameter->page_block*raid4->ssd[raid4->pd_idx]->parameter->subpage_page)*(1-raid4->ssd[raid4->pd_idx]->parameter->overprovide)));
	largest_lsn=15099494;
	request1->lsn=request1->lsn%largest_lsn;
	lsn=request1->lsn;
	lpn=lsn/raid4->ssd[0]->parameter->subpage_page;
	last_lpn=(lsn+request1->size-1)/raid4->ssd[0]->parameter->subpage_page;
	first_lpn=lsn/raid4->ssd[0]->parameter->subpage_page;


	if(request1->operation==READ)        
	{
		first_stripe_num=first_lpn/raid4->data_disks;
		last_stripe_num=last_lpn/raid4->data_disks;

		while(lpn<=last_lpn) 		
		{
			dev_no=lpn%raid4->data_disks;
			sub_state=(raid4->ssd[dev_no]->dram->map->map_entry[lpn].state&0x7fffffff);
			sub_size=size(sub_state);

			/************************************************************************/
			/* added:for data reconstruction                           */
			/************************************************************************/
			if (dev_no == raid4->broken_device_num && rec_map[lpn].need_reconstruct_flag>0 && raid4->raid_state==RECONSTRUCT_STATE)
			{
				bro_dev_no=calc_brother_dev_no(raid4,dev_no);
				in_brother_ssd = check_in_brother_ssd(raid4,bro_dev_no,lpn);

				if (in_brother_ssd == 0)
				{
					sub=init_rec_read(raid4->ssd[dev_no],lpn,sub_size,sub_state,request1);
					rec_write=create_rec_request(raid4,lpn,sub,sub_size,sub_state,0);//�������ȼ�������
					rec_map[lpn].need_reconstruct_flag=0;
				}
				else //in_brother_ssd ==1,ֱ�Ӵ�brother�̶�ȡ���ݣ�����ʼ�ؽ����ؽ�ֻ���ȡ��ֱ��д���滻��
				{
					bro_dev_no=calc_brother_dev_no(raid4,dev_no);
					sub_read=init_rec_read(raid4->ssd[dev_no],lpn,sub_size,sub_state,request1);
					sub=creat_sub_request(raid4->ssd[bro_dev_no],lpn,sub_size,sub_state,NULL,READ);	
					sub1=creat_sub_request(raid4->ssd[raid4->data_disks+raid4->par_disks],lpn,sub_size,sub_state,NULL,WRITE);
					sub->priority=0;	//�������ȼ�
					sub1->priority=0;
					sub1->trig_req=sub_read;
					sub1->next_pre_read=sub;

					//��д��������ؽ�����֮��
					if (raid4->rec_list == NULL)
					{
						raid4->rec_list=sub1;
						sub1->next_rec=NULL;
					}
					else
					{
						sub1->next_rec=raid4->rec_list;
						raid4->rec_list=sub1;
					}
					rec_map[lpn].need_reconstruct_flag=0;


					//Ҫ��Ҫ�����ĸ��£�������������Ҫ�������������
					id=lpn/raid4->ssd[0]->node_size;
					for (rec_lpn=id*raid4->ssd[0]->node_size; rec_lpn<(id+1)*raid4->ssd[0]->node_size; rec_lpn+=raid4->data_disks)
					{
						if (rec_map[rec_lpn].need_reconstruct_flag>0)
						{
							sub_size = size(rec_map[rec_lpn].need_reconstruct_flag);
							sub=creat_sub_request(raid4->ssd[0],rec_lpn,sub_size,rec_map[rec_lpn].need_reconstruct_flag,NULL,READ);	
							sub1=creat_sub_request(raid4->ssd[raid4->data_disks+2],rec_lpn,sub_size,rec_map[rec_lpn].need_reconstruct_flag,NULL,WRITE);
							sub1->update=sub;

							if (raid4->rec_list == NULL)
							{
								raid4->rec_list=sub1;
								sub1->next_rec=NULL;
							}
							else
							{
								sub1->next_rec=raid4->rec_list;
								raid4->rec_list=sub1;
							}
							raid4->rec_list_length++;
							rec_map[rec_lpn].need_reconstruct_flag=0;
						}
					}
				}

			}

			else if (raid4->raid_state==RECONSTRUCT_STATE&&dev_no == raid4->broken_device_num && rec_map[lpn].need_reconstruct_flag==0)
			{
				sub=creat_sub_request(raid4->ssd[raid4->data_disks+raid4->par_disks],lpn,sub_size,sub_state,request1,request1->operation);
			}
			else 
			{
				sub=creat_sub_request(raid4->ssd[dev_no],lpn,sub_size,sub_state,request1,request1->operation);
				if (raid4->raid_state==NORMAL_STATE)
				{
					check_in_visit_table(raid4,dev_no,sub);
				}				
			}
			lpn++;
		}
	}
	else if(request1->operation==WRITE)
	{
		//д����Ĵ���
		first_stripe_num=first_lpn/raid4->data_disks;
		last_stripe_num=last_lpn/raid4->data_disks;
		for (stripe_num=first_stripe_num;stripe_num<=last_stripe_num;stripe_num++)
		{
			/************************************************************************/
			/* ������RMW����RCW�����ն�Ҫ��Parity disk����һ��д,����parity  */
			/************************************************************************/
			pd_idx=stripe_num%raid4->par_disks+raid4->data_disks;	
			parity_write=creat_sub_request(raid4->ssd[pd_idx],stripe_num*raid4->data_disks,4,15,request1,WRITE);


			//if-elseѭ������ÿ��stripe����ʼlpn�Լ�stripe�ڵ�request��Ŀ
			if (first_stripe_num==last_stripe_num)
			{
				lpn=first_lpn;
				sub_in_stripe=last_lpn+1-first_lpn;
				last_lpn_in_stripe=last_lpn;
			}
			else
			{
				if (stripe_num==first_stripe_num)
				{
					lpn=first_lpn;
					sub_in_stripe=(first_stripe_num+1)*raid4->data_disks-first_lpn;
					last_lpn_in_stripe=(first_stripe_num+1)*raid4->data_disks-1;
				}
				else if (stripe_num==last_stripe_num)
				{
					lpn=stripe_num*raid4->data_disks;
					sub_in_stripe=last_lpn+1-last_stripe_num*raid4->data_disks;
					last_lpn_in_stripe=last_lpn;
				}
				else
				{
					lpn=stripe_num*raid4->data_disks;
					sub_in_stripe=raid4->data_disks;
					last_lpn_in_stripe=(stripe_num+1)*raid4->data_disks-1;
				}
			}

			/************************************************************************/
			/* ͨ��sub_in_stripe��Ŀ������Ԥ����Ŀȷ���ǲ���RCW����RMW            */
			/************************************************************************/
			rcw_pre_read_count=raid4->data_disks-sub_in_stripe;	//û��д�����dev
			rmw_pre_read_count=sub_in_stripe+1;	//��д�����dev+praity dev
			if(rcw_pre_read_count<=rmw_pre_read_count)
			{
				M_C_flag=0;	//RCW��Ԥ��û��д�����dev������д����������
			}
			else
			{
				M_C_flag=1;	//RMW,Ԥ����д�����dev��parity��������Ҫд����������
			}
			/************************************************************************/
			/* added:for data reconstruction          
			�ؽ�״̬�£���дǣ�浽broken diskʱ��һ�ɲ���RCW*/
			/************************************************************************/
			if (raid4->raid_state==RECONSTRUCT_STATE)
			{	
				M_C_flag=1;
				for (tmp_lpn=lpn;tmp_lpn<=last_lpn_in_stripe;tmp_lpn++)
				{
					if (tmp_lpn%raid4->data_disks==raid4->broken_device_num)
					{
						M_C_flag=0;
						break;
					}
				}		
			}
			//RMW,Ԥ����д�����dev��parity��������Ҫд����������
			//��3�������1����ͨ״̬�£� 2���ؽ�����£�дûǣ�浽broken disk��ǣ�浽����תΪRCW�� 3.�ؽ������������RCW��������ΪRCWԤ������broken
			//disk������תΪRMW
			if(M_C_flag==1)
			{	
				parity_write->RMW_flag=1;
				while(lpn<=last_lpn&&lpn<(stripe_num+1)*raid4->data_disks)     	
				{
					sub2=NULL;
					dev_no=lpn%raid4->data_disks;
					if (raid4->raid_state==RECONSTRUCT_STATE&&dev_no==raid4->broken_device_num)//���򲻿������е�����
					{
						dev_no=raid4->data_disks+raid4->par_disks;
						printf_s("error 404");
					}
					mask=~(0xffffffff<<(raid4->ssd[dev_no]->parameter->subpage_page));
					state=mask;
					if(lpn==first_lpn)
					{
						offset1=raid4->ssd[dev_no]->parameter->subpage_page-((lpn+1)*raid4->ssd[dev_no]->parameter->subpage_page-request1->lsn);
						state=state&(0xffffffff<<offset1);
					}
					if(lpn==last_lpn)
					{
						offset2=raid4->ssd[dev_no]->parameter->subpage_page-((lpn+1)*raid4->ssd[dev_no]->parameter->subpage_page-(request1->lsn+request1->size));
						state=state&(~(0xffffffff<<offset2));
					}
					sub_size=size(state);
					sub=creat_sub_request(raid4->ssd[dev_no],lpn,sub_size,state,request1,request1->operation);

					//add for �ȵ����ݸ��¼�⣬����ʱ��Ҫͬʱ����ԭ���������Ѿ�restore��brother ssd������
					if (raid4->raid_state==NORMAL_STATE)
					{
						bro_dev_no=calc_brother_dev_no(raid4,dev_no);
						in_brother_ssd=check_in_brother_ssd(raid4,bro_dev_no,lpn);
						if (in_brother_ssd==1)
						{				
							sub2=creat_sub_request(raid4->ssd[bro_dev_no],lpn,sub_size,state,request1,request1->operation);
						}
					}

					for (k=0;k<=1;k++)
					{
						if (k==1&&sub2==NULL)
						{
							break;
						}
						if (k==1&&sub2!=NULL)
						{
							dev_no=bro_dev_no;	//��Ӧ��sub2��pre read
							sub=sub2;
						}
						if (raid4->ssd[dev_no]->dram->map->map_entry[lpn].state!=0)
						{

							raid4->ssd[dev_no]->read_count++;
							raid4->pre_read_count++;

							pre_read=(struct sub_request *)malloc(sizeof(struct sub_request));
							alloc_assert(pre_read,"pre_read");
							memset(pre_read,0, sizeof(struct sub_request));

							if(pre_read==NULL)
							{
								return ERROR;
							}
							pre_read->location=NULL;
							pre_read->next_node=NULL;
							pre_read->next_subs=NULL;
							pre_read->update=NULL;						
							location = find_location(raid4->ssd[dev_no],raid4->ssd[dev_no]->dram->map->map_entry[lpn].pn);
							pre_read->location=location;
							pre_read->begin_time = raid4->current_time;
							pre_read->current_state = SR_WAIT;
							pre_read->current_time=0x7fffffffffffffff;
							pre_read->next_state = SR_R_C_A_TRANSFER;
							pre_read->next_state_predict_time=0x7fffffffffffffff;
							pre_read->lpn = lpn;
							pre_read->state=((raid4->ssd[dev_no]->dram->map->map_entry[lpn].state)&0x7fffffff);
							pre_read->size=size(pre_read->state);
							pre_read->ppn = raid4->ssd[dev_no]->dram->map->map_entry[lpn].pn;
							pre_read->operation = READ;



							if (raid4->ssd[dev_no]->channel_head[location->channel].subs_r_tail!=NULL)            //*�����µĶ����󣬲��ҹҵ�channel��subs_r_tail����β
							{
								raid4->ssd[dev_no]->channel_head[location->channel].subs_r_tail->next_node=pre_read;
								raid4->ssd[dev_no]->channel_head[location->channel].subs_r_tail=pre_read;
							} 
							else
							{
								raid4->ssd[dev_no]->channel_head[location->channel].subs_r_tail=pre_read;
								raid4->ssd[dev_no]->channel_head[location->channel].subs_r_head=pre_read;
							}
							if (pre_read!=NULL)
							{
								sub->update=pre_read;	//�����update��Ϊpre readʹ��
							}
							/************************************************************************/
							/* ά��parity write��pre read����     */
							/************************************************************************/
							if (parity_write->next_pre_read==NULL)
							{
								parity_write->next_pre_read=pre_read;
							}
							else
							{
								pre_read->next_pre_read=parity_write->next_pre_read;
								parity_write->next_pre_read=pre_read;
							}
						}
					}
					lpn++;	
				}
			}
			//RCW	Ԥ��û��д�����dev,��Ҫ���ǵ��ؽ��������ͨ���
			//��2�������1����ͨ״̬  2.�ؽ�ʱ��Ԥ��ûǣ�浽 broken disk 3���ؽ�ʱ�������ǲ���RMW������дǣ�浽broken disk ֻ�ܲ���RCW
			else 
			{
				parity_write->RMW_flag=-1;

				for (tmp_lpn=stripe_num*raid4->data_disks;tmp_lpn<(stripe_num+1)*raid4->data_disks;tmp_lpn++)
				{
					dev_no=tmp_lpn%raid4->data_disks;

					//�ؽ�ģʽ�£��Զ���д�뻵�̵������ض����滻��
					if (raid4->raid_state==RECONSTRUCT_STATE&&dev_no==raid4->broken_device_num)
					{
						dev_no=raid4->data_disks+raid4->par_disks;
					}

					mask=~(0xffffffff<<(raid4->ssd[dev_no]->parameter->subpage_page));
					state=mask;
					if (tmp_lpn<lpn||tmp_lpn>last_lpn)
					{
						in_stripe_flag=0;
					}
					else
					{
						in_stripe_flag=1;
					}

					if ((in_stripe_flag==0)&&(raid4->ssd[dev_no]->dram->map->map_entry[tmp_lpn].state!=0))	//Ԥ��û��д�����dev
					{
						pre_read=(struct sub_request *)malloc(sizeof(struct sub_request));
						alloc_assert(pre_read,"pre_read");
						memset(pre_read,0, sizeof(struct sub_request));

						raid4->ssd[dev_no]->read_count++;
						raid4->pre_read_count++;

						if(pre_read==NULL)
						{
							return ERROR;
						}
						pre_read->location=NULL;
						pre_read->next_node=NULL;
						pre_read->next_subs=NULL;
						pre_read->update=NULL;						
						location = find_location(raid4->ssd[dev_no],raid4->ssd[dev_no]->dram->map->map_entry[tmp_lpn].pn);
						pre_read->location=location;
						pre_read->begin_time = raid4->current_time;
						pre_read->current_state = SR_WAIT;
						pre_read->current_time=0x7fffffffffffffff;
						pre_read->next_state = SR_R_C_A_TRANSFER;
						pre_read->next_state_predict_time=0x7fffffffffffffff;
						pre_read->lpn = tmp_lpn;
						pre_read->state=((raid4->ssd[dev_no]->dram->map->map_entry[tmp_lpn].state)&0x7fffffff);
						pre_read->size=size(pre_read->state);
						pre_read->ppn = raid4->ssd[dev_no]->dram->map->map_entry[tmp_lpn].pn;
						pre_read->operation = READ;


						if (raid4->ssd[dev_no]->channel_head[location->channel].subs_r_tail!=NULL)            //*�����µĶ����󣬲��ҹҵ�channel��subs_r_tail����β
						{
							raid4->ssd[dev_no]->channel_head[location->channel].subs_r_tail->next_node=pre_read;
							raid4->ssd[dev_no]->channel_head[location->channel].subs_r_tail=pre_read;
						} 
						else
						{
							raid4->ssd[dev_no]->channel_head[location->channel].subs_r_tail=pre_read;
							raid4->ssd[dev_no]->channel_head[location->channel].subs_r_head=pre_read;
						}

						/************************************************************************/
						/* ά��parity write��pre read����     */
						/************************************************************************/
						if (parity_write->next_pre_read==NULL)
						{
							parity_write->next_pre_read=pre_read;
						}
						else
						{
							pre_read->next_pre_read=parity_write->next_pre_read;
							parity_write->next_pre_read=pre_read;
						}

					}
					if (in_stripe_flag==1)					 	//in_stripe_flag=1,����д�����dev����д����
					{
						if(tmp_lpn==first_lpn)
						{
							offset1=raid4->ssd[dev_no]->parameter->subpage_page-((tmp_lpn+1)*raid4->ssd[dev_no]->parameter->subpage_page-request1->lsn);
							state=state&(0xffffffff<<offset1);
						}
						if(tmp_lpn==last_lpn)
						{
							offset2=raid4->ssd[dev_no]->parameter->subpage_page-((tmp_lpn+1)*raid4->ssd[dev_no]->parameter->subpage_page-(request1->lsn+request1->size));
							state=state&(~(0xffffffff<<offset2));
						}
						sub_size=size(state);
						sub=creat_sub_request(raid4->ssd[dev_no],tmp_lpn,sub_size,state,request1,request1->operation);
						/************************************************************************/
						/* ��ͨģʽ�£���Ҫ��������Ƿ���restore list�У����ڣ�����Ҫ����      */
						/************************************************************************/
						if (raid4->raid_state==NORMAL_STATE)
						{
							bro_dev_no=calc_brother_dev_no(raid4,dev_no);
							in_brother_ssd=check_in_brother_ssd(raid4,bro_dev_no,lpn);
							if (in_brother_ssd==1)
							{
								sub2=creat_sub_request(raid4->ssd[bro_dev_no],lpn,sub_size,state,request1,request1->operation);
							}
						}

						/************************************************************************/
						/* added:for data reconstruction                           */
						/************************************************************************/
						if (raid4->raid_state==RECONSTRUCT_STATE && dev_no==raid4->data_disks+raid4->par_disks)
						{
							rec_map[lpn].need_reconstruct_flag=0;
						}
					}
				}

			}


			/************************************************************************/
			/* �����RMW������ҪԤ��parity���������            */
			/************************************************************************/
			if(M_C_flag==1&&raid4->ssd[pd_idx]->dram->map->map_entry[stripe_num*raid4->data_disks].state!=0)
			{
				dev_no=pd_idx;
				pre_read=(struct sub_request *)malloc(sizeof(struct sub_request));
				alloc_assert(pre_read,"pre_read");
				memset(pre_read,0, sizeof(struct sub_request));

				if(pre_read==NULL)
				{
					return ERROR;
				}
				pre_read->location=NULL;
				pre_read->next_node=NULL;
				pre_read->next_subs=NULL;
				pre_read->update=NULL;						
				location = find_location(raid4->ssd[dev_no],raid4->ssd[dev_no]->dram->map->map_entry[lpn].pn);
				pre_read->location=location;
				pre_read->begin_time = raid4->current_time;
				pre_read->current_state = SR_WAIT;
				pre_read->current_time=0x7fffffffffffffff;
				pre_read->next_state = SR_R_C_A_TRANSFER;
				pre_read->next_state_predict_time=0x7fffffffffffffff;
				pre_read->lpn = lpn;
				pre_read->state=((raid4->ssd[dev_no]->dram->map->map_entry[lpn].state)&0x7fffffff);
				pre_read->size=size(pre_read->state);
				pre_read->ppn = raid4->ssd[dev_no]->dram->map->map_entry[lpn].pn;
				pre_read->operation = READ;

				if (raid4->ssd[dev_no]->channel_head[location->channel].subs_r_tail!=NULL)            //*�����µĶ����󣬲��ҹҵ�channel��subs_r_tail����β
				{
					raid4->ssd[dev_no]->channel_head[location->channel].subs_r_tail->next_node=pre_read;
					raid4->ssd[dev_no]->channel_head[location->channel].subs_r_tail=pre_read;
				} 
				else
				{
					raid4->ssd[dev_no]->channel_head[location->channel].subs_r_tail=pre_read;
					raid4->ssd[dev_no]->channel_head[location->channel].subs_r_head=pre_read;
				}
				if (parity_write!=NULL)
				{
					parity_write->update=pre_read;
				}

			}

		}
	}

}




__int64 find_nearest_time(struct raid *raid4)
{
	unsigned int i,dev_no;
	__int64 time_t,nearest_time=0x7fffffffffffffff;
	for (i=0;i<raid4->data_disks+raid4->par_disks;i++)
	{
		dev_no=i;
		if ((raid4->raid_state==RECONSTRUCT_STATE||raid4->raid_state==RECONSTRUCT_COMPLETE)&&i==raid4->broken_device_num)
		{
			dev_no=raid4->data_disks+raid4->par_disks;
		}
		time_t=find_nearest_event(raid4->ssd[dev_no]);
		if (time_t<nearest_time)
		{
			nearest_time=time_t;
		}

	}
	return nearest_time;
}



int get_request(struct raid *raid4)
{
	char buffer[200];
	__int64 time_t=0,nearest_event_time=0x7fffffffffffffff;
	int device,  size, ope, i=0,j=0,dev_no;
	unsigned int lsn;
	long filepoint;
	struct request *request1=NULL;


	filepoint = ftell(raid4->tracefile);	
	fgets(buffer, 200, raid4->tracefile); 
	sscanf(buffer,"%I64u %d %d %d %d",&time_t,&device,&lsn,&size,&ope);
	if ((device<0)&&(lsn<0)&&(size<0)&&(ope<0))
	{
		return 100;
	}

	if (lsn<raid4->min_lsn) 
		raid4->min_lsn=lsn;
	if (lsn>raid4->max_lsn)
		raid4->max_lsn=lsn;

	nearest_event_time=find_nearest_time(raid4);
	if (nearest_event_time==0x7fffffffffffffff)
	{
		raid4->current_time=time_t; 
		//if (ssd->request_queue_length>ssd->parameter->queue_length)    //���������еĳ��ȳ����������ļ��������õĳ���                     
		//{
		//printf("error in get request , the queue length is too long\n");
		//}
	}
	else	//nearest_event_time!=0x7fffffffffffffff
	{   
		if(nearest_event_time<time_t) 
		{
			/*******************************************************************************
			*�ع��������û�а�time_t����ssd->current_time����trace�ļ��Ѷ���һ����¼�ع�
			*filepoint��¼��ִ��fgets֮ǰ���ļ�ָ��λ�ã��ع����ļ�ͷ+filepoint��
			*int fseek(FILE *stream, long offset, int fromwhere);���������ļ�ָ��stream��λ�á�
			*���ִ�гɹ���stream��ָ����fromwhere��ƫ����ʼλ�ã��ļ�ͷ0����ǰλ��1���ļ�β2��Ϊ��׼��
			*ƫ��offset��ָ��ƫ���������ֽڵ�λ�á����ִ��ʧ��(����offset�����ļ������С)���򲻸ı�streamָ���λ�á�
			*�ı��ļ�ֻ�ܲ����ļ�ͷ0�Ķ�λ��ʽ���������д��ļ���ʽ��"r":��ֻ����ʽ���ı��ļ�	
			**********************************************************************************/
			fseek(raid4->tracefile,filepoint,0); 
			if(raid4->current_time<=nearest_event_time)
				raid4->current_time=nearest_event_time;
			for (dev_no=0;dev_no<raid4->data_disks+raid4->par_disks+1;dev_no++)
			{
				raid4->ssd[dev_no]->current_time=raid4->current_time;
			}
			return -1;
		}
		else	//nearest_event_time>=time_t
		{

			if (raid4->request_queue_length>=raid4->request_queue_length_max)
			{
				fseek(raid4->tracefile,filepoint,0);
				raid4->current_time=nearest_event_time;
				for (dev_no=0;dev_no<raid4->data_disks+raid4->par_disks+1;dev_no++)
				{
					raid4->ssd[dev_no]->current_time=raid4->current_time;
				}
				return -1;
			} 
			else
			{
				raid4->current_time=time_t;
			}
		}
	}

	for (dev_no=0;dev_no<raid4->data_disks+raid4->par_disks+1;dev_no++)
	{
		raid4->ssd[dev_no]->current_time=raid4->current_time;
	}

	if(time_t < 0)
	{
		printf("error!\n");
		while(1){}
	}

	if(feof(raid4->tracefile)||raid4->raid_state==RECONSTRUCT_COMPLETE)
	{
		return 0;
	}

	if (time_t > raid4->broken_time)
	{
		raid4->raid_state = RECONSTRUCT_STATE;
		for (dev_no=0; dev_no<raid4->data_disks+raid4->par_disks+1;dev_no++)
		{
			raid4->ssd[dev_no]->state=RECONSTRUCT_STATE;
		}
	}

	request1 = (struct request*)malloc(sizeof(struct request));
	alloc_assert(request1,"request");
	memset(request1,0, sizeof(struct request));

	request1->time = time_t;
	request1->lsn = lsn;
	request1->size = size;
	request1->operation = ope;	
	request1->begin_time = time_t;
	request1->response_time = 0;	
	request1->energy_consumption = 0;	
	request1->next_node = NULL;
	request1->distri_flag = 0;              // indicate whether this request has been distributed already
	request1->subs = NULL;
	request1->need_distr_flag = NULL;
	request1->complete_lsn_count=0;         //record the count of lsn served by buffer

	//filepoint = ftell(raid4->tracefile);		// set the file point

	if(raid4->request_queue == NULL)          //The queue is empty
	{
		raid4->request_queue = request1;
		raid4->request_tail = request1;
		raid4->request_queue_length++;
	}
	else
	{			
		(raid4->request_tail)->next_node = request1;	
		raid4->request_tail = request1;			
		raid4->request_queue_length++;
	}

	if (request1->operation==1)             //����ƽ�������С 1Ϊ��,0Ϊд
	{
		raid4->ave_read_size=(raid4->ave_read_size*raid4->read_request_count+request1->size)/(raid4->read_request_count+1);
	} 
	else
	{
		raid4->ave_write_size=(raid4->ave_write_size*raid4->write_request_count+request1->size)/(raid4->write_request_count+1);
	}


	filepoint = ftell(raid4->tracefile);	
	fgets(buffer, 200, raid4->tracefile);    //Ѱ����һ������ĵ���ʱ��
	sscanf(buffer,"%I64u %d %d %d %d",&time_t,&device,&lsn,&size,&ope);
	raid4->next_request_time=time_t;
	fseek(raid4->tracefile,filepoint,0);

	return 1;


}



/************************************************************************/
/* ��������������ֻ���ж������е���������ɼ�����Ϊ�ö��������
д���������Ҫ�ж������ڵ�sh����ɣ���sh�������Ҫ�ж������е�д�������*/
/************************************************************************/
void trace_out_put(struct raid *raid4)
{
	__int64 start_time,end_time;
	struct request *req, *pre_node;
	struct sub_request *sub, *tmp,*tmp_pre_read;
	int flag=0,write_complete_flag=1;
	pre_node=NULL;
	end_time=0;
	req=raid4->request_queue;



	if (req==NULL)
	{
		return;
	}


	while (req!=NULL)	//distribute flag�жϸ������Ƿ񱻼�����delay queue��û�б�distribute
	{
		/************************************************************************/
		/* ������Ϊд����ʱ��ͨ������д����ĸ�������sh_complete��־�Ƿ�Ϊ1��ȷ
		����request�Ƿ���ɣ�����sh_complete��Ϊ1��subʱ,��������һ�μ��
		write_complete_flag���ڱ�־��request�Ƿ���ɣ�1--��ɣ�0--δ���*/
		/************************************************************************/

		flag=1;
		sub=req->subs;
		while(sub != NULL)
		{
			start_time=0;
			if(start_time == 0)
				start_time = sub->begin_time;
			if(start_time > sub->begin_time)
				start_time = sub->begin_time;
			if(end_time < sub->complete_time)
				end_time = sub->complete_time;
			if((sub->current_state == SR_COMPLETE)||((sub->next_state==SR_COMPLETE)&&(sub->next_state_predict_time<=raid4->current_time)))	// if any sub-request is not completed, the request is not completed
			{
				sub = sub->next_subs;
			}
			else
			{
				flag=0;
				break;
			}

		}

		if (flag == 1)	//request is complete
		{		
			fprintf(raid4->outputfile,"%16I64u %10u %6u %2u %16I64u %16I64u %10I64u\n",req->time,	req->lsn,	req->size,	req->operation,	start_time,	end_time,	end_time-req->time);
			fflush(raid4->outputfile);

			if(end_time-start_time==0)
			{
				printf("the response time is 0?? \n");
				getchar();
			}


			if(req->operation==READ)
			{
				raid4->read_request_count++;
				raid4->read_avg=raid4->read_avg+(end_time-req->time);
				if (raid4->raid_state==RECONSTRUCT_STATE)
				{
					raid4->read_request_count_in_rec++;
					raid4->total_read_time_in_rec=raid4->total_read_time_in_rec+(end_time-req->time);
				}
			}
			else	//write
			{
				raid4->write_request_count++;
				raid4->write_avg=raid4->write_avg+(end_time-req->time);
				if (raid4->raid_state==RECONSTRUCT_STATE)
				{
					raid4->write_request_count_in_rec++;
					raid4->total_write_time_in_rec=raid4->total_write_time_in_rec+(end_time-req->time);
				}
			}

			while(req->subs!=NULL)
			{
				tmp = req->subs;
				req->subs = tmp->next_subs;
				if (tmp->update!=NULL)
				{
					free(tmp->update->location);
					tmp->update->location=NULL;
					free(tmp->update);
					tmp->update=NULL;
				}

				//free��pre read
				while (tmp->next_pre_read!=NULL&&tmp->RMW_flag==-1)
				{
					tmp_pre_read=tmp->next_pre_read;
					tmp->next_pre_read=tmp_pre_read->next_pre_read;
					free(tmp_pre_read->location);
					tmp_pre_read->location=NULL;
					free(tmp_pre_read);
					tmp_pre_read=NULL;						
				}
				free(tmp->location);
				tmp->location=NULL;
				free(tmp);
				tmp=NULL;

			}

			/************************************************************************/
			/* ������ɺ󣬴�raid4�����������ժ��������,����free����������Ŀռ�  */
			/************************************************************************/
			if(pre_node==NULL)
			{
				if(req->next_node == NULL)
				{
					free(req->need_distr_flag);
					req->need_distr_flag=NULL;
					free(req);
					req = NULL;
					raid4->request_queue = NULL;
					raid4->request_tail = pre_node;	
					raid4->request_queue_length--;
				}
				else		
				{
					raid4->request_queue=req->next_node;
					pre_node=req;
					req=req->next_node;
					free(req->need_distr_flag);
					req->need_distr_flag=NULL;
					free(pre_node);
					pre_node = NULL;
					raid4->request_queue_length--;
				}
			}
			else		//pre_node��=NULL��֤������֮ǰ����һ��δ��ɵ�����pre_node->next_node��ǰ���ڴ���
			{
				if(req->next_node == NULL) 
				{
					pre_node->next_node = NULL;
					free(req->need_distr_flag);
					req->need_distr_flag=NULL;
					free(req);
					req = NULL;
					raid4->request_tail = pre_node;	
					raid4->request_queue_length--;
				}
				else
				{
					pre_node->next_node = req->next_node;
					free(req->need_distr_flag);
					req->need_distr_flag=NULL;
					free(req);
					req = pre_node->next_node;
					raid4->request_queue_length--;
				}

			}
		}

		else		//flag=0,����������δ��ɣ�����Ϊ������
		{	
			pre_node = req;
			req = req->next_node;
		}

	}

}





struct raid *process(struct raid *raid4)
{
	int i,dev_no;
	int parity_flag;

	parity_flag=0;

	for(i=0;i<raid4->data_disks+raid4->par_disks;i++)
	{
		dev_no=i;
		if (dev_no==raid4->pd_idx)
		{
			parity_flag=1;
		}
		else if (dev_no==raid4->broken_device_num)
		{
			if (raid4->raid_state==RECONSTRUCT_STATE)
			{
				dev_no=raid4->data_disks+raid4->par_disks;
			}		
		}
		ssd_process(raid4,raid4->ssd[dev_no],parity_flag);
	}
	return raid4;
}



/************************************************************************/
/* Ԥ������������Ҫ��ǰ��ֹ����             */
/************************************************************************/

struct raid *pre_process(struct raid *raid4)
{
	int fl=0;
	unsigned int dev_no,dev;
	unsigned int device,lsn,size,ope,lpn,full_page;
	unsigned int largest_lsn,sub_size,ppn,add_size=0;
	unsigned int i=0,j,k;
	int map_entry_new,map_entry_old,modify;
	int flag=0;
	char buffer_request[200];
	struct local *location;
	__int64 time;
	errno_t err;

	printf("\n");
	printf("begin pre_process_page.................\n");

	if((err=fopen_s(&(raid4->tracefile),raid4->tracefilename,"r")) != 0 )      /*��trace�ļ����ж�ȡ����*/
	{
		printf("the trace file can't open\n");
		return NULL;
	}

	full_page=~(0xffffffff<<(raid4->ssd[0]->parameter->subpage_page));
	/*��������ssd������߼�������*/
	//largest_lsn=(unsigned int )(((raid4->ssd[raid4->pd_idx]->parameter->chip_num*raid4->ssd[raid4->pd_idx]->parameter->die_chip*raid4->ssd[raid4->pd_idx]->parameter->plane_die*raid4->ssd[raid4->pd_idx]->parameter->block_plane*raid4->ssd[raid4->pd_idx]->parameter->page_block*raid4->ssd[raid4->pd_idx]->parameter->subpage_page)*(1-raid4->ssd[raid4->pd_idx]->parameter->overprovide)));
	largest_lsn=15099494;
	while(fgets(buffer_request,200,raid4->tracefile))
	{
		sscanf_s(buffer_request,"%I64u %d %d %d %d",&time,&device,&lsn,&size,&ope);
		fl++;
		trace_assert(time,device,lsn,size,ope);                         /*���ԣ���������time��device��lsn��size��ope���Ϸ�ʱ�ͻᴦ��*/

		add_size=0;                                                     /*add_size����������Ѿ�Ԥ����Ĵ�С*/

		if(ope==1)                                                      /*����ֻ�Ƕ������Ԥ������Ҫ��ǰ����Ӧλ�õ���Ϣ������Ӧ�޸�*/
		{
			lsn=lsn%largest_lsn; 
			while(add_size<size)
			{		
				lpn=lsn/raid4->ssd[0]->parameter->subpage_page;
				dev_no=lpn%raid4->data_disks;
				sub_size=raid4->ssd[0]->parameter->subpage_page-(lsn%raid4->ssd[dev_no]->parameter->subpage_page);		
				if(add_size+sub_size>=size)                             /*ֻ�е�һ������Ĵ�СС��һ��page�Ĵ�Сʱ�����Ǵ���һ����������һ��pageʱ������������*/
				{		
					sub_size=size-add_size;		
					add_size+=sub_size;		
				}

				if((sub_size>raid4->ssd[0]->parameter->subpage_page)||(add_size>size))/*��Ԥ����һ���Ӵ�Сʱ�������С����һ��page�����Ѿ�����Ĵ�С����size�ͱ���*/		
				{		
					printf("pre_process sub_size:%d\n",sub_size);		
				}

				/*******************************************************************************************************
				*�����߼�������lsn������߼�ҳ��lpn
				*�ж����dram��ӳ���map����lpnλ�õ�״̬
				*A�����״̬==0����ʾ��ǰû��д����������Ҫֱ�ӽ�ub_size��С����ҳд��ȥд��ȥ
				*B�����״̬>0����ʾ����ǰ��д��������Ҫ��һ���Ƚ�״̬����Ϊ��д��״̬��������ǰ��״̬���ص��������ĵط�
				********************************************************************************************************/

				if(raid4->ssd[dev_no]->dram->map->map_entry[lpn].state==0)                 /*״̬Ϊ0�����*/
				{
					/**************************************************************
					*�������get_ppn_for_pre_process�������ppn���ٵõ�location
					*�޸�ssd����ز�����dram��ӳ���map���Լ�location�µ�page��״̬
					***************************************************************/
					ppn=get_ppn_for_pre_process(raid4->ssd[dev_no],lsn);                  
					location=find_location(raid4->ssd[dev_no],ppn);
					raid4->ssd[dev_no]->program_count++;	
					raid4->ssd[dev_no]->channel_head[location->channel].program_count++;
					raid4->ssd[dev_no]->channel_head[location->channel].chip_head[location->chip].program_count++;		
					raid4->ssd[dev_no]->dram->map->map_entry[lpn].pn=ppn;	
					raid4->ssd[dev_no]->dram->map->map_entry[lpn].state=set_entry_state(raid4->ssd[dev_no],lsn,sub_size);   //0001
					raid4->ssd[dev_no]->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn=lpn;
					raid4->ssd[dev_no]->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state=raid4->ssd[dev_no]->dram->map->map_entry[lpn].state;
					raid4->ssd[dev_no]->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].free_state=(~(raid4->ssd[dev_no]->dram->map->map_entry[lpn].state)&full_page);
					rec_map[lpn].need_reconstruct_flag=raid4->ssd[dev_no]->dram->map->map_entry[lpn].state;

					location=NULL;
				}//if(ssd->dram->map->map_entry[lpn].state==0)
				else if(raid4->ssd[dev_no]->dram->map->map_entry[lpn].state>0)           /*״̬��Ϊ0�����*/
				{
					map_entry_new=set_entry_state(raid4->ssd[dev_no],lsn,sub_size);      /*�õ��µ�״̬������ԭ����״̬���ĵ�һ��״̬*/
					map_entry_old=raid4->ssd[dev_no]->dram->map->map_entry[lpn].state;
					modify=map_entry_new|map_entry_old;
					ppn=raid4->ssd[dev_no]->dram->map->map_entry[lpn].pn;
					location=find_location(raid4->ssd[dev_no],ppn);

					raid4->ssd[dev_no]->program_count++;	
					raid4->ssd[dev_no]->channel_head[location->channel].program_count++;
					raid4->ssd[dev_no]->channel_head[location->channel].chip_head[location->chip].program_count++;		
					raid4->ssd[dev_no]->dram->map->map_entry[lsn/raid4->ssd[0]->parameter->subpage_page].state=modify; 
					raid4->ssd[dev_no]->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state=modify;
					raid4->ssd[dev_no]->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].free_state=((~modify)&full_page);
					rec_map[lpn].need_reconstruct_flag=modify;

					free(location);
					location=NULL;
				}//else if(ssd->dram->map->map_entry[lpn].state>0)
				lsn=lsn+sub_size;                                         /*�¸����������ʼλ��*/
				add_size+=sub_size;                                       /*�Ѿ������˵�add_size��С�仯*/
			}//while(add_size<size)
		}//if(ope==1) 
	}	

	printf("\n");
	printf("pre_process is complete!\n");

	fclose(raid4->tracefile);

	for (dev=0;dev<raid4->data_disks;dev++)
		for(i=0;i<raid4->ssd[dev]->parameter->channel_number;i++)
			for(j=0;j<raid4->ssd[dev]->parameter->die_chip;j++)
				for(k=0;k<raid4->ssd[dev]->parameter->plane_die;k++)
				{
					fprintf(raid4->outputfile,"ssd:%d,chip:%d,die:%d,plane:%d have free page: %d\n",dev,i,j,k,raid4->ssd[dev]->channel_head[i].chip_head[0].die_head[j].plane_head[k].free_page);				
					fflush(raid4->outputfile);
				}

				return raid4;
}








//����parity dev�е�д����ʱ����Ҫ�鿴����Ԥ���Ƿ���ɣ��������Կ�ʼ�����д����
int get_pre_read_flag(struct raid *raid4,struct sub_request *sub)
{
	unsigned int lpn,channel;
	unsigned int i;
	int flag;
	struct local *loc;
	struct sub_request *sub_on_channel;

	flag=0;
	lpn=sub->lpn;
	for (i=0;i<raid4->data_disks;i++)
	{		
		lpn+=i;
		loc = find_location(raid4->ssd[i],raid4->ssd[i]->dram->map->map_entry[lpn].pn);
		channel=loc->channel;
		sub_on_channel=raid4->ssd[i]->channel_head[channel].subs_r_head;
		while (sub_on_channel!=NULL)
		{
			if ((sub_on_channel->lpn==lpn)&&(sub_on_channel->current_state==SR_COMPLETE||(sub_on_channel->next_state==SR_COMPLETE&&sub_on_channel->next_state_predict_time<=raid4->ssd[i]->current_time)))
			{
				flag=0;
				break;
			}
			else if (sub_on_channel->lpn==lpn)
			{
				flag=1;
				break;
			}
			sub_on_channel=sub_on_channel->next_node;

		}
		if (flag==1)
		{
			break;
		}

	}
	return flag;
}





/*******************************************************************************
*statistic_output()������Ҫ�����������һ����������ش�����Ϣ��
*1�������ÿ��plane�Ĳ���������plane_erase���ܵĲ���������erase
*2����ӡmin_lsn��max_lsn��read_count��program_count��ͳ����Ϣ���ļ�outputfile�С�
*3����ӡ��ͬ����Ϣ���ļ�statisticfile��
*******************************************************************************/
void statistic_output(struct raid *raid4)
{
	unsigned int lpn_count=0,i,j,k,m,dev_no,erase=0,plane_erase=0;
	double gc_energy=0.0;
#ifdef DEBUG
	printf("enter statistic_output,  current time:%I64u\n",raid4->current_time);
#endif
	for(dev_no=0;dev_no<raid4->data_disks+raid4->par_disks+1;dev_no++)
	{
		for(i=0;i<raid4->ssd[dev_no]->parameter->channel_number;i++)
		{
			for(j=0;j<raid4->ssd[dev_no]->parameter->die_chip;j++)
			{
				for(k=0;k<raid4->ssd[dev_no]->parameter->plane_die;k++)
				{
					plane_erase=0;
					for(m=0;m<raid4->ssd[dev_no]->parameter->block_plane;m++)
					{
						if(raid4->ssd[dev_no]->channel_head[i].chip_head[0].die_head[j].plane_head[k].blk_head[m].erase_count>0)
						{
							erase=erase+raid4->ssd[dev_no]->channel_head[i].chip_head[0].die_head[j].plane_head[k].blk_head[m].erase_count;
							plane_erase+=raid4->ssd[dev_no]->channel_head[i].chip_head[0].die_head[j].plane_head[k].blk_head[m].erase_count;
						}
					}
					fprintf(raid4->outputfile,"the %d ssd, %d channel, %d chip, %d die, %d plane has : %13d erase operations\n",dev_no,i,j,k,m,plane_erase);
					fprintf(raid4->statisticfile,"the %d ssd,%d channel, %d chip, %d die, %d plane has : %13d erase operations\n",dev_no,i,j,k,m,plane_erase);
				}
			}
		}
	}

	fprintf(raid4->outputfile,"\n");
	fprintf(raid4->outputfile,"\n");
	fprintf(raid4->outputfile,"---------------------------raid statistic data---------------------------\n");
	fprintf(raid4->outputfile,"read request count: %13d\n",raid4->read_request_count);
	fprintf(raid4->outputfile,"write request count: %13d\n",raid4->write_request_count);
	fprintf(raid4->outputfile,"read request average size: %13f\n",raid4->ave_read_size);
	fprintf(raid4->outputfile,"write request average size: %13f\n",raid4->ave_write_size);
	fprintf(raid4->outputfile,"read request average response time: %16I64u\n",raid4->read_avg/raid4->read_request_count);
	fprintf(raid4->outputfile,"write request average response time: %16I64u\n",raid4->write_avg/raid4->write_request_count);
	fprintf(raid4->outputfile,"average response time: %16I64u\n",(raid4->read_avg+raid4->write_avg)/(raid4->write_request_count+raid4->read_request_count));
	fprintf(raid4->outputfile,"min lsn: %13d\n",raid4->min_lsn);	
	fprintf(raid4->outputfile,"max lsn: %13d\n",raid4->max_lsn);
	fprintf(raid4->outputfile,"preread count: %13d\n",raid4->pre_read_count);
	fprintf(raid4->outputfile,"reconstruction complete time: %I64d\n",raid4->rec_complete_time);


	fprintf(raid4->outputfile,"---------------------------ssd statistic data---------------------------\n");	 
	for (dev_no=0;dev_no<raid4->data_disks+raid4->par_disks+1;dev_no++)
	{	
		fprintf(raid4->outputfile,"---------------------------ssd:%d,---------------------------\n",dev_no);
		fprintf(raid4->outputfile,"read count: %13d\n",raid4->ssd[dev_no]->read_count);	  
		fprintf(raid4->outputfile,"program count: %13d",raid4->ssd[dev_no]->program_count);	
		fprintf(raid4->outputfile,"                        include the flash write count leaded by read requests\n");
		fprintf(raid4->outputfile,"the read operation leaded by un-covered update count: %13d\n",raid4->ssd[dev_no]->update_read_count);
		fprintf(raid4->outputfile,"erase count: %13d\n",raid4->ssd[dev_no]->erase_count);
		fprintf(raid4->outputfile,"direct erase count: %13d\n",raid4->ssd[dev_no]->direct_erase_count);
		fprintf(raid4->outputfile,"copy back count: %13d\n",raid4->ssd[dev_no]->copy_back_count);
		fprintf(raid4->outputfile,"multi-plane program count: %13d\n",raid4->ssd[dev_no]->m_plane_prog_count);
		fprintf(raid4->outputfile,"multi-plane read count: %13d\n",raid4->ssd[dev_no]->m_plane_read_count);
		fprintf(raid4->outputfile,"interleave write count: %13d\n",raid4->ssd[dev_no]->interleave_count);
		fprintf(raid4->outputfile,"interleave read count: %13d\n",raid4->ssd[dev_no]->interleave_read_count);
		fprintf(raid4->outputfile,"interleave two plane and one program count: %13d\n",raid4->ssd[dev_no]->inter_mplane_prog_count);
		fprintf(raid4->outputfile,"interleave two plane count: %13d\n",raid4->ssd[dev_no]->inter_mplane_count);
		fprintf(raid4->outputfile,"gc copy back count: %13d\n",raid4->ssd[dev_no]->gc_copy_back);
		fprintf(raid4->outputfile,"write flash count: %13d\n",raid4->ssd[dev_no]->write_flash_count);
		fprintf(raid4->outputfile,"interleave erase count: %13d\n",raid4->ssd[dev_no]->interleave_erase_count);
		fprintf(raid4->outputfile,"multiple plane erase count: %13d\n",raid4->ssd[dev_no]->mplane_erase_conut);
		fprintf(raid4->outputfile,"interleave multiple plane erase count: %13d\n",raid4->ssd[dev_no]->interleave_mplane_erase_count);

		fprintf(raid4->outputfile,"buffer read hits: %13d\n",raid4->ssd[dev_no]->dram->buffer->read_hit);
		fprintf(raid4->outputfile,"buffer read miss: %13d\n",raid4->ssd[dev_no]->dram->buffer->read_miss_hit);
		fprintf(raid4->outputfile,"buffer write hits: %13d\n",raid4->ssd[dev_no]->dram->buffer->write_hit);
		fprintf(raid4->outputfile,"buffer write miss: %13d\n",raid4->ssd[dev_no]->dram->buffer->write_miss_hit);
		fprintf(raid4->outputfile,"erase: %13d\n",erase);
	}
	fflush(raid4->outputfile);
	fclose(raid4->outputfile);

	fprintf(raid4->statisticfile,"\n");
	fprintf(raid4->statisticfile,"\n");
	fprintf(raid4->statisticfile,"---------------------------raid statistic data---------------------------\n");
	fprintf(raid4->statisticfile,"read request count: %13d\n",raid4->read_request_count);
	fprintf(raid4->statisticfile,"write request count: %13d\n",raid4->write_request_count);
	fprintf(raid4->statisticfile,"read request average size: %13f\n",raid4->ave_read_size);
	fprintf(raid4->statisticfile,"write request average size: %13f\n",raid4->ave_write_size);
	fprintf(raid4->statisticfile,"read request average response time: %16I64u\n",raid4->read_avg/raid4->read_request_count);
	fprintf(raid4->statisticfile,"write request average response time: %16I64u\n",raid4->write_avg/raid4->write_request_count);
	fprintf(raid4->statisticfile,"read request average response time before reconstruction: %16I64u\n",raid4->read_avg_in_normal);
	fprintf(raid4->statisticfile,"write request average response time before reconstruction: %16I64u\n",raid4->write_avg_in_normal);
	fprintf(raid4->statisticfile,"request average response time before reconstruction: %16I64u\n",raid4->response_time_in_normal);
	if (raid4->read_request_count_in_rec!=0)
	{
		fprintf(raid4->statisticfile,"read request average response time during reconstruction: %16I64u\n",raid4->total_read_time_in_rec/raid4->read_request_count_in_rec);
	} 
	else
	{
		fprintf(raid4->statisticfile,"read request average response time during reconstruction: 0\n");
	}
	fprintf(raid4->statisticfile,"write request average response time during reconstruction: %16I64u\n",raid4->total_write_time_in_rec/raid4->write_request_count_in_rec);
	fprintf(raid4->statisticfile,"request average response time during reconstruction: %16I64u\n",(raid4->total_read_time_in_rec+raid4->total_write_time_in_rec)/(raid4->read_request_count_in_rec+raid4->write_request_count_in_rec));
	fprintf(raid4->statisticfile,"average response time: %16I64u\n",(raid4->read_avg+raid4->write_avg)/(raid4->write_request_count+raid4->read_request_count));
	fprintf(raid4->statisticfile,"min lsn: %13d\n",raid4->min_lsn);	
	fprintf(raid4->statisticfile,"max lsn: %13d\n",raid4->max_lsn);
	fprintf(raid4->statisticfile,"pre read count: %13d\n",raid4->pre_read_count);
	fprintf(raid4->statisticfile,"reconstruction complete time: %I64d\n",raid4->rec_complete_time);



	for (dev_no=0;dev_no<raid4->data_disks+raid4->par_disks+1;dev_no++)
	{	
		fprintf(raid4->statisticfile,"---------------------------ssd:%d,---------------------------\n",dev_no);
		fprintf(raid4->statisticfile,"read count: %13d\n",raid4->ssd[dev_no]->read_count);	  
		fprintf(raid4->statisticfile,"program count: %13d",raid4->ssd[dev_no]->program_count);	
		fprintf(raid4->statisticfile,"                        include the flash write count leaded by read requests\n");
		fprintf(raid4->statisticfile,"the read operation leaded by un-covered update count: %13d\n",raid4->ssd[dev_no]->update_read_count);
		fprintf(raid4->statisticfile,"erase count: %13d\n",raid4->ssd[dev_no]->erase_count);
		fprintf(raid4->statisticfile,"direct erase count: %13d\n",raid4->ssd[dev_no]->direct_erase_count);
		fprintf(raid4->statisticfile,"copy back count: %13d\n",raid4->ssd[dev_no]->copy_back_count);
		fprintf(raid4->statisticfile,"multi-plane program count: %13d\n",raid4->ssd[dev_no]->m_plane_prog_count);
		fprintf(raid4->statisticfile,"multi-plane read count: %13d\n",raid4->ssd[dev_no]->m_plane_read_count);
		fprintf(raid4->statisticfile,"interleave write count: %13d\n",raid4->ssd[dev_no]->interleave_count);
		fprintf(raid4->statisticfile,"interleave read count: %13d\n",raid4->ssd[dev_no]->interleave_read_count);
		fprintf(raid4->statisticfile,"interleave two plane and one program count: %13d\n",raid4->ssd[dev_no]->inter_mplane_prog_count);
		fprintf(raid4->statisticfile,"interleave two plane count: %13d\n",raid4->ssd[dev_no]->inter_mplane_count);
		fprintf(raid4->statisticfile,"gc copy back count: %13d\n",raid4->ssd[dev_no]->gc_copy_back);
		fprintf(raid4->statisticfile,"write flash count: %13d\n",raid4->ssd[dev_no]->write_flash_count);
		fprintf(raid4->statisticfile,"interleave erase count: %13d\n",raid4->ssd[dev_no]->interleave_erase_count);
		fprintf(raid4->statisticfile,"multiple plane erase count: %13d\n",raid4->ssd[dev_no]->mplane_erase_conut);
		fprintf(raid4->statisticfile,"interleave multiple plane erase count: %13d\n",raid4->ssd[dev_no]->interleave_mplane_erase_count);

		fprintf(raid4->statisticfile,"buffer read hits: %13d\n",raid4->ssd[dev_no]->dram->buffer->read_hit);
		fprintf(raid4->statisticfile,"buffer read miss: %13d\n",raid4->ssd[dev_no]->dram->buffer->read_miss_hit);
		fprintf(raid4->statisticfile,"buffer write hits: %13d\n",raid4->ssd[dev_no]->dram->buffer->write_hit);
		fprintf(raid4->statisticfile,"buffer write miss: %13d\n",raid4->ssd[dev_no]->dram->buffer->write_miss_hit);
		fprintf(raid4->statisticfile,"erase: %13d\n",erase);
	}
	fflush(raid4->statisticfile);
	fclose(raid4->statisticfile);
}


/************************************************************************/
/* set_raid4���ڳ�ʼ��raid4��һЩ����        */
/************************************************************************/
void set_raid4(struct raid *raid4)
{
	//printf("input data disk number:");
	//scanf("%d",&(raid4->data_disks));
	//printf("input parity disk number:");
	//scanf("%d",&(raid4->par_disks));
	raid4->data_disks=4;
	raid4->min_lsn=0x7fffffff;
	raid4->mode=4;
	raid4->par_disks=2;
	raid4->pd_idx=raid4->data_disks;
	raid4->request_queue=NULL;
	raid4->STRIPE_SECTORS=4;
	raid4->request_queue_length_max=3;
	raid4->rec_buf_num=1024;
	raid4->raid_state=NORMAL_STATE;
	raid4->node_size = 64;
	raid4->read_lru_mex_length = 512;
	raid4->broken_time=900000000000;	//���̳��ֹ��ϵ�ʱ��
}


struct ssd_info *ssd_process(struct raid *raid4,struct ssd_info *ssd,int parity_flag)   //parity flag=1��ʾ����parity���Ͻ��д���
{

	/*********************************************************************************************************
	*flag_die��ʾ�Ƿ���Ϊdie��busy��������ʱ��ǰ����-1��ʾû�У���-1��ʾ��������
	*flag_die��ֵ��ʾdie��,old ppn��¼��copyback֮ǰ������ҳ�ţ������ж�copyback�Ƿ���������ż��ַ�����ƣ�
	*two_plane_bit[8],two_plane_place[8]�����Ա��ʾͬһ��channel��ÿ��die��������������
	*chg_cur_time_flag��Ϊ�Ƿ���Ҫ������ǰʱ��ı�־λ������Ϊchannel����busy������������ʱ����Ҫ������ǰʱ�䣻
	*��ʼ��Ϊ��Ҫ��������Ϊ1�����κ�һ��channel�����˴��������������ʱ�����ֵ��Ϊ0����ʾ����Ҫ������
	**********************************************************************************************************/
	int old_ppn=-1,flag_die=-1; 
	unsigned int i,chan,random_num;     
	unsigned int flag=0,new_write=0,chg_cur_time_flag=1,flag2=0,flag_gc=0;       
	__int64 time, channel_time=0x7fffffffffffffff;
	struct sub_request *sub;          

#ifdef debug
	printf("enter process,  current time:%i64u\n",ssd->current_time);
#endif

	/*********************************************************
	*�ж��Ƿ��ж�д�������������ôflag��Ϊ0��û��flag��Ϊ1
	*��flagΪ1ʱ����ssd����gc������ʱ�Ϳ���ִ��gc����
	**********************************************************/
	for(i=0;i<ssd->parameter->channel_number;i++)
	{          
		if((ssd->channel_head[i].subs_r_head==NULL)&&(ssd->channel_head[i].subs_w_head==NULL)&&(ssd->subs_w_head==NULL))
		{
			flag=1;
		}
		else
		{
			flag=0;
			break;
		}
	}
	if(flag==1)
	{
		ssd->flag=1;                                                                
		if (ssd->gc_request>0)                                                            /*ssd����gc����������*/
		{
			gc(ssd,0,1);                                                                  /*���gcҪ������channel�����������*/
		}
		return ssd;
	}
	else
	{
		ssd->flag=0;
	}

	time = ssd->current_time;
	services_2_r_cmd_trans_and_complete(ssd);                                            /*����ǰ״̬��sr_r_c_a_transfer���ߵ�ǰ״̬��sr_complete��������һ״̬��sr_complete������һ״̬Ԥ��ʱ��С�ڵ�ǰ״̬ʱ��*/

	random_num=ssd->program_count%ssd->parameter->channel_number;                        /*����һ�����������֤ÿ�δӲ�ͬ��channel��ʼ��ѯ*/

	/*****************************************
	*ѭ����������channel�ϵĶ�д������
	*���������������д���ݣ�����Ҫռ�����ߣ�
	******************************************/
	for(chan=0;chan<ssd->parameter->channel_number;chan++)	     
	{
		i=(random_num+chan)%ssd->parameter->channel_number;
		flag=0;
		flag_gc=0;                                                                       /*ÿ�ν���channelʱ����gc�ı�־λ��Ϊ0��Ĭ����Ϊû�н���gc����*/
		if((ssd->channel_head[i].current_state==CHANNEL_IDLE)||(ssd->channel_head[i].next_state==CHANNEL_IDLE&&ssd->channel_head[i].next_state_predict_time<=ssd->current_time))		
		{

			if (ssd->gc_request>0)                                                       /*��gc��������Ҫ����һ�����ж�*/
			{
				if (ssd->channel_head[i].gc_command!=NULL)
				{
					flag_gc=gc(ssd,i,0);                                                 /*gc��������һ��ֵ����ʾ�Ƿ�ִ����gc���������ִ����gc���������channel�����ʱ�̲��ܷ�������������*/
				}
				if (flag_gc==1)                                                          /*ִ�й�gc��������Ҫ�����˴�ѭ��*/
				{
					continue;
				}
			}

			sub=ssd->channel_head[i].subs_r_head;                                        /*�ȴ��������*/
			services_2_r_wait(ssd,i,&flag,&chg_cur_time_flag);                           /*�����ڵȴ�״̬�Ķ�������,����֮��flag=1,chg_cur_time_flag=0*/

			if((flag==0)&&(ssd->channel_head[i].subs_r_head!=NULL))                      /*if there are no new read request and data is ready in some dies, send these data to controller and response this request*/		
			{		     
				services_2_r_data_trans(ssd,i,&flag,&chg_cur_time_flag);                    

			}
			if(flag==0)                                                                  /*if there are no read request to take channel, we can serve write requests*/ 		
			{
				/*if(parity_flag==0)*/
				services_2_write(ssd,i,&flag,&chg_cur_time_flag);
				/*		else
				services_2_write_for_par(raid4,raid4->ssd[raid4->pd_idx],i,&flag,&chg_cur_time_flag);*/

			}	
		}	
	}

	return ssd;
}


/************************************************************************/
/* ����һ���ؽ�����д���󣩣��ؽ�����Ϊ�ȶ�ȡ����������룩��Ч�����Ķ�����ϵ����ݺ����
д���µ��滻��,*/
/************************************************************************/
struct sub_request* create_rec_request(struct raid* raid4,unsigned int lpn,struct sub_request* sub_read,unsigned int sub_size,unsigned int sub_state,int priority)
{
	struct sub_request *rec_read,*rec_write;
	unsigned int stripe_num,pd_idx;
	struct local* location;
	unsigned int dev_no;
	int in_buf_flag;

	in_buf_flag = 0;
	stripe_num=lpn/raid4->data_disks;
	pd_idx=stripe_num%raid4->par_disks+raid4->data_disks;

	rec_write=creat_sub_request(raid4->ssd[raid4->data_disks+raid4->par_disks],lpn,sub_size,sub_state,NULL,WRITE);
	rec_write->rec_flag=1;
	rec_write->priority=priority;

	//���ؽ�д�������raid4���ؽ����������
	if (raid4->rec_list == NULL)
	{
		raid4->rec_list=rec_write;
		rec_write->next_rec=NULL;
	}
	else
	{
		rec_write->next_rec=raid4->rec_list;
		raid4->rec_list=rec_write;
	}

	//�ؽ�ʱ�����Ķ�����
	for (dev_no=0;dev_no<raid4->data_disks+1;dev_no++)
	{
		lpn=stripe_num*raid4->data_disks+dev_no;
		if (dev_no == raid4->broken_device_num)
		{
			continue;
		}
		if (dev_no==raid4->data_disks)
		{
			dev_no=pd_idx;
			lpn=stripe_num*raid4->data_disks;
		}

		//in_buf_flag=check_in_rec_buf(raid4,lpn);

		if (raid4->ssd[dev_no]->dram->map->map_entry[lpn].state!=0&&dev_no!=raid4->broken_device_num&&dev_no!=raid4->data_disks&&in_buf_flag==0)
		{

			raid4->ssd[dev_no]->read_count++;
			//raid4->pre_read_count++;

			rec_read=(struct sub_request *)malloc(sizeof(struct sub_request));
			alloc_assert(rec_read,"rec_read");
			memset(rec_read,0, sizeof(struct sub_request));

			if(rec_read==NULL)
			{
				return NULL;
			}
			rec_read->location=NULL;
			rec_read->next_node=NULL;
			rec_read->next_subs=NULL;
			rec_read->update=NULL;						
			location = find_location(raid4->ssd[dev_no],raid4->ssd[dev_no]->dram->map->map_entry[lpn].pn);
			rec_read->location=location;
			rec_read->begin_time = raid4->current_time;
			rec_read->current_state = SR_WAIT;
			rec_read->current_time=0x7fffffffffffffff;
			rec_read->next_state = SR_R_C_A_TRANSFER;
			rec_read->next_state_predict_time=0x7fffffffffffffff;
			rec_read->lpn = lpn;
			rec_read->state=((raid4->ssd[dev_no]->dram->map->map_entry[lpn].state)&sub_state);
			rec_read->size=size(rec_read->state);
			rec_read->ppn = raid4->ssd[dev_no]->dram->map->map_entry[lpn].pn;
			rec_read->operation = READ;
			rec_read->priority=priority;

			if (raid4->ssd[dev_no]->channel_head[location->channel].subs_r_tail!=NULL)            //*�����µĶ����󣬲��ҹҵ�channel��subs_r_tail����β
			{
				raid4->ssd[dev_no]->channel_head[location->channel].subs_r_tail->next_node=rec_read;
				raid4->ssd[dev_no]->channel_head[location->channel].subs_r_tail=rec_read;
			} 
			else
			{
				raid4->ssd[dev_no]->channel_head[location->channel].subs_r_tail=rec_read;
				raid4->ssd[dev_no]->channel_head[location->channel].subs_r_head=rec_read;
			}

			/************************************************************************/
			/* ά��rec write��pre read����     */
			/************************************************************************/
			if (rec_write->next_pre_read==NULL)
			{
				rec_write->next_pre_read=rec_read;
			}
			else
			{
				rec_read->next_pre_read=rec_write->next_pre_read;
				rec_write->next_pre_read=rec_read;
			}


		}
	}

	/************************************************************************/
	/*trig_req�ֶα�ʾ��������rec�����Ķ����� */
	/************************************************************************/
	if (sub_read!=NULL)
	{
		rec_write->trig_req=sub_read;
	}
	raid4->rec_list_length++;
	return rec_write;

}

void init_rec_map(struct raid* raid4)
{
	unsigned int page_num;
	unsigned int i;
	page_num=15099494;
	rec_map = (struct rec_info*)malloc(sizeof(struct rec_info) * page_num);
	alloc_assert(rec_map,"rec_map");
	memset(rec_map,0,sizeof(struct rec_info)*page_num);

	for (i=0;i<page_num;i++)
	{
		rec_map[i].need_reconstruct_flag=-1;
	}
}


//��ʼ�������ؽ�������sub read��Ϊ������ռ䣬�����ص��������������
struct sub_request * init_rec_read(struct ssd_info * ssd,unsigned int lpn,unsigned int sub_size,unsigned int sub_state,struct request * req)
{
	struct sub_request* sub=NULL;

	sub = (struct sub_request*)malloc(sizeof(struct sub_request));                        /*����һ��������Ľṹ*/
	alloc_assert(sub,"sub_request");
	memset(sub,0, sizeof(struct sub_request));

	if(sub==NULL)
	{
		return NULL;
	}
	sub->location=NULL;
	sub->next_node=NULL;
	sub->next_subs=NULL;
	sub->update=NULL;
	sub->rec_flag = 1;

	if(req!=NULL)
	{
		sub->next_subs = req->subs;
		req->subs = sub;
	}

	sub->location = find_location(ssd,ssd->dram->map->map_entry[lpn].pn);
	sub->begin_time = ssd->current_time;
	sub->current_state = SR_WAIT;
	sub->current_time = 0x7fffffffffffffff;
	sub->next_state = SR_R_C_A_TRANSFER;
	sub->next_state_predict_time = 0x7fffffffffffffff;
	sub->lpn = lpn;
	sub->size = size;                                                               /*��Ҫ�������������������С*/


	sub->ppn = ssd->dram->map->map_entry[lpn].pn;
	sub->operation = READ;
	sub->state=(ssd->dram->map->map_entry[lpn].state&0x7fffffff);


	return sub;

}


//��rec_map��ȡ��һ��lpn������current_rec_lpn��Ϊ��Ӧ��ֵ
unsigned int get_rec_lpn(struct raid* raid4)
{
	int page_num;
	int dev_no;

	page_num=15099494/4;

	raid4->current_rec_lpn = raid4->current_rec_lpn+raid4->data_disks;
	while (rec_map[raid4->current_rec_lpn].need_reconstruct_flag<= 0)
	{
		raid4->current_rec_lpn = raid4->current_rec_lpn+raid4->data_disks;
		if(raid4->current_rec_lpn >= page_num)
		{
			raid4->rec_complete_flag=1;
			raid4->rec_complete_time = raid4->current_time;
			raid4->raid_state = RECONSTRUCT_COMPLETE;
			for (dev_no=0; dev_no<raid4->data_disks+raid4->par_disks+1; dev_no++)
			{
				raid4->ssd[dev_no]->state = RECONSTRUCT_COMPLETE;
			}
			//raid4->read_avg_in_rec = raid4->read_avg/raid4->read_request_count;
			//raid4->write_avg_in_rec = raid4->write_avg/raid4->write_request_count;
			break;			
		}
	}
	return raid4->current_rec_lpn;
}



void check_rec_done(struct raid* raid4)
{
	struct sub_request *sub,*tmp_pre_read,*pre_node;
	__int64 start_time,end_time;
	int flag;

	sub=raid4->rec_list;
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


		if (flag == 1) //�Ѿ���ɣ���rec_list����ժ��������
		{
			fprintf(raid4->recfile,"%10u %16I64u %16I64u %10I64u\n",sub->lpn,	sub->begin_time,	sub->complete_time,	sub->complete_time - sub->begin_time);
			rec_map[sub->lpn].need_reconstruct_flag = 0;
			if (sub->trig_req != NULL)
			{
				sub->trig_req->current_state = SR_COMPLETE;
				sub->trig_req->complete_time = sub->complete_time; //�����trig_req�����ʱ������Ϊ��rec_write�����ʱ��һ�£�ʵ���Ͽ��Խ����Ż���trig_req�����ʱ������Ϊrec_write��pre_read�����ʱ��
			}


			//�Ƚ��������pre read���ϵ�����ժ��
			while (sub->next_pre_read!=NULL)
			{
				tmp_pre_read=sub->next_pre_read;
				sub->next_pre_read=tmp_pre_read->next_pre_read;
				free(tmp_pre_read->location);
				tmp_pre_read->location=NULL;
				free(tmp_pre_read);
				tmp_pre_read=NULL;						
			}

			//�ٽ��������rec list��ժ��
			if (pre_node==NULL)
			{
				if (sub->next_rec==NULL)
				{
					raid4->rec_list=NULL;
					free(sub->location);
					sub->location=NULL;
					free(sub);
					sub=NULL;
				}
				else
				{	
					raid4->rec_list=sub->next_rec;
					pre_node=sub;
					free(pre_node->location);
					pre_node->location=NULL;
					free(pre_node);
					pre_node=NULL;
					sub=raid4->rec_list;
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
		raid4->rec_list_length--;
		}		
		else		//flag!=0,δ���
		{
			pre_node=sub;
			sub=sub->next_rec;
		}


	}
}



