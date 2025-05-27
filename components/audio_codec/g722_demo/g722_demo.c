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
#include "modules/g722.h"
#include "ff.h"
#include "diskio.h"
#include "cli.h"

static void cli_audio_g722_help(void)
{
	os_printf("g722_encoder_test {xxx.pcm xxx.pcm} \r\n");
	os_printf("g722_decoder_test {xxx.pcm xxx.pcm} \r\n");
}

void cli_g722_encoder_test_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	char mic_file_name[50];
	char out_encoder_file_name[50];

	FIL file_mic;
	FIL file_encoder_out;
	FRESULT fr;
	uint32 uiTemp = 0;
	uint32_t encoder_size = 0;
    g722_encode_state_t g722_enc = {0};
    int16_t *raw_data_buf = NULL;
    uint8_t *g722_data_buf = NULL;

	if (argc != 3) {
		cli_audio_g722_help();
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

    if (0 != g722_encode_init(&g722_enc, 64000, 0)) {
        os_printf("g722_encode_init fail.\n");
        return;
    }

    raw_data_buf = (int16_t *)os_malloc(640);
    if (raw_data_buf == NULL) {
        os_printf("malloc raw_data_buf fail.\n");
        return;
    }

    g722_data_buf = (uint8_t *)os_malloc(640);
    if (g722_data_buf == NULL) {
        os_printf("malloc g722_data_buf fail.\n");
        return;
    }

	encoder_size = f_size(&file_mic);
	os_printf("encoder_size = %d \r\n", encoder_size);
    while (encoder_size > 0) {
		fr = f_read(&file_mic, raw_data_buf, 640, &uiTemp);
		if (fr != FR_OK) {
			os_printf("read ref file fail.\r\n");
			break;
		}

        //GPIO_UP(35);
        int enc_size = g722_encode(&g722_enc, g722_data_buf, raw_data_buf, uiTemp/2);
        //GPIO_DOWN(35);
        if (enc_size <= 0)
        {
            os_printf("g722_encode fail, enc_size: %d\n", enc_size);
            break;
        }
        os_printf("enc_size: %d\n", enc_size);

        encoder_size -= uiTemp;

		fr = f_write(&file_encoder_out, (void *)g722_data_buf, enc_size, &uiTemp);
		if (fr != FR_OK) {
			os_printf("write output data %s fail\n", out_encoder_file_name);
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

    g722_encode_release(&g722_enc);

    os_free(raw_data_buf);
    raw_data_buf = NULL;
    os_free(g722_data_buf);
    g722_data_buf = NULL;

	os_printf("encoder test complete \r\n");

	os_printf("test finish \r\n");
}


void cli_g722_decoder_test_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	char encoder_file_name[50];
	char out_decoder_file_name[50];

	FIL file_encoder_mic;
	FIL file_decoder_out;
	FRESULT fr;
	uint32 uiTemp = 0;
	uint32_t decoder_size = 0;
    g722_decode_state_t g722_dec = {0};
    int16_t *raw_data_buf = NULL;
    uint8_t *g722_data_buf = NULL;

	if (argc != 3) {
		cli_audio_g722_help();
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

    if (0 != g722_decode_init(&g722_dec, 64000, 0)) {
        os_printf("g722_decode_init fail.\n");
        return;
    }

    raw_data_buf = (int16_t *)os_malloc(640 * 2 * 2);
    if (raw_data_buf == NULL) {
        os_printf("malloc raw_data_buf fail.\n");
        return;
    }

    g722_data_buf = (uint8_t *)os_malloc(640);
    if (g722_data_buf == NULL) {
        os_printf("malloc g722_data_buf fail.\n");
        return;
    }

	decoder_size = f_size(&file_encoder_mic);
	os_printf("decoder_size = %d \r\n", decoder_size);
    while (decoder_size > 0) {
		fr = f_read(&file_encoder_mic, g722_data_buf, 320, &uiTemp);
		if (fr != FR_OK) {
			os_printf("read file fail.\r\n");
			break;
		}

        //GPIO_UP(35);
        int dec_size = g722_decode(&g722_dec, raw_data_buf, g722_data_buf, uiTemp);
        //GPIO_DOWN(35);
        if (dec_size <= 0)
        {
            os_printf("g722_decode fail, dec_size: %d\n", dec_size);
            break;
        }
        os_printf("dec_size: %d\n", dec_size);

        decoder_size -= uiTemp;

		fr = f_write(&file_decoder_out, (void *)raw_data_buf, dec_size*2, &uiTemp);
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

    os_free(raw_data_buf);
    raw_data_buf = NULL;
    os_free(g722_data_buf);
    g722_data_buf = NULL;

    g722_decode_release(&g722_dec);

	os_printf("decoder test complete \r\n");

	os_printf("test finish \r\n");
}

#define G722_CMD_CNT (sizeof(s_g722_commands) / sizeof(struct cli_command))
static const struct cli_command s_g722_commands[] = {
	{"g722_encoder_test", "g722_encoder_test", cli_g722_encoder_test_cmd},
	{"g722_decoder_test", "g722_decoder_test", cli_g722_decoder_test_cmd},
};

int cli_g722_init(void)
{
	return cli_register_commands(s_g722_commands, G722_CMD_CNT);
}

