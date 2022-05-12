/**
 **************************************************************************************
 * @file    media_play_api.c
 * @brief   
 *
 * @author  Sam
 * @version V1.0.0
 *
 * $Created: 2017-3-17 13:06:47$
 * 
 * @Copyright (C) 2016, Shanghai Mountain View Silicon Co.,Ltd. All rights reserved.
 **************************************************************************************
 */

#include "string.h"
#include "type.h"
#include "app_config.h"
#include "app_message.h"
#include "gpio.h"
#include "irqn.h"
#include "gpio.h"
#include "clk.h"
#include "dac.h"
#include "otg_host_standard_enum.h"
#include "otg_detect.h"
#include "delay.h"
#include "rtos_api.h"
#include "FreeRTOS.h"
#include "ff.h"
#include "ffpresearch.h"
#include "sd_card.h"
#include "debug.h"
#include "dac_interface.h"
#include "media_play_api.h"
#include "audio_core_api.h"
#include "decoder_service.h"
#include "device_detect.h"
#include "remind_sound_service.h"
#include "breakpoint.h"
#include "mode_switch_api.h"
#include "main_task.h"
#include "recorder_service.h"
#include "string_convert.h"
#include "random.h"
#include "timeout.h"
#include "browser_parallel.h"
#include "browser_tree.h"
#ifdef CFG_FUNC_LRC_EN
#include "lrc.h"
#endif


#define SYS_FOLDER_NAME "System Volume Information"
#if defined(CFG_APP_USB_PLAY_MODE_EN) || defined(CFG_APP_CARD_PLAY_MODE_EN) || defined(CFG_FUNC_RECORDER_EN)
extern uint8_t MediaPlayDevice(void);
extern uint32_t PlaySoneNum; ////only test
extern bool IsUDiskLink(void);//device_detect.c


bool MediaPlayerOpenSongFile(void);
bool MediaPlayerFileDecoderInit(void);

FRESULT f_open_recfile_by_num(FIL *filehandle, UINT number);
bool RecFilePlayerInit(void);
#ifdef CFG_FUNC_RECORD_FLASHFS
uint32_t FlashFsReadFile(void *buffer, uint32_t length); // ��Ϊflashfs �ط�callback
#endif

//������� ��Խ�������Ч��ʵʩ��ͬ����:
//������������Чʱ������ʹ����Ϣ�����������������ļ�ʱ���жϽ��Ⱥ�������
#define FF_FB_STEP                  2000
#define FF_PLAY_TIME_MAX			0x7FFFFFFF//����ⶥֵ���ⶥ���ٱ�����Ƚ���������һ��
#define FB_PLAY_LAST				0xFFFFFFFF //���˷��Ϊ0����׺����˼�FB_PLAY_LAST���Ƚ���������һ��

MEDIA_PLAYER* gpMediaPlayer = NULL;
#if  (defined(FUNC_BROWSER_PARALLEL_EN)||defined(FUNC_BROWSER_TREE_EN)||defined(FUNC_SPECIFY_FOLDER_PLAY_EN))
char current_vol[8];//disk volume like 0:/, 1:/
#else
static char current_vol[8];//disk volume like 0:/, 1:/
#endif
uint8_t	file_longname[FF_LFN_BUF + 1];
FileType SongFileType;

#if defined(CFG_PARA_RECORDS_FOLDER) && !defined(CFG_FUNC_FOLDER_PLAY)
bool FolderExclude_callback(FILINFO *finfo)
{
#ifndef MEDIAPLAYER_SUPPORT_REC_FILE
	if(finfo->fname[0]=='R')
	{
		if(strlen(finfo->fname) != strlen(CFG_PARA_RECORDS_FOLDER))
		{
			return FALSE;
		}
		if(strcmp(finfo->fname, CFG_PARA_RECORDS_FOLDER))//������ͬʱ���ַ�����ͬ
		{
			return FALSE;
		}
		return TRUE;
	}
	else 
#endif
	{
		if(strlen(finfo->fname) != strlen(SYS_FOLDER_NAME))
		{
			return FALSE;
		}
		if(strcmp(finfo->fname, SYS_FOLDER_NAME))//������ͬʱ���ַ�����ͬ
		{
			return FALSE;
		}
		return TRUE;
	}
}
#else //δ����¼���ļ���ʱ���������ļ��й���
#define FolderExclude_callback 		NULL
#endif
#ifdef CFG_FUNC_UDISK_DETECT
extern bool UDiskRemovedFlagGet(void);
extern void UDiskRemovedFlagSet(bool State);
#endif
//ý��Դ�豸��Ӳ����ʼ����ɨ��
//Ŀǰ֧��TF��/SD����U�̡�ע����غ�ж�ء�
static bool HardwareInit(DEV_ID DevId)
{
	uint8_t Retry = 8;//���س��Դ�����
	APP_DBG("hardware init start.\n");
	switch(DevId)
	{
#ifdef CFG_RES_CARD_USE
		case DEV_ID_SD:
		{
			CardPortInit(CFG_RES_CARD_GPIO);
			if(SDCard_Init() != NONE_ERR)
			{
				APP_DBG("Card init err.\n");
				return FALSE;
			}
			APP_DBG("SDCard Init Success!\n");
			strcpy(current_vol, MEDIA_VOLUME_STR_C);

			while(f_mount(&gpMediaPlayer->gFatfs_sd, current_vol, 1) != FR_OK && --Retry);
			if(!Retry)
			{
				APP_DBG("SD������ ʧ��\n");
				return FALSE;
			}
			APP_DBG("SD������  �ɹ�\n");
			return TRUE;
		}
#endif
#ifdef CFG_FUNC_UDISK_DETECT
		case DEV_ID_USB:
			//OTG_SetCurrentPort(UDISK_PORT_NUM);
			if(!IsUDiskLink())
			{
				APP_DBG("UNLINK UDisk\n");
				return FALSE;
			}
			strcpy(current_vol, MEDIA_VOLUME_STR_U);
			do
			{
				if(UDiskRemovedFlagGet())
				{
					if(!OTG_HostInit() == 1)
					{
						if(!OTG_HostInit() == 1)
						{
							APP_DBG("Host Init Disk Error\n");
							return FALSE;
						}
					}
				}

			}while(f_mount(&gpMediaPlayer->gFatfs_u, current_vol, 1) != FR_OK && Retry--);
			if(!Retry && UDiskRemovedFlagGet())
			{
				APP_DBG("USB����  ʧ��\n");
				return FALSE;
			}
			UDiskRemovedFlagSet(0);
			APP_DBG("USB����  �ɹ�\n");
			return TRUE;
#endif
#ifdef CFG_FUNC_RECORD_FLASHFS
		case DEV_ID_FLASHFS:
			return TRUE;
#endif

		default:
			break;
	}
	return FALSE;
}


#ifdef CFG_FUNC_LRC_EN
static bool PlayerParseLrc(void)
{
	uint32_t SeekTime = gpMediaPlayer->CurPlayTime * 1000;
	static int32_t LrcStartTime = -1;

	if((gpMediaPlayer->LrcRow.MaxLrcLen = (int16_t)LrcTextLengthGet(SeekTime)) >= 0)
	{
		TEXT_ENCODE_TYPE CharSet;
		bool bCheckFlag = FALSE;
		
		if(gpMediaPlayer->LrcRow.MaxLrcLen > 128)
		{
			bCheckFlag = TRUE;
			gpMediaPlayer->LrcRow.MaxLrcLen = 128;
		}

		memset(gpMediaPlayer->LrcRow.LrcText, 0, 128);

		LrcInfoGet(&gpMediaPlayer->LrcRow, SeekTime, 0);
		if(gpMediaPlayer->LrcRow.StartTime + gpMediaPlayer->LrcRow.Duration == LrcStartTime)
		{
			return FALSE;
		}
		// ??? ����Ĵ�����Ҫȷ��ִ��ʱ�䣬���ʱ��ϳ���Ӱ�춨ʱ�ľ�ȷ�ȣ���Ҫ�Ƶ�����ִ��
		// ??? ����ת��
		CharSet = LrcEncodeTypeGet();
		//APP_DBG("CharSet = %d\n", CharSet);
		if(gpMediaPlayer->LrcRow.MaxLrcLen > 0 
		  && !(CharSet == ENCODE_UNKNOWN || CharSet == ENCODE_GBK || CharSet == ENCODE_ANSI))
		{
#ifdef  CFG_FUNC_STRING_CONVERT_EN
			uint32_t  ConvertType = UNICODE_TO_GBK;
			uint8_t* TmpStr = gpMediaPlayer->TempBuf2;

			if(CharSet == ENCODE_UTF8)
			{
				ConvertType = UTF8_TO_GBK;
			}
			else if(CharSet == ENCODE_UTF16_BIG)
			{
				ConvertType = UNICODE_BIG_TO_GBK;
			}
			//APP_DBG("ConvertType = %d\n", ConvertType);
			memset(TmpStr, 0, 128);

			gpMediaPlayer->LrcRow.MaxLrcLen = (uint16_t)StringConvert(TmpStr,
			                                (uint32_t)gpMediaPlayer->LrcRow.MaxLrcLen,
			                                gpMediaPlayer->LrcRow.LrcText,
			                                (uint32_t)gpMediaPlayer->LrcRow.MaxLrcLen,
			                                ConvertType);

			memcpy(gpMediaPlayer->LrcRow.LrcText, TmpStr, gpMediaPlayer->LrcRow.MaxLrcLen);
			memset(TmpStr, 0, 128);

			memset((void*)(gpMediaPlayer->LrcRow.LrcText + gpMediaPlayer->LrcRow.MaxLrcLen),
			               0, 128 - gpMediaPlayer->LrcRow.MaxLrcLen);
#endif
			//gPlayContrl->LrcRow.LrcText[gPlayContrl->LrcRow.MaxLrcLen] = '\0';
		}

		// ??? ĩβ���������Լ��
		if(bCheckFlag)
		{
			uint32_t i = 0;
			while(i < (uint32_t)gpMediaPlayer->LrcRow.MaxLrcLen)
			{
				if(gpMediaPlayer->LrcRow.LrcText[i] > 0x80)
				{
					i += 2;
				}
				else
				{
					i++;
				}
			}

			if(i >= (uint32_t)gpMediaPlayer->LrcRow.MaxLrcLen)
			{
				gpMediaPlayer->LrcRow.MaxLrcLen--;
				gpMediaPlayer->LrcRow.LrcText[gpMediaPlayer->LrcRow.MaxLrcLen] = '\0';
			}
		}

		APP_DBG("<%s>\r\n", gpMediaPlayer->LrcRow.LrcText);
		
		LrcStartTime = gpMediaPlayer->LrcRow.StartTime + gpMediaPlayer->LrcRow.Duration;
		return TRUE;
	}

	APP_DBG("gpMediaPlayer->LrcRow.MaxLrcLen = %d\n", gpMediaPlayer->LrcRow.MaxLrcLen);
	if(gpMediaPlayer->LrcRow.MaxLrcLen < 0)
	{
		gpMediaPlayer->LrcRow.MaxLrcLen = 0;
	}

	return FALSE;	
}

