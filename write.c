/*fifo_write.c*/  
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>  
#include <unistd.h>  
#include <limits.h> 
#include <fcntl.h> 
#include "vmm.h"
#define FIFO_SERVER "/tmp/server"  
int main(int argc, char* argv[])
{
	int fd;
	int type;
	Ptr_MemoryAccessRequest ptr_memAccReq;
	int num;
	srandom(time(NULL));
	ptr_memAccReq = (Ptr_MemoryAccessRequest) malloc(sizeof(MemoryAccessRequest));
	while(1)
	{
		printf("Please enter page number:\n");
		scanf("%u",&num);
	ptr_memAccReq->virAddr = num % VIRTUAL_MEMORY_SIZE;
	printf("Please enter the instruction type: 1:read 2:write 3:execute\n");
	scanf("%d",&type);
	printf("Enter the process number: 0 or 1\n");
	scanf("%d",&ptr_memAccReq->proccessNum);
	switch (type)
	{
		case 1: //读请求
		{
			ptr_memAccReq->reqType = REQUEST_READ;
			printf("产生请求：\n地址：%u\t进程号：%u\t类型：读取\n", ptr_memAccReq->virAddr,ptr_memAccReq->proccessNum);
			break;
		}
		case 2: //写请求
		{
			ptr_memAccReq->reqType = REQUEST_WRITE;
			/* 随机产生待写入的值 */
			ptr_memAccReq->value = random() % 0xFFu;
			printf("产生请求：\n地址：%u\t进程号：%u\t类型：写入\t值：%02X\n", ptr_memAccReq->virAddr, ptr_memAccReq->proccessNum,ptr_memAccReq->value);
			break;
		}
		case 3:
		{
			ptr_memAccReq->reqType = REQUEST_EXECUTE;
			printf("产生请求：\n地址：%u\t进程号：%u\t类型：执行\n", ptr_memAccReq->virAddr,ptr_memAccReq->proccessNum);
			break;
		}
		default:
			break;
	}	

	if((fd=open(FIFO_SERVER,O_WRONLY))<0){
		printf("12332414\n");
		printf("open fifo filed\n");
	}
	if(write(fd, ptr_memAccReq, sizeof(MemoryAccessRequest)) < 0)  
		printf("write failed!\n");  
		close(fd);
			
	}
}
