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
#include <components/log.h>

#include <driver/psram.h>
#include "frame_buffer.h"

#include "media_evt.h"
#include "storage_act.h"

#define TAG "storage_major"

#define LOGI(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

typedef struct
{
	uint8_t enable : 1;// save state
	uint16_t img_format;
	void *stream;
	beken_queue_t queue;
	beken_thread_t thread;
	beken_semaphore_t sem;
} storage_config_t;

static storage_config_t *s_storage_major_config = NULL;

static bk_err_t storage_major_task_send_msg(uint8_t msg_type, uint32_t data)
{
	bk_err_t ret = BK_ERR_NOT_INIT;
	storages_task_msg_t msg;

	storage_config_t *config = s_storage_major_config;

	if (config)
	{
		msg.type = msg_type;
		msg.data = data;

		ret = rtos_push_to_queue(&config->queue, &msg, BEKEN_NO_WAIT);
		if (BK_OK != ret)
		{
			LOGE("storage_major_task_queue failed\r\n");
			return BK_ERR_NO_MEM;
		}

		return ret;
	}

	return ret;
}

void *storage_major_get_target_stream(uint16_t format)
{
	void *stream = NULL;
	uint8_t retry_times = 5;
	do {
		if (format & IMAGE_H264)
		{
			stream = frame_buffer_list_get_by_format(IMAGE_H264);
		}
		else
		{
			stream = frame_buffer_list_get_main_stream();
		}

		retry_times--;

		if (stream == NULL)
		{
			rtos_delay_milliseconds(1000);
		}

	} while(stream == NULL && retry_times);

	if (stream == NULL)
	{
		LOGE("%s, get target format:%d stream timeout 5s, please check camera or stream have been opend\n", __func__, format);
	}

	return stream;
}

static void storage_frame_major_notify_app(uint32_t param)
{
	bk_err_t ret = BK_FAIL;
	frame_buffer_t *frame = NULL;
	media_mailbox_msg_t *msg = (media_mailbox_msg_t *)param;

	storage_config_t *config = s_storage_major_config;

	if (config->enable != STORAGE_STATE_ENABLED)
	{
		LOGE("%s, capture state error...\n", __func__);
		goto out;
	}

	config->img_format = (uint16_t)msg->param;

	config->stream = storage_major_get_target_stream(config->img_format);

	if (config->stream == NULL)
	{
		goto out;
	}

	frame_buffer_fb_register(config->stream, MODULE_CAPTURE);

	frame = frame_buffer_fb_read(config->stream, MODULE_CAPTURE, 100);

	if (frame == NULL)
	{
		LOGE("read frame NULL timeout\n");
		goto out;
	}
	else
	{
		LOGI("%s, seq:%d, length:%d\n", __func__, frame->sequence, frame->length);
	}

	ret = msg_send_req_to_media_major_mailbox_sync(EVENT_VID_CAPTURE_NOTIFY, APP_MODULE, (uint32_t)frame, NULL);

	frame_buffer_fb_read_free(config->stream, frame, MODULE_CAPTURE);

	frame_buffer_fb_deregister(config->stream, MODULE_CAPTURE);

	config->stream = NULL;

out:
	config->enable = STORAGE_STATE_DISABLED;
	msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);
}

