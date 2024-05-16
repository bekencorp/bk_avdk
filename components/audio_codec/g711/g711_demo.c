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
#include "modules/g711.h"
#include "ff.h"
#include "diskio.h"
#include "cli.h"

static void cli_audio_g711_help(void)
{
	os_printf("g711_encoder_test {xxx.pcm xxx.pcm} \r\n");
	os_printf("g711_decoder_test {xxx.pcm xxx.pcm} \r\n");
}

void cli_g711_encoder_test_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	char mic_file_name[50];
	char out_encoder_file_name[50];

	FIL file_mic;
	FIL file_encoder_out;
	int16_t mic_addr = 0;
	int8_t encoder_temp = 0;
	FRESULT fr;
	uint32 uiTemp = 0;
	uint32_t encoder_size = 0;
	uint32_t i = 0;

	if (argc != 3) {
		cli_audio_g711_help();
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
	for (i=0; i<encoder_size/2; i++) {
		fr = f_read(&file_mic, &mic_addr, 2, &uiTemp);
		if (fr != FR_OK) {
			os_printf("read ref file fail.\r\n");
			break;
		}

		encoder_temp = linear2alaw(mic_addr);
		fr = f_write(&file_encoder_out, (void *)&encoder_temp, 1, &uiTemp);
		if (fr != FR_OK) {
			os_printf("write output data %s fail.\r\n", out_encoder_file_name);
			break;
		}
	}

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

void cli_g711_decoder_test_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	char encoder_file_name[50];
	char out_decoder_file_name[50];

	FIL file_encoder_mic;
	FIL file_decoder_out;
	int8_t encoder_addr = 0;
	int16_t decoder_temp = 0;
	FRESULT fr;
	uint32 uiTemp = 0;
	uint32_t decoder_size = 0;
	uint32_t i = 0;
//	int8_t encoder_temp = 0;

	if (argc != 3) {
		cli_audio_g711_help();
		return;
	}

	sprintf(encoder_file_name, "1:/%s", argv[1]);
	fr = f_open(&file_encoder_mic, encoder_file_name, FA_READ);
	if (fr != FR_OK) {
		os_printf("open %s fail.\r\n", encoder_file_name);
		return;
	}

	sprintf(out_decoder_file_name, "1:/%s", argv[2]);
	fr = f_open(&file_decoder_out, out_decoder_file_name, FA_OPEN_APPEND | FA_WRITE);
	if (fr != FR_OK) {
		os_printf("open %s fail.\r\n", out_decoder_file_name);
		return;
	}

//	encoder_temp = 8;
//	f_write(&file_decoder_out, (void *)&encoder_temp, 1, &uiTemp);


	decoder_size = f_size(&file_encoder_mic);
	os_printf("decoder_size = %d \r\n", decoder_size);
	for (i=0; i<decoder_size; i++) {
		fr = f_read(&file_encoder_mic, &encoder_addr, 1, &uiTemp);
		if (fr != FR_OK) {
			os_printf("read file fail.\r\n");
			break;
		}

		decoder_temp = alaw2linear((unsigned char)encoder_addr);
		//os_printf("decoder_temp = %d \r\n", decoder_temp);
		fr = f_write(&file_decoder_out, (void *)&decoder_temp, 2, &uiTemp);
		if (fr != FR_OK) {
			os_printf("write output data %s fail.\r\n", out_decoder_file_name);
			break;
		}
	}

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

#define G711_CMD_CNT (sizeof(s_g711_commands) / sizeof(struct cli_command))
static const struct cli_command s_g711_commands[] = {
	{"g711_encoder_test", "g711_encoder_test", cli_g711_encoder_test_cmd},
	{"g711_decoder_test", "g711_decoder_test", cli_g711_decoder_test_cmd},
};

int cli_g711_init(void)
{
	return cli_register_commands(s_g711_commands, G711_CMD_CNT);
}

