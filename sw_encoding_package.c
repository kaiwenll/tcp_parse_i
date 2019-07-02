#include "sw_encoding_package.h"

/*
函数功能:   封装数据包的数据
函数形参:   *datapack :存放数据包结构体的地址
函数返回值: 0表示成功 其他值表示失败
*/
int sw_set_data_package(struct sw_socket_package_data *datapack)
{
	/*1. 封装帧头*/
	datapack->frame_head[0]=0xA1;
	datapack->frame_head[1]=0xA2;
	datapack->frame_head[2]=0xA3;
	datapack->frame_head[3]=0xA4;
	
	/*2. 计算校验和*/
	datapack->check_sum=0;
	int i;
	for(i=0;i<sizeof(datapack->SrcDataBuffer)/sizeof(datapack->SrcDataBuffer[0]);i++)
	{
		datapack->check_sum+=datapack->SrcDataBuffer[i];
	}
}
/*
函数功能: 校验数据包是否正确
函数形参:   data :校验的数据包结构
函数返回值: 0表示成功 其他值表示失败
*/
int sw_check_data_package(struct sw_socket_package_data *datapack)
{
	unsigned int check_sum=0;
	int i;
	/*1. 判断帧头是否正确*/
	if(datapack->frame_head[0]!=0xA1|| datapack->frame_head[1]!=0xA2||
	   	datapack->frame_head[2]!=0xA3||datapack->frame_head[3]!=0xA4)
	{
		return -1;
	}
	/*2. 判断校验和*/
	for(i=0;i<sizeof(datapack->SrcDataBuffer)/sizeof(datapack->SrcDataBuffer[0]);i++)
	{
		check_sum+=datapack->SrcDataBuffer[i];
	}
	if(check_sum!=datapack->check_sum)
	{
		return -1;
	}
	return 0;
}
