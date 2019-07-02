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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "sw_encoding_package.h"

char g_send_file_name[50]; //存放发送的文件名称


/*
函数功能:  TCP客户端处理函数
*/
void *sw_socket_pthread_func(void *arg)
{
	/*1. 定义相关需要使用的变量*/
	unsigned int  rx_cnt;
	unsigned int  send_ok_byte=0; 	   //已经发送成功的字节数
	struct sw_socket_package_data rxtx_data; //保存接收和发送的数据
	struct sw_socket_ack_package_data ack_data; //保存客户端的应答状态
	fd_set readfds;   				     //select读事件的文件操作集合
	struct timeval timeout;            	 //select超时时间值
	int select_state; 				   	 //接收select返回值
	int tcp_client_fd=*(int*)arg;      //客户端套接字描述符
	free(arg);      			       //释放占用的空间
	
	//计算接收速度
	time_t time1;   //时间1
	time_t time2; 	//时间2
	unsigned int send_byte_cnt;  //记录上一次的字节数据
	char state=1;   //状态值
	double speed=0.0;   //保存速度
	
	/*2. 打开将要发送的文件*/
	FILE *fp=fopen(g_send_file_name,"rb");
	if(fp==NULL)
	{
		printf("服务器提示:%s文件打开失败!\n",g_send_file_name);
		goto ERROR;
	}
	
	/*3. 获取文件的状态信息*/
	struct stat file_info_buf;
	if(stat(g_send_file_name,&file_info_buf))
	{
		printf("服务器提示:文件信息获取失败!\n");
		goto ERROR;
	}

	/*3. 进入到文件发送状态*/
	memset(&rxtx_data,0,sizeof(struct sw_socket_package_data));//初始化结构体
	rxtx_data.file_size=file_info_buf.st_size; //文件总大小
	strcpy(rxtx_data.file_name,g_send_file_name); //文件名称 "\123\ 456.c"
	while(1)
	{
		/*4. 文件数据封装发送*/
		//读取文件数据
		rx_cnt=fread(rxtx_data.SrcDataBuffer,1,sizeof(rxtx_data.SrcDataBuffer),fp);
		rxtx_data.num_cnt++; //包数量
		rxtx_data.current_size=rx_cnt; //当前读取的字节数
		sw_set_data_package(&rxtx_data);   //结构体数据封包
		send_ok_byte+=rx_cnt; 		 //记录已经发送的字节数量
		printf("服务器发送进度提示: 总大小:%d字节,已发送:%d字节,百分比:%.0f%%\n",rxtx_data.file_size,send_ok_byte,send_ok_byte/1.0/rxtx_data.file_size*100.0);
							
SEND_SRC_DATA: //触发重发数据的标签
		write(tcp_client_fd,&rxtx_data,sizeof(struct sw_socket_package_data)); //发送数据
		
		/*计算接收的速度*/
		time1=time(NULL);   //获取时间1
		if(state)
		{
			state=0;
			time2=time1; 	//保存时间1
			send_byte_cnt=send_ok_byte;  //记录上一次的字节数据
		}
		
		if(time1-time2>=1) //1秒时间到达
		{
			state=1;
			speed=(send_ok_byte-send_byte_cnt)*1.0/1024; //按每秒KB算
		}
		
		if(speed>1024) //大于1024字节
		{
			printf("实际接收速度:%0.2f mb/s\n",speed*1.0/1024);
		}
		else
		{
			printf("接收速度:%0.2f kb/s\n",speed);
		}
		
		/*5. 等待客户端的回应*/
WAIT_ACK: //触发继续等待客户端应答
		FD_ZERO(&readfds);
		FD_SET(tcp_client_fd,&readfds);
		timeout.tv_sec=5;  //超时时间
		timeout.tv_usec=0;
		
		select_state=select(tcp_client_fd+1,&readfds,NULL,NULL,&timeout);
		if(select_state>0)//表示有事件产生
		{
			//测试指定的文件描述符是否产生了读事件
			if(FD_ISSET(tcp_client_fd,&readfds))
			{
				//读取数据
				rx_cnt=read(tcp_client_fd,&ack_data,sizeof(struct sw_socket_ack_package_data));
				if(rx_cnt==sizeof(struct sw_socket_ack_package_data))//等于4的情况
				{
					if(ack_data.ack_stat!=0x80)
					{
						goto SEND_SRC_DATA;	//客户端接收失败，触发重发
					}
					else
					{
						//判断文件是否读取完毕
						if(rx_cnt!=sizeof(struct sw_socket_ack_package_data))
						{
							printf("服务器提示:文件发送成功!\n");
							break;
						}
					}
				}
				else if(rx_cnt>0)//大于0不等于4
				{
					printf("服务器提示:数据包大小接收不正确!\n");
					goto WAIT_ACK; //重新等待应答
				}
				if(rx_cnt==0)
				{
					if(send_ok_byte==rxtx_data.file_size)
					{
						printf("服务器提示:文件发送成功!\n");
					}
					printf("服务器提示:客户端已经断开连接!\n");
					break;
				}
			}
		}
		else if(select_state==0) //接收客户端的应答超时,上一包数据需要重发
		{
			goto SEND_SRC_DATA;
		}
		else //表示产生了错误
		{
			printf("服务器提示:select函数产生异常!\n");
			break;
		}
	}
	
ERROR:	
	/*6. 关闭连接*/
	fclose(fp);
	close(tcp_client_fd);
	pthread_exit(NULL);
}


