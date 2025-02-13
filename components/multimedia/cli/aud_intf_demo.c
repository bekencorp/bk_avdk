// Copyright 2020-2021 Beken
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include <stdio.h>
#include <stdlib.h>
#include "sys_driver.h"
#include "aud_intf.h"
#include "aud_intf_types.h"
#if CONFIG_FATFS
#include "ff.h"
#include "diskio.h"
#endif
#include <driver/uart.h>
#include "gpio_driver.h"
#include <modules/g711.h>

#include "cli.h"
#include "media_cli.h"


#if CONFIG_FATFS
static FIL voc_red_file;
static FIL voc_ply_file;
static char voc_record_file_name[50];
static char voc_play_file_name[50];
static int32_t *temp_spk_addr = NULL;     //存放从SDcard中取的pcm信号
static bool spk_file_empty = false;
#endif


static void cli_aud_intf_help(void)
{
#if CONFIG_FATFS
	os_printf("aud_intf_sd_voc_test {start|stop xx.pcm, xx.pcm} \r\n");
#endif
	os_printf("aud_intf_set_voc_param_test {param value} \r\n");
	os_printf("aud_intf_set_aec_param_test {param value} \r\n");
	os_printf("aud_intf_get_aec_param_test \r\n");
	os_printf("aud_intf_doorbell_test {start|stop} \r\n");
}

#if 0
static void uac_connect_state_cb_handle(uint8_t state)
{
	os_printf("[--%s--] state: %d \n", __func__, state);
}
#endif



#if CONFIG_FATFS
static bk_err_t aud_write_sd_data_to_spk(unsigned int size)
{
	bk_err_t ret = BK_OK;
	FRESULT fr;
	uint32 uiTemp = 0;

	//os_printf("enter %s \r\n", __func__);

	if (spk_file_empty)
		return BK_FAIL;

	temp_spk_addr = os_malloc(size);

	/* read data from file */
	fr = f_read(&voc_ply_file, (void *)temp_spk_addr, size, &uiTemp);
	if (fr != FR_OK) {
		os_printf("read %s fail.\r\n", voc_play_file_name);
	}

	if (uiTemp != size) {
		spk_file_empty = true;
		os_printf("the %s is empty \r\n", voc_play_file_name);
		//TODO
	}

	/* write a fram speaker data to speaker_ring_buff */
	ret = bk_aud_intf_write_spk_data((uint8_t*)temp_spk_addr, size);
	if (ret != BK_OK) {
		os_printf("write voc spk data fail \r\n");
		return ret;
	}

	os_free(temp_spk_addr);

	return ret;
}

static int aud_voc_write_mic_to_sd(UINT8 *data, UINT32 len)
{
	FRESULT fr;
	uint32 uiTemp = 0;

	//os_printf("[send_mic_data_to_sd] \r\n");

	/* write data to file */
	fr = f_write(&voc_red_file, (void *)data, len, &uiTemp);
	if (fr != FR_OK) {
		os_printf("write %s fail.\r\n", voc_record_file_name);
	}

	return uiTemp;
}

