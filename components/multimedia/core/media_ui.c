// Copyright 2020-2021 Beken
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <os/os.h>
#include <os/mem.h>
#include <components/log.h>

#include "media_core.h"
#include "media_evt.h"
#include "camera_act.h"
#include "img_service.h"
#include "storage_act.h"
#include "transfer_act.h"
#include "frame_buffer.h"
#include "media_mailbox_list_util.h"
#include "bt_audio_act.h"
#include "uvc_pipeline_act.h"
#include <driver/timer.h>
#include <components/system.h>
#include <driver/pwr_clk.h>
#include "yuv_encode.h"

#include "lcd_display_service.h"

#if (CONFIG_CACHE_ENABLE)
#include "cache.h"
#endif


#define TAG "media_ui"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

#define DEBUG_INTERVAL (2)

media_debug_t *media_debug = NULL;
media_debug_t *media_debug_cached = NULL;
static beken_thread_t media_ui_task = NULL;
static beken_queue_t media_ui_msg_que = NULL;
beken_timer_t media_debug_timer = {0};

media_share_ptr_t media_share = {0};


bk_err_t media_send_msg(media_msg_t *msg)
{
	int ret = BK_FAIL;

	if (media_ui_msg_que)
	{
		ret = rtos_push_to_queue(&media_ui_msg_que, msg, BEKEN_NO_WAIT);
	}

	return ret;
}

static void media_debug_dump(void)
{
#if (CONFIG_CACHE_ENABLE)
	flush_dcache(media_debug, sizeof(media_debug_t));
#endif

#if (CONFIG_MEDIA_DISPLAY_SERVICE)
//	if (!check_lcd_task_is_open())
//		return;
#endif

	uint16_t h264 = (media_debug->isr_h264 - media_debug_cached->isr_h264) / DEBUG_INTERVAL;
	uint16_t dec = (media_debug->isr_decoder - media_debug_cached->isr_decoder) / DEBUG_INTERVAL;
	uint16_t lcd = (media_debug->isr_lcd - media_debug_cached->isr_lcd) / DEBUG_INTERVAL;
	uint16_t fps = (media_debug->fps_lcd - media_debug_cached->fps_lcd) / DEBUG_INTERVAL;
	uint16_t wifi = (media_debug->fps_wifi - media_debug_cached->fps_wifi) / DEBUG_INTERVAL;
	uint32_t jpeg_kps = (media_debug->jpeg_kbps - media_debug_cached->jpeg_kbps) * 8 / DEBUG_INTERVAL / 1000;
	uint32_t h264_kps = (media_debug->h264_kbps - media_debug_cached->h264_kbps) * 8 / DEBUG_INTERVAL / 1000;
	uint32_t wifi_kps = (media_debug->wifi_kbps - media_debug_cached->wifi_kbps) * 8 / DEBUG_INTERVAL / 1000;
	uint32_t meantimes = (media_debug->meantimes - media_debug_cached->meantimes) / DEBUG_INTERVAL / 1000;
	if (wifi != 0 && meantimes != 0)
	{
		meantimes = meantimes / wifi;
	}
	else
	{
		if (media_debug->begin_trs == false)
			meantimes = 0;
		else
			meantimes = 1000;
	}
	uint16_t lvgl = (media_debug->lvgl_draw - media_debug_cached->lvgl_draw) / DEBUG_INTERVAL;

	media_debug_cached->isr_h264 = media_debug->isr_h264;
	media_debug_cached->isr_decoder = media_debug->isr_decoder;
	media_debug_cached->isr_lcd = media_debug->isr_lcd;
	media_debug_cached->fps_lcd = media_debug->fps_lcd;
	media_debug_cached->fps_wifi = media_debug->fps_wifi;
	media_debug_cached->jpeg_kbps = media_debug->jpeg_kbps;
	media_debug_cached->h264_kbps = media_debug->h264_kbps;
	media_debug_cached->wifi_kbps = media_debug->wifi_kbps;
	media_debug_cached->meantimes = media_debug->meantimes;
	media_debug_cached->lvgl_draw = media_debug->lvgl_draw;

	LOGI("h264:%d[%d], dec:%d[%d], lcd:%d[%d], lcd_fps:%d[%d], lvgl:%d[%d]\n",
			h264, media_debug->isr_h264,
			dec, media_debug->isr_decoder,
			lcd, media_debug->isr_lcd,
			fps, media_debug->fps_lcd,
			lvgl, media_debug->lvgl_draw);

	LOGI("wifi:%d[%d, %dkbps, %dms, %d-%d], jpg:%dKB[%dKbps], h264:%dKB[%dKbps]\n",
			wifi, media_debug->fps_wifi, wifi_kps, meantimes, media_debug->begin_trs, media_debug->end_trs,
			media_debug->jpeg_length / 1024, jpeg_kps,
			media_debug->h264_length / 1024, h264_kps);
}