void SearchAndOpenLrcFile(void)
{
	int32_t i;
	int32_t Len = 0;
	bool ret;
	//FOLDER CurFolder;		
	{
		memset(gpMediaPlayer->TempBuf2, 0, sizeof(gpMediaPlayer->TempBuf2));
		
		//if(FR_OK != f_open_by_num(current_vol, &gpMediaPlayer->PlayerFile, gpMediaPlayer->CurFileIndex, (char*)file_longname))
		if(FR_OK != f_opendir_by_num(current_vol, &gpMediaPlayer->PlayerFolder, gpMediaPlayer->PlayerFile.dir_num, NULL))
		{
			APP_DBG("folder open err!!\n");
		}
		if(file_longname[0] != 0)
		{
			i = strlen((char*)file_longname);
			while(--i)
			{
				if(file_longname[i] == '.')
				{
					memcpy(&gpMediaPlayer->TempBuf2[Len], file_longname, i + 1);
					{
						gpMediaPlayer->TempBuf2[i + 1 + Len] = (uint8_t)'l';
						gpMediaPlayer->TempBuf2[i + 2 + Len] = (uint8_t)'r';
						gpMediaPlayer->TempBuf2[i + 3 + Len] = (uint8_t)'c';
					}
					break;
				}
			}
			//APP_DBG("lrc file long Name: %s\n", TempBuf2);
		}
		else
		{
			//memcpy(TempBuf2, (void*)gpMediaPlayer->PlayerFile.ShortName, 8);
			//memcpy(&gpMediaPlayer->TempBuf2[Len], f_audio_get_name(gpMediaPlayer->CurFileIndex), 8);//�˴������⣬������Ҫ�޸�
			memcpy(&gpMediaPlayer->TempBuf2[Len], gpMediaPlayer->PlayerFile.fn, 8);
			memcpy(&gpMediaPlayer->TempBuf2[Len + 8], (void*)"lrc", 3);
			APP_DBG("lrc file short Name: %s\n", gpMediaPlayer->TempBuf2);
		}

		//ret = f_open(&gpMediaPlayer->LrcFile, (TCHAR*)gpMediaPlayer->TempBuf2, FA_READ);
		ret = f_open_by_name_in_dir(&gpMediaPlayer->PlayerFolder, &gpMediaPlayer->LrcFile, (TCHAR*)gpMediaPlayer->TempBuf2);

		if(ret == FR_OK)
		{
			APP_DBG("open lrc file ok\n");
			gpMediaPlayer->LrcRow.LrcText = gpMediaPlayer->TempBuf1; // ����ڴ���ӳ��
			gpMediaPlayer->IsLrcRunning = TRUE;
			LrcInit(&gpMediaPlayer->LrcFile, gpMediaPlayer->ReadBuffer, sizeof(gpMediaPlayer->ReadBuffer), &gpMediaPlayer->LrcInfo);
		}
		else
		{
			gpMediaPlayer->IsLrcRunning = FALSE;
			APP_DBG("No lrc\n");
		}
	}
}
#endif


//��������ʼ��
bool MediaPlayerInitialize(DEV_ID DeviceIndex, int32_t FileIndex, uint32_t FolderIndex)
{
	//1���豸��ʼ��
	//2���ļ�ϵͳ��ʼ��
	//3�����ҵ��ļ�����decoder��ʼ��
//	if(gpMediaPlayer != NULL)
//	{
//		APP_DBG("player is reopened\n");
//	}
//	else
//	{
//		gpMediaPlayer = osPortMalloc(sizeof(MEDIA_PLAYER));
//		if(gpMediaPlayer == NULL)
//		{
//			APP_DBG("gpMediaPlayer malloc error\n");
//			return FALSE;
//		}
//	}
//
//	memset(gpMediaPlayer, 0, sizeof(MEDIA_PLAYER));

	SetMediaPlayerState(PLAYER_STATE_IDLE);
	if(!HardwareInit(DeviceIndex))
	{
		APP_DBG("Hardware initialize error\n");
		return FALSE;
	}
	if(GetSystemMode() == AppModeUDiskAudioPlay || GetSystemMode() == AppModeCardAudioPlay)
	{
#if defined(FUNC_MATCH_PLAYER_BP) && (defined(CFG_APP_USB_PLAY_MODE_EN) || defined(CFG_APP_CARD_PLAY_MODE_EN))
		DiskSongSearchBPInit();
		ffpresearch_init(DeviceIndex == DEV_ID_SD ? &gpMediaPlayer->gFatfs_sd : &gpMediaPlayer->gFatfs_u, DiskSongSearchBP, FolderExclude_callback,gpMediaPlayer->AccRamBlk);
#else
		ffpresearch_init(DeviceIndex == DEV_ID_SD ? &gpMediaPlayer->gFatfs_sd : &gpMediaPlayer->gFatfs_u, NULL, FolderExclude_callback,gpMediaPlayer->AccRamBlk);
#endif
		if(FR_OK != f_scan_vol(current_vol))
		{
			APP_DBG("f_scan_vol err!");
			return FALSE;
		}

		APP_DBG("Hardware initialize success.\n");

#ifdef FUNC_SPECIFY_FOLDER_PLAY_EN
		ScanAllFolderToGetTotalSongs();
#endif

		//Ĭ��ʹ��ȫ�̲��ŵ�һ���ļ�
		gpMediaPlayer->CurFolderIndex = 1; // ��Ŀ¼
#if defined(FUNC_MATCH_PLAYER_BP) && (defined(CFG_APP_USB_PLAY_MODE_EN) || defined(CFG_APP_CARD_PLAY_MODE_EN))
		gpMediaPlayer->CurFileIndex = BPDiskSongNumGet();
		APP_DBG("CurFileIndex = %d\n", gpMediaPlayer->CurFileIndex);
		gpMediaPlayer->CurPlayTime = BPDiskSongPlayTimeGet();
		APP_DBG("CurPlayTime = %d\n", (int)gpMediaPlayer->CurPlayTime);
		if(gpMediaPlayer->CurFileIndex == 0)
#endif
		{
			gpMediaPlayer->CurFileIndex = 1;
			gpMediaPlayer->CurPlayTime = 0;
		}

		gpMediaPlayer->TotalFileSumInDisk = f_file_real_quantity();
		gpMediaPlayer->ValidFolderSumInDisk = f_dir_with_song_real_quantity();//f_dir_real_quantity();
		if(!gpMediaPlayer->TotalFileSumInDisk)
		{
			return FALSE;
		}
		// �򿪲����ļ��У�Ĭ��ȫ�̲���
		// �ļ�����ز�����������
	}
#ifdef CFG_FUNC_RECORDER_EN
	else //playback
	{
#ifdef CFG_FUNC_RECORD_UDISK_FIRST
		if(!RecFilePlayerInit())
		{
			return FALSE;
		}
#else //flashfs �ط�
#ifdef CFG_FUNC_RECORD_FLASHFS
		if((gpMediaPlayer->FlashFsFile = Fopen(CFG_PARA_FLASHFS_FILE_NAME, "r")) == NULL)//flashfs����Ч�ļ�
		{
			APP_DBG("open error");
			return FALSE;
		}
		Fclose(gpMediaPlayer->FlashFsFile);
		gpMediaPlayer->FlashFsFile = NULL;
		gpMediaPlayer->FlashFsMemHandle.addr = NULL;//�������壬�ɽ�����ֱ�ӻ�ȡ
		gpMediaPlayer->FlashFsMemHandle.mem_capacity = 0;
		gpMediaPlayer->FlashFsMemHandle.mem_len = 0;
		gpMediaPlayer->FlashFsMemHandle.p = 0;
#endif
		gpMediaPlayer->TotalFileSumInDisk = 1;//flashfs��Чʱֻ�е���¼���ļ���
#endif
		gpMediaPlayer->CurFolderIndex = 1; // ���ڼ�����������
		gpMediaPlayer->ValidFolderSumInDisk = 1;
		gpMediaPlayer->CurFileIndex = 1;
		gpMediaPlayer->CurPlayTime = 0;
		gpMediaPlayer->CurPlayMode = PLAY_MODE_REPEAT_ALL;//�ط�¼��ֻ��ѭ��(����)���ţ���֧�ֲ���ģʽ�л�
	}
#endif
	uint16_t i;//�ײ��������ֹ���ʱ����һ�ף�ֱ����ʼ���������ɹ��ĸ���
	for(i = 0; i < gpMediaPlayer->TotalFileSumInDisk; i++)
	{
		if(MediaPlayerFileDecoderInit())
		{
			break;
		}
		gpMediaPlayer->CurFileIndex = (gpMediaPlayer->CurFileIndex % gpMediaPlayer->TotalFileSumInDisk) + 1;
	}
	if(i == gpMediaPlayer->TotalFileSumInDisk)
	{
		return FALSE;//û����Ч������
	}

	if((gpMediaPlayer->CurPlayMode == PLAY_MODE_REPEAT_FOLDER || gpMediaPlayer->CurPlayMode == PLAY_MODE_RANDOM_FOLDER)
		&& (GetSystemMode() == AppModeUDiskAudioPlay || GetSystemMode() == AppModeCardAudioPlay))
	{
		f_file_count_in_dir(&gpMediaPlayer->PlayerFolder);//ȷ�ϴ��ļ��� ��������
	}
	MediaPlayerSwitchPlayMode(gpMediaPlayer->CurPlayMode);

	SetMediaPlayerState(PLAYER_STATE_PLAYING);
	return TRUE;
}

void MediaPlayerDeinitialize(void)
{
	if(gpMediaPlayer != NULL)
	{
#ifdef CFG_FUNC_RECORD_UDISK_FIRST
		if(gpMediaPlayer->RecFileList != NULL)
		{
			osPortFree(gpMediaPlayer->RecFileList);
		}
#endif
	    osPortFree(gpMediaPlayer);
	    gpMediaPlayer = NULL;
	}
}