/* audio voice transfer pcm data by sdcard */
void cli_aud_intf_sd_voc_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	FRESULT fr;
	bk_err_t ret = BK_OK;

	aud_intf_drv_setup_t aud_intf_drv_setup = DEFAULT_AUD_INTF_DRV_SETUP_CONFIG();
	aud_intf_voc_setup_t aud_intf_voc_setup = DEFAULT_AUD_INTF_VOC_SETUP_CONFIG();

	if (argc != 4) {
		cli_aud_intf_help();
		return;
	}

	if (os_strcmp(argv[1], "start") == 0) {
		os_printf("start voice transfer test \r\n");

		/*open file to save voice mic data */
		sprintf(voc_record_file_name, "1:/%s", argv[2]);
		fr = f_open(&voc_red_file, voc_record_file_name, FA_CREATE_ALWAYS | FA_WRITE);
		if (fr != FR_OK) {
			os_printf("open %s fail.\r\n", voc_record_file_name);
			return;
		}

		/*open file to read and write voice spk data */
		sprintf(voc_play_file_name, "1:/%s", argv[3]);
		fr = f_open(&voc_ply_file, voc_play_file_name, FA_OPEN_EXISTING | FA_READ);
		if (fr != FR_OK) {
			os_printf("open %s fail.\r\n", voc_play_file_name);
			return;
		}

		//aud_intf_drv_setup.work_mode = AUD_INTF_WORK_MODE_NULL;
		//aud_intf_drv_setup.task_config.priority = 3;
		aud_intf_drv_setup.aud_intf_rx_spk_data = aud_write_sd_data_to_spk;
		aud_intf_drv_setup.aud_intf_tx_mic_data = aud_voc_write_mic_to_sd;
		bk_aud_intf_drv_init(&aud_intf_drv_setup);
		bk_aud_intf_set_mode(AUD_INTF_WORK_MODE_VOICE);
		//aud_intf_voc_setup.aec_enable = true;
		//aud_intf_voc_setup.samp_rate = 8000;
		//aud_intf_voc_setup.data_type = AUD_INTF_VOC_DATA_TYPE_G711A;
		aud_intf_voc_setup.data_type = AUD_INTF_VOC_DATA_TYPE_PCM;
		//aud_intf_voc_setup.spk_mode = AUD_DAC_WORK_MODE_SIGNAL_END;
		//aud_intf_voc_setup.mic_gain = 0x2d;
		//aud_intf_voc_setup.spk_gain = 0x2d;
		//aud_intf_voc_setup.aec_cfg.ec_depth = 20;
		//aud_intf_voc_setup.aec_cfg.TxRxThr = 30;
		//aud_intf_voc_setup.aec_cfg.TxRxFlr = 6;
		//aud_intf_voc_setup.aec_cfg.ns_level = 2;
		//aud_intf_voc_setup.aec_cfg.ns_para = 1;
		bk_aud_intf_voc_init(aud_intf_voc_setup);

		temp_spk_addr = os_malloc(640);
		os_memset(temp_spk_addr, 0, 640);
		rtos_delay_milliseconds(2000);
		os_printf("start write spk data \r\n");
		for (int i = 0; i < 10; i++) {
			/* write a fram speaker data to speaker_ring_buff */
			ret = bk_aud_intf_write_spk_data((uint8_t*)temp_spk_addr, 640);
			if (ret != BK_OK) {
				os_printf("write voc spk data fail \r\n");
				return;
			}
		}
		os_free(temp_spk_addr);

		os_printf("start voice \r\n");
	} else if (os_strcmp(argv[1], "stop") == 0) {
		bk_aud_intf_voc_deinit();
		bk_aud_intf_set_mode(AUD_INTF_WORK_MODE_NULL);
		bk_aud_intf_drv_deinit();

		/* close voice record file */
		fr = f_close(&voc_red_file);
		if (fr != FR_OK) {
			os_printf("close %s fail!\r\n", voc_record_file_name);
			return;
		}

		/* close voice play file */
		fr = f_close(&voc_ply_file);
		if (fr != FR_OK) {
			os_printf("close %s fail!\r\n", voc_play_file_name);
			return;
		}

		os_printf("stop voice \r\n");
		os_printf("stop voice test \r\n");
	} else {
		cli_aud_intf_help();
		return;
	}

}
#endif	//#if CONFIG_FATFS

void cli_aud_intf_set_voc_param_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	uint8_t value = 0;

	if (argc != 3) {
		cli_aud_intf_help();
		return;
	}

	value = strtoul(argv[2], NULL, 0);
	if (os_strcmp(argv[1], "mic_gain") == 0) {
		bk_aud_intf_set_mic_gain(value);
		os_printf("set voc mic gain:%d \r\n", value);
	} else if (os_strcmp(argv[1], "spk_gain") == 0) {
		bk_aud_intf_set_spk_gain(value);
		os_printf("set voc spk gain:%d \r\n", value);
	} else if (os_strcmp(argv[1], "gain") == 0) {
		os_printf("stop play \r\n");
	} else {
		cli_aud_intf_help();
		return;
	}

}