/*
TCP服务器创建
*/
int main(int argc,char **argv)
{
	int tcp_server_fd;       //服务器套接字描述符
	int *tcp_client_fd=NULL; //客户端套接字描述符
	struct sockaddr_in tcp_server;
	struct sockaddr_in tcp_client;
	socklen_t tcp_client_addrlen=0;
	int tcp_server_port;  //服务器的端口号
	
	 //判断传入的参数是否合理
	if(argc!=3)
	{
		printf("参数格式:./tcp_server <端口号> <file_name>\n");
		return -1;
	}	
	strcpy(g_send_file_name,argv[2]);  //存放传入的文件名称	
	tcp_server_port=atoi(argv[1]); //将字符串转为整数
	
	/*1. 创建网络套接字*/
	tcp_server_fd=socket(AF_INET,SOCK_STREAM,0);
	if(tcp_server_fd<0)
	{
		printf("TCP服务器端套接字创建失败!\n");
		goto ERROR;
	}
	
	int snd_size = 0; /* 发送缓冲区大小 */
	int rcv_size = 0; /* 接收缓冲区大小 */
	socklen_t optlen; /* 选项值长度 */
	int err = -1;     /* 返回值 */
	/*
	* 设置发送缓冲区大小
	*/
	snd_size = 20*1024; /* 发送缓冲区大小为*/
	optlen = sizeof(snd_size);
	err = setsockopt(tcp_server_fd, SOL_SOCKET, SO_SNDBUF, &snd_size, optlen);
	if(err<0)
	{
		printf("服务器提示:设置发送缓冲区大小错误\n");
	}
	
	/*
	* 设置接收缓冲区大小
	*/
	rcv_size = 20*1024; /* 接收缓冲区大小*/
	optlen = sizeof(rcv_size);
	err = setsockopt(tcp_server_fd,SOL_SOCKET,SO_RCVBUF, (char *)&rcv_size, optlen);
	if(err<0)
	{
		printf("服务器提示:设置接收缓冲区大小错误\n");
	}

	/*2. 绑定端口号,创建服务器*/
	tcp_server.sin_family=AF_INET; //IPV4协议类型
	tcp_server.sin_port=htons(tcp_server_port);//端口号赋值,将本地字节序转为网络字节序
	tcp_server.sin_addr.s_addr=INADDR_ANY; //将本地IP地址赋值给结构体成员
	
	if(bind(tcp_server_fd,(const struct sockaddr*)&tcp_server,sizeof(struct sockaddr))<0)
	{
		printf("TCP服务器端口绑定失败!\n");
		goto ERROR;
	}
	
	/*3. 设置监听的客户端数量*/
	if(listen(tcp_server_fd,100))
	{
		printf("监听数量设置失败!\n");
		goto ERROR;
	}
	
	/*4. 等待客户端连接*/
	pthread_t thread_id;
	
	while(1)
	{
		tcp_client_addrlen=sizeof(struct sockaddr);
		// tcp_client_fd=malloc(sizeof(int)); //申请空间
		*tcp_client_fd=accept(tcp_server_fd,(struct sockaddr *)&tcp_client,&tcp_client_addrlen);
		if(*tcp_client_fd<0)
		{
			printf("TCP服务器:等待客户端连接失败!\n");
		}
		else
		{
			//打印连接的客户端地址信息
		    printf("客户端上线: %s:%d\n",inet_ntoa(tcp_client.sin_addr),ntohs(tcp_client.sin_port));
			/*1. 创建线程*/
			if(pthread_create(&thread_id,NULL,sw_socket_pthread_func,(void*)tcp_client_fd)==0)
			{
				/*2. 设置分离属性，让线程结束之后自己释放资源*/
				pthread_detach(thread_id);
			}
		}
	}
	
	//关闭套接字
ERROR:
	close(tcp_server_fd);
	return 0;
}

