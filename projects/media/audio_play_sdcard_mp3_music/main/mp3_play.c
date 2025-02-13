// Copyright 2023-2024 Beken
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//	   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "stdio.h"
#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include <modules/pm.h>
#include <modules/mp3dec.h>
#include "ff.h"
#include "diskio.h"
#include "audio_play.h"
#include "mp3_play.h"

#define MP3_PLAY_TAG  "mp3_play"
#define LOGI(...) BK_LOGI(MP3_PLAY_TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(MP3_PLAY_TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(MP3_PLAY_TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(MP3_PLAY_TAG, ##__VA_ARGS__)


#define PCM_SIZE_MAX		(MAX_NSAMP * MAX_NCHAN * MAX_NGRAN)
#define PLAY_FINISH     (1)

typedef struct {
    HMP3Decoder hMP3Decoder;
    MP3FrameInfo mp3FrameInfo;
    unsigned char *readBuf;
    short *pcmBuf;
    int bytesLeft;

    audio_play_t *play;

    FIL mp3file;
    char mp3_file_name[50];
    unsigned char *g_readptr;

    bool mp3_file_is_empty;
} mp3_play_info_t;


static mp3_play_info_t *mp3_play_info = NULL;
static bool mp3_play_run = false;
static FATFS *pfs = NULL;
static beken_thread_t mp3_play_task_hdl = NULL;
static beken_semaphore_t mp3_play_sem = NULL;


static bk_err_t tf_mount(void)
{
	FRESULT fr;

	if (pfs != NULL)
	{
		os_free(pfs);
	}

	pfs = os_malloc(sizeof(FATFS));
	if(NULL == pfs)
	{
		LOGI("f_mount malloc failed!\r\n");
		return BK_FAIL;
	}

	fr = f_mount(pfs, "1:", 1);
	if (fr != FR_OK)
	{
		LOGE("f_mount failed:%d\r\n", fr);
		return BK_FAIL;
	}
	else
	{
		LOGI("f_mount OK!\r\n");
	}

	return BK_OK;
}

static bk_err_t tf_unmount(void)
{
	FRESULT fr;
	fr = f_unmount(DISK_NUMBER_SDIO_SD, "1:", 1);
	if (fr != FR_OK)
	{
		LOGE("f_unmount failed:%d\r\n", fr);
		return BK_FAIL;
	}
	else
	{
	    LOGI("f_unmount OK!\r\n");
	}

	if (pfs)
	{
		os_free(pfs);
		pfs = NULL;
	}

	return BK_OK;
}

static bk_err_t mp3_play_write_spk_data(audio_play_t *play, char *buffer, int len)
{
    uint32_t w_sum = 0;
    uint32_t w_size = 0;

    while(len - w_sum)
    {
        w_size = audio_play_write_data(play, buffer + w_sum, len - w_sum);
        if (w_size < 0)
        {
            LOGI("audio play write fail, ret: %d\n", w_size);
            return BK_FAIL;
        }
        w_sum += w_size;
    }

    return w_sum;
}

static bk_err_t mp3_decode_handler(void)
{
	bk_err_t ret = BK_OK;

	FRESULT fr;
	uint32 uiTemp = 0;
    static bool empty_already_flag = false;

    if (!mp3_play_info) {
        return BK_FAIL;
    }

	if (mp3_play_info->mp3_file_is_empty) {
        if (empty_already_flag == false) {
            empty_already_flag = true;
		    LOGW("==========================================================\n");
		    LOGW("%s playback is over\n", mp3_play_info->mp3_file_name);
		    LOGW("==========================================================\n");
        }
		return PLAY_FINISH;
	}

    empty_already_flag = false;

	if (mp3_play_info->bytesLeft < MAINBUF_SIZE) {
		os_memmove(mp3_play_info->readBuf, mp3_play_info->g_readptr, mp3_play_info->bytesLeft);
		fr = f_read(&mp3_play_info->mp3file, (void *)(mp3_play_info->readBuf + mp3_play_info->bytesLeft), MAINBUF_SIZE - mp3_play_info->bytesLeft, &uiTemp);
		if (fr != FR_OK) {
			LOGE("read %s failed\n", mp3_play_info->mp3_file_name);
			return fr;
		}

		if ((uiTemp == 0) && (mp3_play_info->bytesLeft == 0)) {
			LOGI("uiTemp = 0 and bytesLeft = 0\n");
			mp3_play_info->mp3_file_is_empty = true;
			LOGI("the %s is empty\n", mp3_play_info->mp3_file_name);
			return ret;
		}

		mp3_play_info->bytesLeft = mp3_play_info->bytesLeft + uiTemp;
		mp3_play_info->g_readptr = mp3_play_info->readBuf;
	}

	int offset = MP3FindSyncWord(mp3_play_info->g_readptr, mp3_play_info->bytesLeft);

	if (offset < 0) {
		LOGE("MP3FindSyncWord not find\n");
		mp3_play_info->bytesLeft = 0;
	} else {
		mp3_play_info->g_readptr += offset;
		mp3_play_info->bytesLeft -= offset;
		
		ret = MP3Decode(mp3_play_info->hMP3Decoder, &mp3_play_info->g_readptr, &mp3_play_info->bytesLeft, mp3_play_info->pcmBuf, 0);
		if (ret != ERR_MP3_NONE) {
			LOGE("MP3Decode failed, code is %d\n", ret);
			return ret;
		}

		MP3GetLastFrameInfo(mp3_play_info->hMP3Decoder, &mp3_play_info->mp3FrameInfo);
		LOGD("Bitrate: %d kb/s, Samprate: %d\r\n", mp3_play_info->mp3FrameInfo.bitrate / 1000, mp3_play_info->mp3FrameInfo.samprate);
		LOGD("Channel: %d, Version: %d, Layer: %d\r\n", mp3_play_info->mp3FrameInfo.nChans, mp3_play_info->mp3FrameInfo.version, mp3_play_info->mp3FrameInfo.layer);
		LOGD("OutputSamps: %d\r\n", mp3_play_info->mp3FrameInfo.outputSamps);
	}

	return ret;
}