void cli_aud_intf_set_aec_param_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	bk_err_t ret = BK_OK;
	uint8_t value = 0;
	aud_intf_voc_aec_para_t op;

	if (argc != 3) {
		cli_aud_intf_help();
		return;
	}

	value = strtoul(argv[2], NULL, 0);
	if (os_strcmp(argv[1], "init_flags") == 0) {
		op = AUD_INTF_VOC_AEC_INIT_FLAG;
		os_printf("set aec init_flags:%d \r\n", value);
	} else if (os_strcmp(argv[1], "mic_delay") == 0) {
		op = AUD_INTF_VOC_AEC_MIC_DELAY;
		os_printf("set aec mic_delay:%d \r\n", value);
	} else if (os_strcmp(argv[1], "ec_depth") == 0) {
		op = AUD_INTF_VOC_AEC_EC_DEPTH;
		os_printf("set aec ec_depth:%d \r\n", value);
	} else if (os_strcmp(argv[1], "ref_scale") == 0) {
		op = AUD_INTF_VOC_AEC_REF_SCALE;
		os_printf("set aec ref_scale:%d \r\n", value);
	} else if (os_strcmp(argv[1], "voice_vol") == 0) {
		op = AUD_INTF_VOC_AEC_VOICE_VOL;
		os_printf("set aec voice_vol:%d \r\n", value);
	} else if (os_strcmp(argv[1], "TxRxThr") == 0) {
		op = AUD_INTF_VOC_AEC_TXRX_THR;
		os_printf("set aec TxRxThr:%d \r\n", value);
	} else if (os_strcmp(argv[1], "TxRxFlr") == 0) {
		op = AUD_INTF_VOC_AEC_TXRX_FLR;
		os_printf("set aec TxRxFlr:%d \r\n", value);
	} else if (os_strcmp(argv[1], "ns_level") == 0) {
		op = AUD_INTF_VOC_AEC_NS_LEVEL;
		os_printf("set aec ns_level:%d \r\n", value);
	} else if (os_strcmp(argv[1], "ns_para") == 0) {
		op = AUD_INTF_VOC_AEC_NS_PARA;
		os_printf("set aec ns_para:%d \r\n", value);
	} else if (os_strcmp(argv[1], "drc") == 0) {
		op = AUD_INTF_VOC_AEC_DRC;
		os_printf("set aec drc:%d \r\n", value);
	} else {
		cli_aud_intf_help();
		return;
	}
	ret = bk_aud_intf_set_aec_para(op, value);
	if (ret != BK_OK)
		os_printf("test fail \r\n");

	os_printf("set aec parameters complete \r\n");
}

void cli_aud_intf_get_aec_param_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	bk_err_t ret = BK_OK;

	if (argc != 1) {
		cli_aud_intf_help();
		return;
	}

	ret = bk_aud_intf_get_aec_para();
	if (ret != BK_OK)
		os_printf("test fail \r\n");

	os_printf("get aec parameters complete \r\n");
}


