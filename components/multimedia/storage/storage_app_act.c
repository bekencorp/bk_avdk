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
#include <stdio.h>
#include <os/mem.h>
#include <os/str.h>
#include <components/log.h>

#include "frame_buffer.h"
#include "media_app.h"
#include "media_evt.h"
#include "storage_act.h"

#if (CONFIG_CACHE_ENABLE)
#include "cache.h"
#endif

#if (CONFIG_FATFS)
#include "ff.h"
#include "diskio.h"
#endif

#include "driver/flash.h"
#include "driver/flash_partition.h"

#define TAG "storage"

#define SECTOR                  0x1000
#define STORAGE_NAME_LEN        20

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

typedef struct
{
	uint8_t enable : 1;// save state
	uint8_t mode: 1; // 0:save picture, 1:save stream
	beken_queue_t queue;
	beken_thread_t thread;
	media_mailbox_msg_t *node;
	beken_semaphore_t sem;
	char name[STORAGE_NAME_LEN];
	frame_cb_t cb;
} storage_config_t;

static storage_config_t *s_storage_app_config = NULL;

static bk_err_t storage_app_task_send_msg(uint8_t msg_type, uint32_t data)
{
	bk_err_t ret = BK_ERR_NOT_INIT;
	storages_task_msg_t msg;
	storage_config_t *config = s_storage_app_config;

	if (config)
	{
		msg.type = msg_type;
		msg.data = data;

		ret = rtos_push_to_queue(&config->queue, &msg, BEKEN_NO_WAIT);
		if (BK_OK != ret)
		{
			LOGE("storage_app_task_send_msg failed\r\n");
		}
	}

	return ret;
}

void storage_frame_buffer_dump(frame_buffer_t *frame, char *name)
{
	char cFileName[30];
	LOGI("%s dump frame: %p, %u, size: %d\n", __func__, frame->frame, frame->sequence, frame->length);

	sprintf(cFileName, "%d%s", frame->sequence, name);

#if (CONFIG_FATFS)
	bk_mem_save_to_sdcard(cFileName, frame->frame, frame->length);
	LOGI("%s, complete\n", __func__);
#endif
}

static void storage_capture_save(frame_buffer_t *frame, char *name)
{
	LOGI("%s save frame: %d, size: %d\n", __func__, frame->sequence ,frame->length);

	bk_mem_save_to_sdcard(name, frame->frame, frame->length);

	LOGI("%s, complete\n", __func__);
}

static void storage_app_save_handle(uint32_t param)
{
	int ret = BK_FAIL;

	storage_config_t *config = (storage_config_t *)param;

	if (config->enable == STORAGE_STATE_ENABLED)
	{
#if (CONFIG_CACHE_ENABLE)
		flush_dcache(config->node, sizeof(media_mailbox_msg_t));
#endif

		frame_buffer_t *frame = (frame_buffer_t *)config->node->param;
		if (frame == NULL)
		{
			LOGE("%s, frame NULL\n", __func__);
			goto out;
		}

		if (config->cb)
		{
			config->cb(frame);
		}
		else
		{
			if (config->mode == STORAGE_PICTURE_MODE)
			{
				bk_mem_save_to_sdcard(&config->name[0], frame->frame, frame->length);
			}
			else
			{
				if (frame->fmt == PIXEL_FMT_H264 || frame->fmt == PIXEL_FMT_H265)
				{
					// only h26x need save stream
					bk_mem_append_save_to_sdcard(&config->name[0], frame->frame, frame->length);
				}
				else
				{
					char rename[20] = {0};

					sprintf(rename, "%d_%s", frame->sequence, &config->name[0]);

					bk_mem_save_to_sdcard(rename, frame->frame, frame->length);
				}
			}
		}

		ret = BK_OK;
		config->enable = STORAGE_STATE_DISABLED;
	}
	else
	{
		LOGE("%s, storage state error:%d\n", __func__, config->enable);
	}

out:
	// send finish notify to cp1
	msg_send_rsp_to_media_app_mailbox(config->node, ret);
}

static void storage_app_task_entry(beken_thread_arg_t data)
{
	bk_err_t ret = BK_OK;
	storages_task_msg_t msg;
	storage_config_t *config = (storage_config_t *)data;

	rtos_set_semaphore(&config->sem);

	while (1)
	{
		ret = rtos_pop_from_queue(&config->queue, &msg, BEKEN_WAIT_FOREVER);

		if (BK_OK == ret)
		{
			switch (msg.type)
			{
				case STORAGE_TASK_CAPTURE:
				case STORAGE_TASK_SAVE:
					storage_app_save_handle(msg.data);
					break;

				case STORAGE_TASK_EXIT:
					goto exit;
				default:
					break;
			}
		}
	}

exit:

	LOGI("storage_app_task exit\r\n");

	rtos_deinit_queue(&config->queue);
	config->queue = NULL;

	rtos_set_semaphore(&config->sem);
	rtos_delete_thread(NULL);
}