static void storage_video_major_notify_app(uint32_t param)
{
	storage_config_t *config = (storage_config_t *)param;
	frame_buffer_t *frame = NULL;
	frame_list_node_t *stream = NULL;
	uint8_t retry_time = 5;

	while (config->enable == STORAGE_STATE_ENABLED)
	{
		stream = storage_major_get_target_stream(config->img_format);

		if (stream)
		{
			if (config->stream == NULL)
			{
				LOGI("%s, stream:%p, fmt:%d\n", __func__, stream, config->img_format);
				config->stream = stream;
				frame_buffer_fb_register(config->stream, MODULE_CAPTURE);
			}
			else
			{
				if (config->stream != stream)
				{
					frame_buffer_fb_deregister(config->stream, MODULE_CAPTURE);
					config->stream = NULL;
				}
			}
		}

		if (config->stream == NULL)
		{
			if (stream == NULL)
			{
				retry_time--;
				rtos_delay_milliseconds(1000);
				if (retry_time == 0)
				{
					LOGI("%s %d get stream fail\n", __func__, __LINE__);
					break;
				}
			}
			continue;
		}
	
		frame = frame_buffer_fb_read(config->stream, MODULE_CAPTURE, 100);

		if (frame == NULL)
		{
			frame_list_node_t *node = (frame_list_node_t *)stream;
			LOGE("read frame NULL timeout, stream:%p, %d\n", stream, node->invalid);
			continue;
		}

		if (frame->sequence % 30 == 0)
		{
			LOGI("%s, %d\n", __func__, frame->sequence);
		}

		msg_send_req_to_media_major_mailbox_sync(EVENT_VID_SAVE_ALL_NOTIFY, APP_MODULE, (uint32_t)frame, NULL);

		frame_buffer_fb_read_free(config->stream, frame, MODULE_CAPTURE);
	};

	if (config->stream)
	{
		frame_buffer_fb_deregister(config->stream, MODULE_CAPTURE);
		config->stream = NULL;
	}

	if (config->sem)
	{
		rtos_set_semaphore(&config->sem);
	}
}

static void storage_major_task_entry(beken_thread_arg_t data)
{
	bk_err_t ret = BK_OK;
	storages_task_msg_t msg;

	storage_config_t *config = (storage_config_t *)data;

	rtos_set_semaphore(&config->sem);

	while (1)
	{
		ret = rtos_pop_from_queue(&config->queue, &msg, BEKEN_WAIT_FOREVER);

		if (ret == BK_OK)
		{
			switch (msg.type)
			{
				case STORAGE_TASK_CAPTURE:
					storage_frame_major_notify_app(msg.data);
					break;

				case STORAGE_TASK_SAVE:
					storage_video_major_notify_app(msg.data);
					break;

				case STORAGE_TASK_EXIT:
					goto exit;

				default:
					break;
			}
		}
	}

exit:

	LOGI("storage_major_task exit success!\r\n");
	rtos_deinit_queue(&config->queue);
	config->queue = NULL;
	config->thread = NULL;
	rtos_set_semaphore(&config->sem);
	rtos_delete_thread(NULL);
}

static bk_err_t storage_major_task_init(void)
{
	int ret = BK_FAIL;

	storage_config_t *config = (storage_config_t *)os_malloc(sizeof(storage_config_t));

	if (config == NULL)
	{
		LOGE("%s, malloc failed\r\n", __func__);
		return ret;
	}

	os_memset(config, 0, sizeof(storage_config_t));

	ret = rtos_init_semaphore_ex(&config->sem, 1, 0);

	if (ret != BK_OK)
	{
		LOGE("%s, init semaphore failed\r\n", __func__);
		goto error;
	}

	ret = rtos_init_queue(&config->queue,
								"storage_queue",
								sizeof(storages_task_msg_t),
								5);

	if (BK_OK != ret)
	{
		LOGE("%s storage_queue init failed\n", __func__);
		goto error;
	}

	ret = rtos_create_thread(&config->thread,
							BEKEN_DEFAULT_WORKER_PRIORITY,
							"storage_thread",
							(beken_thread_function_t)storage_major_task_entry,
							1024 * 2,
							(beken_thread_arg_t)config);

	if (BK_OK != ret)
	{
		LOGE("%s storage_major_task_thread init failed\n", __func__);
		ret = BK_ERR_NO_MEM;
		goto error;
	}

	rtos_get_semaphore(&config->sem, BEKEN_NEVER_TIMEOUT);

	s_storage_major_config = config;

	return ret;

error:

	if (config)
	{
		if (config->sem)
		{
			rtos_deinit_semaphore(&config->sem);
		}

		if (config->queue)
		{
			rtos_deinit_queue(&config->queue);
		}
	}

	return ret;
}

static bk_err_t storage_major_task_open_handle(media_mailbox_msg_t *msg)
{
	int ret = BK_FAIL;

	storage_config_t *config = s_storage_major_config;

	if (config)
	{
		LOGI("%s already open\r\n", __func__);
	}
	else
	{
		ret = storage_major_task_init();
	}

	msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);

	LOGI("%s complete\n", __func__);

	return ret;
}

