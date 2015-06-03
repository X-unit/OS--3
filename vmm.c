#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>  
#include <unistd.h>  
#include <limits.h> 
#include <fcntl.h> 
#include "vmm.h"

/* 页表 */
//PageTableItem pageTable[PAGE_SUM];
PageTableItem pageTable[4][16];
/* 实存空间 */
BYTE actMem[ACTUAL_MEMORY_SIZE];
/* 用文件模拟辅存空间 */
FILE *ptr_auxMem;
/* 物理块使用标识 */
BOOL blockStatus[BLOCK_SUM];
/* 访存请求 */
Ptr_MemoryAccessRequest ptr_memAccReq;



/* 初始化环境 */
void do_init()
{
	int i, j,k;
	srandom(time(NULL));
	for (i = 0; i < 4; i++)
	{
		for(k=0; k<16;k++)
		{
			pageTable[i][k].pageNum = i;
			pageTable[i][k].filled = FALSE;
			pageTable[i][k].edited = FALSE;
			pageTable[i][k].count = 0;
			pageTable[i][k].time = 0;
		if(i<2)
			pageTable[i][k].proccessNum=0;
		else
			pageTable[i][k].proccessNum=1;
		/* 使用随机数设置该页的保护类型 */
		switch (random() % 7)
		{
			case 0:
			{
				pageTable[i][k].proType = READABLE;
				break;
			}
			case 1:
			{
				pageTable[i][k].proType = WRITABLE;
				break;
			}
			case 2:
			{
				pageTable[i][k].proType = EXECUTABLE;
				break;
			}
			case 3:
			{
				pageTable[i][k].proType = READABLE | WRITABLE;
				break;
			}
			case 4:
			{
				pageTable[i][k].proType = READABLE | EXECUTABLE;
				break;
			}
			case 5:
			{
				pageTable[i][k].proType = WRITABLE | EXECUTABLE;
				break;
			}
			case 6:
			{
				pageTable[i][k].proType = READABLE | WRITABLE | EXECUTABLE;
				break;
			}
			default:
				break;
		}
		/* 设置该页对应的辅存地址 */
		pageTable[i][k].auxAddr = i * PAGE_SIZE * 2;
	}
}
	for (j = 0; j < BLOCK_SUM; j++)
	{
		/* 随机选择一些物理块进行页面装入 */
		if (random() % 2 == 0)
		{
			do_page_in(&pageTable[j/16][j%16], j);
			pageTable[j/16][j%16].blockNum = j;
			pageTable[j/16][j%16].filled = TRUE;
			pageTable[j/16][j%16].time = 1;
			blockStatus[j] = TRUE;
		}
		else
			blockStatus[j] = FALSE;
	
}
}


/* 响应请求 */
void do_response()
{
	Ptr_PageTableItem ptr_pageTabIt;
	unsigned int pageNum, offAddr;
	unsigned int actAddr;
	

	/* 检查地址是否越界 */
	if (ptr_memAccReq->virAddr < 0 || ptr_memAccReq->virAddr >= VIRTUAL_MEMORY_SIZE)
	{
		do_error(ERROR_OVER_BOUNDARY);
		return;
	}
	
	/* 计算页号和页内偏移值 */
	pageNum = ptr_memAccReq->virAddr / PAGE_SIZE;
	offAddr = ptr_memAccReq->virAddr % PAGE_SIZE;
	printf("页目录为：%u\t页号为：%u\t页内偏移为：%u\n", pageNum/16,pageNum%16, offAddr);

	/* 获取对应页表项 */
	ptr_pageTabIt = &pageTable[pageNum /16][pageNum % 16];

	if(ptr_memAccReq->proccessNum != ptr_pageTabIt->proccessNum)
	{
		do_error(ERROR_INVALID_REQUEST);
		return;
	}
	/* 根据特征位决定是否产生缺页中断 */
	if (!ptr_pageTabIt->filled)
	{
		do_page_fault(ptr_pageTabIt);
	}
	
	actAddr = ptr_pageTabIt->blockNum * PAGE_SIZE + offAddr;
	printf("实地址为：%u\n", actAddr);
	
	/* 检查页面访问权限并处理访存请求 */
	switch (ptr_memAccReq->reqType)
	{
		case REQUEST_READ: //读请求
		{
			ptr_pageTabIt->count++;
			if (!(ptr_pageTabIt->proType & READABLE)) //页面不可读
			{
				do_error(ERROR_READ_DENY);
				return;
			}
			/* 读取实存中的内容 */
			printf("读操作成功：值为%02X\n", actMem[actAddr]);
			break;
		}
		case REQUEST_WRITE: //写请求
		{
			ptr_pageTabIt->count++;
			if (!(ptr_pageTabIt->proType & WRITABLE)) //页面不可写
			{
				do_error(ERROR_WRITE_DENY);	
				return;
			}
			/* 向实存中写入请求的内容 */
			actMem[actAddr] = ptr_memAccReq->value;
			ptr_pageTabIt->edited = TRUE;			
			printf("写操作成功\n");
			printf("da yin shi cun:\n");
			printActMem();
			break;
		}
		case REQUEST_EXECUTE: //执行请求
		{
			ptr_pageTabIt->count++;
			if (!(ptr_pageTabIt->proType & EXECUTABLE)) //页面不可执行
			{
				do_error(ERROR_EXECUTE_DENY);
				return;
			}			
			printf("执行成功\n");
			break;
		}
		default: //非法请求类型
		{	
			do_error(ERROR_INVALID_REQUEST);
			return;
		}
	}
}

