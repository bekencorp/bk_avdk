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

#include <os/mem.h>
#include <os/os.h>
#include <stdio.h>
#include <stdlib.h>
#include <modules/adpcm.h>
#include "ff.h"
#include "diskio.h"

static void cli_audio_adpcm_help(void)
{
	os_printf("adpcm_encoder_test {xxx.pcm xxx.div4} \r\n");
	os_printf("adpcm_decoder_test {xxx.div4 xxx.pcm} \r\n");
}

void cli_adpcm_encoder_test_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	char mic_file_name[50];
	char out_encoder_file_name[50];
	FIL file_mic;
	FIL file_encoder_out;
	FRESULT fr;
	uint32 uiTemp = 0;
	uint32_t encoder_size = 0;
	uint8_t ucInBuff[1024] = {0};
	uint8_t ucOutBuff[256] = {0};
	adpcm_state_t state;
	bool empty_flag = false;
	state.valprev = 0;
	state.index = 0;

	if (argc != 3) {
		cli_audio_adpcm_help();
		return;
	}

	sprintf(mic_file_name, "1:/%s", argv[1]);
	fr = f_open(&file_mic, mic_file_name, FA_READ);
	if (fr != FR_OK) {
		os_printf("open %s fail.\r\n", mic_file_name);
		return;
	}

	sprintf(out_encoder_file_name, "1:/%s", argv[2]);
	fr = f_open(&file_encoder_out, out_encoder_file_name, FA_CREATE_ALWAYS | FA_WRITE);
	if (fr != FR_OK) {
		os_printf("open %s fail.\r\n", out_encoder_file_name);
		return;
	}

	encoder_size = f_size(&file_mic);
	os_printf("encoder_size = %d \r\n", encoder_size);
	while (!empty_flag)
	{
		fr = f_read(&file_mic, (uint8_t *)ucInBuff, 1024, &uiTemp);
		if (fr != FR_OK) {
			os_printf("read ref file fail.\r\n");
			break;
		}

		if (uiTemp < 1024)
			empty_flag = true;

		if (uiTemp > 0) {
			adpcm_coder((short *)ucInBuff, (char *)ucOutBuff, (uiTemp/2), &state);
			//encoder_temp = linear2alaw(mic_addr);
			fr = f_write(&file_encoder_out, (void *)ucOutBuff, uiTemp/4, &uiTemp);
			if (fr != FR_OK) {
				os_printf("write output data %s fail.\r\n", out_encoder_file_name);
				break;
			}
		}
	}

	empty_flag = false;

	fr = f_close(&file_encoder_out);
	if (fr != FR_OK) {
		os_printf("close out file %s fail!\r\n", out_encoder_file_name);
		return;
	}
	fr = f_close(&file_mic);
	if (fr != FR_OK) {
		os_printf("close out file %s fail!\r\n", mic_file_name);
		return;
	}
	os_printf("encoder test complete \r\n");

	os_printf("test finish \r\n");
}


void cli_adpcm_decoder_test_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	char encoder_file_name[50];
	char out_decoder_file_name[50];
	FIL file_encoder_mic;
	FIL file_decoder_out;
	FRESULT fr;
	uint32 uiTemp = 0;
	uint32_t decoder_size = 0;
	uint8_t ucInBuff[256] = {0};
	uint8_t ucOutBuff[1024] = {0};
	adpcm_state_t state;
	bool empty_flag = false;
	state.valprev = 0;
	state.index = 0;

	if (argc != 3) {
		cli_audio_adpcm_help();
		return;
	}

	sprintf(encoder_file_name, "1:/%s", argv[1]);
	fr = f_open(&file_encoder_mic, encoder_file_name, FA_READ);
	if (fr != FR_OK) {
		os_printf("open %s fail.\r\n", encoder_file_name);
		return;
	}

	sprintf(out_decoder_file_name, "1:/%s", argv[2]);
	fr = f_open(&file_decoder_out, out_decoder_file_name, FA_CREATE_ALWAYS | FA_WRITE);
	if (fr != FR_OK) {
		os_printf("open %s fail.\r\n", out_decoder_file_name);
		return;
	}

	decoder_size = f_size(&file_encoder_mic);
	os_printf("decoder_size = %d \r\n", decoder_size);
	while (!empty_flag)
	{
		fr = f_read(&file_encoder_mic, (uint8_t *)ucInBuff, 256, &uiTemp);
		if (fr != FR_OK) {
			os_printf("read file fail.\r\n");
			break;
		}

		if (uiTemp < 256)
			empty_flag = true;

		if (uiTemp > 0) {
			adpcm_decoder((char *)ucInBuff, (short *)ucOutBuff, (uiTemp*2), &state);
			fr = f_write(&file_decoder_out, (void *)ucOutBuff, uiTemp*4, &uiTemp);
			if (fr != FR_OK) {
				os_printf("write output data %s fail.\r\n", out_decoder_file_name);
				break;
			}
		}
	}

	empty_flag = false;

	fr = f_close(&file_decoder_out);
	if (fr != FR_OK) {
		os_printf("close out file %s fail!\r\n", out_decoder_file_name);
		return;
	}
	fr = f_close(&file_encoder_mic);
	if (fr != FR_OK) {
		os_printf("close out file %s fail!\r\n", encoder_file_name);
		return;
	}
	os_printf("decoder test complete \r\n");

	os_printf("test finish \r\n");
}

