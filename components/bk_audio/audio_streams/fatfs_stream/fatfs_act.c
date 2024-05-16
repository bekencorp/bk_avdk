// Copyright 2023-2024 Beken
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
#include "semphr.h"
#include "task.h"
#include "fatfs_stream.h"
#include "audio_common.h"
#include "audio_mem.h"
#include "audio_element.h"
#include "fatfs_act.h"


#define TAG  "FTFS_STR"

bk_err_t fatfs_init(int task_stack, int task_prio, void **coprocess_hdl, char *tag)
{
#if CONFIG_SYS_CPU0
		//not need
#else
	media_mailbox_msg_t fatfs_init_mb_msg;
	bk_err_t ret = BK_OK;

	ret = rtos_init_semaphore(&fatfs_init_mb_msg.sem, 1);
	if (ret != BK_OK)
	{
		BK_LOGE(TAG, "%s init semaphore failed\n", __func__);
		return BK_FAIL;
	}

	audio_element_mb_t mb_param = {0};
	fatfs_init_param_t init_param = {0};
	init_param.task_prio = task_prio;
	init_param.task_stack = task_stack;
	mb_param.coprocess_hdl = NULL;
	mb_param.module = AUDIO_ELEMENT_FATFS;
	mb_param.param = &init_param;
	fatfs_init_mb_msg.event = EVENT_FATFS_INIT;
	fatfs_init_mb_msg.param = (uint32_t)&mb_param;
	ret = msg_send_req_to_media_major_mailbox_sync(&fatfs_init_mb_msg, APP_MODULE);
	rtos_deinit_semaphore(&fatfs_init_mb_msg.sem);

	if (ret != BK_OK) {
		BK_LOGE(TAG, "[%s] %s Failed to init fatfs coprocess task. ret: %d, line: %d \n", tag, __func__, ret, __LINE__);
		return BK_FAIL;
	}
	*coprocess_hdl = mb_param.coprocess_hdl;
#endif
	return BK_OK;
}

bk_err_t fatfs_deinit(void *coprocess_hdl, char *tag)
{
#if CONFIG_SYS_CPU0
	//not need
#else
	media_mailbox_msg_t fatfs_deinit_mb_msg;
	bk_err_t ret = BK_OK;

	ret = rtos_init_semaphore(&fatfs_deinit_mb_msg.sem, 1);
	if (ret != BK_OK)
	{
		BK_LOGE(TAG, "%s init semaphore failed\n", __func__);
		return BK_FAIL;
	}

	audio_element_mb_t mb_param = {0};
	mb_param.coprocess_hdl = coprocess_hdl;
	mb_param.module = AUDIO_ELEMENT_FATFS;
	mb_param.param = NULL;
	fatfs_deinit_mb_msg.event = EVENT_FATFS_DEINIT;
	fatfs_deinit_mb_msg.param = (uint32_t)&mb_param;
	ret = msg_send_req_to_media_major_mailbox_sync(&fatfs_deinit_mb_msg, APP_MODULE);
	rtos_deinit_semaphore(&fatfs_deinit_mb_msg.sem);

	if (ret != BK_OK) {
		BK_LOGE(TAG, "[%s] %s Failed to deinit fatfs coprocess task. ret: %d, line: %d \n", tag, __func__, ret, __LINE__);
		return BK_FAIL;
	}
#endif
	return BK_OK;
}

bk_err_t fatfs_open(void **fp, char* path, uint8_t mode, void *coprocess_hdl, char *tag)
{
#if CONFIG_SYS_CPU0
	FRESULT fr;

	FIL *fp_open = os_malloc(sizeof(FIL));
	if (!fp_open) {
		BK_LOGE(TAG, "%s calloc fp_open fail, line: %d \n", __func__, __LINE__);
		return BK_FAIL;
	}
	os_memset(fp_open, 0x00, sizeof(FIL));
	*fp = fp_open;
	BK_LOGI(TAG, "[%s] %s fatfs open ok, fp: %p. line: %d \n", tag, __func__, *fp, __LINE__);

	fr = f_open(*fp, path, mode);
	if (fr != BK_OK) {
		BK_LOGE(TAG, "Failed to open. File name: %s, error: %d, line: %d \n", path, fr, __LINE__);
		return BK_FAIL;
	}
#else
	media_mailbox_msg_t fatfs_open_mb_msg;
	bk_err_t ret = BK_OK;

	ret = rtos_init_semaphore(&fatfs_open_mb_msg.sem, 1);
	if (ret != BK_OK)
	{
		BK_LOGE(TAG, "%s init semaphore failed\n", __func__);
		return BK_FAIL;
	}

	audio_element_mb_t mb_param = {0};
	fatfs_open_param_t open_param = {0};
	mb_param.coprocess_hdl = coprocess_hdl;
	mb_param.module = AUDIO_ELEMENT_FATFS;
	open_param.fp = NULL;
	open_param.path = path;
	open_param.mode = mode;
	fatfs_open_mb_msg.event = EVENT_FATFS_OPEN;
	mb_param.param = &open_param;
	fatfs_open_mb_msg.param = (uint32_t)&mb_param;
	ret = msg_send_req_to_media_major_mailbox_sync(&fatfs_open_mb_msg, APP_MODULE);
	rtos_deinit_semaphore(&fatfs_open_mb_msg.sem);

	if (ret != BK_OK) {
		BK_LOGE(TAG, "[%s] %s Failed to fatfs open, ret:%d, line: %d \n", tag, __func__, ret, __LINE__);
		return BK_FAIL;
	}
	*fp = open_param.fp;
#endif

	return BK_OK;
}

