#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <termios.h>
#include <curses.h>
#include <term.h>
#include "sw_encoding_package.h"

unsigned short program_pid;
char *rx_p;
int package_cnt=0;              //记录已经接收的数据包
int tcp_client_fd;              //客户端套接字描述符
int server_port;                //服务器端口号
struct sockaddr_in tcp_server;  //存放服务器的IP地址信息
int  rx_len;
fd_set readfds; 				//读事件的文件操作集合
int select_state,rx_cnt; 		//接收返回值
struct sw_socket_package_data rxtx_data; //保存接收和发送的数据
struct sw_socket_ack_package_data ack_data; //保存客户端的应答状态
FILE *new_file=NULL;    			//文件指针
unsigned int  send_ok_byte=0; 	//记录已经接收的字节数量
unsigned int rx_size=0;
unsigned int all_size=0;
pthread_t thread_id;
//记录键值
char *key_value;
int ch = 0;
//计算接收速度
time_t time1;   //时间1
time_t time2; 	//时间2
unsigned int send_byte_cnt;  //记录上一次的字节数据
char state=1;   //状态值
double speed=0.0;   //保存速度
char file_name[1024]; //存放文件的路径

//设置socket缓冲区大小
int snd_size = 0; /* 发送缓冲区大小 */
int rcv_size = 0; /* 接收缓冲区大小 */
socklen_t optlen; /* 选项值长度 */
int err = -1; 	  /* 返回值 */
unsigned char mbuff[10*188 * 1024];

//检测按键
static struct termios initial_settings, new_settings;
static int peek_character = -1;
void init_keyboard();
void close_keyboard();
int kbhit();
int readch();

// int video_pid=641;

//sw_parse参数
#define TS_PATH "/root/recv/shoes.ts" //TS文件的绝对路径
//读一个TS流的packet
void read_ts_packet(FILE *file_handle,unsigned char *packet_buf,int len); 
//分析TS流，并找出PAT的PID和PAT的table
int parse_ts(unsigned char *buffer,int file_size);	
//分析PAT，并找出所含频道的数目和PMT的PID
void parse_pat(unsigned char *buffer,int len);	
//分析PAT，并找出所含频道的数目和PMT的PID
void pronum_pmtid_printf(); 
//找出PMT的table
unsigned char* find_pmt_table(unsigned short pmt_pid); 
//解析PMT，找出其中的Video和Audio的PID
void parse_pmt(unsigned char *buffer,int len,unsigned short pmt_pid);
//打印PMT table中包含的stream的类型和PID
void printf_program_list(); 
//找出Video或者Audio的table
unsigned char* find_video_audio(unsigned short program_pid,unsigned char type); 

//用来存储节目号和节目表pid
typedef struct
{
    unsigned short program_num;
    unsigned short pmt_pid; 
}stru_program;

//用来保存流类型和各个类型的pid
typedef struct
{
    unsigned char stream_type;
    unsigned short elementary_pid;
}stru_pro_list;

stru_program programs[10] = {{0,0}}; //用来存储PMT的PID和数量
unsigned int num = 0; //total program
stru_pro_list program_list[10] = {{0,0}};	//用来存储PMT中stream的类型和PID
unsigned int program_list_num = 0;//节目表数量
FILE *file_handle;	//指向TS流的指针
unsigned int file_size = 0;

//read one TS packet's data
void read_ts_packet(FILE *file_handle,unsigned char *packet_buf,int len)
{
    fread(packet_buf,188,1,file_handle);
}
//解析TS包
int parse_ts(unsigned char *buffer,int file_size)
{
    unsigned char *temp = buffer;
    short pat_pid;
    int i = 0;

    if(buffer[0] != 0x47)
    {
        printf("it's not a ts packet!\n");
        return 0;
    }
    while(temp < buffer + file_size)
    {
        pat_pid = (temp[1] & 0x1f)<<8 | temp[2];//13
        if(pat_pid != 0)
       		printf("finding PAT table ....\n");
        else
        {
            printf("already find the PAT table\n");
            printf("pat_pid = 0x%x\n",pat_pid);
            printf("pat table ------->\n");
            for(i=0;i<=187;i++)
            {
               printf("0x%x ",buffer[i]);
            }
            printf("\n");
            return 1;
        }
        temp = temp + 188;
    }
    return 0;
}