/* 处理缺页中断 */
void do_page_fault(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int i;
	printf("产生缺页中断，开始进行调页...\n");
	for (i = 0; i < BLOCK_SUM; i++)
	{
		if (!blockStatus[i])
		{
			/* 读辅存内容，写入到实存 */
			do_page_in(ptr_pageTabIt, i);
			
			/* 更新页表内容 */
			ptr_pageTabIt->blockNum = i;
			ptr_pageTabIt->filled = TRUE;
			ptr_pageTabIt->edited = FALSE;
			ptr_pageTabIt->count = 0;
			
			blockStatus[i] = TRUE;
			return;
		}
	}
	/* 没有空闲物理块，进行页面替换 */
	//do_LFU(ptr_pageTabIt);
	do_fifo(ptr_pageTabIt);
}

/* 根据LFU算法进行页面替换 */
void do_LFU(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int i, min, page;
	printf("没有空闲物理块，开始进行LFU页面替换...\n");
	for (i = 0, min = 0xFFFFFFFF, page = 0; i < PAGE_SUM; i++)
	{
		if (pageTable[i/16][i%16].count < min)
		{
			min = pageTable[i/16][i%16].count;
			page = i;
		}
	}
	printf("选择第%u页进行替换\n", page);
	if (pageTable[page/16][page%16].edited)
	{
		/* 页面内容有修改，需要写回至辅存 */
		printf("该页内容有修改，写回至辅存\n");
		do_page_out(&pageTable[page/16][page%16]);
	}
	pageTable[page/16][page%16].filled = FALSE;
	pageTable[page/16][page%16].count = 0;


	/* 读辅存内容，写入到实存 */
	do_page_in(ptr_pageTabIt, pageTable[page/16][page%16].blockNum);
	
	/* 更新页表内容 */
	ptr_pageTabIt->blockNum = pageTable[page/16][page%16].blockNum;
	ptr_pageTabIt->filled = TRUE;
	ptr_pageTabIt->edited = FALSE;
	ptr_pageTabIt->count = 0;
	printf("页面替换成功\n");
}

void do_fifo(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int i, max, page;
	printf("没有空闲物理块，开始进行LFU页面替换...\n");
	for (i = 0, max = 0, page = 0; i < PAGE_SUM; i++)
	{
		if (pageTable[i/16][i%16].time > max)
		{
			max = pageTable[i/16][i%16].time;
			page = i;
		}
	}
	printf("选择第%u页进行替换\n", page);
	if (pageTable[page/16][page%16].edited)
	{
		/* 页面内容有修改，需要写回至辅存 */
		printf("该页内容有修改，写回至辅存\n");
		do_page_out(&pageTable[page/16][page%16]);
	}
	pageTable[page/16][page%16].filled = FALSE;
	pageTable[page/16][page%16].count = 0;


	/* 读辅存内容，写入到实存 */
	do_page_in(ptr_pageTabIt, pageTable[page/16][page%16].blockNum);
	
	/* 更新页表内容 */
	ptr_pageTabIt->blockNum = pageTable[page/16][page%16].blockNum;
	ptr_pageTabIt->filled = TRUE;
	ptr_pageTabIt->edited = FALSE;
	ptr_pageTabIt->count = 0;
	printf("页面替换成功\n");
}

/* 将辅存内容写入实存 */
void do_page_in(Ptr_PageTableItem ptr_pageTabIt, unsigned int blockNum)
{
	unsigned int readNum;
	if (fseek(ptr_auxMem, ptr_pageTabIt->auxAddr, SEEK_SET) < 0)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
#endif
		do_error(ERROR_FILE_SEEK_FAILED);
		exit(1);
	}
	if ((readNum = fread(actMem + blockNum * PAGE_SIZE, 
		sizeof(BYTE), PAGE_SIZE, ptr_auxMem)) < PAGE_SIZE)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
		printf("DEBUG: blockNum=%u\treadNum=%u\n", blockNum, readNum);
		printf("DEGUB: feof=%d\tferror=%d\n", feof(ptr_auxMem), ferror(ptr_auxMem));