bk_err_t fatfs_size(void *fp, void *coprocess_hdl, char *tag)
{
#if CONFIG_SYS_CPU0
	return f_size((FIL *)fp);
#else
	media_mailbox_msg_t fatfs_size_mb_msg;
	bk_err_t ret = BK_OK;

	ret = rtos_init_semaphore(&fatfs_size_mb_msg.sem, 1);
	if (ret != BK_OK)
	{
		BK_LOGE(TAG, "%s init semaphore failed\n", __func__);
		return BK_FAIL;
	}

	audio_element_mb_t mb_param = {0};
	fatfs_fp_param_t size_param = {0};
	size_param.fp = fp;
	BK_LOGD(TAG, "%s size_param.fp: %p \n", __func__, size_param.fp);
	mb_param.coprocess_hdl = coprocess_hdl;
	mb_param.module = AUDIO_ELEMENT_FATFS;
	mb_param.param = &size_param;
	fatfs_size_mb_msg.event = EVENT_FATFS_SIZE;
	fatfs_size_mb_msg.param = (uint32_t)&mb_param;
	ret = msg_send_req_to_media_major_mailbox_sync(&fatfs_size_mb_msg, APP_MODULE);
	rtos_deinit_semaphore(&fatfs_size_mb_msg.sem);

	if (ret < 0) {
		BK_LOGE(TAG, "[%s] %s Failed to fatfs size, ret: %d, line: %d \n", tag, __func__, ret, __LINE__);
		return BK_FAIL;
	}
	return ret;
#endif
}

bk_err_t fatfs_lseek(void *fp, uint64_t ofs, void *coprocess_hdl, char *tag)
{
#if CONFIG_SYS_CPU0
	int ret = f_lseek((FIL *)fp, ofs);
	if (ret < 0)
		BK_LOGE(TAG, "%s fail seek file. Error: %s, line: %d \n", __func__, f_error((FIL *)fp), __LINE__);
	return ret;
#else
	media_mailbox_msg_t fatfs_lseek_mb_msg;
	bk_err_t ret = BK_OK;

	ret = rtos_init_semaphore(&fatfs_lseek_mb_msg.sem, 1);
	if (ret != BK_OK)
	{
		BK_LOGE(TAG, "%s init semaphore failed\n", __func__);
		return BK_FAIL;
	}

	audio_element_mb_t mb_param = {0};
	fatfs_lseek_param_t lseek_param = {0};
	lseek_param.fp = fp;
	lseek_param.ofs = ofs;
	mb_param.coprocess_hdl = coprocess_hdl;
	mb_param.module = AUDIO_ELEMENT_FATFS;
	mb_param.param = &lseek_param;
	fatfs_lseek_mb_msg.event = EVENT_FATFS_LSEEK;
	fatfs_lseek_mb_msg.param = (uint32_t)&mb_param;
	ret = msg_send_req_to_media_major_mailbox_sync(&fatfs_lseek_mb_msg, APP_MODULE);
	rtos_deinit_semaphore(&fatfs_lseek_mb_msg.sem);

	if (ret < 0) {
		BK_LOGE(TAG, "[%s] %s Failed to fatfs lseek, ret: %d. line: %d \n", tag, __func__, ret, __LINE__);
		//return BK_FAIL;
	}

	return ret;
#endif
}