//�ļ����� ������Ϣlog
static void MediaPlayerSongInfoLog(void)
{
	SongInfo* PlayingSongInfo = audio_decoder_get_song_info();

	APP_DBG("PlayCtrl:MediaPlayerSongInfoLog\n");
	if(PlayingSongInfo == NULL)
	{
		return ;
	}

#ifdef CFG_FUNC_LRC_EN
    APP_DBG("LRC:%d\n", gpMediaPlayer->IsLrcRunning);
#else
    APP_DBG("LRC:0\n");
#endif
	APP_DBG("----------TAG Info----------\n");

	APP_DBG("CharSet:");
	switch(PlayingSongInfo->char_set)
	{
		case CHAR_SET_ISO_8859_1:
			APP_DBG("CHAR_SET_ISO_8859_1\n");
			break;
		case CHAR_SET_UTF_16:
			APP_DBG("CHAR_SET_UTF_16\n");
			break;
		case CHAR_SET_UTF_8:
			APP_DBG("CHAR_SET_UTF_8\n");
			break;
		default:
			APP_DBG("CHAR_SET_UNKOWN\n");
			break;
	}
#ifdef CFG_FUNC_STRING_CONVERT_EN
    if(PlayingSongInfo->char_set == CHAR_SET_UTF_8)
    {
        StringConvert(PlayingSongInfo->title,	  MAX_TAG_LEN, PlayingSongInfo->title,	   MAX_TAG_LEN, UTF8_TO_GBK);
        StringConvert(PlayingSongInfo->artist,	  MAX_TAG_LEN, PlayingSongInfo->artist,    MAX_TAG_LEN, UTF8_TO_GBK);
        StringConvert(PlayingSongInfo->album,	  MAX_TAG_LEN, PlayingSongInfo->album,	   MAX_TAG_LEN, UTF8_TO_GBK);
        StringConvert(PlayingSongInfo->comment,   MAX_TAG_LEN, PlayingSongInfo->comment,   MAX_TAG_LEN, UTF8_TO_GBK);
        StringConvert(PlayingSongInfo->genre_str, MAX_TAG_LEN, PlayingSongInfo->genre_str, MAX_TAG_LEN, UTF8_TO_GBK);
    }
    else if(PlayingSongInfo->char_set == CHAR_SET_UTF_16)
    {
    	if(PlayingSongInfo->stream_type == STREAM_TYPE_WMA)
		{
			StringConvert(PlayingSongInfo->title,	  MAX_TAG_LEN, &PlayingSongInfo->title[0],	   MAX_TAG_LEN, UNICODE_TO_GBK);
			StringConvert(PlayingSongInfo->artist,	  MAX_TAG_LEN, &PlayingSongInfo->artist[0],    MAX_TAG_LEN, UNICODE_TO_GBK);
			StringConvert(PlayingSongInfo->album,	  MAX_TAG_LEN, &PlayingSongInfo->album[0],	   MAX_TAG_LEN, UNICODE_TO_GBK);
			StringConvert(PlayingSongInfo->comment,   MAX_TAG_LEN, &PlayingSongInfo->comment[0],   MAX_TAG_LEN, UNICODE_TO_GBK);
			StringConvert(PlayingSongInfo->genre_str, MAX_TAG_LEN, &PlayingSongInfo->genre_str[0], MAX_TAG_LEN, UNICODE_TO_GBK);
		}
    	else
    	{
			if(PlayingSongInfo->title[0] == 0xFE && PlayingSongInfo->title[1] == 0xFF)
			{
				StringConvert(PlayingSongInfo->title,	  MAX_TAG_LEN, &PlayingSongInfo->title[2],	   MAX_TAG_LEN, UNICODE_BIG_TO_GBK);
				StringConvert(PlayingSongInfo->artist,	  MAX_TAG_LEN, &PlayingSongInfo->artist[2],    MAX_TAG_LEN, UNICODE_BIG_TO_GBK);
				StringConvert(PlayingSongInfo->album,	  MAX_TAG_LEN, &PlayingSongInfo->album[2],	   MAX_TAG_LEN, UNICODE_BIG_TO_GBK);
				StringConvert(PlayingSongInfo->comment,   MAX_TAG_LEN, &PlayingSongInfo->comment[2],   MAX_TAG_LEN, UNICODE_BIG_TO_GBK);
				StringConvert(PlayingSongInfo->genre_str, MAX_TAG_LEN, &PlayingSongInfo->genre_str[2], MAX_TAG_LEN, UNICODE_BIG_TO_GBK);
			}
			else
			{
				StringConvert(PlayingSongInfo->title,	  MAX_TAG_LEN, &PlayingSongInfo->title[2],	   MAX_TAG_LEN, UNICODE_TO_GBK);
				StringConvert(PlayingSongInfo->artist,	  MAX_TAG_LEN, &PlayingSongInfo->artist[2],    MAX_TAG_LEN, UNICODE_TO_GBK);
				StringConvert(PlayingSongInfo->album,	  MAX_TAG_LEN, &PlayingSongInfo->album[2],	   MAX_TAG_LEN, UNICODE_TO_GBK);
				StringConvert(PlayingSongInfo->comment,   MAX_TAG_LEN, &PlayingSongInfo->comment[2],   MAX_TAG_LEN, UNICODE_TO_GBK);
				StringConvert(PlayingSongInfo->genre_str, MAX_TAG_LEN, &PlayingSongInfo->genre_str[2], MAX_TAG_LEN, UNICODE_TO_GBK);
			}
    	}
    }
    else if((PlayingSongInfo->char_set & 0xF0000000) == CHAR_SET_DIVERSE)
    {
        uint32_t type = PlayingSongInfo->char_set & 0xF;

        if(type == CHAR_SET_UTF_8)
        {
        	 StringConvert(PlayingSongInfo->title,	  MAX_TAG_LEN, PlayingSongInfo->title,	   MAX_TAG_LEN, UTF8_TO_GBK);
        }
        else if(type == CHAR_SET_UTF_16)
        {
        	if(PlayingSongInfo->title[0] == 0xFF && PlayingSongInfo->title[1] == 0xFE)
        	{
        		StringConvert(PlayingSongInfo->title,	  MAX_TAG_LEN, &PlayingSongInfo->title[2],	   MAX_TAG_LEN, UNICODE_TO_GBK);
        	}
        	else if(PlayingSongInfo->title[0] == 0xFE && PlayingSongInfo->title[1] == 0xFF)
        	{
        		StringConvert(PlayingSongInfo->title,	  MAX_TAG_LEN, &PlayingSongInfo->title[2],	   MAX_TAG_LEN, UNICODE_BIG_TO_GBK);
        	}
        }

        type = (audio_decoder->song_info->char_set >> 4)  & 0xF;
        if(type == CHAR_SET_UTF_8)
		{
        	StringConvert(PlayingSongInfo->artist,	  MAX_TAG_LEN, PlayingSongInfo->artist,	   MAX_TAG_LEN, UTF8_TO_GBK);
		}
		else if(type == CHAR_SET_UTF_16)
		{
			if(PlayingSongInfo->artist[0] == 0xFF && PlayingSongInfo->artist[1] == 0xFE)
			{
				StringConvert(PlayingSongInfo->artist,	  MAX_TAG_LEN, &PlayingSongInfo->artist[2],	   MAX_TAG_LEN, UNICODE_TO_GBK);
			}
			else if(PlayingSongInfo->artist[0] == 0xFE && PlayingSongInfo->artist[1] == 0xFF)
			{
				StringConvert(PlayingSongInfo->artist,	  MAX_TAG_LEN, &PlayingSongInfo->artist[2],	   MAX_TAG_LEN, UNICODE_BIG_TO_GBK);
			}
		}

        type = (audio_decoder->song_info->char_set >> 8)  & 0xF;
        if(type == CHAR_SET_UTF_8)
		{
			StringConvert(PlayingSongInfo->album,	  MAX_TAG_LEN, PlayingSongInfo->album,	   MAX_TAG_LEN, UTF8_TO_GBK);
		}
		else if(type == CHAR_SET_UTF_16)
		{
			if(PlayingSongInfo->album[0] == 0xFF && PlayingSongInfo->album[1] == 0xFE)
			{
				StringConvert(PlayingSongInfo->album,	  MAX_TAG_LEN, &PlayingSongInfo->album[2],	   MAX_TAG_LEN, UNICODE_TO_GBK);
			}
			else if(PlayingSongInfo->album[0] == 0xFE && PlayingSongInfo->album[1] == 0xFF)
			{
				StringConvert(PlayingSongInfo->album,	  MAX_TAG_LEN, &PlayingSongInfo->album[2],	   MAX_TAG_LEN, UNICODE_BIG_TO_GBK);
			}
		}

        type = (audio_decoder->song_info->char_set >> 12) & 0xF;
        if(type == CHAR_SET_UTF_8)
		{
			StringConvert(PlayingSongInfo->comment,	  MAX_TAG_LEN, PlayingSongInfo->comment,	   MAX_TAG_LEN, UTF8_TO_GBK);
		}
		else if(type == CHAR_SET_UTF_16)
		{
			if(PlayingSongInfo->comment[0] == 0xFF && PlayingSongInfo->comment[1] == 0xFE)
			{
				StringConvert(PlayingSongInfo->comment,	  MAX_TAG_LEN, &PlayingSongInfo->comment[2],	   MAX_TAG_LEN, UNICODE_TO_GBK);
			}
			else if(PlayingSongInfo->comment[0] == 0xFE && PlayingSongInfo->comment[1] == 0xFF)
			{
				StringConvert(PlayingSongInfo->comment,	  MAX_TAG_LEN, &PlayingSongInfo->comment[2],	   MAX_TAG_LEN, UNICODE_BIG_TO_GBK);
			}
		}

        type = (audio_decoder->song_info->char_set >> 16) & 0xF;
        if(type == CHAR_SET_UTF_8)
		{
			StringConvert(PlayingSongInfo->genre_str,	  MAX_TAG_LEN, PlayingSongInfo->genre_str,	   MAX_TAG_LEN, UTF8_TO_GBK);
		}
		else if(type == CHAR_SET_UTF_16)
		{
			if(PlayingSongInfo->genre_str[0] == 0xFF && PlayingSongInfo->genre_str[1] == 0xFE)
			{
				StringConvert(PlayingSongInfo->genre_str,	  MAX_TAG_LEN, &PlayingSongInfo->genre_str[2],	   MAX_TAG_LEN, UNICODE_TO_GBK);
			}
			else if(PlayingSongInfo->genre_str[0] == 0xFE && PlayingSongInfo->genre_str[1] == 0xFF)
			{
				StringConvert(PlayingSongInfo->genre_str,	  MAX_TAG_LEN, &PlayingSongInfo->genre_str[2],	   MAX_TAG_LEN, UNICODE_BIG_TO_GBK);
			}
		}
    }
#endif
    
    APP_DBG("title: %s\n", PlayingSongInfo->title);
    APP_DBG("artist: %s\n", PlayingSongInfo->artist);
    APP_DBG("Album: %s\n", PlayingSongInfo->album);
    APP_DBG("comment: %s\n", PlayingSongInfo->comment);
    APP_DBG("genre: %d %s\n", PlayingSongInfo->genre, PlayingSongInfo->genre_str);
    APP_DBG("year: %s\n", PlayingSongInfo->year);
    
    APP_DBG("\n");
    APP_DBG("----------------------------\n");
    APP_DBG("**********Song Info*********\n");
    APP_DBG("SongType:");
    switch(PlayingSongInfo->stream_type)
    {
        case STREAM_TYPE_MP2:
            APP_DBG("MP2");
            break;
        case STREAM_TYPE_MP3:
            APP_DBG("MP3");
            break;
        case STREAM_TYPE_WMA:
            APP_DBG("WMA");
            break;
        case STREAM_TYPE_SBC:
            APP_DBG("SBC");
            break;
        case STREAM_TYPE_PCM:
            APP_DBG("PCM");
            break;
        case STREAM_TYPE_ADPCM:
            APP_DBG("ADPCM");
            break;
        case STREAM_TYPE_FLAC:
            APP_DBG("FLAC");
            break;
        case STREAM_TYPE_AAC:
            APP_DBG("AAC");
            break;
        default:
            APP_DBG("UNKNOWN");
            break;
    }
    APP_DBG("\n");
    APP_DBG("Chl Num:%d\n", (int)PlayingSongInfo->num_channels);
    APP_DBG("SampleRate:%d\n", (int)PlayingSongInfo->sampling_rate);
    APP_DBG("BitRate:%d\n", (int)PlayingSongInfo->bitrate);
    APP_DBG("File Size:%d\n", (int)PlayingSongInfo->file_size);
    APP_DBG("TotalPlayTime:%dms\n", (int)PlayingSongInfo->duration);
    APP_DBG("CurPlayTime:%dms\n", (int)gpMediaPlayer->CurPlayTime);
    APP_DBG("IsVBR:%d\n", (int)PlayingSongInfo->vbr_flag);
    APP_DBG("MpegVer:");

    switch(audio_decoder_get_mpeg_version())
    {
        case MPEG_VER2d5:
            APP_DBG("MPEG_2_5");
            break;
        case MPEG_VER1:
            APP_DBG("MPEG_1");
            break;
        case MPEG_VER2:
            APP_DBG("MPEG_2");
            break;
        default:
            APP_DBG("MPEG_UNKNOWN");
            break;
    }
    APP_DBG("\n");
    APP_DBG("Id3Ver:%d\n", (int)audio_decoder_get_id3_version());

    APP_DBG("**************************\n");
    return ;
}

// ����ý�����������audiocore����
//����ý���ļ�����������DacƵ�ʺͲ�����
static void MediaPlayerUpdataDecoderSourceNum(void)
{
	AudioCoreSourceEnable(DecoderSourceNumGet());
#ifndef CFG_FUNC_MIXER_SRC_EN
	SongInfo* PlayingSongInfo = audio_decoder_get_song_info();
#ifdef CFG_RES_AUDIO_DACX_EN
	AudioDAC_SampleRateChange(ALL, PlayingSongInfo->sampling_rate);
#endif

#ifdef CFG_RES_AUDIO_DAC0_EN
	AudioDAC_SampleRateChange(DAC0, PlayingSongInfo->sampling_rate);
#endif
	APP_DBG("DAC Sample rate = %d\n", (int)AudioDAC_SampleRateGet(DAC0));
#endif
}

