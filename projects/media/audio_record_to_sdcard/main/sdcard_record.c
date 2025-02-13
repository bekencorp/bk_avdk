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

#include <os/os.h>
#include <os/mem.h>
#include "sdcard_record.h"
#include "audio_record.h"
#include "ff.h"
#include "diskio.h"


#define SDCARD_RECORD_TAG  "sd_rec"
#define LOGI(...) BK_LOGI(SDCARD_RECORD_TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(SDCARD_RECORD_TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(SDCARD_RECORD_TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(SDCARD_RECORD_TAG, ##__VA_ARGS__)


#define READ_FRAME_SIZE         (320)

typedef struct {
    audio_record_t *record;

    FIL mic_file;
    char mic_file_name[50];

    char *read_buf;
    uint32_t read_buf_size;

    beken_thread_t task_hdl;
    beken_semaphore_t sem;
} sdcard_record_info_t;

static sdcard_record_info_t *sdcard_record_info = NULL;
static bool sdcard_record_run = false;
static FATFS *pfs = NULL;


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
		LOGI("f_mount malloc failed!\n");
		return BK_FAIL;
	}

	fr = f_mount(pfs, "1:", 1);
	if (fr != FR_OK)
	{
		LOGE("f_mount failed:%d\n", fr);
		return BK_FAIL;
	}
	else
	{
		LOGI("f_mount OK!\n");
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

static int write_data_to_sdcard(FIL file, char *data, uint32_t len)
{
	FRESULT fr;
	uint32 uiTemp = 0;

	/* write data to file */
	fr = f_write(&file, (void *)data, len, &uiTemp);
	if (fr != FR_OK) {
		LOGE("write file fail\n");
	}

	return uiTemp;
}

void sdcard_record_main(beken_thread_arg_t param_data)
{
    bk_err_t ret = BK_OK;
    FRESULT fr;

    ret = tf_mount();
    if (ret != BK_OK) {
        LOGE("tfcard mount fail, ret:%d\n", ret);
        goto fail;
    }

    /* open file to save pcm data */
    fr = f_open(&sdcard_record_info->mic_file, sdcard_record_info->mic_file_name, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK) {
        LOGE("open %s fail\n", sdcard_record_info->mic_file_name);
        goto fail;
    }

    /* malloc read buffer */
    sdcard_record_info->read_buf_size = READ_FRAME_SIZE;
    sdcard_record_info->read_buf = os_malloc(sdcard_record_info->read_buf_size);
    if (!sdcard_record_info->read_buf)
    {
        LOGE("malloc read buffer: %d fail\n", sdcard_record_info->read_buf_size);
        goto fail;
    }
    os_memset(sdcard_record_info->read_buf, 0, sdcard_record_info->read_buf_size);

    /* init audio record */
    audio_record_cfg_t config = DEFAULT_AUDIO_RECORD_CONFIG();
    sdcard_record_info->record = audio_record_create(AUDIO_RECORD_ONBOARD_MIC, &config);
    if (!sdcard_record_info->record)
    {
        LOGE("create audio record fail\n");
        goto fail;
    }

    ret = audio_record_open(sdcard_record_info->record);
    if (ret != BK_OK)
    {
        LOGE("open audio record fail, ret: %d\n", ret);
        goto fail;
    }

    rtos_set_semaphore(&sdcard_record_info->sem);

    sdcard_record_run = true;

    while (sdcard_record_run)
    {
        uint32_t r_len = audio_record_read_data(sdcard_record_info->record, sdcard_record_info->read_buf, sdcard_record_info->read_buf_size);
        if (r_len <= 0)
        {
            LOGE("audio record read data fail, ret: %d\n", r_len);
            goto fail;
        }

        ret = write_data_to_sdcard(sdcard_record_info->mic_file, sdcard_record_info->read_buf, r_len);
        if (ret != r_len)
        {
            LOGE("write mic data to sdcard fail, ret: %d, r_len: %d\n", ret, r_len);
            goto fail;
        }
    }

fail:
    sdcard_record_run = false;

    if (sdcard_record_info->record)
    {
        audio_record_close(sdcard_record_info->record);
        audio_record_destroy(sdcard_record_info->record);
        sdcard_record_info->record = NULL;
    }

    /* close mic file */
    f_close(&sdcard_record_info->mic_file);

    tf_unmount();

    /* delete task */
    sdcard_record_info->task_hdl = NULL;

    rtos_set_semaphore(&sdcard_record_info->sem);

    rtos_delete_thread(NULL);
}

bk_err_t audio_record_to_sdcard_stop(void)
{
    if (!sdcard_record_run)
    {
        return BK_OK;
    }

    sdcard_record_run = false;

    rtos_get_semaphore(&sdcard_record_info->sem, BEKEN_NEVER_TIMEOUT);

    rtos_deinit_semaphore(&sdcard_record_info->sem);
    sdcard_record_info->sem = NULL;

    os_free(sdcard_record_info);

    return BK_OK;
}


bk_err_t audio_record_to_sdcard_start(char *file_name)
{
    bk_err_t ret = BK_OK;

    if (!file_name)
    {
        return BK_FAIL;
    }

    sdcard_record_info = (sdcard_record_info_t *)os_malloc(sizeof(sdcard_record_info_t));
    if (!sdcard_record_info) {
        LOGE("malloc sdcard_record_info: %d fail\n", sizeof(sdcard_record_info_t));
        goto fail;
    }
    os_memset(sdcard_record_info, 0, sizeof(sdcard_record_info_t));

    sprintf(sdcard_record_info->mic_file_name, "1:/%s", file_name);

    ret = rtos_init_semaphore(&sdcard_record_info->sem, 1);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, ceate semaphore fail\n", __func__, __LINE__);
        goto fail;
    }

    ret = rtos_create_thread(&sdcard_record_info->task_hdl,
                             (BEKEN_DEFAULT_WORKER_PRIORITY - 1),
                             "sdcard_record",
                             (beken_thread_function_t)sdcard_record_main,
                             2048,
                             NULL);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, create sdcard_record task fail\n", __func__, __LINE__);
        goto fail;
    }

    rtos_get_semaphore(&sdcard_record_info->sem, BEKEN_NEVER_TIMEOUT);

    LOGI("init sdcard record task complete\n");

    return BK_OK;

fail:
    if (sdcard_record_info && sdcard_record_info->sem)
    {
        rtos_deinit_semaphore(&sdcard_record_info->sem);
        sdcard_record_info->sem = NULL;
    }

    if (sdcard_record_info && sdcard_record_info->read_buf)
    {
        os_free(sdcard_record_info->read_buf);
        sdcard_record_info->read_buf = NULL;
    }

    if (sdcard_record_info)
    {
        os_free(sdcard_record_info);
        sdcard_record_info = NULL;
    }

    return BK_FAIL;
}