//parse PAT table, get the PMT's PID
void parse_pat(unsigned char *buffer,int len)
{
    unsigned char *temp, *p;
    char adaptation_control;
    int adaptation_length,i=0;
    unsigned short section_length,prg_No,PMT_Pid;

    temp = buffer;
    adaptation_control = temp[3] & 0x30;
	//...
    if(adaptation_control == 0x10)
        temp = buffer + 4 + 1;
    else if (adaptation_control == 0x30)
    {
        adaptation_length = buffer[4];
        temp = buffer + 4 + 1 +adaptation_length + 1;
    }
    else
    {
        return ;
    }
    section_length = (temp[1]&0x0f)<<8 | temp[2];

    p = temp + 1 +section_length;
    temp = temp + 8;

    while(temp < p - 4)
    {
        prg_No = (temp[0]<<8) | (temp[1]);

        if(prg_No == 0)
        {
            temp = temp + 4;
            continue;
        }
        else
        {
            PMT_Pid = (temp[2]&0x1f)<<8 | temp[3];

            programs[num].program_num = prg_No;
            programs[num].pmt_pid = PMT_Pid;
            //	printf("pmt_pid is ox%x\n", PMT_Pid);
            num ++;

            temp = temp + 4;
        }
    }
}

void pronum_pmtid_printf()
{
    unsigned int i;
    printf("PAT table's program_num and PMT's PID:\n");
    for(i=0;i<num;i++)
    {
        printf("program_num = 0x%x (%d),PMT_Pid = 0x%x (%d)\n",
        programs[i].program_num,programs[i].program_num,
        programs[i].pmt_pid,programs[i].pmt_pid);
    }
}

void printf_program_list()
{
    unsigned int i;
    printf("All PMT Table's program list: \n");

    for(i=0;i<program_list_num;i++)
    {
        printf("stream_type = 0x%x, elementary_pid = 0x%x\n",program_list[i].stream_type,program_list[i].elementary_pid);
    }
    printf("\n");
}

unsigned char* find_pmt_table(unsigned short pmt_pid)
{
    unsigned int i=0,j=0;
    int pid;
    unsigned char *buffer;
    buffer = (unsigned char *)malloc(sizeof(char)*188);
    memset(buffer,0,sizeof(char)*188);

    rewind(file_handle);
    for(j=0;j<file_size/188;j++)
    {
        read_ts_packet(file_handle,buffer,188);
        if(buffer[0] != 0x47)
        {
            printf("It's not TS packet !\n");
        }
        else
        {
            pid = (buffer[1] & 0x1f)<< 8 | buffer[2];
            if(pid == pmt_pid)
            {
                printf("PMT Table already find!\n");
                return buffer;
            }
            else
            printf("finding PMT table.......\n");
        }
    }
}

