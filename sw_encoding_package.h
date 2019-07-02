#ifndef ENCODEINGPACKAGE_H
#define ENCODEINGPACKAGE_H

//定义socket文件数据传输的结构体
#pragma pack(1)
struct sw_socket_package_data
{
	unsigned char frame_head[4]; //帧头固定为0xA1 0xA2 0xA3 0xA4
	char file_name[50];          //存放文件名称
	char SrcDataBuffer[1024*10];//源数据
	unsigned int num_cnt;        //记录接收的编号数量
	unsigned int file_size;      //文件总大小
	unsigned int current_size;   //当前数据包里的的字节大小
	unsigned int check_sum;      //检验和
};


//定义应答结构,报告文件是否接收成功
struct sw_socket_ack_package_data
{
	unsigned int ack_stat; //应答状态 0x80表示接收成功 0x81表示接收失败
};

//数据包的封装与校验
int sw_set_data_package(struct sw_socket_package_data *datapack);
int sw_check_data_package(struct sw_socket_package_data *datapack);
#endif