bk_err_t mp3_play_stop(void)
{
	bk_err_t ret;

    if (!mp3_play_info) {
        return BK_OK;
    }

    mp3_play_run = false;

    if (mp3_play_info->play)
    {
        ret = audio_play_close(mp3_play_info->play);
        if (ret != BK_OK) {
            LOGE("audio play close fail, ret:%d\n", ret);
        }

        audio_play_destroy(mp3_play_info->play);
        mp3_play_info->play = NULL;
    }

	mp3_play_info->bytesLeft = 0;
	mp3_play_info->mp3_file_is_empty = false;

	f_close(&mp3_play_info->mp3file);

    if (mp3_play_info->hMP3Decoder) {
	    MP3FreeDecoder(mp3_play_info->hMP3Decoder);
        mp3_play_info->hMP3Decoder = NULL;
    }

    if (mp3_play_info->readBuf) {
        os_free(mp3_play_info->readBuf);
        mp3_play_info->readBuf = NULL;
    }

    if (mp3_play_info->pcmBuf) {
        os_free(mp3_play_info->pcmBuf);
        mp3_play_info->pcmBuf = NULL;
    }

    if (mp3_play_info) {
        os_free(mp3_play_info);
        mp3_play_info = NULL;
    }

    tf_unmount();

    return BK_OK;
}