extern uint32_t CmdErrCnt;
//�����ļ�
bool MediaPlayerOpenSongFile(void)
{
    memset(&gpMediaPlayer->PlayerFile, 0, sizeof(gpMediaPlayer->PlayerFile));
    if(CmdErrCnt >= 3)//��ʱ����U���쳣����U�̲��ŵ����
	{
		MessageContext      msgSend;

		msgSend.msgId = MSG_MODE;
		MessageSend(GetMainMessageHandle(), &msgSend);
		osTaskDelay(5);
		return FALSE;
		APP_DBG("MSG MODE switch\n");
	}
#ifdef CFG_FUNC_RECORD_UDISK_FIRST
	if(GetSystemMode() == AppModeUDiskPlayBack || GetSystemMode() == AppModeCardPlayBack)
	{
		if(FR_OK != f_open_recfile_by_num(&gpMediaPlayer->PlayerFile, gpMediaPlayer->CurFileIndex))
		{
			APP_DBG(("FileOpenByNum() error!\n"));
			return FALSE;
		}
	}
	else
#elif defined(CFG_FUNC_RECORD_FLASHFS)
	if(GetSystemMode() == AppModeFlashFsPlayBack)
	{
		APP_DBG("open flashfs");
		if((gpMediaPlayer->FlashFsFile = Fopen(CFG_PARA_FLASHFS_FILE_NAME, "r")) == NULL)
		{
			return FALSE;
		}
	}
	else
#endif
	{
	
#ifdef FUNC_BROWSER_PARALLEL_EN
		if(gpMediaPlayer->CurPlayMode == PLAY_MODE_BROWSER)
		{
			gpMediaPlayer->PlayerFolder.FolderIndex = 0;
		}
#endif
		APP_DBG("CurFileIndex = %d\n", gpMediaPlayer->CurFileIndex);
#ifdef	FUNC_SPECIFY_FOLDER_PLAY_EN
		//find song use number in forlder when story machine  
		if(gpMediaPlayer->StoryPlayByIndexFlag)
		{
			if(!PlayStorySongByIndex())// play by index in folder
			{
			APP_DBG("error !!!!!!!\n");
			return FALSE;
			}
			gpMediaPlayer->StoryPlayByIndexFlag=FALSE;
		}
		else if(gpMediaPlayer->StorySelectPlayFlag)
		{
			if(gpMediaPlayer->StorySelectPlayFlag==STORY_PLAY_CURRENT)
				FindFileHdrByDir(0);//current song play
			else
				FindFileHdrByDir(1);// next song play
			
			gpMediaPlayer->StorySelectPlayFlag=0;
		}
		else 
#endif
		{
			if(FR_OK != f_open_by_num(current_vol, &gpMediaPlayer->PlayerFolder, &gpMediaPlayer->PlayerFile, gpMediaPlayer->CurFileIndex, (char*)file_longname))
			{
				APP_DBG(("FileOpenByNum() error!\n"));
				return FALSE;
			}
		}

		if(gpMediaPlayer->PlayerFile.fn[0]==0)// bkd add for exfat short file name
		{
			memcpy(gpMediaPlayer->PlayerFile.fn, file_longname, FF_SFN_BUF);
			gpMediaPlayer->PlayerFile.fn[FF_SFN_BUF]=0;
		}
		
		

		if(GetSystemMode() == AppModeUDiskAudioPlay || GetSystemMode() == AppModeCardAudioPlay)
		{
			gpMediaPlayer->CurFolderIndex = f_dir_with_song_real_quantity_cur();
		}

		if(file_longname[0] != 0)
		{
			APP_DBG("Song file long Name: %s\n", file_longname);
			SongFileType = get_audio_type((TCHAR*)file_longname);
		}
		else
		{
			APP_DBG("Song Name: %s\n", gpMediaPlayer->PlayerFile.fn);
			SongFileType = get_audio_type((TCHAR *)gpMediaPlayer->PlayerFile.fn);
		}

#ifdef CFG_FUNC_LRC_EN
		SearchAndOpenLrcFile();
#endif
		//APP_DBG("Song type: %d\n", SongFileType);
	}

	return TRUE;
}

bool MediaPlayerDecoderInit(void)
{
	gpMediaPlayer->DecoderSync = DecoderStateDeinitializing;//�� ��ʼ��ǰ״̬
#ifdef CFG_FUNC_RECORD_SD_UDISK
	if(GetSystemMode() == AppModeUDiskPlayBack || GetSystemMode() == AppModeCardPlayBack)
	{
		if(DecoderInit(&gpMediaPlayer->PlayerFile, (int32_t)IO_TYPE_FILE, (int32_t)FILE_TYPE_MP3) != RT_SUCCESS)
		{
			APP_DBG("Decoder Init Err\n");
			return FALSE;
		}
	}
	else 
#elif defined(CFG_FUNC_RECORD_FLASHFS)
	if(GetSystemMode() == AppModeFlashFsPlayBack)
	{
 
		mv_mread_callback_set(&gpMediaPlayer->FlashFsMemHandle,FlashFsReadFile);

		if(DecoderInit(&gpMediaPlayer->FlashFsMemHandle, (uint32_t)IO_TYPE_MEMORY, (uint32_t)MP3_DECODER) != RT_SUCCESS)
		{
			APP_DBG("Decoder Init Err\n");
			return FALSE;
		}

	}	
	else
#endif
	{
		if(DecoderInit(&gpMediaPlayer->PlayerFile, (int32_t)IO_TYPE_FILE, (int32_t)SongFileType) != RT_SUCCESS)
		{
			return FALSE;
		}
	    APP_DBG("Open File ok! i=%d, ptr=%d, filesize=%uKB\n", (int)gpMediaPlayer->CurFileIndex, (int)f_tell(&gpMediaPlayer->PlayerFile), (uint16_t)(f_size(&gpMediaPlayer->PlayerFile)/1024));

	}
	gpMediaPlayer->DecoderSync = DecoderStateInitialized;//�� ��ʼ��״̬
	return TRUE;
}

//���ļ��ͽ���������
bool MediaPlayerFileDecoderInit(void)
{
	if(!MediaPlayerOpenSongFile())
	{
		return FALSE;
	}

	if(MediaPlayerDecoderInit() == FALSE)
	{
		return FALSE;
	}
#ifdef FUNC_MATCH_PLAYER_BP

	//APP_DBG("999 CurPlayTime=%ld\n",gpMediaPlayer->CurPlayTime);
	if(gpMediaPlayer->CurPlayTime != 0)// && (GetSystemMode() == AppModeUDiskPlayBack || GetSystemMode() == AppModeCardPlayBack))
	{
		if(DecoderSeek(gpMediaPlayer->CurPlayTime) != TRUE) //bkd find break point play
		{
			return FALSE;
		}
	}
#endif

	MediaPlayerSongInfoLog();
	MediaPlayerUpdataDecoderSourceNum();
	
    return TRUE;
}


//�رղ����ļ�
void MediaPlayerCloseSongFile(void)
{
#ifdef CFG_FUNC_RECORD_FLASHFS
    if(GetSystemMode() == AppModeFlashFsPlayBack && gpMediaPlayer->FlashFsFile)
    {
		APP_DBG("Closefs");
    	Fclose(gpMediaPlayer->FlashFsFile);
		mv_mread_callback_unset(&gpMediaPlayer->FlashFsMemHandle);
		gpMediaPlayer->FlashFsFile = NULL;
    }
	else
#endif
	{
		f_close(&gpMediaPlayer->PlayerFile);
#ifdef CFG_FUNC_LRC_EN
		f_close(&gpMediaPlayer->LrcFile);

#endif
	}
}

//������ ��һ�� �趨
void MediaPlayerNextSong(bool IsSongPlayEnd)
{
#ifdef CFG_FUNC_DISPLAY_EN
	MessageContext	msgSend;
#endif
	APP_DBG("PlayCtrl:MediaPlayerNextSong\n");
#ifdef CFG_FUNC_UDISK_DETECT
	if((GetSystemMode() == AppModeUDiskAudioPlay)
		&& (UDiskRemovedFlagGet() == TRUE))
	{
		return;
	}
#endif
	if(CmdErrCnt >= 3)//��ʱ����U���쳣����U�̲��ŵ����
	{
		osTaskDelay(5);
		return;
	}
//	gpMediaPlayer->CurPlayMode = PLAY_MODE_REPEAT_ALL;
	switch(gpMediaPlayer->CurPlayMode)
	{
		case PLAY_MODE_RANDOM_ALL:
			gpMediaPlayer->CurFileIndex = GetRandomNum((uint16_t)GetSysTick1MsCnt(), gpMediaPlayer->TotalFileSumInDisk);
			break;
		case PLAY_MODE_RANDOM_FOLDER:
			if(!gpMediaPlayer->PlayerFolder.FileNumLen
					|| gpMediaPlayer->CurFileIndex < gpMediaPlayer->PlayerFolder.FirstFileIndex
					|| gpMediaPlayer->CurFileIndex > gpMediaPlayer->PlayerFolder.FirstFileIndex + gpMediaPlayer->PlayerFolder.FileNumLen - 1)
			{//�Ϸ��Լ��
				APP_DBG("No file in dir or file Index out dir!\n ");
			}
			gpMediaPlayer->CurFileIndex = gpMediaPlayer->PlayerFolder.FirstFileIndex + GetRandomNum((uint16_t)GetSysTick1MsCnt(), gpMediaPlayer->PlayerFolder.FileNumLen) - 1;
			APP_DBG("Cur File Num = %d in Folder %d\n", gpMediaPlayer->CurFileIndex - gpMediaPlayer->PlayerFolder.FirstFileIndex + 1 , gpMediaPlayer->PlayerFolder.FolderIndex);
			break;

		case PLAY_MODE_REPEAT_FOLDER:
#ifdef FUNC_SPECIFY_FOLDER_PLAY_EN
			gpMediaPlayer->StorySelectPlayFlag=STORY_PLAY_NEXT;
			break;
#endif		
			if(!gpMediaPlayer->PlayerFolder.FileNumLen
					|| gpMediaPlayer->CurFileIndex < gpMediaPlayer->PlayerFolder.FirstFileIndex
					|| gpMediaPlayer->CurFileIndex > gpMediaPlayer->PlayerFolder.FirstFileIndex + gpMediaPlayer->PlayerFolder.FileNumLen - 1)
			{//�Ϸ��Լ��
				APP_DBG("No file in dir or file Index out dir!\n ");
			}
			gpMediaPlayer->CurFileIndex++;
			if(gpMediaPlayer->CurFileIndex > gpMediaPlayer->PlayerFolder.FirstFileIndex + gpMediaPlayer->PlayerFolder.FileNumLen - 1)
			{
				gpMediaPlayer->CurFileIndex = gpMediaPlayer->PlayerFolder.FirstFileIndex;
			}
			APP_DBG("Cur File Num = %d in Folder %d\n", gpMediaPlayer->CurFileIndex - gpMediaPlayer->PlayerFolder.FirstFileIndex + 1 , gpMediaPlayer->PlayerFolder.FolderIndex);
			break;
		case PLAY_MODE_REPEAT_ONE:
			if(IsSongPlayEnd)
			{
				break;
			}
		case PLAY_MODE_REPEAT_OFF:
		case PLAY_MODE_REPEAT_ALL:
		case PLAY_MODE_PREVIEW_PLAY:
			gpMediaPlayer->CurFileIndex++;
			if(gpMediaPlayer->CurFileIndex > gpMediaPlayer->TotalFileSumInDisk)
			{
				gpMediaPlayer->CurFileIndex = 1;
			}
			APP_DBG("Cur File Num = %d\n", gpMediaPlayer->CurFileIndex);
			break;
#ifdef FUNC_BROWSER_PARALLEL_EN
		case PLAY_MODE_BROWSER:
			if(gpMediaPlayer->CurPlayMode == PLAY_MODE_BROWSER && GetBrowserPlay_state() == Browser_Play_Normal)
			{
				APP_DBG("browser normal play,next song\n");
				BrowserDn(Browser_File);
			}
			else if(gpMediaPlayer->CurPlayMode == PLAY_MODE_BROWSER && (GetBrowserPlay_state() != Browser_Play_Normal))
			{
				
				if(!gpMediaPlayer->PlayerFolder.FileNumLen
					|| gpMediaPlayer->CurFileIndex < gpMediaPlayer->PlayerFolder.FirstFileIndex
					|| gpMediaPlayer->CurFileIndex > gpMediaPlayer->PlayerFolder.FirstFileIndex + gpMediaPlayer->PlayerFolder.FileNumLen - 1)
				{//�Ϸ��Լ��
					APP_DBG("No file in dir or file Index out dir!\n ");
				}
				gpMediaPlayer->CurFileIndex++;
				if(gpMediaPlayer->CurFileIndex > gpMediaPlayer->PlayerFolder.FirstFileIndex + gpMediaPlayer->PlayerFolder.FileNumLen - 1)
				{
					gpMediaPlayer->CurFileIndex = gpMediaPlayer->PlayerFolder.FirstFileIndex;
				}
				APP_DBG("Cur File Num = %d in Folder %d\n", gpMediaPlayer->CurFileIndex - gpMediaPlayer->PlayerFolder.FirstFileIndex + 1, gpMediaPlayer->PlayerFolder.FolderIndex);

				}
			else
			{
				//PLAY_MODE_BROWSER mode ,but not play,so play as repeat all
				gpMediaPlayer->CurFileIndex++;
				if(gpMediaPlayer->CurFileIndex > gpMediaPlayer->TotalFileSumInDisk)
				{
					gpMediaPlayer->CurFileIndex = 1;
				}
				//APP_DBG("browser not normal play,next song=CurFileIndex TotalFileSumInDisk\n",gpMediaPlayer->CurFileIndex,gpMediaPlayer->TotalFileSumInDisk);
			}
		break;
#endif
		
		//case PLAY_MODE_REPEAT_ONE:
		default:
			break;
	}
	gpMediaPlayer->CurPlayTime = 0;
	gpMediaPlayer->SongSwitchFlag = 0;

	MediaPlayerRepeatABClear();
	SetMediaPlayerState(PLAYER_STATE_PLAYING);

#ifdef CFG_FUNC_DISPLAY_EN
	msgSend.msgId = MSG_DISPLAY_SERVICE_FILE_NUM;
	MessageSend(GetDisplayMessageHandle(), &msgSend);
#endif
}