static void media_major_cpu1_free(void)
{
	msg_send_req_to_media_major_mailbox_sync(EVENT_MEDIA_CPU1_POWEROFF_IND, APP_MODULE, 0, NULL);

	bk_pm_module_vote_psram_ctrl(PM_POWER_PSRAM_MODULE_NAME_MEDIA, PM_POWER_MODULE_STATE_OFF);

	bk_pm_cp1_recovery_response(PM_CP1_RECOVERY_CMD, PM_CP1_PREPARE_CLOSE_MODULE_NAME_MEDIA,PM_CP1_MODULE_RECOVERY_STATE_FINISH);
}


static void media_ui_timer_debug_handle(timer_id_t timer_id)
{
	media_msg_t msg;
	msg.event = EVENT_MEDIA_DEBUG_IND;
	msg.param = 0;
	media_send_msg(&msg);
}

static void media_ui_major_common_event_handle(uint32_t event)
{
	switch (event)
	{
		case EVENT_MEDIA_DEBUG_IND:
			media_debug_dump();
			break;

		case EVENT_MEDIA_CPU1_POWEROFF_IND:
			media_major_cpu1_free();
			break;

		default:
			break;
	}
}

static void media_ui_task_main(beken_thread_arg_t data)
{
	int ret = kNoErr;

	msg_send_req_to_media_major_mailbox_sync(EVENT_MEDIA_CPU1_POWERUP_IND, APP_MODULE, (uint32_t)&media_share, NULL);

	rtos_init_timer(&media_debug_timer, DEBUG_INTERVAL * 1000, (timer_handler_t)media_ui_timer_debug_handle, NULL);
	rtos_start_timer(&media_debug_timer);

	while (1)
	{
		media_msg_t msg;
		media_mailbox_msg_t *mb_msg = NULL;

		ret = rtos_pop_from_queue(&media_ui_msg_que, &msg, BEKEN_WAIT_FOREVER);

		if (kNoErr == ret)
		{
			switch (msg.event >> MEDIA_EVT_BIT)
			{
				case CAM_EVENT:
				case FRAME_BUFFER_EVENT:
					mb_msg = (media_mailbox_msg_t *)msg.param;
					camera_event_handle(mb_msg);
					break;

#ifdef CONFIG_LCD
				case LCD_EVENT:
					mb_msg = (media_mailbox_msg_t *)msg.param;
					img_event_handle(mb_msg);

					break;
#endif
#ifdef CONFIG_LVGL
				case LVGL_EVENT:
					mb_msg = (media_mailbox_msg_t *)msg.param;
					extern void lvgl_event_handle(media_mailbox_msg_t *msg);
					lvgl_event_handle(mb_msg);
					break;
#endif

#ifdef CONFIG_WIFI_TRANSFER
				case TRS_EVENT:
					mb_msg = (media_mailbox_msg_t *)msg.param;
					transfer_major_event_handle(mb_msg);
					break;
#endif

#ifdef CONFIG_IMAGE_STORAGE
				case STORAGE_EVENT:
					mb_msg = (media_mailbox_msg_t *)msg.param;
					storage_major_event_handle(mb_msg);
					break;
#endif

#ifdef CONFIG_USB_TRANSFER
				case USB_TRS_EVENT:
					mb_msg = (media_mailbox_msg_t *)msg.param;
					usb_major_event_handle(mb_msg);
					break;
#endif

				case QUEUE_EVENT:
					break;

#if CONFIG_MEDIA_BT_AUDIO
				case BT_EVENT:
					mb_msg = (media_mailbox_msg_t *)msg.param;
					bt_audio_event_handle(mb_msg);
					break;
#endif

				case MAJOR_COMM_EVENT:
					media_ui_major_common_event_handle(msg.event);
					break;

				case EXIT_EVENT:
					goto exit;
					break;

				default:
					LOGD("mb_msg:%p\n", mb_msg);
					break;
			}
		}
	}

exit:

	rtos_stop_timer(&media_debug_timer);
	rtos_deinit_timer(&media_debug_timer);

	if (media_debug)
	{
		os_free(media_debug);
		media_debug = NULL;
	}

	if (media_debug_cached)
	{
		os_free(media_debug_cached);
		media_debug_cached = NULL;
	}

	rtos_deinit_queue(&media_ui_msg_que);
	media_ui_msg_que = NULL;

	media_ui_task = NULL;
	rtos_delete_thread(NULL);
}