static bk_err_t storage_major_task_close_handle(media_mailbox_msg_t *msg)
{
	int ret = BK_FAIL;

	storage_config_t *config = s_storage_major_config;

	if (config == NULL
		|| config->enable == STORAGE_STATE_ENABLED)
	{
		LOGW("%s already close or is saving...\n", __func__);
	}
	else
	{
		config->enable = STORAGE_STATE_DISABLED;
	
		ret = storage_major_task_send_msg(STORAGE_TASK_EXIT, (uint32_t)msg);

		if (ret != BK_OK)
		{
			LOGE("%s msg send fail\r\n", __func__);
		}
		else
		{
			rtos_get_semaphore(&config->sem, BEKEN_NEVER_TIMEOUT);

			rtos_deinit_semaphore(&config->sem);
			os_free(config);
			s_storage_major_config = NULL;
		}
	}

	msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);

	LOGI("%s complete\n", __func__);

	return ret;
}

static bk_err_t storage_major_capture_handle(media_mailbox_msg_t *msg)
{
	int ret = BK_FAIL;

	storage_config_t *config = s_storage_major_config;

	if (config == NULL
		|| config->enable == STORAGE_STATE_ENABLED)
	{
		LOGW("%s storage major task close or is capturing...\n", __func__);
		msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);
	}
	else
	{
		ret = storage_major_task_send_msg(STORAGE_TASK_CAPTURE, (uint32_t)msg);

		if (ret != BK_OK)
		{
			LOGW("%s msg send fail\r\n", __func__);
			msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);
		}
		else
		{
			config->enable = STORAGE_STATE_ENABLED;
		}
	}

	LOGI("%s %d complete\n", __func__, ret);

	return ret;
}

static bk_err_t storage_major_video_save_start_handle(media_mailbox_msg_t *msg)
{
	int ret = BK_FAIL;

	storage_config_t *config = s_storage_major_config;

	if (config == NULL
		|| config->enable == STORAGE_STATE_ENABLED)
	{
		LOGW("%s storage major task close or is saveing/capture...\n", __func__);
	}
	else
	{
		config->img_format = (uint16_t)msg->param;
	
		ret = storage_major_task_send_msg(STORAGE_TASK_SAVE, (uint32_t)config);

		if (ret != BK_OK)
		{
			LOGW("%s msg send fail\r\n", __func__);
		}
		else
		{
			config->enable = STORAGE_STATE_ENABLED;
		}
	}

	msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);

	LOGI("%s complete\n", __func__);

	return ret;
}

static bk_err_t storage_major_video_save_stop_handle(media_mailbox_msg_t *msg)
{
	int ret = BK_FAIL;

	storage_config_t *config = s_storage_major_config;

	if (config == NULL
		|| config->enable == STORAGE_STATE_DISABLED)
	{
		LOGW("%s storage major task close or saveing already stop...\n", __func__);
	}
	else
	{
		config->enable = STORAGE_STATE_DISABLED;

		rtos_get_semaphore(&config->sem, BEKEN_NEVER_TIMEOUT);

		ret = BK_OK;
	}

	msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);

	LOGI("%s complete\n", __func__);

	return ret;
}

bk_err_t storage_major_event_handle(media_mailbox_msg_t *msg)
{
	int ret = BK_OK;

	switch (msg->event)
	{
		case EVENT_STORAGE_OPEN_IND:
			ret = storage_major_task_open_handle(msg);
			break;

		case EVENT_STORAGE_CLOSE_IND:
			ret = storage_major_task_close_handle(msg);
			break;

		case EVENT_STORAGE_CAPTURE_IND:
			ret = storage_major_capture_handle(msg);
			break;

		case EVENT_STORAGE_SAVE_START_IND:
			ret = storage_major_video_save_start_handle(msg);
			break;

		case EVENT_STORAGE_SAVE_STOP_IND:
			ret = storage_major_video_save_stop_handle(msg);
			break;

		default:
			break;
	}

	return ret;
}