unsigned char* find_video_audio(unsigned short program_pid,unsigned char type)
{
    unsigned int i = 0, j = 0 ;
    int pid;
    unsigned char *buffer;

    buffer = (unsigned char *)malloc(sizeof(char)*188);
    memset(buffer,0,sizeof(char)*188);
    rewind(file_handle);
    for(j=0;j<file_size/188;j++)
    {
        read_ts_packet(file_handle,buffer,188);
        if(buffer[0] != 0x47)
        {
            printf("It's not TS packet !\n");
        }
        else
        {
            pid = (buffer[1] & 0x1f)<< 8 | buffer[2];
            if(program_pid == pid)
            {
                if(type == 0x02)
                   printf("Find a program and this program is Video type!\n");
                // else if(type == 0x03)
                //     printf("Find a program and this program is Audio type!\n");
                // else
                //     printf("Find a program but this program is other type !\n");

                return buffer;
            }
            else
                printf("finding Video or Audio table.....\n ");
        }
    }
}
//解析PMT表
void parse_pmt(unsigned char *buffer,int len,unsigned short pmt_pid)
{
    unsigned char *temp, *p;
    char adaptation_control;
    int adaptation_length,i=0;
    int program_info_length;
    int es_info_length;
    unsigned short section_length,pid;
    temp = buffer;

    adaptation_control = temp[3] & 0x30;
    if(adaptation_control == 0x10)
    {
        temp = buffer + 4 +1;
    }
    else if (adaptation_control == 0x30)
    {
        adaptation_length = buffer[4];
        temp = buffer + 5 + adaptation_length + 1; 
    }
    else
        return;
    section_length = (temp[1]&0x0f)<<8 | temp[2];
    p = temp + 1 + section_length;
    //	temp = temp + 10;
    program_info_length = (temp[10] & 0x0f) << 8 | temp[11];

    temp = temp + 12 + program_info_length ;

    for(;temp < p - 4;)
    {
        program_list[program_list_num].stream_type = temp[0],
        program_list[program_list_num].elementary_pid = (temp[1]&0x1f) << 8 | temp[2];
        es_info_length = (temp[3]&0x0f) << 8 | temp[4];
        temp = temp + 4 + es_info_length + 1;
        program_list_num ++ ;
    }
}
//提取PES 
int sw_ts_to_pes(char *tsfile,char *pesfile,unsigned short pid)
{
	FILE *fpd,*fp;
	int start = 0;
	int size,num,total;
	int count = 0;
	unsigned char *p,*payload;
	unsigned short Pid;
	unsigned char Lcounter = 0xff;
	unsigned char adapcontrol;
	unsigned char counter; //计数器
	
	fp = fopen( tsfile, "rb");
	fpd = fopen( pesfile, "wb");
	if(fp == NULL || fpd == NULL )
		return -1;
	//初始化设置值大小
	total = 0; 
	num = 0;
	size = 0;
	
	while( ! feof( fp ))
	{
		size = fread(mbuff, 1,sizeof(mbuff), fp);
		p = mbuff;

		do{
			Pid = (((p[1] & 0x1f)<<8) | p[2]); //获取pid
			adapcontrol = (p[3]>>4)&0x3 ;  //判断自适应区是否有可调整字段
			counter = p[3]&0xf ; //提取连续计数器
			if( Pid == pid)
			{
				payload = NULL;
				switch(adapcontrol)
				{
				case 0: //保留 
				case 2:
					break;  //2为只有调整无负载
				case 1:
					payload = p + 4;  //无调整字段
					break;
				case 3:
					payload = p + 5 + p[4];  //调整字段后是净荷
					break;
				}
				
				if((p[1] & 0x40)!= 0 )//取出有效荷载单元起始指示符，确定TS,pes数据开始
				{
					start = 1;
				}
				
				if(start && payload && fpd)//往fpd写pes包数据
				{
					fwrite(payload, 1,p+188-payload, fpd);
				}
				
				if( Lcounter != 0xff && ((Lcounter + 1)&0xf) != counter)//判断数据是否丢失
				{
					count++;
				}
				Lcounter = counter ; 
			}
			p += 188;
			total += 188;
		}while(p<mbuff+size);
	}
	fclose( fp );
	fclose( fpd );
	printf( "PES包分离完！\n" );
	return 0;
}
//提取ES
int sw_pes_to_es( char *pesfile, char *esfile )
{
	//定义文件描述符
	FILE *fpd, *fp;
	unsigned char *p, *payload, *tmp;
	int size, num, rdsize;
	unsigned int last = 0;
	long long total = 0, wrsize = 0;
	unsigned int Lenght;
	unsigned char PES_extension_flag;
	unsigned char PES_header_data_Lenghtgth;
    unsigned char PTS_DTS_flags;
	int k=0;
	
	fp = fopen( pesfile, "rb" );  
	fpd = fopen( esfile, "wb" );

	if( fp == NULL || fpd == NULL )
		return -1;
	
	num = 0;
	size = 0;
	p = mbuff;
	
	while(1)
	{
		REDO:
		if( mbuff + size <= p )
		{
			p = mbuff;
			size = 0;
		}
		else if( mbuff < p && p < mbuff + size )
		{
			size -= p - mbuff;
			memmove(mbuff, p, size );
			p = mbuff;
		}
		
		if( !feof(fp) && size < sizeof(mbuff) )
		{
			rdsize = fread( mbuff+size, 1, sizeof(mbuff)-size, fp );
			size += rdsize;
			total += rdsize;
		}
		if( size <= 0 )
			break;
		
		tmp = p;
		// 寻找PES-HEADER: 0X000001E0 
		while( p[0] != 0 || p[1] != 0 || p[2] != 0x01 ||
				( ( p[3] & 0xe0 ) != 0xe0 ))
		{
			p++;
			if( mbuff + size <= p )
				goto REDO;
		}
		// PES_packet_Lenghtgth 
		Lenght = (p[4]<<8) | p[5];
		if( Lenght == 0 )
		{
			unsigned char *end = p + 6;
			while( end[0] != 0 || end[1] != 0 || end[2] != 0x01 ||( ( end[3] & 0xe0 ) != 0xe0 ) )
			{
				if( mbuff + size <= end )
				{
					if( feof(fp) )
						break;	
					goto REDO;
				}
				end++;
			}
			Lenght = end - p - 6;
		}
		if( mbuff + size < p + 6 + Lenght )
		{
			if( feof(fp) )
				break;
			continue;
		}
		p += 6;
		p++;
		PTS_DTS_flags = (*p>>6)&0x3;
		PES_extension_flag = (*p)&0x1;
		p++;
		PES_header_data_Lenghtgth = *p;
		p++;
		payload = p + PES_header_data_Lenghtgth;

			if (PTS_DTS_flags == 0x2 )//'10'
			{
				unsigned int pts;
				pts = (*p>>1) & 0x7;
				pts = pts << 30;
				p++;
				pts += (*p)<<22;
				p++;
				pts += ((*p)>>1)<<15;
				p++;
				pts += (*p)<<7;
				p++;
				pts += (*p)>>1;
				p++;
				p -= 5;

				if(last!=0)	k=k+pts-last;
		//		if( pts < last) 
		//		{
		//			printf( "?\n" );
		//		}
				last = pts;
			}
			else if( PTS_DTS_flags == 0x3 )//'11'
			{
				unsigned int pts, dts;

				pts = (*p>>1) & 0x7;
				pts = pts << 30;
				p++;
				pts += (*p)<<22;
				p++;
				pts += ((*p)>>1)<<15;
				p++;
				pts += (*p)<<7;
				p++;
				pts += (*p)>>1;
				p++;

				dts = (*p>>1) & 0x7;
				dts = dts << 30;
				p++;
				dts += (*p)<<22;
				p++;
				dts += ((*p)>>1)<<15;
				p++;
				dts += (*p)<<7;
				p++;
				dts += (*p)>>1;
				p++;

				p -= 10;
				if(last!=0)
					k=k+pts-last;
			//	if( pts < last )
			//	{
			//	printf( "?\n" );
			//	}
				last = pts;
			}
			else if( PTS_DTS_flags != 0 )
			{
				printf( "error\n" );
			}

		if( fpd )
		{
			fwrite( payload, 1, Lenght - 3 - PES_header_data_Lenghtgth, fpd );
		}
		num++;
		p += Lenght - 3;

		payload = p;
		size -= p - mbuff;
		memmove( mbuff, p, size );
		p = mbuff;
	}
	
	fclose( fp );
	fclose( fpd );
	printf("视屏流的PES的个数为：%d\n",num);
	printf("pes平均时间间隔为：%d\n",k/(num*90));
	printf("ES分离完！\n");
	return 0;
} 