#endif
		do_error(ERROR_FILE_READ_FAILED);
		exit(1);
	}
	printf("调页成功：辅存地址%u-->>物理块%u\n", ptr_pageTabIt->auxAddr, blockNum);
}

/* 将被替换页面的内容写回辅存 */
void do_page_out(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int writeNum;
	if (fseek(ptr_auxMem, ptr_pageTabIt->auxAddr, SEEK_SET) < 0)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt, ftell(ptr_auxMem));
#endif
		do_error(ERROR_FILE_SEEK_FAILED);
		exit(1);
	}
	if ((writeNum = fwrite(actMem + ptr_pageTabIt->blockNum * PAGE_SIZE, 
		sizeof(BYTE), PAGE_SIZE, ptr_auxMem)) < PAGE_SIZE)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
		printf("DEBUG: writeNum=%u\n", writeNum);
		printf("DEGUB: feof=%d\tferror=%d\n", feof(ptr_auxMem), ferror(ptr_auxMem));
#endif
		do_error(ERROR_FILE_WRITE_FAILED);
		exit(1);
	}
	printf("写回成功：物理块%u-->>辅存地址%03X\n", ptr_pageTabIt->auxAddr, ptr_pageTabIt->blockNum);
}

/* 错误处理 */
void do_error(ERROR_CODE code)
{
	switch (code)
	{
		case ERROR_READ_DENY:
		{
			printf("访存失败：该地址内容不可读\n");
			break;
		}
		case ERROR_WRITE_DENY:
		{
			printf("访存失败：该地址内容不可写\n");
			break;
		}
		case ERROR_EXECUTE_DENY:
		{
			printf("访存失败：该地址内容不可执行\n");
			break;
		}		
		case ERROR_INVALID_REQUEST:
		{
			printf("访存失败：非法访存请求\n");
			break;
		}
		case ERROR_OVER_BOUNDARY:
		{
			printf("访存失败：地址越界\n");
			break;
		}
		case ERROR_FILE_OPEN_FAILED:
		{
			printf("系统错误：打开文件失败\n");
			break;
		}
		case ERROR_FILE_CLOSE_FAILED:
		{
			printf("系统错误：关闭文件失败\n");
			break;
		}
		case ERROR_FILE_SEEK_FAILED:
		{
			printf("系统错误：文件指针定位失败\n");
			break;
		}
		case ERROR_FILE_READ_FAILED:
		{
			printf("系统错误：读取文件失败\n");
			break;
		}
		case ERROR_FILE_WRITE_FAILED:
		{
			printf("系统错误：写入文件失败\n");
			break;
		}
		default:
		{
			printf("未知错误：没有这个错误代码\n");
		}
	}
}

/* 产生访存请求 */
void do_request()
{

	/* 随机产生请求地址 */
	ptr_memAccReq->virAddr = random() % VIRTUAL_MEMORY_SIZE;

	ptr_memAccReq->proccessNum = random() % 2;
	/* 随机产生请求类型 */
	switch (random() % 3)
	{
		case 0: //读请求
		{
			ptr_memAccReq->reqType = REQUEST_READ;
			printf("产生请求：\n地址：%u\t进程号：%u\t类型：读取\n", ptr_memAccReq->virAddr,ptr_memAccReq->proccessNum);
			break;
		}
		case 1: //写请求
		{
			ptr_memAccReq->reqType = REQUEST_WRITE;
			/* 随机产生待写入的值 */
			ptr_memAccReq->value = random() % 0xFFu;
			printf("产生请求：\n地址：%u\t进程号：%u\t类型：写入\t值：%02X\n", ptr_memAccReq->virAddr, ptr_memAccReq->proccessNum,ptr_memAccReq->value);
			break;
		}
		case 2:
		{
			ptr_memAccReq->reqType = REQUEST_EXECUTE;
			printf("产生请求：\n地址：%u\t进程号：%u\t类型：执行\n", ptr_memAccReq->virAddr,ptr_memAccReq->proccessNum);
			break;
		}
		default:
			break;
	}	
}
void do_requestbyhand()
{
	int type;
	ptr_memAccReq->virAddr = random() % VIRTUAL_MEMORY_SIZE;
	printf("Please enter the instruction type: 1:read 2:write 3:execute\n");
	scanf("%d",&type);
	printf("Enter the process number: 0 or 1\n");
	scanf("%d",&ptr_memAccReq->proccessNum);
	switch (type)
	{
		case 0: //读请求
		{
			ptr_memAccReq->reqType = REQUEST_READ;
			printf("产生请求：\n页目录：%u\t页号：%u\t类型：读取\n", ptr_memAccReq->virAddr/16,ptr_memAccReq->virAddr%16);
			break;
		}
		case 1: //写请求
		{
			ptr_memAccReq->reqType = REQUEST_WRITE;
			/* 随机产生待写入的值 */
			ptr_memAccReq->value = random() % 0xFFu;
			printf("产生请求：\n页目录：%u\t页号：%u\t类型：写入\t值：%02X\n", ptr_memAccReq->virAddr/16,ptr_memAccReq->virAddr%16, ptr_memAccReq->value);
			break;
		}
		case 2:
		{
			ptr_memAccReq->reqType = REQUEST_EXECUTE;
			printf("产生请求：\n页目录：%u\t页号：%u\t类型：执行\n", ptr_memAccReq->virAddr/16,ptr_memAccReq->virAddr%16);
			break;
		}
		default:
			break;
	}	
}