int storage_app_task_init(uint32_t param)
{
	int ret = BK_OK;

	storage_config_t *config = (storage_config_t *)os_malloc(sizeof(storage_config_t));

	if (config == NULL)
	{
		LOGE("s_storage_app_config init failed\n");
		goto error;
	}

	os_memset(config, 0, sizeof(storage_config_t));

	if (param)
	{
		config->cb = (frame_cb_t)param;
		LOGW("%s, register user cb:0x%x, the frame will ouput to user, do not save frame to sdcard\n", __func__, param);
	}

	ret = rtos_init_semaphore(&config->sem, 1);

	if (ret != BK_OK)
	{
		LOGE("s_storage_app_config->sem init failed\n");
		goto error;
	}

	ret = rtos_init_queue(&config->queue,
							"storage_app_task_queue",
							sizeof(storages_task_msg_t),
							5);

	if (BK_OK != ret)
	{
		LOGE("s_storage_app_config->queue init failed\n");
		goto error;
	}

	ret = rtos_create_thread(&config->thread,
							BEKEN_DEFAULT_WORKER_PRIORITY,
							"storage_thread",
							(beken_thread_function_t)storage_app_task_entry,
							2560,
							(beken_thread_arg_t)config);

	if (BK_OK != ret)
	{
		LOGE("s_storage_app_config->thread failed\n");
		goto error;
	}

	rtos_get_semaphore(&config->sem, BEKEN_NEVER_TIMEOUT);

	s_storage_app_config = config;

	return ret;

error:

	if (config)
	{
		if (config->sem)
		{
			rtos_deinit_semaphore(&config->sem);
			config->sem = NULL;
		}

		if (config->queue)
		{
			rtos_deinit_queue(&config->queue);
			config->queue = NULL;
		}

		os_free(config);
	}

	return ret;
}

static bk_err_t storage_capture_notify_handle(media_mailbox_msg_t *msg)
{
	int ret = BK_OK;

	storage_config_t *config = s_storage_app_config;

	if (config == NULL
		|| config->enable == STORAGE_STATE_ENABLED)
	{
		LOGI("%s already capture or not open:%p\n", __func__, config);
		goto error;
	}

	config->enable = STORAGE_STATE_ENABLED;
	config->mode = STORAGE_PICTURE_MODE;
	config->node = msg;

	ret = storage_app_task_send_msg(STORAGE_TASK_CAPTURE, (uint32_t)config);

	if (ret != BK_OK)
	{
		LOGE("%s send msg failed, ret:%d\n", __func__, ret);
		goto error;
	}

	return ret;

error:

	msg_send_rsp_to_media_app_mailbox(msg, ret);

	return ret;
}

static bk_err_t storage_save_all_notify_handle(media_mailbox_msg_t *msg)
{
	int ret = BK_FAIL;

	storage_config_t *config = s_storage_app_config;

	if (config == NULL
		|| config->enable == STORAGE_STATE_ENABLED)
	{
		LOGW("%s already capture or not open:%p\n", __func__, config);
		msg_send_rsp_to_media_app_mailbox(msg, ret);
	}
	else
	{
		config->enable = STORAGE_STATE_ENABLED;
		config->mode = STORAGE_STREAM_MODE;
		config->node = msg;

		ret = storage_app_task_send_msg(STORAGE_TASK_SAVE, (uint32_t)config);

		if (ret != BK_OK)
		{
			LOGE("%s, send msg failed, ret:%d\n", __func__, ret);
			msg_send_rsp_to_media_app_mailbox(msg, ret);
		}
	}

	return ret;
}

bk_err_t storage_app_task_open_handle(uint32_t param)
{
	bk_err_t ret = BK_OK;

	LOGI("%s\n", __func__);

	if (s_storage_app_config != NULL)
	{
		LOGI("%s already open\n", __func__);
		return ret;
	}

	ret = storage_app_task_init(param);

	return ret;
}

bk_err_t storage_app_task_close_handle(void)
{
	bk_err_t ret = BK_OK;

	storage_config_t *config = s_storage_app_config;

	if (config == NULL)
	{
		LOGI("%s already close\n", __func__);
		return ret;
	}

	config->enable = STORAGE_STATE_DISABLED;

	ret = storage_app_task_send_msg(STORAGE_TASK_EXIT, 0);
	if (ret != BK_OK)
	{
		LOGE("%s, msg send fail, ret:%d\n", __func__, ret);
		return ret;
	}

	rtos_get_semaphore(&config->sem, BEKEN_NEVER_TIMEOUT);

	rtos_deinit_semaphore(&config->sem);
	config->sem = NULL;
	os_free(config);
	s_storage_app_config = NULL;

	return ret;
}

bk_err_t storage_app_set_frame_name(char *name)
{
	bk_err_t ret = BK_FAIL;
	storage_config_t *config = s_storage_app_config;

	if (config == NULL
		|| config->enable == STORAGE_STATE_ENABLED)
	{
		LOGW("%s, not support\n", __func__);
		return ret;
	}

	os_memset(&config->name[0], 0, STORAGE_NAME_LEN);

	int len = os_strlen(name);

	if (len > STORAGE_NAME_LEN)
	{
		LOGW("%s, name length over max length:%d\n", STORAGE_NAME_LEN);
		len = STORAGE_NAME_LEN - 1;
	}

	os_memcpy(config->name, (char *)name, len);

	config->name[STORAGE_NAME_LEN - 1] = '\0';

	return BK_OK;
}

bk_err_t storage_app_event_handle(media_mailbox_msg_t *msg)
{
	int ret = BK_OK;

	switch (msg->event)
	{
		case EVENT_STORAGE_OPEN_IND:
			ret = storage_app_task_open_handle(msg->param);
			break;

		case EVENT_STORAGE_CLOSE_IND:
			ret = storage_app_task_close_handle();
			break;

		case EVENT_VID_CAPTURE_NOTIFY:
			ret = storage_capture_notify_handle(msg);
			break;

		case EVENT_VID_SAVE_ALL_NOTIFY:
			ret = storage_save_all_notify_handle(msg);
			break;

		default:
			break;

	}

	return ret;
}