//提取I帧，B帧，P帧
int es2iframe(char *esfile, char *ifile )
{
	FILE *fes, *fi;
	int size, Lenght;
	unsigned char *p, *PI=NULL,*s=NULL;
	unsigned char picture_coding_type;
	int nsqueue, niframe;
	int npframe=0;
	int nbframe=0;
	//printf( "Begin get iframe...\n" );
	// 打开ES文件 和 IFRAME文件 
	fes = fopen( esfile, "rb" );
	fi = fopen( ifile, "wb" );
	
	if( NULL == fes || NULL == fi )
	{
		printf( "error: open file error!\n" );
		return -1;
	}
	
	size = 0;
	nsqueue = 0;
	niframe = 0;
	while( !feof( fes ) )
	{
		/* 读入ES文件，size表示当前缓存中还有多少字节数据 */
		Lenght = fread( mbuff+size, 1, sizeof(mbuff) - size, fes );
		
		p = mbuff;
		PI = NULL; /* 指向Sequence 或 Picture 的开始位置 */
		
		while( p+6 < mbuff+size+Lenght )
		{
			/* 寻找前缀0x000001 */
			if( 0x00 == p[0] && 0x00 == p[1] && p[2]==  0x01 )
			{
				/* 以Sequence header与Picture header 为I帧 */
				if( p[3]==0xB3   ||p[3] ==  0x00 )
				{				 
			    	if(s!=NULL)
					{
						fwrite( s, 1, p-s, fi );
						s=NULL;
					}
				}
				/* Sequence header 的开始代码 0xB3 */
				if( 0xB3 == p[3] )
				{
					nsqueue++;
					s=p;
				}
				/* Picture header 的开始代码 0x00 */
				picture_coding_type = (p[5]>>3) & 0x7; /* 帧类型, 第42-44位 */
				if( 0x00 == p[3] && 1 == picture_coding_type )
				{
					niframe++;
                    s=p;
				}
				if( 0x00 == p[3] && 2 == picture_coding_type )
				{
					npframe++;
				}
				if( 0x00 == p[3] && 3 == picture_coding_type )
				{
					nbframe++;
				}
			}
			p++;
		}
		/* 确定缓存中未处理数据的大小, 并移至缓存的开始处 */
		if( NULL == PI )
		{
			size = mbuff+Lenght+size - p;
			memmove( mbuff, p, size );
		}
		else
		{
			size = mbuff+Lenght+size - PI;
			memmove( mbuff, PI, size );
		}
	}
	printf("vedio sequence个数是：%d\n",nsqueue);
	printf("i fream个数是：%d\n",niframe);
	printf("p fream个数是：%d\n",npframe);
	printf("b fream个数是：%d\n",nbframe);
	fclose( fes );
	fclose( fi );
	printf( "I帧提取完成！\n" );
	return 0;
}

