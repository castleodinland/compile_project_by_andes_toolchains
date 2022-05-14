#include "string.h"
#include "file.h"
#include "BOOT_DEF.h"

/****************Lib��ʹ��,��̬�ڴ棬������****************/
FAT			ramFAT;

int FlashFSInit(void)
{
	/*********************************************************************
	 * ע�⣺�û������޸�FLASH_BASE��FLASH_FS_SIZE��PAGE_SIZE��CLUSTER_SIZE �ĸ��꣬
	 * ���ں��������ϵ����Ҫֱ����FlashParaInit()�����������д������
	 *********************************************************************/
	FlashParaInit(FLASH_BASE,FLASH_FS_SIZE,PAGE_SIZE,CLUSTER_SIZE);//��������

	if(b_file_initialize() != 0)//flash FS��ʼ��
	{
		return -1;
	}
	memset(&_iob, 0, sizeof(_iob));

	return 0;
}

int	CleanupCluster(BOOL bBusyCluster)
{
    BYTE		bClusterChain[ NUM_OF_CLUSTER ];

	return(CleanupCluster_sub(bBusyCluster,bClusterChain));
}
