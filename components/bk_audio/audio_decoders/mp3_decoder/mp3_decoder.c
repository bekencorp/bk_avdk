// Copyright 2022-2023 Beken
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "mp3_decoder.h"
#include "audio_common.h"
#include "audio_mem.h"
#include "audio_element.h"
#include <os/os.h>

//#include "BK7256_RegList.h"

#define TAG  "MP3_DECODER"

typedef struct mp3_decoder {
	HMP3Decoder dec_handle;             /**< mp3 decoder handle */
	MP3FrameInfo frame_info;            /**< mp3 frame infomation */
	uint8_t *main_buff;                 /**< mainbuff save data read */
	uint32_t main_buff_size;            /**< mainbuff size */
	uint32_t main_buff_remain_size;     /**< mainbuff remain size */
	int16_t *out_pcm_buff;              /**< out pcm buffer save data decoded */
	uint32_t out_pcm_buff_size;         /**< out pcm buffer size */
	uint8_t *main_buff_readptr;         /**< read ptr of main buffer */
} mp3_decoder_t;


static bk_err_t _mp3_decoder_open(audio_element_handle_t self)
{
	BK_LOGD(TAG, "[%s] _mp3_decoder_open \n", audio_element_get_tag(self));
	mp3_decoder_t *mp3_dec = (mp3_decoder_t *)audio_element_getdata(self);
	mp3_dec->main_buff_readptr = mp3_dec->main_buff;

	return BK_OK;
}

static bk_err_t _mp3_decoder_close(audio_element_handle_t self)
{
	BK_LOGD(TAG, "[%s] _mp3_decoder_close \n", audio_element_get_tag(self));

	return BK_OK;
}

static bk_err_t music_info_report(audio_element_handle_t self)
{
	mp3_decoder_t *mp3_dec = (mp3_decoder_t *)audio_element_getdata(self);

	audio_element_info_t info = {0};
	bk_err_t ret = audio_element_getinfo(self, &info);
	if (ret != BK_OK) {
		BK_LOGE(TAG, "[%s] audio_element_getinfo fail \n", audio_element_get_tag(self));
		return BK_FAIL;
	}

	/* check frame information, report new frame information if frame information change */
	if (mp3_dec->frame_info.bitsPerSample != info.bits
		|| mp3_dec->frame_info.samprate != info.sample_rates
		|| mp3_dec->frame_info.nChans != info.channels) {
		info.bits = mp3_dec->frame_info.bitsPerSample;
		info.sample_rates = mp3_dec->frame_info.samprate;
		info.channels = mp3_dec->frame_info.nChans;
		ret = audio_element_setinfo(self, &info);
		if (ret != BK_OK) {
			BK_LOGE(TAG, "[%s] audio_element_setinfo fail \n", audio_element_get_tag(self));
			return BK_FAIL;
		}
		ret = audio_element_report_info(self);
		if (ret != BK_OK) {
			BK_LOGE(TAG, "[%s] audio_element_report_info fail \n", audio_element_get_tag(self));
			return BK_FAIL;
		}
	}
	return BK_OK;
}

static int _mp3_decoder_process(audio_element_handle_t self, char *in_buffer, int in_len)
{
	bk_err_t ret = BK_OK;
	int r_size = 0;

	BK_LOGD(TAG, "[%s] _mp3_decoder_process \n", audio_element_get_tag(self));
	mp3_decoder_t *mp3_dec = (mp3_decoder_t *)audio_element_getdata(self);

	if (mp3_dec->main_buff_remain_size < mp3_dec->main_buff_size) {
		os_memmove(mp3_dec->main_buff, mp3_dec->main_buff_readptr, mp3_dec->main_buff_remain_size);
		//fr = f_read(&mp3file, (void *)(readBuf + bytesLeft), MAINBUF_SIZE - bytesLeft, &uiTemp);
		int r_size = audio_element_input(self, (char *)(mp3_dec->main_buff + mp3_dec->main_buff_remain_size), mp3_dec->main_buff_size - mp3_dec->main_buff_remain_size);
		mp3_dec->main_buff_remain_size = mp3_dec->main_buff_remain_size + r_size;
		mp3_dec->main_buff_readptr = mp3_dec->main_buff;
	}

	uint32_t offset = MP3FindSyncWord(mp3_dec->main_buff_readptr, mp3_dec->main_buff_remain_size);
	if (offset < 0) {
		BK_LOGE(TAG, "[%s] MP3FindSyncWord not find \n", audio_element_get_tag(self));
		mp3_dec->main_buff_remain_size = 0;
	} else {
		mp3_dec->main_buff_readptr += offset;
		mp3_dec->main_buff_remain_size -= offset;

		ret = MP3Decode(mp3_dec->dec_handle, &mp3_dec->main_buff_readptr, (int *)&mp3_dec->main_buff_remain_size, mp3_dec->out_pcm_buff, 0);
		if (ret != ERR_MP3_NONE) {
			BK_LOGE(TAG, "MP3Decode failed, code is %d \n", ret);
			return 0;
			//r_size = AEL_PROCESS_FAIL;
		} else {
			MP3GetLastFrameInfo(mp3_dec->dec_handle, &mp3_dec->frame_info);
			BK_LOGD(TAG, "bitsPerSample: %d, Samprate: %d\r\n", mp3_dec->frame_info.bitsPerSample, mp3_dec->frame_info.samprate);
			BK_LOGD(TAG, "Channel: %d, Version: %d, Layer: %d\r\n", mp3_dec->frame_info.nChans, mp3_dec->frame_info.version, mp3_dec->frame_info.layer);
			BK_LOGD(TAG, "OutputSamps: %d\r\n", mp3_dec->frame_info.outputSamps);
			r_size = mp3_dec->frame_info.outputSamps * mp3_dec->frame_info.bitsPerSample / 8;
			BK_LOGD(TAG, "MP3Decode complete, r_size: %d \n", r_size);
		}
	}
#if 1
//	addAON_GPIO_Reg0x8 = 2;
	ret = music_info_report(self);
	if (ret != BK_OK) {
		BK_LOGE(TAG, "music_info_report \n");
		return 0;
	}
//	addAON_GPIO_Reg0x8 = 0;
#endif

	int w_size = 0;
	if (r_size > 0) {
		w_size = audio_element_output(self, (char *)mp3_dec->out_pcm_buff, r_size);
	} else {
		w_size = r_size;
	}
	return w_size;
}

