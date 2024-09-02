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
#include "audio_record.h"
#include "aud_intf.h"
#include "aud_intf_types.h"
#include "ff.h"
#include "diskio.h"


#define TAG  "AUD_RECORD_SDCARD"

static FIL mic_file;
static char mic_file_name[50];

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
		BK_LOGI(TAG, "f_mount malloc failed!\r\n");
		return BK_FAIL;
	}

	fr = f_mount(pfs, "1:", 1);
	if (fr != FR_OK)
	{
		BK_LOGE(TAG, "f_mount failed:%d\r\n", fr);
		return BK_FAIL;
	}
	else
	{
		BK_LOGI(TAG, "f_mount OK!\r\n");
	}

	return BK_OK;
}

static bk_err_t tf_unmount(void)
{
	FRESULT fr;
	fr = f_unmount(DISK_NUMBER_SDIO_SD, "1:", 1);
	if (fr != FR_OK)
	{
		BK_LOGE(TAG, "f_unmount failed:%d\r\n", fr);
		return BK_FAIL;
	}
	else
	{
		BK_LOGI(TAG, "f_unmount OK!\r\n");
	}

	if (pfs)
	{
		os_free(pfs);
		pfs = NULL;
	}

	return BK_OK;
}

static int send_mic_data_to_sd(uint8_t *data, unsigned int len)
{
	FRESULT fr;
	uint32 uiTemp = 0;

	/* write data to file */
	fr = f_write(&mic_file, (void *)data, len, &uiTemp);
	if (fr != FR_OK) {
		BK_LOGE(TAG, "write %s fail.\r\n", mic_file_name);
	}

	return uiTemp;
}

bk_err_t audio_record_to_sdcard_start(char *file_name, uint32_t samp_rate)
{
    bk_err_t ret = BK_OK;
    FRESULT fr;

    aud_intf_drv_setup_t aud_intf_drv_setup = DEFAULT_AUD_INTF_DRV_SETUP_CONFIG();
    aud_intf_mic_setup_t aud_intf_mic_setup = DEFAULT_AUD_INTF_MIC_SETUP_CONFIG();

    ret = tf_mount();
    if (ret != BK_ERR_AUD_INTF_OK) {
        BK_LOGE(TAG, "tfcard mount fail, ret:%d\n", ret);
        goto fail;
    }

    /*open file to save pcm data */
    sprintf(mic_file_name, "1:/%s", file_name);
    fr = f_open(&mic_file, mic_file_name, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK) {
        BK_LOGE(TAG, "open %s fail\n", mic_file_name);
        goto fail;
    }

    aud_intf_drv_setup.aud_intf_tx_mic_data = send_mic_data_to_sd;
    ret = bk_aud_intf_drv_init(&aud_intf_drv_setup);
    if (ret != BK_ERR_AUD_INTF_OK) {
        BK_LOGE(TAG, "bk_aud_intf_drv_init fail, ret:%d\n", ret);
        goto fail;
    }

    ret = bk_aud_intf_set_mode(AUD_INTF_WORK_MODE_GENERAL);
    if (ret != BK_ERR_AUD_INTF_OK) {
        BK_LOGE(TAG, "bk_aud_intf_set_mode fail, ret:%d\n", ret);
        goto fail;
    }

    //aud_intf_mic_setup.mic_chl = AUD_INTF_MIC_CHL_MIC1;
    aud_intf_mic_setup.samp_rate = samp_rate;
    //aud_intf_mic_setup.mic_type = AUD_INTF_MIC_TYPE_UAC;
    aud_intf_mic_setup.frame_size = 640;
    //aud_intf_mic_setup.mic_gain = 0x2d;
    ret = bk_aud_intf_mic_init(&aud_intf_mic_setup);
    if (ret != BK_ERR_AUD_INTF_OK) {
        BK_LOGE(TAG, "bk_aud_intf_mic_init fail, ret:%d\n", ret);
        goto fail;
    }

    ret = bk_aud_intf_mic_start();
    if (ret != BK_ERR_AUD_INTF_OK) {
        BK_LOGE(TAG, "bk_aud_intf_mic_start fail, ret:%d\n", ret);
        goto fail;
    }

	return BK_OK;

fail:

    bk_aud_intf_mic_stop();
    bk_aud_intf_mic_deinit();
    bk_aud_intf_set_mode(AUD_INTF_WORK_MODE_NULL);
    bk_aud_intf_drv_deinit();

    /* close mic file */
    f_close(&mic_file);

    return BK_FAIL;
}

bk_err_t audio_record_to_sdcard_stop(void)
{
	bk_err_t ret;
    ret = bk_aud_intf_mic_stop();
    if (ret != BK_ERR_AUD_INTF_OK) {
        BK_LOGE(TAG, "bk_aud_intf_mic_stop fail, ret:%d\n", ret);
    }

    ret = bk_aud_intf_mic_deinit();
    if (ret != BK_ERR_AUD_INTF_OK) {
        BK_LOGE(TAG, "bk_aud_intf_mic_deinit fail, ret:%d\n", ret);
    }

    ret = bk_aud_intf_set_mode(AUD_INTF_WORK_MODE_NULL);
    if (ret != BK_ERR_AUD_INTF_OK) {
        BK_LOGE(TAG, "bk_aud_intf_set_mode fail, ret:%d\n", ret);
    }

    ret = bk_aud_intf_drv_deinit();
    if (ret != BK_ERR_AUD_INTF_OK) {
        BK_LOGE(TAG, "bk_aud_intf_drv_deinit fail, ret:%d\n", ret);
    }

    /* close mic file */
    f_close(&mic_file);

    tf_unmount();

    return BK_OK;
}