/*
处理异常信号
*/
void sighandler(int a)
{
	printf("触发SIGSEGV信号: 产生段错误! 信号值:%d\n",a);
	exit(-1); //退出进程  return
}
void sighandler3(int a)
{
	return;
}

//下面是 用于按键检测 引用的接口函数
void init_keyboard()
{
	tcgetattr(0, &initial_settings);
	new_settings = initial_settings;
	new_settings.c_lflag &= ~ICANON;
	new_settings.c_lflag &= ~ECHO;
	new_settings.c_lflag &= ~ISIG;
	new_settings.c_cc[VMIN] = 1;
	new_settings.c_cc[VTIME] = 0;
	tcsetattr(0,TCSANOW, &new_settings);
}

void close_keyboard()
{
	tcsetattr(0,TCSANOW, &initial_settings);
}

int kbhit()
{
	char ch;
	int nread;

	if(peek_character != -1)
	{
		return -1;
	}
	new_settings.c_cc[VMIN] = 0;
	tcsetattr(0, TCSANOW, &new_settings);
	nread = read(0, &ch, 1);
	new_settings.c_cc[VMIN] = 1;
	tcsetattr(0,TCSANOW, &new_settings);

	if(nread == 1)
	{
		peek_character = ch;
		return 1;
	}
	return 0;
}

int readch()
{
	char ch;
	if(peek_character != -1)
	{
		ch = peek_character;
		peek_character = -1;
		return ch;
	}
	read (0, &ch, 1);
	return ch;
}
//上面是用于按键检测引用的接口函数