static bk_err_t _mp3_decoder_destroy(audio_element_handle_t self)
{
	mp3_decoder_t *mp3_dec = (mp3_decoder_t *)audio_element_getdata(self);

	if (mp3_dec->main_buff) {
		audio_free(mp3_dec->main_buff);
		mp3_dec->main_buff = NULL;
	}
	if (mp3_dec->out_pcm_buff) {
		audio_free(mp3_dec->out_pcm_buff);
		mp3_dec->out_pcm_buff = NULL;
	}
	if (mp3_dec->dec_handle) {
		MP3FreeDecoder(mp3_dec->dec_handle);
		mp3_dec->dec_handle = NULL;
	}
	audio_free(mp3_dec);
	return BK_OK;
}

audio_element_handle_t mp3_decoder_init(mp3_decoder_cfg_t *config)
{
	audio_element_handle_t el;
	mp3_decoder_t *mp3_dec = audio_calloc(1, sizeof(mp3_decoder_t));
	AUDIO_MEM_CHECK(TAG, mp3_dec, return NULL);
	os_memset(mp3_dec, 0, sizeof(mp3_decoder_t));

	audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
	cfg.open = _mp3_decoder_open;
	cfg.close = _mp3_decoder_close;
	cfg.seek = NULL;
	cfg.process = _mp3_decoder_process;
	cfg.destroy = _mp3_decoder_destroy;
	cfg.read = NULL;
	cfg.write = NULL;
	cfg.task_stack = config->task_stack;
	cfg.task_prio = config->task_prio;
	cfg.task_core = config->task_core;
	cfg.out_rb_size = config->out_rb_size;
	cfg.buffer_len = cfg.out_rb_size;
	cfg.tag = "mp3_decoder";

	mp3_dec->main_buff_size = config->main_buff_size;
	mp3_dec->out_pcm_buff_size = config->out_pcm_buff_size;
	mp3_dec->dec_handle = MP3InitDecoder();
	AUDIO_MEM_CHECK(TAG, mp3_dec->dec_handle, goto _mp3_decoder_init_exit);
	mp3_dec->main_buff = (uint8_t *)audio_malloc(mp3_dec->main_buff_size);
	AUDIO_MEM_CHECK(TAG, mp3_dec->main_buff, goto _mp3_decoder_init_exit);
	mp3_dec->out_pcm_buff = (int16_t *)audio_malloc(mp3_dec->out_pcm_buff_size);
	AUDIO_MEM_CHECK(TAG, mp3_dec->out_pcm_buff, goto _mp3_decoder_init_exit);

	el = audio_element_init(&cfg);

	AUDIO_MEM_CHECK(TAG, el, goto _mp3_decoder_init_exit);
	audio_element_setdata(el, mp3_dec);

	audio_element_info_t info = {0};
	audio_element_getinfo(el, &info);
	info.sample_rates = 8000;
	info.channels = 2;
	info.bits = 16;
	info.codec_fmt = BK_CODEC_TYPE_MP3;
	audio_element_setinfo(el, &info);

	return el;
_mp3_decoder_init_exit:
	if (mp3_dec->main_buff) {
		audio_free(mp3_dec->main_buff);
		mp3_dec->main_buff = NULL;
	}
	if (mp3_dec->out_pcm_buff) {
		audio_free(mp3_dec->out_pcm_buff);
		mp3_dec->out_pcm_buff = NULL;
	}
	if (mp3_dec->dec_handle) {
		MP3FreeDecoder(mp3_dec->dec_handle);
		mp3_dec->dec_handle = NULL;
	}
	audio_free(mp3_dec);
	mp3_dec = NULL;
	return NULL;
}