//������ ��һ�� �趨
void MediaPlayerPreSong(void)
{
#ifdef CFG_FUNC_DISPLAY_EN
	MessageContext	msgSend;
#endif
#ifdef CFG_FUNC_UDISK_DETECT
	if((GetSystemMode() == AppModeUDiskAudioPlay)
		&& (UDiskRemovedFlagGet() == TRUE))
	{
		return;
	}
#endif
	if(CmdErrCnt >= 3)//��ʱ����U���쳣����U�̲��ŵ����
	{
		osTaskDelay(5);
		return;
	}
	APP_DBG("PlayCtrl:MediaPlayerPreSong\n");
	switch(gpMediaPlayer->CurPlayMode)
	{
		case PLAY_MODE_RANDOM_ALL:
			gpMediaPlayer->CurFileIndex = GetRandomNum((uint16_t)GetSysTick1MsCnt(), gpMediaPlayer->TotalFileSumInDisk);
			break;
		case PLAY_MODE_RANDOM_FOLDER:
			if(!gpMediaPlayer->PlayerFolder.FileNumLen
					|| gpMediaPlayer->CurFileIndex < gpMediaPlayer->PlayerFolder.FirstFileIndex
					|| gpMediaPlayer->CurFileIndex > gpMediaPlayer->PlayerFolder.FirstFileIndex + gpMediaPlayer->PlayerFolder.FileNumLen - 1)
			{//�Ϸ��Լ��
				APP_DBG("No file in dir or file Index out dir!\n ");
			}
			gpMediaPlayer->CurFileIndex = gpMediaPlayer->PlayerFolder.FirstFileIndex + GetRandomNum((uint16_t)GetSysTick1MsCnt(), gpMediaPlayer->PlayerFolder.FileNumLen) - 1;
			APP_DBG("Cur File Num = %d in Folder %d\n", gpMediaPlayer->CurFileIndex - gpMediaPlayer->PlayerFolder.FirstFileIndex + 1, gpMediaPlayer->PlayerFolder.FolderIndex);
			break;

		case PLAY_MODE_REPEAT_FOLDER:
			
#ifdef FUNC_SPECIFY_FOLDER_PLAY_EN
			DecSongNumber();
			gpMediaPlayer->StoryPlayByIndexFlag=TRUE;
			break;
#endif		
			if(!gpMediaPlayer->PlayerFolder.FileNumLen
					|| gpMediaPlayer->CurFileIndex < gpMediaPlayer->PlayerFolder.FirstFileIndex
					|| gpMediaPlayer->CurFileIndex > gpMediaPlayer->PlayerFolder.FirstFileIndex + gpMediaPlayer->PlayerFolder.FileNumLen - 1)
			{//�Ϸ��Լ��
				APP_DBG("No file in dir or file Index out dir!\n ");
			}
			gpMediaPlayer->CurFileIndex--;
			if(gpMediaPlayer->CurFileIndex < gpMediaPlayer->PlayerFolder.FirstFileIndex)
			{
				gpMediaPlayer->CurFileIndex = gpMediaPlayer->PlayerFolder.FirstFileIndex  + gpMediaPlayer->PlayerFolder.FileNumLen - 1;
			}
			APP_DBG("Cur File Num = %d in Folder %d\n", gpMediaPlayer->CurFileIndex - gpMediaPlayer->PlayerFolder.FirstFileIndex + 1 , gpMediaPlayer->PlayerFolder.FolderIndex);
			break;

		case PLAY_MODE_REPEAT_OFF:
		case PLAY_MODE_REPEAT_ALL:
		case PLAY_MODE_REPEAT_ONE:
		case PLAY_MODE_PREVIEW_PLAY:
			gpMediaPlayer->CurFileIndex--;
			if(gpMediaPlayer->CurFileIndex < 1)
			{
				gpMediaPlayer->CurFileIndex = gpMediaPlayer->TotalFileSumInDisk;
				APP_DBG("gpMediaPlayer->CurFileIndex = %d\n", gpMediaPlayer->CurFileIndex);
			}			
			break;
		//case PLAY_MODE_REPEAT_ONE:

#ifdef FUNC_BROWSER_PARALLEL_EN
		case PLAY_MODE_BROWSER:
			if(gpMediaPlayer->CurPlayMode == PLAY_MODE_BROWSER && GetBrowserPlay_state() == Browser_Play_Normal)
			{
				APP_DBG("browser normal play,prev song\n");
				BrowserUp(Browser_File);
			}
			else if(gpMediaPlayer->CurPlayMode == PLAY_MODE_BROWSER && (GetBrowserPlay_state() != Browser_Play_Normal))
			{

				if(!gpMediaPlayer->PlayerFolder.FileNumLen
				|| gpMediaPlayer->CurFileIndex < gpMediaPlayer->PlayerFolder.FirstFileIndex
				|| gpMediaPlayer->CurFileIndex > gpMediaPlayer->PlayerFolder.FirstFileIndex + gpMediaPlayer->PlayerFolder.FileNumLen - 1)
				{//�Ϸ��Լ��
					APP_DBG("No file in dir or file Index out dir!\n ");
				}
				gpMediaPlayer->CurFileIndex--;
				if(gpMediaPlayer->CurFileIndex < gpMediaPlayer->PlayerFolder.FirstFileIndex)
				{
					gpMediaPlayer->CurFileIndex = gpMediaPlayer->PlayerFolder.FirstFileIndex + gpMediaPlayer->PlayerFolder.FileNumLen - 1;
				}
				APP_DBG("Cur File Num = %d in Folder %d\n", gpMediaPlayer->CurFileIndex - gpMediaPlayer->PlayerFolder.FirstFileIndex + 1, gpMediaPlayer->PlayerFolder.FolderIndex);

			}
			else
			{
				//PLAY_MODE_BROWSER mode ,but not play,so play as repeat all
				gpMediaPlayer->CurFileIndex--;
				if(gpMediaPlayer->CurFileIndex < 1)
				{
					gpMediaPlayer->CurFileIndex = gpMediaPlayer->TotalFileSumInDisk;
					APP_DBG("gpMediaPlayer->CurFileIndex = %d\n", gpMediaPlayer->CurFileIndex);
				}			
			}
			break;
#endif

		default:
			break;
	}	
	gpMediaPlayer->CurPlayTime = 0;
	gpMediaPlayer->SongSwitchFlag = 1;

	MediaPlayerRepeatABClear();
	SetMediaPlayerState(PLAYER_STATE_PLAYING);

#ifdef CFG_FUNC_DISPLAY_EN
	msgSend.msgId = MSG_DISPLAY_SERVICE_FILE_NUM;
	MessageSend(GetDisplayMessageHandle(), &msgSend);
#endif
}

//������ ��ͣ�Ͳ��� �趨
void MediaPlayerPlayPause(void)
{
	switch(GetMediaPlayerState())
	{
		case PLAYER_STATE_PLAYING:
			SetMediaPlayerState(PLAYER_STATE_PAUSE);
			if(!SoftFlagGet(SoftFlagDecoderMask & ~SoftFlagDecoderApp))
			{
				DecoderPause();
			}
			break;
		case PLAYER_STATE_STOP:
		case PLAYER_STATE_IDLE:
			SetMediaPlayerState(PLAYER_STATE_PLAYING);
			if(!SoftFlagGet(SoftFlagDecoderMask & ~SoftFlagDecoderApp))
			{
				MediaPlayerSongRefresh();
			}
			break;
		case PLAYER_STATE_FB:
			DecoderPause();
			break;
		default:
			SetMediaPlayerState(PLAYER_STATE_PLAYING);
			if(!SoftFlagGet(SoftFlagDecoderMask & ~SoftFlagDecoderApp))
			{
				DecoderResume();
			}
			break;
	}	
}

// ������ ֹ̬ͣ�趨
void MediaPlayerStop(void)
{
	APP_DBG("MediaPlayerStop\n");	
	gpMediaPlayer->CurPlayTime = 0;
	SetMediaPlayerState(PLAYER_STATE_IDLE);
	MediaPlayerDecoderStop();
}

//������״̬ ����趨
void MediaPlayerFastForward(void)
{
	APP_DBG("MediaPlayerFastForward\n");

	if (gpMediaPlayer->RepeatAB.RepeatFlag == REPEAT_OPENED_PAUSE) {
		return;
	}
	if(SoftFlagGet(SoftFlagDecoderMask & ~SoftFlagDecoderApp))
	{
		if(gpMediaPlayer->CurPlayTime >= FF_PLAY_TIME_MAX - FF_FB_STEP / 1000)
		{
			gpMediaPlayer->CurPlayTime = FF_PLAY_TIME_MAX;
		}
		else
		{
			gpMediaPlayer->CurPlayTime += FF_FB_STEP / 1000;
		}
	}
	else //ֱ�ӵ���������
	{
		if((GetMediaPlayerState() == PLAYER_STATE_IDLE)	// ֹͣ״̬�£���ֹ���������
			|| (GetMediaPlayerState() == PLAYER_STATE_STOP)
			|| (GetMediaPlayerState() == PLAYER_STATE_PAUSE))
		{
			return;
		}
		if(GetDecoderState() == 0)//decoder end Ӧ�ý�ֹ�˲���
		{
			MediaPlayerCloseSongFile();
			
			MediaPlayerNextSong(TRUE);
			if(!MediaPlayerFileDecoderInit())
			{
				;//�ļ�������
			}
			//SetMediaPlayerState(PLAYER_STATE_PLAYING);

			return;
		}
		DecoderFF(FF_FB_STEP);
		SetMediaPlayerState(PLAYER_STATE_FF);
	}
}

//������״̬ ����
void MediaPlayerFastBackward(void)
{
	APP_DBG("MediaPlayerFastBackward\n");

	if (gpMediaPlayer->RepeatAB.RepeatFlag == REPEAT_OPENED_PAUSE) {
		return;
	}
	
	if (GetMediaPlayerState() == PLAYER_STATE_FB && GetDecoderState() == DecoderStatePause) {
		APP_DBG("Doesn't Fast Backward\n");	
		return;
	}
	
	if(SoftFlagGet(SoftFlagDecoderMask & ~SoftFlagDecoderApp))
	{
		if(gpMediaPlayer->CurPlayTime == 0 || gpMediaPlayer->CurPlayTime == FB_PLAY_LAST)
		{
			gpMediaPlayer->CurPlayTime = FB_PLAY_LAST;
		}
		else if(gpMediaPlayer->CurPlayTime <= FF_FB_STEP / 1000)
		{
			gpMediaPlayer->CurPlayTime = 0;
		}
		else
		{
			gpMediaPlayer->CurPlayTime -= FF_FB_STEP / 1000;
		}
	}
	else //������Ч
	{
		if((GetMediaPlayerState() == PLAYER_STATE_IDLE)	// ֹͣ״̬����ֹ���������
			|| (GetMediaPlayerState() == PLAYER_STATE_STOP)
			|| (GetMediaPlayerState() == PLAYER_STATE_PAUSE))
		{
			return;
		}
		if(GetDecoderState() == 0)//���������У�δ��ʼ������������һ���ļ���Ӧ�ý�ֹ�˲���
		{
			MediaPlayerCloseSongFile();
			MediaPlayerPreSong();

			if(!MediaPlayerFileDecoderInit())
			{
				;//������
			}
			//SetMediaPlayerState(PLAYER_STATE_PLAYING);
			return;
		}

		DecoderFB(FF_FB_STEP);//�ɽ�������ȡ�ļ�ʱ�����ж�seek���ж�������
		SetMediaPlayerState(PLAYER_STATE_FB);
	}
}