//检测按键是否按下 线程处理函数
void *sw_key_scan(void *arg)
{
	init_keyboard();
	while(1)
	{
		while(ch != 'q')
		{
			// printf("looping\n");
			sleep(1);
			if(kbhit())
			{
				ch = readch();	
				printf("you hit %c\n",ch);
			}
		}
	}
	// close_keyboard();
	exit(0);
}
//提取video_pid
int sw_find_video_pid(void)
{
	unsigned char buffer[188] = {0};
    unsigned char *pmt_buffer, *video_or_audio_buffer; 
    unsigned int i=0,j=0,ret=0;

    pmt_buffer = (unsigned char*)malloc(sizeof(char)*188); //给buffer分配空间
    memset(pmt_buffer,0,sizeof(char)*188);	//清空buffer

    video_or_audio_buffer = (unsigned char*)malloc(sizeof(char)*188);
    memset(video_or_audio_buffer,0,sizeof(char)*188);

    file_handle = fopen(TS_PATH,"rb+"); //以二进制方式打开TS文件

    if(NULL == file_handle) //判断是否打开文件
    {
        perror("fopen");
        printf("open file error!\n");
        return 0;
    }
    else 
        printf("open file success!\n");

    fseek(file_handle,0,SEEK_END);	//指针file_handle将以SEEK_END位置偏移0个位置，即将指针移动到文件尾	
    file_size = ftell(file_handle); // 计算file_handle到文件头的偏移字节数，即计算文件的大小

    printf("file size = %d\n",file_size);

    rewind(file_handle); // equivalent (void) feek(file_handle,0L,SEEK_SET) 将file_handle指针移动到文件头位置

    printf("find PAT begin-------->\n");

    for(i=0;i<file_size/188;i++)
    {
        read_ts_packet(file_handle,buffer,188); //读TS的packet函数，每次读188个字节到buffer
        ret = parse_ts(buffer,188);	//解析188个字节的TS's packet，并打印找到的PAT’s table。如果解析成功即找到PAT，则返回 1，否则返回0

        if(ret == 1)
        {
            break;
        }
        else
        {
            printf("There is no PAT table!\n");
        }
    }
    if(ret == 1)
    {
        parse_pat(buffer,188);	//解析PAT，并找出所含频道的数目和PMT的PID
    }
    pronum_pmtid_printf(); //打印PMT的PID
    rewind(file_handle);
    printf("find PMT begin -------->\n");
    for(i=0;i<num;i++)
    {
        pmt_buffer = find_pmt_table(programs[i].pmt_pid); //根据PMT的PID找到PMT's table

        printf("PMT table -------->\n");
        for(j=0;j<188;j++)
        {
            printf("0x%x ",pmt_buffer[j]); //打印PMT
        }
        if(pmt_buffer)
        {
            parse_pmt(pmt_buffer,188,programs[i].pmt_pid); //解析找到的PMT，得到Video、Audio等的PID
        }
        memset(pmt_buffer,0,sizeof(char)*188);
        printf("\n");
    }
    printf_program_list();	//打印elementary流的PID和type。
    rewind(file_handle);

    printf("find Audio and Video begin-------->\n");
    for(i=0;i<program_list_num;i++)
    {
		//根据PID找到elementary流-ES
        video_or_audio_buffer = find_video_audio(program_list[i].elementary_pid, 
        											program_list[i].stream_type); 
													

        printf("the program's PID is 0x%x\n",program_list[i].elementary_pid);
        printf("the program's Table --------->\n");
		printf("i=%d/n",i);
        for(j=0;j<188;j++)
        {
            printf("0x%x ",video_or_audio_buffer[j]); //打印elementary's table
        }
        memset(video_or_audio_buffer,0,sizeof(char)*188);
        printf("\n");
    }
    free(pmt_buffer);
    free(video_or_audio_buffer);
    pmt_buffer = NULL;
    video_or_audio_buffer = NULL;
    fclose(file_handle);
    printf("\n");
}