bk_err_t media_ui_task_init(void)
{
	int ret = kNoErr;

	if ((!media_ui_task) && (!media_ui_msg_que))
	{
		ret = rtos_init_queue(&media_ui_msg_que,
								"media_ui_msg_que",
								sizeof(media_msg_t),
								20);
		if (kNoErr != ret)
		{
			LOGE("media_ui_msg_que init failed\n");
			ret = kNoMemoryErr;
			goto error;
		}

		ret = rtos_create_sram_thread(&media_ui_task,
								BEKEN_DEFAULT_WORKER_PRIORITY,
								"media_ui_task",
								(beken_thread_function_t)media_ui_task_main,
								CONFIG_MEDIA_UI_TASK_STACK_SIZE,
								NULL);

		if (kNoErr != ret)
		{
			LOGE("media_ui_task init failed\n");
			ret = kNoMemoryErr;
			goto error;
		}
	}

	return ret;

error:

	rtos_stop_timer(&media_debug_timer);
	rtos_deinit_timer(&media_debug_timer);

	if (media_ui_msg_que)
	{
		rtos_deinit_queue(&media_ui_msg_que);
		media_ui_msg_que = NULL;
	}

	if (media_ui_task)
	{
		media_ui_task = NULL;
	}

	return ret;
}

static bk_err_t media_free_cpu1_handle(void *param)
{
	media_msg_t msg;
	msg.event = EVENT_MEDIA_CPU1_POWEROFF_IND;
	msg.param = 0;
	media_send_msg(&msg);

	return 0;
}

void media_ui_init()
{
	bk_pm_cp1_recovery_response(PM_CP1_RECOVERY_CMD, PM_CP1_PREPARE_CLOSE_MODULE_NAME_MEDIA,PM_CP1_MODULE_RECOVERY_STATE_INIT);
	bk_pm_module_vote_psram_ctrl(PM_POWER_PSRAM_MODULE_NAME_MEDIA, PM_POWER_MODULE_STATE_ON);
	stop_cpu1_register_notification(media_free_cpu1_handle, NULL);
	frame_buffer_list_init();

#ifdef CONFIG_WIFI_TRANSFER
	transfer_init();
#endif

#ifdef CONFIG_LCD
    lcd_display_service_init();
#endif

#ifdef CONFIG_MEDIA_PIPELINE
	uvc_pipeline_init();
#endif

	if (media_debug == NULL)
	{
		media_debug = (media_debug_t *)os_malloc(sizeof(media_debug_t));

		if (media_debug == NULL)
		{
			LOGE("malloc media_debug fail\n");
		}

		media_share.media_debug = media_debug;
	}

	if (media_debug_cached == NULL)
	{
		media_debug_cached = (media_debug_t *)os_malloc(sizeof(media_debug_t));
		if (media_debug_cached == NULL)
		{
			LOGE("malloc media_debug_cached fail\n");
		}
	}

	os_memset(media_debug, 0, sizeof(media_debug_t));
	os_memset(media_debug_cached, 0, sizeof(media_debug_t));

	media_ui_task_init();
}

void media_ui_deinit()
{
	media_msg_t msg;

	msg.event = EXIT_EVENT << MEDIA_EVT_BIT;

	media_send_msg(&msg);

	frame_buffer_list_deinit();

	if (media_debug)
	{
		os_free(media_debug);
		media_debug = NULL;
	}

	if (media_debug_cached)
	{
		os_free(media_debug_cached);
		media_debug_cached = NULL;
	}
}

