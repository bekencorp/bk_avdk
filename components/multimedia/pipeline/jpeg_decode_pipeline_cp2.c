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

#include <driver/jpeg_dec.h>
#include <modules/jpeg_decode_sw.h>
#include <modules/tjpgd.h>
#include <modules/image_scale.h>

#include "media_mailbox_list_util.h"
#include "media_evt.h"
#include "frame_buffer.h"
#include "lcd_decode.h"
#include "yuv_encode.h"

#include "mux_pipeline.h"

#define TAG "jdec_pip_cp2"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

//#define DECODE_DIAG_DEBUG
#ifdef DECODE_DIAG_DEBUG
#define DECODER_FRAME_START() do { GPIO_UP(GPIO_DVP_D1); } while (0)
#define DECODER_FRAME_END() do { GPIO_DOWN(GPIO_DVP_D1); } while (0)
#else
#define DECODER_FRAME_START()
#define DECODER_FRAME_END()
#endif

#define JPEGDEC_BUFFER_LENGTH        (60 * 1024)

static beken_queue_t jpeg_dec_msg_queue;
static beken_thread_t jpeg_dec_th_hd = NULL;
static beken_semaphore_t jpeg_dec_sem;
static uint8_t jpeg_dec_th_hd_status = 0;
static media_mailbox_msg_t jpeg_dec_to_media_major_msg = {0};
static uint8_t *jpeg_rotate_buffer = NULL;
static uint32_t jpeg_rotate_angle = ROTATE_NONE;

bk_err_t jpeg_dec_task_send_msg(uint32_t type, uint32_t param)
{
	int ret = BK_OK;
	media_msg_t msg;

	if (jpeg_dec_msg_queue)
	{
		msg.event = type;
		msg.param = param;

		ret = rtos_push_to_queue(&jpeg_dec_msg_queue, &msg, BEKEN_WAIT_FOREVER);

		if (ret != BK_OK)
		{
			LOGE("%s push failed\n", __func__);
		}
	}
	return ret;
}

void jpeg_dec_set_rotate_angle(media_rotate_t rotate_angle)
{
	jpeg_dec_task_send_msg(EVENT_JPEG_DEC_SET_ROTATE_ANGLE_NOTIFY, rotate_angle);
}

static void jpeg_dec_start_handle(void *paramters)
{
	media_mailbox_msg_t *media_msg = (media_mailbox_msg_t *)paramters;
	media_software_decode_info_t *sw_dec_info = NULL;
	sw_jpeg_dec_res_t result;
	bk_err_t ret = BK_OK;
	if (media_msg == NULL)
	{
		LOGI("%s %d param error\r\n", __func__, __LINE__);
		return;
	}
	sw_dec_info = (media_software_decode_info_t *)media_msg->param;

	if (sw_dec_info == NULL || sw_dec_info->in_frame == NULL || sw_dec_info->out_frame == NULL
		|| sw_dec_info->in_frame->frame == NULL || sw_dec_info->out_frame->frame == NULL)
	{
		jpeg_dec_to_media_major_msg.event = EVENT_JPEG_DEC_START_COMPLETE_NOTIFY;
		jpeg_dec_to_media_major_msg.result = BK_ERR_PARAM;
		LOGI("%s %d param error\r\n", __func__, __LINE__);
		msg_send_notify_to_media_minor_mailbox(&jpeg_dec_to_media_major_msg, MAJOR_MODULE);
		return;
	}

	jd_set_rotate(jpeg_rotate_angle, jpeg_rotate_buffer);
	if (jpeg_rotate_angle == ROTATE_90 || jpeg_rotate_angle == ROTATE_270)
	{
		sw_dec_info->out_frame->width = sw_dec_info->in_frame->height;
		sw_dec_info->out_frame->height = sw_dec_info->in_frame->width;
	}
	DECODER_FRAME_START();
	ret = bk_jpeg_dec_sw_start(JPEGDEC_BY_FRAME, sw_dec_info->in_frame->frame, sw_dec_info->out_frame->frame,
		sw_dec_info->in_frame->length, JPEGDEC_BUFFER_LENGTH, &result);
	DECODER_FRAME_END();
	jpeg_dec_to_media_major_msg.event = EVENT_JPEG_DEC_START_COMPLETE_NOTIFY;
	jpeg_dec_to_media_major_msg.result = ret;
	msg_send_notify_to_media_minor_mailbox(&jpeg_dec_to_media_major_msg, MAJOR_MODULE);
}