int fatfs_read(void *fp, void* buff, uint64_t len, void *coprocess_hdl, char *tag)
{
#if CONFIG_SYS_CPU0
	FRESULT fr;
	int ret = 0;
	/* use file descriptors to access files */
	uint32 rlen = 0;
	fr = f_read((FIL *)fp, buff, len, &rlen);
	if (fr == BK_OK) {
		ret = rlen;
	}else {
		BK_LOGE(TAG, "%s The error is happened in reading data. Error: %s, line: %d \n", __func__, f_error((FIL *)fp), __LINE__);
		ret = -1;
	}
	return ret;
#else
	media_mailbox_msg_t fatfs_read_mb_msg;
	bk_err_t ret = BK_OK;

	ret = rtos_init_semaphore(&fatfs_read_mb_msg.sem, 1);
	if (ret != BK_OK)
	{
		BK_LOGE(TAG, "%s init semaphore failed\n", __func__);
		return BK_FAIL;
	}
	audio_element_mb_t mb_param = {0};
	fatfs_rw_param_t read_param = {0};
	read_param.fp = fp;
	read_param.buff = buff;
	read_param.len = len;
	mb_param.coprocess_hdl = coprocess_hdl;
	mb_param.module = AUDIO_ELEMENT_FATFS;
	mb_param.param = &read_param;
	fatfs_read_mb_msg.event = EVENT_FATFS_READ;
	fatfs_read_mb_msg.param = (uint32_t)&mb_param;
	ret = msg_send_req_to_media_major_mailbox_sync(&fatfs_read_mb_msg, APP_MODULE);
	rtos_deinit_semaphore(&fatfs_read_mb_msg.sem);

	if (ret < 0) {
		BK_LOGE(TAG, "[%s] %s Failed to fatfs read task, ret: %d. line: %d \n", tag, __func__, ret,__LINE__);
		//return BK_FAIL;
	}

	return ret;
#endif
}

int fatfs_write(void *fp, void* buff, uint64_t len, void *coprocess_hdl, char *tag)
{
#if CONFIG_SYS_CPU0
	FRESULT fr;
	int ret = 0;
	uint32 wlen = 0;
	fr = f_write((FIL *)fp, buff, len, &wlen);
	fr = BK_OK;
	if (fr == BK_OK) {
		ret = wlen;
		if (ret != len) {
			BK_LOGE(TAG, "%s len: %d, wlen: %d \n", __func__, len, wlen);
		}
	} else {
		BK_LOGE(TAG, "%s writing data error. Error: %s, line: %d \n", __func__, f_error((FIL *)fp), __LINE__);
		ret = -1;
	}
	return ret;
#else
	media_mailbox_msg_t fatfs_write_mb_msg;
	bk_err_t ret = BK_OK;

	ret = rtos_init_semaphore(&fatfs_write_mb_msg.sem, 1);
	if (ret != BK_OK)
	{
		BK_LOGE(TAG, "%s init semaphore failed\n", __func__);
		return BK_FAIL;
	}
	audio_element_mb_t mb_param = {0};
	fatfs_rw_param_t write_param = {0};
	write_param.fp = fp;
	write_param.buff = buff;
	write_param.len = len;
	mb_param.coprocess_hdl = coprocess_hdl;
	mb_param.module = AUDIO_ELEMENT_FATFS;
	mb_param.param = &write_param;
	fatfs_write_mb_msg.event = EVENT_FATFS_WRITE;
	fatfs_write_mb_msg.param = (uint32_t)&mb_param;
	ret = msg_send_req_to_media_major_mailbox_sync(&fatfs_write_mb_msg, APP_MODULE);
	rtos_deinit_semaphore(&fatfs_write_mb_msg.sem);

	if (ret < 0) {
		BK_LOGE(TAG, "[%s] %s Failed to fatfs write, ret: %d. line: %d \n", tag, __func__, ret, __LINE__);
		//return BK_FAIL;
	}

	return ret;
#endif
}

bk_err_t fatfs_close(void *fp, void *coprocess_hdl, char *tag)
{
#if CONFIG_SYS_CPU0
	FRESULT fr = f_close((FIL*)fp);
	if (fr == BK_OK) {
		BK_LOGI(TAG, "[%s] %s fatfs close ok, fp: %p. line: %d \n", tag, __func__, fp, __LINE__);
		os_free((FIL*)fp);
		fp = NULL;
		return BK_OK;
	} else {
		BK_LOGE(TAG, "[%s] %s Failed to fatfs close, ret: %d. line: %d \n", tag, __func__, fr, __LINE__);
		return BK_FAIL;
	}
#else
	media_mailbox_msg_t fatfs_close_mb_msg;
	bk_err_t ret = BK_OK;

	ret = rtos_init_semaphore(&fatfs_close_mb_msg.sem, 1);
	if (ret != BK_OK)
	{
		BK_LOGE(TAG, "%s init semaphore failed\n", __func__);
		return BK_FAIL;
	}

	audio_element_mb_t mb_param = {0};
	fatfs_fp_param_t close_param = {0};
	close_param.fp = fp;
	mb_param.coprocess_hdl = coprocess_hdl;
	mb_param.module = AUDIO_ELEMENT_FATFS;
	mb_param.param = &close_param;
	fatfs_close_mb_msg.event = EVENT_FATFS_CLOSE;
	fatfs_close_mb_msg.param = (uint32_t)&mb_param;
	ret = msg_send_req_to_media_major_mailbox_sync(&fatfs_close_mb_msg, APP_MODULE);
	rtos_deinit_semaphore(&fatfs_close_mb_msg.sem);

	if (ret != BK_OK) {
		BK_LOGE(TAG, "[%s] %s Failed to fatfs close. ret: %d, line: %d \n", tag, __func__, ret, __LINE__);
		return BK_FAIL;
	}

	return ret;
#endif
}