int main(int argc,char **argv)
{
	//捕获段错误信号-内存溢出产生的错误
	signal(SIGSEGV,sighandler);
	
	if(pthread_create(&thread_id,NULL,sw_key_scan,NULL)==0)
	{
		/*设置分离属性，让线程结束之后自己释放资源*/
		pthread_detach(thread_id);
	}
	if(argc!=4)
	{
		printf("客户端形参格式:./tcp_client <服务器IP地址>  <服务器端口号>  <文件存放的目录>\n");
		return -1;
	}
	
	server_port=atoi(argv[2]); //将字符串的端口号转为整型
	
	/*1. 创建网络套接字*/
	tcp_client_fd=socket(AF_INET,SOCK_STREAM,0);
	if(tcp_client_fd<0)
	{
		printf("客户端提示:服务器端套接字创建失败!\n");
		goto ERROR;
	}

	/*
	* 设置发送缓冲区大小
	*/
	snd_size = 20*1024; /* 发送缓冲区大小为*/
	optlen = sizeof(snd_size);
	err = setsockopt(tcp_client_fd, SOL_SOCKET, SO_SNDBUF, &snd_size, optlen);
	if(err<0)
	{
		printf("服务器提示:设置发送缓冲区大小错误\n");
	}
	
	/*
	* 设置接收缓冲区大小
	*/
	rcv_size = 20*1024; /* 接收缓冲区大小*/
	optlen = sizeof(rcv_size);
	err = setsockopt(tcp_client_fd,SOL_SOCKET,SO_RCVBUF, (char *)&rcv_size, optlen);
	if(err<0)
	{
		printf("服务器提示:设置接收缓冲区大小错误\n");
	}
	
	/*2. 连接到指定的服务器*/
	tcp_server.sin_family=AF_INET; //IPV4协议类型
	tcp_server.sin_port=htons(server_port);//端口号赋值,将本地字节序转为网络字节序
	tcp_server.sin_addr.s_addr=inet_addr(argv[1]); //IP地址赋值
	
	if(connect(tcp_client_fd,(const struct sockaddr*)&tcp_server,sizeof(const struct sockaddr))<0)
	{
		 printf("客户端提示: 连接服务器失败!\n");
		 goto ERROR;
	}
	
	rx_p=(char*)&rxtx_data; //指针
	rx_size=sizeof(struct sw_socket_package_data);
	all_size=0;

	while(1)
	{
		// signal(SIGINT,sighandler2);
		// read(stdin,key_value,size);
		if(ch=='p')
		{
			printf("暂停成功，按下\'C\'键继续下载\n");
			while(1)
			{
				// read(stdin,key_value,size);
				if(ch=='c')
				{
					printf("程序开始继续下载,按下\'p\'键暂停下载\n");
					break;
				}
			}
		}

		/*5.1 清空文件操作集合*/
		FD_ZERO(&readfds);
        /*5.2 添加要监控的文件描述符*/
		FD_SET(tcp_client_fd,&readfds);
		/*5.3 监控文件描述符*/
		select_state=select(tcp_client_fd+1,&readfds,NULL,NULL,NULL);
		if(select_state>0)//表示有事件产生
		{
			/*5.4 测试指定的文件描述符是否产生了读事件*/
			if(FD_ISSET(tcp_client_fd,&readfds))
			{
				/*5.5 读取数据*/
				rx_cnt=read(tcp_client_fd,rx_p,rx_size);
				if(rx_cnt>0) //收到不完整的数据
				{
					all_size+=rx_cnt; //记录收到的字节数量
					//收到数据包
					if(all_size>=sizeof(struct sw_socket_package_data))
					{
						printf("rx_cnt=%d,all_size=%d\n",rx_cnt,all_size);
						
						//出现错误的的处理方法
						if(all_size!=sizeof(struct sw_socket_package_data))
						{
							printf("all_size值超出限制=%d\n",all_size);
							all_size=0;
							rx_size=sizeof(struct sw_socket_package_data); //总大小归位
							rx_p=(char*)&rxtx_data; //指针归位
							ack_data.ack_stat=0x81; //表示接收失败
							printf("all_size的值恢复正常=%d\n",all_size);
							continue;  //结束本次循环
						}
						
						all_size=0; //当前已经接收的字节归0
						rx_size=sizeof(struct sw_socket_package_data); //总大小归位
						rx_p=(char*)&rxtx_data; //指针归位
						
						/*校验数据包是否正确*/
						if(sw_check_data_package(&rxtx_data)==0)
						{
							//判断之前是否已经接收到相同的一次数据包了,如果接收过就不需要再继续写入到文件
							//原因: 可能服务器没有收到客户端发送的应答,触发了数据重发
							if(package_cnt!=rxtx_data.num_cnt) 
							{
								printf("包编号:%d,有效数据:%d\n",rxtx_data.num_cnt,rxtx_data.current_size);
								package_cnt=rxtx_data.num_cnt; //记录上一次的接收编号
								
								if(rxtx_data.num_cnt==1)  //表示第一次接收数据包
								{
									printf("第一次接收数据包\n");
									strcpy(file_name,argv[3]); //拷贝路径  /123/456.c
									strcat(file_name,rxtx_data.file_name); //文件名称
									new_file=fopen(file_name,"wb"); //创建文件
									printf("filename=%s\n",file_name);

									if(new_file==NULL)
									{
										printf("客户端提示: %s 文件创建失败!\n",file_name);
										// fclose(new_file);
										goto ERROR; //退出连接
									}
									//第%d包向%s文件写入
									if(fwrite(rxtx_data.SrcDataBuffer,1,rxtx_data.current_size,new_file)!=rxtx_data.current_size)
									{
										printf("客户端提示: 第%d包向%s文件写入失败!\n",rxtx_data.num_cnt,file_name);
										fclose(new_file);
										goto ERROR; //退出连接
									}
								}
								else  //继续接收数据包
								{
									if(fwrite(rxtx_data.SrcDataBuffer,1,rxtx_data.current_size,new_file)!=rxtx_data.current_size)
									{
										printf("客户端提示: 第%d包向%s文件写入失败!\n",rxtx_data.num_cnt,file_name);
										fclose(new_file);
										goto ERROR; //退出连接
									}
								}
								send_ok_byte+=rxtx_data.current_size; 	//记录已经接收的字节数量
								

								printf("客户端接收进度提示: 总大小:%d字节,已接收:%d字节,百分比:%0.0f%%\n",rxtx_data.file_size,send_ok_byte,send_ok_byte/1.0/rxtx_data.file_size*100.0);
							}
							//接收到数据包之后向服务器回发应答信号
							ack_data.ack_stat=0x80; //表示接收成功
							if(write(tcp_client_fd,&ack_data,sizeof(struct sw_socket_ack_package_data))!=sizeof(struct sw_socket_ack_package_data))
							{
								printf("客户端提示: 向服务器应答失败!");
							}
							//判断数据是否接收完毕
							if(rxtx_data.current_size!=sizeof(rxtx_data.SrcDataBuffer))
							{
								printf("客户端提示:文件接收成功!\n");
								break; //退出接收
							}
						}
						else
						{
							ack_data.ack_stat=0x81; //表示接收失败
							if(write(tcp_client_fd,&ack_data,sizeof(struct sw_socket_ack_package_data))!=
														sizeof(struct sw_socket_ack_package_data))
							{
								printf("客户端提示: 向服务器应答失败!");
							}
							printf("客户端提示:校验数据包不正确\n");
						}
					}
					else
					{
						rx_size=sizeof(struct sw_socket_package_data)-rx_cnt;
						rx_p+=rx_cnt; //偏移文件指针
					}
				}
				
				if(rx_cnt==0)
				{
					printf("客户端提示:服务器已经断开连接!\n");
					if(new_file != NULL)
						fclose(new_file);
					break;
				}
			}
		}
		else if(select_state<0) //表示产生了错误
		{
			printf("客户端提示:select函数产生异常!\n");
			break;
		}
	}
	shutdown(tcp_client_fd,SHUT_WR);  //TCP半关闭，保证缓冲区内的数据全部写完
	//提取video_pid
	sw_find_video_pid();	
	//提取I帧
  	sw_ts_to_pes("./recv/shoes.ts","./recv/shoes.pes",program_list[0].elementary_pid);
 	sw_pes_to_es("./recv/shoes.pes","./recv/shoes.es");
  	es2iframe( "./recv/shoes.es", "./recv/shoes.i");

ERROR:	
	/*4. 关闭连接*/
	//close(tcp_client_fd);
	shutdown(tcp_client_fd,SHUT_WR);  //TCP半关闭，保证缓冲区内的数据全部写完

	return 0;
}