//������״̬ �������ֹͣ�趨
void MediaPlayerFFFBEnd(void)
{
	APP_DBG("MediaPlayerFFFBEnd\n");
	if(SoftFlagGet(SoftFlagDecoderMask & ~SoftFlagDecoderApp))//��������ռ��
	{
		if(gpMediaPlayer->CurPlayTime == FB_PLAY_LAST)
		{
			MediaPlayerPreSong();
		}
		else if(gpMediaPlayer->CurPlayTime == FF_PLAY_TIME_MAX)
		{
			MediaPlayerNextSong(TRUE);
		}
		SetMediaPlayerState(PLAYER_STATE_PLAYING);
	}
	else //ֱ�ӵ���������
	{
		if((GetMediaPlayerState() == PLAYER_STATE_IDLE) // ֹͣ״̬����ֹ���������
			|| (GetMediaPlayerState() == PLAYER_STATE_STOP)
			|| (GetMediaPlayerState() == PLAYER_STATE_PAUSE))
		{
			return;
		}

		DecoderResume();
		if(gpMediaPlayer->RepeatAB.RepeatFlag == 3)
		{
			gpMediaPlayer->RepeatAB.RepeatFlag = 2;
			if(GetMediaPlayerState() == PLAYER_STATE_FF)
			{
				DecoderFB(gpMediaPlayer->RepeatAB.Times * 1000);
			} 
			else if(GetMediaPlayerState() == PLAYER_STATE_FF)
			{
				//DecoderFF((gpMediaPlayer->RepeatAB.StartTime - gpMediaPlayer->CurPlayTime) * 1000);
			}
		}
		SetMediaPlayerState(PLAYER_STATE_PLAYING);
	}
}

//playģʽ�л�,ָ��ģʽ/��ѭ��  ����ϸ������ţ�
//��apiǰ���ǵ�ǰ��Ҫ���ŵ��ļ������Ѿ�open�������޷�ˢ���ļ�����Ϣ
void MediaPlayerSwitchPlayMode(uint8_t PlayMode)
{
	if(GetSystemMode() == AppModeUDiskAudioPlay 
	|| GetSystemMode() == AppModeCardAudioPlay
	|| GetSystemMode() == AppModeCardPlayBack
	|| GetSystemMode() == AppModeUDiskPlayBack
	)
	{
		APP_DBG("MediaPlayerSwitchPlayMode\n");

		if(PlayMode < PLAY_MODE_SUM)
		{
			gpMediaPlayer->CurPlayMode = PlayMode;
		}
		else
		{
#ifdef FUNC_BROWSER_PARALLEL_EN
			if(gpMediaPlayer->CurPlayMode == PLAY_MODE_RANDOM_FOLDER)
			{
				EnterBrowserPlayMode();
			}
			else if(gpMediaPlayer->CurPlayMode == PLAY_MODE_BROWSER)
			{
				ExitBrowserPlayMode();
			}
			else
#endif
			{
				gpMediaPlayer->CurPlayMode++;
				gpMediaPlayer->CurPlayMode %= PLAY_MODE_SUM;
			}


			if(gpMediaPlayer->CurPlayMode==PLAY_MODE_PREVIEW_PLAY)
			{
				MediaPlayerStop();
				gpMediaPlayer->CurPlayTime = 0;
				gpMediaPlayer->SongSwitchFlag = 0;
				SetMediaPlayerState(PLAYER_STATE_PLAYING);
			}

		}
		
		APP_DBG("mode = %d\n", gpMediaPlayer->CurPlayMode);
#ifdef CFG_FUNC_BREAKPOINT_EN
		BackupInfoUpdata(BACKUP_PLAYER_INFO);
#ifdef BP_PART_SAVE_TO_NVM
		BackupInfoUpdata(BACKUP_PLAYER_INFO_2NVM);
#endif
#endif
		

	}
}


//������ ��һ�ļ��� �趨
void MediaPlayerPreFolder(void)
{
	uint16_t CurFileIndexTemp=0;

	if((GetSystemMode() == AppModeUDiskAudioPlay || GetSystemMode() == AppModeCardAudioPlay)
			&& gpMediaPlayer->ValidFolderSumInDisk > 1)//
	{
	
		APP_DBG("MediaPlayerPreFolder\n");
		MediaPlayerDecoderStop();
		
		f_file_count_in_dir(&gpMediaPlayer->PlayerFolder);

		if(!gpMediaPlayer->PlayerFolder.FileNumLen) //��ǰ�ļ������ļ���������Ϊ0
		{
			APP_DBG("No play file in folder!\n ");
			return;
		}
		
		if(gpMediaPlayer->PlayerFolder.FirstFileIndex == 1)//��һ����Ч�ļ��У��л����һ���ļ��У��Ȳ����һ��
		{
			CurFileIndexTemp = gpMediaPlayer->TotalFileSumInDisk;
		}
		else //��ǰһ���ļ������һ��
		{
			CurFileIndexTemp = gpMediaPlayer->PlayerFolder.FirstFileIndex - 1;
		}
		gpMediaPlayer->CurPlayTime = 0;
		gpMediaPlayer->SongSwitchFlag = 1;
		
		//�ǵ�ǰfolder �������´��ļ��ţ�get�ļ��к�
		if(FR_OK != f_open_by_num(current_vol, &gpMediaPlayer->PlayerFolder, &gpMediaPlayer->PlayerFile, CurFileIndexTemp, (char*)file_longname))
		{
			APP_DBG(("FileOpenByNum() error!\n"));
			return;
		}
		
		f_close(&gpMediaPlayer->PlayerFile);
		
		
		gpMediaPlayer->CurFileIndex=gpMediaPlayer->PlayerFolder.FirstFileIndex;//gpMediaPlayer->PlayerFolder.FirstFileIndex;
		
				
		if(FR_OK != f_open_by_num(current_vol, &gpMediaPlayer->PlayerFolder, &gpMediaPlayer->PlayerFile, gpMediaPlayer->CurFileIndex, (char*)file_longname))
		{
			APP_DBG(("FileOpenByNum() error!\n"));
			return;
		}

		
		gpMediaPlayer->CurFolderIndex = f_dir_with_song_real_quantity_cur();

		f_file_count_in_dir(&gpMediaPlayer->PlayerFolder);

	}
}
//������ ��һ�ļ��� �趨
void MediaPlayerNextFolder(void)
{
	if((GetSystemMode() == AppModeUDiskAudioPlay || GetSystemMode() == AppModeCardAudioPlay)
			&& gpMediaPlayer->ValidFolderSumInDisk > 1)
	{
		
		APP_DBG("MediaPlayerNextFolder\n");
		
		MediaPlayerDecoderStop();
		f_file_count_in_dir(&gpMediaPlayer->PlayerFolder);

		if(!gpMediaPlayer->PlayerFolder.FileNumLen) //��ǰ�ļ������ļ���������Ϊ0
		{
			APP_DBG("No play file in folder!\n ");
			return;
		}
		if(gpMediaPlayer->PlayerFolder.FirstFileIndex + gpMediaPlayer->PlayerFolder.FileNumLen - 1 == gpMediaPlayer->TotalFileSumInDisk)//���һ����Ч�ļ��У��л���һ���ļ��У��Ȳ���һ��
		{
			gpMediaPlayer->CurFileIndex = 1;
		}
		else //����һ���ļ��е�һ
		{
			gpMediaPlayer->CurFileIndex = gpMediaPlayer->PlayerFolder.FirstFileIndex+ gpMediaPlayer->PlayerFolder.FileNumLen;
		}
		gpMediaPlayer->CurPlayTime = 0;
		gpMediaPlayer->SongSwitchFlag = 0;
		if(FR_OK != f_open_by_num(current_vol, &gpMediaPlayer->PlayerFolder, &gpMediaPlayer->PlayerFile, gpMediaPlayer->CurFileIndex, (char*)file_longname))
		{
			APP_DBG(("FileOpenByNum() error!\n"));
			return;
		}

		gpMediaPlayer->CurFolderIndex = f_dir_with_song_real_quantity_cur();

		f_file_count_in_dir(&gpMediaPlayer->PlayerFolder);

	}
}

void MediaPlayerRepeatABClear(void)
{
	if(gpMediaPlayer->RepeatAB.RepeatFlag & 0x03)
	{
		gpMediaPlayer->RepeatAB.RepeatFlag = 0;
		gpMediaPlayer->RepeatAB.StartTime = 0;
		gpMediaPlayer->RepeatAB.Times = 0;
	}
}

void MediaPlayerTimerCB(void)
{
	if(GetMediaPlayerState() == PLAYER_STATE_PLAYING
			&& (gpMediaPlayer->CurPlayTime >= (gpMediaPlayer->RepeatAB.StartTime + gpMediaPlayer->RepeatAB.Times)))
	{
		DecoderFB(gpMediaPlayer->RepeatAB.Times * 1000);
		APP_DBG("Repeat Mode running\n");
	}
	else if(GetMediaPlayerState() == PLAYER_STATE_FF
			&& (gpMediaPlayer->CurPlayTime >= (gpMediaPlayer->RepeatAB.StartTime + gpMediaPlayer->RepeatAB.Times)))
	{
		gpMediaPlayer->RepeatAB.RepeatFlag = 3;
		DecoderFB(gpMediaPlayer->CurPlayTime * 1000 - (gpMediaPlayer->RepeatAB.StartTime + gpMediaPlayer->RepeatAB.Times) * 1000);
		gpMediaPlayer->CurPlayTime = gpMediaPlayer->RepeatAB.StartTime + gpMediaPlayer->RepeatAB.Times;
		DecoderPause();
		APP_DBG("FF: Pause the AB\n");
	}
	else if(GetMediaPlayerState() == PLAYER_STATE_FB
			&& (gpMediaPlayer->CurPlayTime <= gpMediaPlayer->RepeatAB.StartTime))
	{
		gpMediaPlayer->RepeatAB.RepeatFlag = 3;
		AudioCoreSourceMute(APP_SOURCE_NUM, TRUE, TRUE);
		DecoderFF((gpMediaPlayer->RepeatAB.StartTime - gpMediaPlayer->CurPlayTime) * 1000);
		AudioCoreSourceUnmute(APP_SOURCE_NUM, TRUE, TRUE);
		DecoderPause();
		APP_DBG("FB: Pause The AB");
	}
}


void MediaPlayerRepeatAB(void)
{
	if(GetMediaPlayerState() != PLAYER_STATE_PLAYING
		|| (mainAppCt.appCurrentMode != AppModeCardAudioPlay
		&& mainAppCt.appCurrentMode != AppModeUDiskAudioPlay
		&& mainAppCt.appCurrentMode != AppModeCardPlayBack
		&& mainAppCt.appCurrentMode != AppModeUDiskPlayBack
		))
	{
		APP_DBG("does't response RepeateAB mode\n");
		return;
	} 

	switch (gpMediaPlayer->RepeatAB.RepeatFlag)
	{
		case REPEAT_CLOSED:
			gpMediaPlayer->RepeatAB.StartTime  = gpMediaPlayer->CurPlayTime;
			gpMediaPlayer->RepeatAB.RepeatFlag = REPEAT_A_SETED;
			APP_DBG("Set RepeatAB StartTime = %d s\n", gpMediaPlayer->RepeatAB.StartTime);
			break;
		case REPEAT_A_SETED:
			if (gpMediaPlayer->CurPlayTime <= gpMediaPlayer->RepeatAB.StartTime) {
				// �����쳣�������
				gpMediaPlayer->RepeatAB.RepeatFlag = REPEAT_CLOSED;
				APP_DBG("RepeatAB fail, start time <= end time\n");
				break;
			}
			gpMediaPlayer->RepeatAB.Times      = (gpMediaPlayer->CurPlayTime - gpMediaPlayer->RepeatAB.StartTime);
			gpMediaPlayer->RepeatAB.RepeatFlag = REPEAT_OPENED;
			APP_DBG("Set RepeatAB Duration = %d s\n", gpMediaPlayer->RepeatAB.Times);
			//DecoderFB(gpMediaPlayer->RepeatAB.Times * 1000);
			break;

		case REPEAT_OPENED:
			APP_DBG("open repeat mode\n");
			gpMediaPlayer->RepeatAB.RepeatFlag = REPEAT_CLOSED;
			APP_DBG("Repeat Mode Over\n");
			break;

		default:
			gpMediaPlayer->RepeatAB.RepeatFlag = REPEAT_CLOSED;
			APP_DBG("Cancel RepeatAB\n");
			break;
	}
}

#ifdef FUNC_BROWSER_PARALLEL_EN	
extern uint32_t gStartFolderIndex;
extern uint32_t gFolderFocusing;
extern uint32_t gFileFocusing;// 1-N
extern uint32_t gStartFileIndex;
extern ff_file_win gBrowserFileWin[GUI_ROW_CNT_MAX];
extern ff_dir_win gBrowserDirWin[GUI_ROW_CNT_MAX];	
#endif