/* set mic and speaker sample rate */
void cli_aud_intf_doorbell_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	bk_err_t ret = BK_OK;

	if (argc != 2 && argc != 3) {
		cli_aud_intf_help();
		return;
	}

	if (os_strcmp(argv[1], "stop") == 0) {
		ret = bk_aud_intf_voc_stop();
		if (ret != BK_ERR_AUD_INTF_OK)
			os_printf("stop doorbell fail \r\n");
		os_printf("stop doorbell complete \r\n");
	} else if (os_strcmp(argv[1], "ctrl_mic") == 0) {
		aud_intf_voc_mic_ctrl_t mic_en = AUD_INTF_VOC_MIC_MAX;
		if (os_strcmp(argv[2], "open") == 0)
			mic_en = AUD_INTF_VOC_MIC_OPEN;
		else if (os_strcmp(argv[2], "close") == 0)
			mic_en = AUD_INTF_VOC_MIC_CLOSE;
		else {
			cli_aud_intf_help();
			return;
		}

		ret = bk_aud_intf_voc_mic_ctrl(mic_en);
		if (ret != BK_ERR_AUD_INTF_OK)
			os_printf("ctrl voc mic fail: %d \r\n", mic_en);
		os_printf("ctrl voc mic complete \r\n");
	} else if (os_strcmp(argv[1], "ctrl_spk") == 0) {
		aud_intf_voc_spk_ctrl_t spk_en = AUD_INTF_VOC_SPK_MAX;
		if (os_strcmp(argv[2], "open") == 0)
			spk_en = AUD_INTF_VOC_SPK_OPEN;
		else if (os_strcmp(argv[2], "close") == 0)
			spk_en = AUD_INTF_VOC_SPK_CLOSE;
		else {
			cli_aud_intf_help();
			return;
		}

		ret = bk_aud_intf_voc_spk_ctrl(spk_en);
		if (ret != BK_ERR_AUD_INTF_OK)
			os_printf("ctrl voc spk fail: %d \r\n", spk_en);
		os_printf("ctrl voc spk complete \r\n");
	} else if (os_strcmp(argv[1], "ctrl_aec") == 0) {
		bool aec_en = false;
		if (os_strcmp(argv[2], "open") == 0)
			aec_en = true;
		else if (os_strcmp(argv[2], "close") == 0)
			aec_en = false;
		else {
			cli_aud_intf_help();
			return;
		}

		ret = bk_aud_intf_voc_aec_ctrl(aec_en);
		if (ret != BK_ERR_AUD_INTF_OK)
			os_printf("ctrl voc aec fail: %d \r\n", aec_en);
		os_printf("ctrl voc aec complete \r\n");
	} else if (os_strcmp(argv[1], "uac_vol") == 0) {
		uint32_t value = strtoul(argv[2], NULL, 0);

		ret = bk_aud_intf_set_spk_gain(value);
		if (ret != BK_ERR_AUD_INTF_OK)
		{
			os_printf("aud_tras_uac_auto_connect_ctrl fail, ret:%d\n", ret);
			//break;
		}
		os_printf("set uac volume: %d \r\n", value);
		os_printf("set uac volume complete \r\n");
	} else if (os_strcmp(argv[1], "start") == 0) {
		ret = bk_aud_intf_voc_start();
		if (ret != BK_ERR_AUD_INTF_OK)
			os_printf("start doorbell fail \r\n");
		os_printf("start doorbell complete \r\n");
	} else {
		cli_aud_intf_help();
		return;
	}
}


#define AUD_INTF_CMD_CNT (sizeof(s_aud_intf_commands) / sizeof(struct cli_command))
static const struct cli_command s_aud_intf_commands[] = {
#if CONFIG_FATFS
	{"aud_intf_sd_voc_test", "aud_intf_sd_voc_test {start|stop xx.pcm, xx.pcm}", cli_aud_intf_sd_voc_cmd},
#endif
	{"aud_intf_set_voc_param_test", "aud_intf_set_voc_param_test {param value}", cli_aud_intf_set_voc_param_cmd},
	{"aud_intf_set_aec_param_test", "aud_intf_set_aec_param_test {param value}", cli_aud_intf_set_aec_param_cmd},
	{"aud_intf_get_aec_param_test", "aud_intf_get_aec_param_test", cli_aud_intf_get_aec_param_cmd},
	{"aud_intf_doorbell_test", "aud_intf_doorbell_test {start|stop}", cli_aud_intf_doorbell_cmd},
};

int cli_aud_intf_init(void)
{
	return cli_register_commands(s_aud_intf_commands, AUD_INTF_CMD_CNT);
}