static void mp3_play_main(beken_thread_arg_t param_data)
{
	bk_err_t ret = BK_OK;
    uint32 uiTemp = 0;
	char tag_header[10];
	int tag_size = 0;

    char *file_name = (char *)param_data;

	if (!file_name) {
		LOGE("file_name is NULL\n");
	}

    ret = tf_mount();
    if (ret != BK_OK) {
        LOGE("mount sdcard fail\n");
    }

    mp3_play_info = (mp3_play_info_t *)os_malloc(sizeof(mp3_play_info_t));
    if (!mp3_play_info) {
        LOGE("mount sdcard fail\n");
        goto exit;
    }

    os_memset(mp3_play_info, 0, sizeof(mp3_play_info_t));

	mp3_play_info->readBuf = os_malloc(MAINBUF_SIZE);
	if (mp3_play_info->readBuf == NULL) {
		LOGE("readBuf malloc fail\n");
		goto exit;
	}
    os_memset(mp3_play_info->readBuf, 0, MAINBUF_SIZE);

	mp3_play_info->pcmBuf = os_malloc(PCM_SIZE_MAX * 2);
	if (mp3_play_info->pcmBuf == NULL) {
		LOGE("pcmBuf malloc fail\n");
		goto exit;
	}
    os_memset(mp3_play_info->pcmBuf, 0, PCM_SIZE_MAX * 2);

	mp3_play_info->hMP3Decoder = MP3InitDecoder();
	if (mp3_play_info->hMP3Decoder == NULL) {
		LOGE("MP3Decoder init fail\n");
		goto exit;
	}

	LOGI("audio mp3 play decode init complete\n");

	/*open file to read mp3 data */
    os_memset(mp3_play_info->mp3_file_name, 0, sizeof(mp3_play_info->mp3_file_name)/sizeof(mp3_play_info->mp3_file_name[0]));
	sprintf(mp3_play_info->mp3_file_name, "%d:/%s", DISK_NUMBER_SDIO_SD, file_name);

    /* file_name is invalid */
    rtos_set_semaphore(&mp3_play_sem);

	FRESULT fr = f_open(&mp3_play_info->mp3file, mp3_play_info->mp3_file_name, FA_OPEN_EXISTING | FA_READ);
	if (fr != FR_OK) {
		LOGE("open %s fail\n", mp3_play_info->mp3_file_name);
		goto exit;
	}
	LOGI("mp3 file: %s open successful\n", mp3_play_info->mp3_file_name);

    fr = f_read(&mp3_play_info->mp3file, (void *)tag_header, 10, &uiTemp);
    if (fr != FR_OK)
    {
        LOGE("read %s fail\n", mp3_play_info->mp3_file_name);
        goto exit;
    }

    if (os_memcmp(tag_header, "ID3", 3) == 0)
    {
        tag_size = ((tag_header[6] & 0x7F) << 21) | ((tag_header[7] & 0x7F) << 14) | ((tag_header[8] & 0x7F) << 7) | (tag_header[9] & 0x7F);
        LOGI("tag_size = %d\n", tag_size);
        f_lseek(&mp3_play_info->mp3file, tag_size + 10);
        LOGI("tag_header has found\n");
    }
    else
    {
        LOGI("tag_header not found\n");
        f_lseek(&mp3_play_info->mp3file, 0);
    }

    /* decoder frame to get frameinfo */
	mp3_play_info->g_readptr = mp3_play_info->readBuf;
	ret = mp3_decode_handler();
    if (ret != BK_OK) {
        LOGE("mp3_decode_handler fail, ret:%d\n", ret);
        goto exit;
    }

    LOGI("Bitrate: %d kb/s, Samprate: %d\r\n", mp3_play_info->mp3FrameInfo.bitrate / 1000, mp3_play_info->mp3FrameInfo.samprate);
    LOGI("Channel: %d, Version: %d, Layer: %d\r\n", mp3_play_info->mp3FrameInfo.nChans, mp3_play_info->mp3FrameInfo.version, mp3_play_info->mp3FrameInfo.layer);
    LOGI("OutputSamps: %d\r\n", mp3_play_info->mp3FrameInfo.outputSamps);

    /* create audio_play according to frameinfo */
    audio_play_cfg_t audio_play_config = DEFAULT_AUDIO_PLAY_CONFIG();
    audio_play_config.port = 0;
    audio_play_config.nChans = mp3_play_info->mp3FrameInfo.nChans;
    audio_play_config.sampRate = mp3_play_info->mp3FrameInfo.samprate;
    audio_play_config.bitsPerSample = mp3_play_info->mp3FrameInfo.bitsPerSample;
    audio_play_config.volume = 0x2d;
    audio_play_config.play_mode = AUDIO_PLAY_MODE_DIFFEN;
    audio_play_config.frame_size = audio_play_config.sampRate * audio_play_config.bitsPerSample / 8 * 20 /1000; //20ms data
    audio_play_config.pool_size = audio_play_config.frame_size * 4;
    mp3_play_info->play = audio_play_create(AUDIO_PLAY_ONBOARD_SPEAKER, &audio_play_config);
    if (!mp3_play_info->play)
    {
        LOGE("create audio play fail\n");
        goto exit;
    }

    ret = audio_play_open(mp3_play_info->play);
    if (ret != BK_OK)
    {
        LOGE("open audio play fail, ret: %d\n", ret);
        goto exit;
    }

    ret = mp3_play_write_spk_data(mp3_play_info->play, (char *)mp3_play_info->pcmBuf, mp3_play_info->mp3FrameInfo.outputSamps * 2);
    if (ret < 0) {
        LOGE("write spk data fail \r\n");
        goto exit;
    }

    mp3_play_run = true;

    while (mp3_play_run)
    {
        ret = mp3_decode_handler();
        if (ret != BK_OK) {
            if (ret == PLAY_FINISH)
            {
                goto exit;
            }
            else
            {
                LOGE("mp3_decode_handler fail, ret:%d\n", ret);
                goto exit;
            }
        }

        ret = mp3_play_write_spk_data(mp3_play_info->play, (char *)mp3_play_info->pcmBuf, mp3_play_info->mp3FrameInfo.outputSamps * 2);
        if (ret < 0) {
            LOGE("write spk data fail \r\n");
            goto exit;
        }
    }

exit:
    mp3_play_run = false;
    mp3_play_stop();

    /* delete task */
    mp3_play_task_hdl = NULL;

    rtos_set_semaphore(&mp3_play_sem);

    rtos_delete_thread(NULL);
}


bk_err_t audio_play_sdcard_mp3_music_start(char *file_name)
{
    bk_err_t ret = BK_OK;

    ret = rtos_init_semaphore(&mp3_play_sem, 1);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, ceate semaphore fail\n", __func__, __LINE__);
        goto fail;
    }

    ret = rtos_create_thread(&mp3_play_task_hdl,
                             (BEKEN_DEFAULT_WORKER_PRIORITY - 1),
                             "mp3_play",
                             (beken_thread_function_t)mp3_play_main,
                             2048,
                             (beken_thread_arg_t)file_name);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, create spk data read task fail\n", __func__, __LINE__);
        goto fail;
    }

    rtos_get_semaphore(&mp3_play_sem, BEKEN_NEVER_TIMEOUT);

    LOGI("init mp3 play task complete\n");

    return BK_OK;

fail:

    if (mp3_play_sem)
    {
        rtos_deinit_semaphore(&mp3_play_sem);
        mp3_play_sem = NULL;
    }

    return BK_FAIL;
}

bk_err_t audio_play_sdcard_mp3_music_stop(void)
{
    if (!mp3_play_run)
    {
        return BK_OK;
    }

    mp3_play_run = false;

    rtos_get_semaphore(&mp3_play_sem, BEKEN_NEVER_TIMEOUT);

    rtos_deinit_semaphore(&mp3_play_sem);
    mp3_play_sem = NULL;

    return BK_OK;
}