/* 打印页表 */
void do_print_info()
{
	unsigned int i, j, k;
	char str[4];
	printf("页目录\t页号\t块号\t装入\t修改\t保护\t计数\t辅存\tproccessNum\n");
	for (i = 0; i < PAGE_SUM; i++)
	{
		printf("%u\t%u\t%u\t%u\t%u\t%s\t%u\t%u\t%u\n", i/16,i%16, pageTable[i/16][i%16].blockNum, pageTable[i/16][i%16].filled, 
			pageTable[i/16][i%16].edited, get_proType_str(str, pageTable[i/16][i%16].proType), 
			pageTable[i/16][i%16].count, pageTable[i/16][i%16].auxAddr,pageTable[i/16][i%16].proccessNum);
	}
}

/* 获取页面保护类型字符串 */
char *get_proType_str(char *str, BYTE type)
{
	if (type & READABLE)
		str[0] = 'r';
	else
		str[0] = '-';
	if (type & WRITABLE)
		str[1] = 'w';
	else
		str[1] = '-';
	if (type & EXECUTABLE)
		str[2] = 'x';
	else
		str[2] = '-';
	str[3] = '\0';
	return str;
}
void addtime()
{
	int i,k;
	for(i=0;i<4;i++){
		for(k=0;k<16;k++)
		{
			if(pageTable[i][k].filled)
				pageTable[i][k].time++;
		}
	}
}
void printActMem()
{
	int i=0;
	for(i=0;i<128;i++)
	{
		if((i%8)==0)
			printf("\n");
		printf("%u\t",actMem[i]);
		
	}
}
void print_v_Mem()
{
	//ptr_auxMem
	int i;
	int j;
	
	BYTE buf[264];
	fgets(buf,256,ptr_auxMem);
	for(i=0;i<256;)
	{
		if((i%4)==0)
			printf("\n");
		printf("%u\t%u\t%u\t%u\t",buf[i++],buf[i++],buf[i++],buf[i++]);
	}
}
int main(int argc, char* argv[])
{
	char c;
	int i;
	char yes;
	int fd;
	int count;
	struct stat statbuf;
	
	if (!(ptr_auxMem = fopen(AUXILIARY_MEMORY, "r+")))
	{
		do_error(ERROR_FILE_OPEN_FAILED);
		exit(1);
	}
	
	do_init();
	do_print_info();
	ptr_memAccReq = (Ptr_MemoryAccessRequest) malloc(sizeof(MemoryAccessRequest));

	printf("da yin fu cun :\n");
	print_v_Mem();


	/* FIFO*/
	if(stat("/tmp/server",&statbuf)==0)
	{
		if(remove("/tmp/server")<0)
			printf("remove failed\n");
	}
	if(mkfifo("/tmp/server",0666)<0)
	{
		printf("mkfifo failed\n");
	}
	


	/* 在循环中模拟访存请求与处理过程 */
	while (TRUE)
	{
		/*
		printf("random  ?\n");
		scanf("%c",&yes);
		if( yes== 'y')
		do_request();
		else
			do_requestbyhand();

			*/
		addtime();
		printf("Waiting FIFO....\n");
		if((fd=open("/tmp/server",O_RDONLY))<0)
		printf("open fifo filled");

	if((count=read(fd,ptr_memAccReq,sizeof(MemoryAccessRequest)))<0)
		printf("wrong\n");


		printf("虚拟地址:%u\t\n",ptr_memAccReq->virAddr);
		do_response();
		
		printf("按Y打印页表，按其他键不打印...\n");
		
		
		if ((c = getchar()) == 'y' || c == 'Y')
			do_print_info();
		while (c != '\n')
			c = getchar();
		printf("按X退出程序，按其他键继续...\n");
		if ((c = getchar()) == 'x' || c == 'X')
			break;
		while (c != '\n')
			c = getchar();
		//sleep(5000);
	}

	if (fclose(ptr_auxMem) == EOF)
	{
		do_error(ERROR_FILE_CLOSE_FAILED);
		exit(1);
	}
	return (0);
}