//����ʵʱ��Ϣ���� log
void MediaPlayerTimeUpdate(void)
{
	if(SoftFlagGet(SoftFlagDecoderMask & ~SoftFlagDecoderApp))
	{
		return;
	}
	if((GetMediaPlayerState() == PLAYER_STATE_PLAYING)
		|| (GetMediaPlayerState() == PLAYER_STATE_FF)
		|| (GetMediaPlayerState() == PLAYER_STATE_FB))
	{
		gpMediaPlayer->CurPlayTime = DecoderServicePlayTimeGet();//���²���ʱ��
#if  (defined(FUNC_BROWSER_PARALLEL_EN) || defined(FUNC_BROWSER_TREE_EN))
	if(GetShowGuiTime()==0)
#endif
	{
		if(MediaPlayDevice() == DEV_ID_USB)
		{
			APP_DBG("USB ");
		}
		else if(MediaPlayDevice() == DEV_ID_SD)
		{
			APP_DBG("SD ");
		}
		else if(MediaPlayDevice() == DEV_ID_FLASHFS)
		{
			APP_DBG("Flash ");
		}
		if((gpMediaPlayer->CurPlayMode == PLAY_MODE_RANDOM_FOLDER)
		|| (gpMediaPlayer->CurPlayMode == PLAY_MODE_REPEAT_FOLDER))
		{
#ifdef FUNC_SPECIFY_FOLDER_PLAY_EN

			APP_DBG("^%s (%d/%d)",
					GetFolderStr(),
					GetSongNumberCurrent(),
					GetTotalSongsInFolder());		


#else
			APP_DBG("^F(%d/%d, %d/%d) ",
					//(int)gpMediaPlayer->PlayerFolder.FolderIndex,
					(int)gpMediaPlayer->CurFolderIndex,
					(int)gpMediaPlayer->ValidFolderSumInDisk,
					(int)(gpMediaPlayer->CurFileIndex - gpMediaPlayer->PlayerFolder.FirstFileIndex + 1),
					(int)gpMediaPlayer->PlayerFolder.FileNumLen);

#endif


		}
#ifdef FUNC_BROWSER_PARALLEL_EN
		else if(gpMediaPlayer->CurPlayMode == PLAY_MODE_BROWSER && GetBrowserPlay_state() == Browser_Play_Normal)
		{
			APP_DBG("^F(%d/%d, %d/%d) ",
					//(int)gpMediaPlayer->PlayerFolder.FolderIndex,
					(int)(gStartFolderIndex+gFolderFocusing-1),
					(int)gpMediaPlayer->ValidFolderSumInDisk,
					(int)(gpMediaPlayer->CurFileIndex - gpMediaPlayer->PlayerFolder.FirstFileIndex + 1),
					(int)gpMediaPlayer->PlayerFolder.FileNumLen);
		}	
		else if(gpMediaPlayer->CurPlayMode == PLAY_MODE_BROWSER && GetBrowserPlay_state() != Browser_Play_Normal)
		{
			APP_DBG("^F(%d/%d, %d/%d) ",
					//(int)gpMediaPlayer->PlayerFolder.FolderIndex,
					(int)gpMediaPlayer->CurFolderIndex, //(int)(f_dir_with_song_real_quantity_cur()),
					(int)gpMediaPlayer->ValidFolderSumInDisk,
					(int)(gpMediaPlayer->CurFileIndex - gpMediaPlayer->PlayerFolder.FirstFileIndex + 1),
					(int)gpMediaPlayer->PlayerFolder.FileNumLen);
		}	
#endif
		else
		{
			APP_DBG("^D(%d/%d, %d/%d) ",
					//(int)gpMediaPlayer->PlayerFolder.FolderIndex,
					(int)gpMediaPlayer->CurFolderIndex,
					(int)gpMediaPlayer->ValidFolderSumInDisk,
					(int)gpMediaPlayer->CurFileIndex,
					(int)gpMediaPlayer->TotalFileSumInDisk);
		}
#ifdef	CFG_FUNC_RECORD_SD_UDISK
		if(GetSystemMode() == AppModeUDiskPlayBack || GetSystemMode() == AppModeCardPlayBack)
		{
			TCHAR NameStr[FILE_PATH_LEN];
			IntToStrMP3Name(NameStr, gpMediaPlayer->RecFileList[gpMediaPlayer->CurFileIndex - 1]);
			APP_DBG("^%s, %02d:%02d ",
					NameStr,
					(int)(gpMediaPlayer->CurPlayTime ) / 60,
					(int)(gpMediaPlayer->CurPlayTime ) % 60);

		}
		else
#elif defined(CFG_FUNC_RECORD_FLASHFS)
		if(GetSystemMode() == AppModeFlashFsPlayBack)
		{
			APP_DBG("^%s, %02d:%02d ",
					CFG_PARA_FLASHFS_FILE_NAME,
					(int)(gpMediaPlayer->CurPlayTime ) / 60,
					(int)(gpMediaPlayer->CurPlayTime ) % 60);
		}
		else
#endif
		{
			APP_DBG("^%s, %02d:%02d ",
					gpMediaPlayer->PlayerFile.fn,
					(int)(gpMediaPlayer->CurPlayTime ) / 60,
					(int)(gpMediaPlayer->CurPlayTime ) % 60);
		}
		switch(gpMediaPlayer->CurPlayMode)
		{
			case PLAY_MODE_REPEAT_ONE:
				APP_DBG("^RP_ONE ");
				break;
			case PLAY_MODE_REPEAT_ALL:
				APP_DBG("^RP_ALL ");
				break;
			case PLAY_MODE_REPEAT_FOLDER:
				APP_DBG("^RP_FOLDER ");
				break;
			case PLAY_MODE_RANDOM_FOLDER:
				APP_DBG("^RDM_FOLDER ");
				break;
			case PLAY_MODE_RANDOM_ALL:
				APP_DBG("^RDM_ALL ");
				break;
#ifdef FUNC_BROWSER_PARALLEL_EN
			case PLAY_MODE_BROWSER:
				APP_DBG("^BROWSER ");
				break;
#endif
			case PLAY_MODE_REPEAT_OFF:
 				APP_DBG("^RP_OFF ");
				break;
			
			case PLAY_MODE_PREVIEW_PLAY:
				APP_DBG("^PREVIEW ");
				break;

			default:
				break;
		}
		APP_DBG("^\n");
	}
#if  (defined(FUNC_BROWSER_PARALLEL_EN)||defined(FUNC_BROWSER_TREE_EN))
	else
		DecShowGuiTime();
#endif
#ifdef BP_PART_SAVE_TO_NVM
		if((GetSystemMode() == AppModeUDiskAudioPlay || GetSystemMode() == AppModeCardAudioPlay))
		{
			BackupInfoUpdata(BACKUP_PLAYER_INFO_2NVM);
		}
#endif
	}
#ifdef CFG_FUNC_LRC_EN
	if((GetSystemMode() == AppModeUDiskAudioPlay || GetSystemMode() == AppModeCardAudioPlay) && gpMediaPlayer->IsLrcRunning)
	{
		PlayerParseLrc();
	}
#endif // FUNC_LRC_EN
}

//ˢ�½�������
bool MediaPlayerDecoderRefresh(uint32_t SeekTime)
{
	SoftFlagDeregister(SoftFlagDecoderMask & ~SoftFlagDecoderApp);//������App�Խ������ĵǼ�
	if(!MediaPlayerDecoderInit())
	{
		return FALSE;
	}
	if(SeekTime != 0)
	{
		DecoderSeek(SeekTime);
	}
	MediaPlayerSongInfoLog();
	MediaPlayerUpdataDecoderSourceNum();
    return TRUE;
}

//������״̬���������ļ���ˢ�²�����/��������������������ֹͣ
//���ݲ��ط��룬���������»�ý������
bool MediaPlayerSongRefresh(void)
{
	SoftFlagDeregister(SoftFlagDecoderMask & ~SoftFlagDecoderApp);//������App�Խ������ĵǼ�
	if(gpMediaPlayer->CurPlayTime == FF_PLAY_TIME_MAX)
	{
		MediaPlayerNextSong(TRUE);
	}
	else if(gpMediaPlayer->CurPlayTime == FB_PLAY_LAST)
	{
		MediaPlayerPreSong();
	}
	MediaPlayerCloseSongFile();//�����ظ��ر����������ݡ�

	if(!MediaPlayerOpenSongFile())
	{
		return FALSE;//�ļ�����
	}
	//�����ʼ��ʧ��(��Ч�ļ�)������������ʷ�������л�������
	if(!MediaPlayerDecoderRefresh(gpMediaPlayer->CurPlayTime))
	{
		if((gpMediaPlayer->CurPlayMode == PLAY_MODE_REPEAT_FOLDER) || (gpMediaPlayer->CurPlayMode == PLAY_MODE_RANDOM_FOLDER))
		{
			if(gpMediaPlayer->ErrFileCount )
			{
				gpMediaPlayer->ErrFileCount --;
				if(!gpMediaPlayer->ErrFileCount )
				{
					MessageContext		msgSend;

					msgSend.msgId		= MSG_FOLDER_NEXT;
					MessageSend(GetAppMessageHandle(), &msgSend);
					vTaskDelay(5);
					return FALSE;

				}
			}		
		}
		APP_DBG("Refresh fail\n");
//		if(gpMediaPlayer->SongSwitchFlag == TRUE)
//		{
//			MessageContext		msgSend;
//
//			msgSend.msgId		= MSG_PRE;
//			MessageSend(GetAppMessageHandle(), &msgSend);
//		}
//		else
//		{
//			MessageContext		msgSend;
//
//			msgSend.msgId		= MSG_NEXT;
//			MessageSend(GetAppMessageHandle(), &msgSend);
//		}
		vTaskDelay(5);

		return FALSE;
	}
    else
	{
		if((gpMediaPlayer->CurPlayMode == PLAY_MODE_REPEAT_FOLDER) || (gpMediaPlayer->CurPlayMode == PLAY_MODE_RANDOM_FOLDER))
		{
			gpMediaPlayer->ErrFileCount = gpMediaPlayer->PlayerFolder.FileNumLen;
		}
	}
	switch(GetMediaPlayerState())
	{
		case PLAYER_STATE_PLAYING:
		case PLAYER_STATE_FF:
		case PLAYER_STATE_FB:
			SetMediaPlayerState(PLAYER_STATE_PLAYING);//
			DecoderPlay();
			break;
		case PLAYER_STATE_IDLE:
		case PLAYER_STATE_PAUSE:
			APP_DBG("Pause\n");
			SetMediaPlayerState(PLAYER_STATE_PAUSE);
			DecoderPause();
			break;
		case PLAYER_STATE_STOP:
			APP_DBG("STATE_STOP\n");
			break;
	}
	//�˴��Ƿ��жϿ�������������ȣ���Ϊ��һ�ף���ǰ�ǽ������Զ���
#ifdef CFG_FUNC_BREAKPOINT_EN
	if(GetSystemMode() == AppModeCardAudioPlay || GetSystemMode() == AppModeUDiskAudioPlay)
	{
		BackupInfoUpdata(BACKUP_SYS_INFO);
		BackupInfoUpdata(BACKUP_PLAYER_INFO);
#ifdef BP_PART_SAVE_TO_NVM
			BackupInfoUpdata(BACKUP_PLAYER_INFO_2NVM);
#endif
	}
#endif
	return TRUE;
}

//������ ֹͣ�������
void MediaPlayerDecoderStop(void)
{//��App�⣬����������û�и��ã��ҽ�������ֹ̬ͣ
	if(!SoftFlagGet(SoftFlagDecoderMask & ~SoftFlagDecoderApp))// && gpMediaPlayer->DecoderSync != DecoderStateStop)
	{
		if(gpMediaPlayer->DecoderSync != DecoderStateStop && gpMediaPlayer->DecoderSync != DecoderStateNone)
		{
			gpMediaPlayer->DecoderSync = DecoderStateStop;//�� app����ֹͣ������ ״̬
			DecoderMuteAndStop();
		}
	}
}

void MediaPlaySetDecoderState(DecoderState State)
{
	gpMediaPlayer->DecoderSync = State;
}

DecoderState MediaPlayGetDecoderState(void)
{
	return gpMediaPlayer->DecoderSync;
}