static void jpeg_dec_task_deinit(void)
{
    LOGI("%s\r\n", __func__);
    if (jpeg_dec_msg_queue)
    {
        rtos_deinit_queue(&jpeg_dec_msg_queue);
        jpeg_dec_msg_queue = NULL;
    }
    if (jpeg_dec_sem)
    {
        rtos_deinit_semaphore(&jpeg_dec_sem);
        jpeg_dec_sem = NULL;
    }
    if (jpeg_rotate_buffer)
    {
        os_free(jpeg_rotate_buffer);
        jpeg_rotate_buffer = NULL;
    }

    bk_jpeg_dec_sw_deinit();
    jpeg_dec_th_hd_status = 0;
    jpeg_dec_th_hd = NULL;
}

static void jpeg_dec_main(beken_thread_arg_t data)
{
	int ret = BK_OK;
    bk_jpeg_dec_sw_init();
    rtos_set_semaphore(&jpeg_dec_sem);

	while(jpeg_dec_th_hd_status)
	{
		media_msg_t msg;
		ret = rtos_pop_from_queue(&jpeg_dec_msg_queue, &msg, BEKEN_WAIT_FOREVER);

		if (ret == BK_OK)
		{
			switch (msg.event)
			{
				case EVENT_JPEG_DEC_START_NOTIFY:
				{
					jpeg_dec_start_handle((void *)msg.param);
					break;
				}
				case EVENT_JPEG_DEC_SET_ROTATE_ANGLE_NOTIFY:
				{
					jpeg_rotate_angle = msg.param;
				}
				break;

				case JPEGDEC_STOP:
					goto out;

				default:
					break;
			}
		}
	}

out:
    rtos_set_semaphore(&jpeg_dec_sem);
	LOGI("%s, exit\r\n", __func__);
	rtos_delete_thread(NULL);
	//jpeg_decode_task_deinit();
}

bk_err_t jpeg_dec_task_open()
{
	int ret = BK_OK;
	LOGI("%s\r\n", __func__);

	// step 1: check jdec_task state
	if (jpeg_dec_th_hd != NULL && jpeg_dec_msg_queue != NULL)
	{
		LOGE("%s have been opened!\r\n", __func__);
		return ret;
	}
	ret = rtos_init_semaphore(&jpeg_dec_sem, 1);
	if (ret != BK_OK)
	{
		LOGE("%s, init jdec_config->jdec_sem failed\r\n", __func__);
		goto error;
	}

	jpeg_rotate_buffer = os_malloc(16 * 16 * 2);
	if (jpeg_rotate_buffer == NULL)
	{
		LOGE("%s, malloc jpeg_rotate_buffer failed\r\n", __func__);
		goto error;
	}

	ret = rtos_init_queue(&jpeg_dec_msg_queue,
							"jdec_que_cp2",
							sizeof(media_msg_t),
							15);

	if (ret != BK_OK)
	{
		LOGE("%s, init jdec_queue failed\r\n", __func__);
		goto error;
	}

	ret = rtos_create_thread(&jpeg_dec_th_hd,
							BEKEN_DEFAULT_WORKER_PRIORITY,
							"jdec_task_cp2",
							(beken_thread_function_t)jpeg_dec_main,
							1024 * 2,
							NULL);

	if (ret != BK_OK)
	{
		LOGE("%s, init jdec_task failed\r\n", __func__);
		goto error;
	}

	rtos_get_semaphore(&jpeg_dec_sem, BEKEN_NEVER_TIMEOUT);

    jpeg_dec_th_hd_status = 1;

	return ret;

error:

	LOGE("%s, open failed\r\n", __func__);

	jpeg_dec_task_deinit();

	return ret;
}

bk_err_t jpeg_dec_task_close()
{
	LOGI("%s  %d\n", __func__, __LINE__);

	if (jpeg_dec_th_hd == NULL && jpeg_dec_msg_queue == NULL)
	{
		LOGI("%s %d\n", __func__, __LINE__);
		return BK_FAIL;
	}
    jpeg_dec_th_hd_status = 0;
	jpeg_dec_task_send_msg(JPEGDEC_STOP, 0);
	rtos_get_semaphore(&jpeg_dec_sem, BEKEN_NEVER_TIMEOUT);
	jpeg_dec_task_deinit();

	LOGI("%s complete, %d\n", __func__, __LINE__);

	return BK_OK;
}