//������ ��ȡ��״̬
uint8_t GetMediaPlayerState(void)
{
	if(gpMediaPlayer != NULL)
	{
		return gpMediaPlayer->CurPlayState;
	}

	return PLAYER_STATE_IDLE;
}

//������ ״̬����
void SetMediaPlayerState(uint8_t state)
{
	if(gpMediaPlayer != NULL)
	{
		if(gpMediaPlayer->CurPlayState != state)
		{
			APP_DBG("SetMediaPlayerState %d\n", state);
			gpMediaPlayer->CurPlayState = state;
		}
	}
}

#ifdef CFG_FUNC_RECORD_SD_UDISK
//¼���ļ��� ��������
bool RecFilePlayerInit(void)
{
	FRESULT res;
	TCHAR PathStr[16];
	uint16_t j, k, i = 0;
	uint16_t	Backup;
		
	strcpy(PathStr, current_vol);
	strcat(PathStr, CFG_PARA_RECORDS_FOLDER);
	f_chdrive(current_vol);
	res = f_opendir(&gpMediaPlayer->Dir, PathStr);
	if(res != FR_OK)
	{
		APP_DBG("f_opendir failed: %s ret:%d\n", PathStr, res);
		return FALSE;
	}
	
	if(gpMediaPlayer->RecFileList == NULL)
	{
		gpMediaPlayer->RecFileList = (uint16_t*)osPortMalloc(FILE_INDEX_MAX * FILE_NAME_VALUE_SIZE);//¼���ļ������������
		if(gpMediaPlayer->RecFileList == NULL)
		{
			return FALSE;
		}
		memset(gpMediaPlayer->RecFileList, 0, FILE_INDEX_MAX * FILE_NAME_VALUE_SIZE);
	}

	while(((res = f_readdir(&gpMediaPlayer->Dir, &gpMediaPlayer->FileInfo)) == FR_OK) && gpMediaPlayer->FileInfo.fname[0])// && i != FILE_INDEX_MAX)
	{
		if(gpMediaPlayer->FileInfo.fattrib & AM_ARC && gpMediaPlayer->FileInfo.fsize)//�����Ǵ浵�ļ�������Ϊ0���ļ���
		{
			k = RecFileIndex(gpMediaPlayer->FileInfo.fname);//
			if(k)
			{
				for(j = 0; j <= i && j < FILE_INDEX_MAX; j++)//���򣬼��,����¼��ʱ˳����������
				{
					if(gpMediaPlayer->RecFileList[j] < k)
					{
						Backup = gpMediaPlayer->RecFileList[j];
						gpMediaPlayer->RecFileList[j] = k;
						k = Backup;
					}
				}
				i++;
			}
		}
	}
	gpMediaPlayer->TotalFileSumInDisk = i < FILE_INDEX_MAX ? i : FILE_INDEX_MAX;
	if(gpMediaPlayer->TotalFileSumInDisk == 0)
	{
		APP_DBG("No Rec File\n");
		return FALSE;
	}
	APP_DBG("RecList:%d\n", gpMediaPlayer->TotalFileSumInDisk);
	f_closedir(&gpMediaPlayer->Dir);
	return TRUE;
}

//���ݱ�Ŵ� ¼���ļ�
FRESULT f_open_recfile_by_num(FIL *filehandle, UINT Index)
{
	FRESULT ret;
	TCHAR PathStr[25];
	
	if(Index > 255 || Index > gpMediaPlayer->TotalFileSumInDisk || Index == 0)
	{
		return FR_NO_FILE;
	}
	Index--;
	strcpy(PathStr, current_vol);
	strcat(PathStr, CFG_PARA_RECORDS_FOLDER);
	strcat(PathStr,"/");
	IntToStrMP3Name(PathStr + strlen(PathStr), gpMediaPlayer->RecFileList[Index]);
	APP_DBG("%s", PathStr);

	ret = f_open(filehandle, PathStr, FA_READ);
	return ret;
}
#endif
#ifdef CFG_FUNC_RECORD_FLASHFS
uint32_t FlashFsReadFile(void *buffer, uint32_t length) // ��Ϊflashfs �ط�callback
{
	uint8_t ret = 0;
	if(length == 0)
	{
		return 0;
	}
    if(gpMediaPlayer->FlashFsFile)
    {
		 ret = Fread(buffer, length, 1, gpMediaPlayer->FlashFsFile);
    }
    if(ret)
    {
		//APP_DBG("R:%d\n",length);
    	return length;
    }
    else
    {
    	DecoderMuteAndStop();
    	APP_DBG("Read fail\n");
    	return 0;
    }
}

#endif

#ifdef DEL_REC_FILE_EN
void DelRecFile(void)// bkd add 
{
#ifdef CFG_FUNC_RECORD_SD_UDISK
	char FilePath[FILE_PATH_LEN];
	uint32_t i_count = 0;

/*
#ifdef CFG_FUNC_RECORD_SD_UDISK
			f_close(&gpMediaPlayer->PlayerFile);
#elif defined(CFG_FUNC_RECORD_FLASHFS) 
			if(RecorderCt->RecordFile)
			{
				Fclose(RecorderCt->RecordFile);
				RecorderCt->RecordFile = NULL;
			}
#endif

*/
	strcpy(FilePath, current_vol);
	strcat(FilePath, CFG_PARA_RECORDS_FOLDER);
	strcat(FilePath,"/");
	IntToStrMP3Name(FilePath + strlen(FilePath), gpMediaPlayer->RecFileList[gpMediaPlayer->CurFileIndex - 1]);
	f_unlink(FilePath);
	
	if(gpMediaPlayer->CurFileIndex == gpMediaPlayer->TotalFileSumInDisk)
	{
		gpMediaPlayer->CurFileIndex=1;
	}
	else	
	{
	
	for(i_count = gpMediaPlayer->CurFileIndex-1; i_count <= (gpMediaPlayer->TotalFileSumInDisk - 2); i_count++)
		gpMediaPlayer->RecFileList[i_count] = gpMediaPlayer->RecFileList[i_count + 1];
	
	}
	
	gpMediaPlayer->TotalFileSumInDisk--;
	
	APP_DBG("Del %s.mp3\n",FilePath);
#elif defined(CFG_FUNC_RECORD_FLASHFS) 
	Remove(CFG_PARA_FLASHFS_FILE_NAME);
#endif			
	APP_DBG("MSG_REC_FILE_DEL\n");

}
#endif

#if defined(FUNC_BROWSER_TREE_EN)||defined(FUNC_SPECIFY_FOLDER_PLAY_EN)
void MediaPlayerBrowserEnter(void)
{
	MediaPlayerStop();
	//SoftFlagRegister(SoftFlagDecoderSwitch);
	//vTaskDelay(1);
	gpMediaPlayer->CurPlayTime = 0;
	gpMediaPlayer->SongSwitchFlag = 0;
	SetMediaPlayerState(PLAYER_STATE_PLAYING);
}

void SetMediaPlayMode(uint8_t playmode)
{
	gpMediaPlayer->CurPlayMode = playmode;//PLAY_MODE_BROWSER;
}

#ifdef FUNC_SPECIFY_FOLDER_PLAY_EN
void MediaPlayerStoryEnter(void)
{
	MediaPlayerStop();
	gpMediaPlayer->CurPlayTime = 0;
	gpMediaPlayer->SongSwitchFlag = 0;
	SetMediaPlayerState(PLAYER_STATE_PLAYING);
	gpMediaPlayer->StorySelectPlayFlag=STORY_PLAY_CURRENT;
}
#endif

#endif

#ifdef FUNC_BROWSER_PARALLEL_EN
void SetMediaPlayMode(uint8_t playmode)
{
	gpMediaPlayer->CurPlayMode = playmode;//PLAY_MODE_BROWSER;
}

uint8_t GetMediaPlayMode(void)
{
	return gpMediaPlayer->CurPlayMode;//PLAY_MODE_BROWSER;
}
void MediaPlayerBrowserEnter(void)
{
	if(GetBrowserPlay_state() == Browser_Play_Normal
		&& (gpMediaPlayer->CurFileIndex == gBrowserDirWin[gFolderFocusing - 1].first_file_index + (gStartFileIndex+gFileFocusing - 1) - 1))
	{
		MediaPlayerPlayPause();
	}
	else
	{
		MediaPlayerStop();
		//SoftFlagRegister(SoftFlagDecoderSwitch);
		//vTaskDelay(1);
		gpMediaPlayer->CurPlayTime = 0;
		gpMediaPlayer->SongSwitchFlag = 0;
		SetMediaPlayerState(PLAYER_STATE_PLAYING);
	}
}

void MediaPlayerSongBrowserRefresh(void)
{
	
	SoftFlagDeregister(SoftFlagDecoderMask & ~SoftFlagDecoderApp);//������App�Խ������ĵǼ�
	MediaPlayerCloseSongFile();//�����ظ��ر����������ݡ�
	
	/*
		memset(&gpMediaPlayer->PlayerFile, 0x00, sizeof(FIL));
		memset(&gpMediaPlayer->PlayerFolder, 0x00, sizeof(ff_dir));
		memcpy(&gpMediaPlayer->PlayerFolder, &gBrowserDirWin[gFolderFocusing- 1], sizeof(ff_dir));
		memcpy(&gpMediaPlayer->PlayerFile, &gBrowserFileWin[gFileFocusing - 1], sizeof(FIL));
		gpMediaPlayer->CurFileIndex = gBrowserDirWin[gFolderFocusing - 1].first_file_index + (gStartFileIndex+gFileFocusing - 1) - 1;
		gpMediaPlayer->PlayerFolder.FirstFileIndex = gBrowserDirWin[gFolderFocusing - 1].first_file_index;
		gpMediaPlayer->PlayerFolder.FileNumLen = gBrowserDirWin[gFolderFocusing - 1].valid_file_num;
		APP_DBG("file_index_in_disk=%d file_index_in_folder=%ld\n", gpMediaPlayer->CurFileIndex, (gStartFileIndex+gFileFocusing - 1));
		SongFileType = get_audio_type((TCHAR *)gpMediaPlayer->PlayerFile.fn);
	*/
	
	
		gpMediaPlayer->CurFileIndex = gBrowserDirWin[gFolderFocusing - 1].first_file_index + (gStartFileIndex+gFileFocusing - 1) - 1;
	
		if(!MediaPlayerOpenSongFile())
		{
			return FALSE;//�ļ�����
		}
	
	//�����ʼ��ʧ��(��Ч�ļ�)������������ʷ�������л�������
	if(!MediaPlayerDecoderRefresh(gpMediaPlayer->CurPlayTime))
	{
		APP_DBG("Refresh fail\n");
		{
			if(gpMediaPlayer->ErrFileCount )
			{
				gpMediaPlayer->ErrFileCount --;
				if(!gpMediaPlayer->ErrFileCount )
				{
					MessageContext		msgSend;

					msgSend.msgId		= MSG_MEDIA_PLAY_BROWER_RETURN;
					MessageSend(GetAppMessageHandle(), &msgSend);
					APP_DBG("Can't play ,please select again");
					vTaskDelay(5);
					return;
				}
			}
			
		}
		if(gpMediaPlayer->SongSwitchFlag == TRUE)
		{
			MessageContext		msgSend;

			msgSend.msgId		= MSG_PRE;
			MessageSend(GetAppMessageHandle(), &msgSend);
			
		}
		else
		{
			MessageContext		msgSend;

			msgSend.msgId		= MSG_NEXT;
			MessageSend(GetAppMessageHandle(), &msgSend);
		}
		vTaskDelay(5);

		return ;
	}

	SetMediaPlayerState(PLAYER_STATE_PLAYING);
	DecoderPlay();
		
	//�˴��Ƿ��жϿ�������������ȣ���Ϊ��һ�ף���ǰ�ǽ������Զ���
#ifdef CFG_FUNC_BREAKPOINT_EN
	if(GetSystemMode() == AppModeCardAudioPlay || GetSystemMode() == AppModeUDiskAudioPlay)
	{
		BackupInfoUpdata(BACKUP_SYS_INFO);
		BackupInfoUpdata(BACKUP_PLAYER_INFO);
#ifdef BP_PART_SAVE_TO_NVM
			BackupInfoUpdata(BACKUP_PLAYER_INFO_2NVM);
#endif
	}
#endif
}
#endif
#endif
