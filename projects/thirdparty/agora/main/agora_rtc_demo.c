#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <components/system.h>
#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include <components/shell_task.h>
#include <components/event.h>
#include <components/netif_types.h>
#include "bk_rtos_debug.h"
#include "agora_config.h"
#include "agora_rtc.h"
#include "audio_transfer.h"
#include "aud_intf.h"
#include "aud_intf_types.h"
#include <driver/media_types.h>
#include <driver/lcd.h>
#include <modules/wifi.h>
#include "modules/wifi_types.h"
#include "media_app.h"
#include "lcd_act.h"


#if defined(CONFIG_FATFS)
#include "ff.h"
#include "diskio.h"

static bool need_file_close = false;
static bool file_open_flag = false;
static bool start_storage_flag = false;
static int fatfs_state = -1;
#endif


#define TAG "agora_main"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)


static bool g_connected_flag = false;
static char agora_appid[50] = {0};
static bool audio_en = false;
static bool video_en = false;
static media_camera_device_t camera_device = {
#if defined(CONFIG_ENABLE_DUAL_STREAM)
	/* dual stream */
	.num_uvc_dev = 2,
	.dualstream  = 1,
#if defined(CONFIG_ENABLE_VIDEO_H265)
	/* h265 video */
	.d_mode = H265_MODE,
	.d_fmt  = PIXEL_FMT_H265,
#else
	/* h264 video */
	.d_mode = H264_MODE,
	.d_fmt  = PIXEL_FMT_H264,
#endif //#if defined(CONFIG_ENABLE_VIDEO_H265)
	.d_info.resolution.width  = 1920,
	.d_info.resolution.height = 1080,
	.d_info.fps = FPS25,
#else
	/* single stream */
	.num_uvc_dev = 1,
	.dualstream  = 0,
#endif //#if defined(CONFIG_ENABLE_DUAL_STREAM)

#if defined(CONFIG_UVC_CAMERA)
	.type = UVC_CAMERA,
	.mode = JPEG_MODE,
	.fmt  = PIXEL_FMT_JPEG,
	/* expect the width and length */
	.info.resolution.width  = 640,//640,//864,
	.info.resolution.height = 480,
	.info.fps = FPS25,
#elif defined(CONFIG_DVP_CAMERA)
	/* DVP Camera */
	.type = DVP_CAMERA,
	.mode = H264_MODE,
	.fmt  = PIXEL_FMT_H264,
	/* expect the width and length */
	.info.resolution.width  = 640,//1280,
	.info.resolution.height = 480,//720,
	.info.fps = FPS20,
#endif
};

static uint8_t audio_type = 0;
static uint32_t audio_samp_rate = 8000;
static bool aec_enable = false;

static beken_thread_t  agora_thread_hdl = NULL;
static bool agora_runing = false;
static agora_rtc_config_t agora_rtc_config = DEFAULT_AGORA_RTC_CONFIG();
static agora_rtc_option_t agora_rtc_option = DEFAULT_AGORA_RTC_OPTION();

static uint32_t g_target_bps = BANDWIDTH_ESTIMATE_MIN_BITRATE;


#if CONFIG_WIFI_ENABLE
extern void rwnxl_set_video_transfer_flag(uint32_t video_transfer_flag);
#else
#define rwnxl_set_video_transfer_flag(...)
#endif


static void cli_agora_rtc_help(void)
{
	os_printf("agora_test {audio start|stop appid audio_type sampple_rate aec_en} \r\n");
	os_printf("agora_test {video start|stop appid video_type} \r\n");
	os_printf("agora_test {both start|stop appid audio_type sample_rate video_type aec_en} \r\n");
}

#if defined (CONFIG_ENABLE_LCD_DISPLAY)
static int lcd_id = 0;

int _display_turn_on(uint16_t id, uint16_t rotate, uint16_t fmt)
{
	LOGI("%s, id: %d, rotate: %d fmt: %d\n", __func__, id, rotate, fmt);

	const lcd_device_t *device = (const lcd_device_t *)media_app_get_lcd_device_by_id(id);
	if ((uint32_t)device == BK_FAIL || device == NULL) {
		LOGI("%s, could not find device id: %d\n", __func__, id);
		return BK_FAIL;
	}

	lcd_open_t lcd_open = {0};
	lcd_open.device_ppi = device->ppi;
	lcd_open.device_name = device->name;

#if 1// (!CONFIG_SOC_BK7256XX)
    uint8_t rot_angle = 0;
	uint8_t pipeline_enable = true;
	if (pipeline_enable) {
		if (fmt == 0) {
			media_app_lcd_fmt(PIXEL_FMT_RGB565_LE);
		} else if (fmt == 1) {
			media_app_lcd_fmt(PIXEL_FMT_RGB888);
		}

        switch (rotate) {
            case 90:
                rot_angle = ROTATE_90;
                break;
            case 180:
                rot_angle = ROTATE_180;
                break;
            case 270:
                rot_angle = ROTATE_270;
                break;
            case 0:
            default:
                rot_angle = ROTATE_NONE;
                break;
        }
        media_app_pipline_set_rotate(rot_angle);
		media_app_lcd_pipeline_open(&lcd_open);
	} else
#endif
	{
		if (rotate == 90)
			media_app_lcd_rotate(ROTATE_90);

		media_app_lcd_open(&lcd_open);
	}

	lcd_id = id;
	return 0;
}

int _display_turn_off(void)
{
	LOGI("%s id %d", __func__, lcd_id);

#if 1//(!CONFIG_SOC_BK7256XX)
	uint8_t pipeline_enable = true;
	if (pipeline_enable) {
	 	media_app_lcd_pipeline_close();
	} else
#endif
	{
		media_app_lcd_close();
	}

    lcd_id = 0;
	return 0;
}
#endif



static void agora_rtc_user_notify_msg_handle(agora_rtc_msg_t *p_msg)
{
	switch(p_msg->code)
	{
		case AGORA_RTC_MSG_JOIN_CHANNEL_SUCCESS:
			g_connected_flag = true;
			LOGI("Join channel success.\n");
			break;
		case AGORA_RTC_MSG_USER_JOINED:
			LOGI("User Joined.\n");
	#if defined(CONFIG_FATFS)
			/* close sdcard storage */
			start_storage_flag = true;
	#endif
			break;
		case AGORA_RTC_MSG_USER_OFFLINE:
			LOGI("User Offline.\n");
	#if defined(CONFIG_FATFS)
			/* close sdcard storage */
			need_file_close = true;
	#endif
			break;
		case AGORA_RTC_MSG_CONNECTION_LOST:
			LOGE("Lost connection. Please check wifi status.\n");
			g_connected_flag = false;
			break;
		case AGORA_RTC_MSG_INVALID_APP_ID:
			LOGE("Invalid App ID. Please double check.\n");
			break;
		case AGORA_RTC_MSG_INVALID_CHANNEL_NAME:
			LOGE("Invalid channel name. Please double check.\n");
			break;
		case AGORA_RTC_MSG_INVALID_TOKEN:
		case AGORA_RTC_MSG_TOKEN_EXPIRED:
			LOGE("Invalid token. Please double check.\n");
			break;
		case AGORA_RTC_MSG_BWE_TARGET_BITRATE_UPDATE:
			g_target_bps = p_msg->data.bwe.target_bitrate;
			break;
		case AGORA_RTC_MSG_KEY_FRAME_REQUEST:
	#if 0
			media_app_h264_regenerate_idr(camera_device.type);
	#endif
			break;
		default:
			break;
	}
}


static void memory_free_show(void)
{
	uint32_t total_size,free_size,mini_size;

	LOGI("%-5s   %-5s   %-5s   %-5s   %-5s\r\n", "name", "total", "free", "minimum", "peak");

	total_size = rtos_get_total_heap_size();
	free_size  = rtos_get_free_heap_size();
	mini_size  = rtos_get_minimum_free_heap_size();
	LOGI("heap:\t%d\t%d\t%d\t%d\r\n",  total_size, free_size, mini_size, total_size - mini_size);

#if CONFIG_PSRAM_AS_SYS_MEMORY
	total_size = rtos_get_psram_total_heap_size();
	free_size  = rtos_get_psram_free_heap_size();
	mini_size  = rtos_get_psram_minimum_free_heap_size();
	LOGI("psram:\t%d\t%d\t%d\t%d\r\n", total_size, free_size, mini_size, total_size - mini_size);
#endif
}

#if defined(CONFIG_UVC_CAMERA)
#if !defined(CONFIG_ENABLE_DUAL_STREAM)
static void media_checkout_uvc_device_info(bk_uvc_device_brief_info_t *info, uvc_state_t state)
{
	bk_uvc_config_t uvc_config_info_param = {0};
	uint8_t format_index = 0;
	uint8_t frame_num = 0;
	uint8_t index = 0;

	if (state == UVC_CONNECTED) {
		uvc_config_info_param.vendor_id  = info->vendor_id;
		uvc_config_info_param.product_id = info->product_id;

		format_index = info->format_index.mjpeg_format_index;
		frame_num    = info->all_frame.mjpeg_frame_num;
		if (format_index > 0) {
			LOGI("%s uvc_get_param MJPEG format_index:%d\r\n",__func__, format_index);
			for(index = 0; index < frame_num; index++) {
				LOGI("uvc_get_param MJPEG width:%d heigth:%d index:%d\r\n",
						info->all_frame.mjpeg_frame[index].width,
						info->all_frame.mjpeg_frame[index].height,
						info->all_frame.mjpeg_frame[index].index);
				for (int i = 0; i < info->all_frame.mjpeg_frame[index].fps_num; i++) {
					LOGI("uvc_get_param MJPEG fps:%d\r\n", info->all_frame.mjpeg_frame[index].fps[i]);
				}

				if (info->all_frame.mjpeg_frame[index].width == camera_device.info.resolution.width
				      && info->all_frame.mjpeg_frame[index].height == camera_device.info.resolution.height) {
					uvc_config_info_param.frame_index = info->all_frame.mjpeg_frame[index].index;
					uvc_config_info_param.fps         = info->all_frame.mjpeg_frame[index].fps[0];
					uvc_config_info_param.width       = camera_device.info.resolution.width;
					uvc_config_info_param.height      = camera_device.info.resolution.height;
				}
			}
		}

		uvc_config_info_param.format_index = format_index;

		if (media_app_set_uvc_device_param(&uvc_config_info_param) != BK_OK) {
			LOGE("%s, failed\r\n, __func__");
		}
	} else {
		LOGI("%s, %d\r\n", __func__, state);
	}
}
#endif
#endif

#if defined(CONFIG_FATFS)
#define FF_MAX_LFN   255
static FIL fp1;
static void _storage_save_frame(frame_buffer_t *frame)
{
	unsigned int uiTemp = 0;
	FRESULT fr = FR_OK;
	char f_name[50] = { 0 };

	if (!start_storage_flag || fatfs_state == -1) {
		return;
	}

	if (false == file_open_flag) {
		sprintf(f_name, "%d:/%d%s", DISK_NUMBER_SDIO_SD, rtos_get_time(), "test.h264");

		fr = f_open(&fp1, f_name, FA_OPEN_APPEND | FA_WRITE);
		if (fr != FR_OK) {
			LOGE("file %s open failed \r\n", f_name);
			return;
		}
		LOGI("file %s open success \r\n", f_name);

		file_open_flag = true;
	}

	fr = f_write(&fp1, (char *)frame->frame, frame->length, &uiTemp);
	if (fr != FR_OK) {
		LOGE("file write failed \r\n");
		return;
	}

	f_sync(&fp1);

	if (need_file_close) {
		// FILINFO fileInfo = {0};
		// fr = f_stat(f_name, &fileInfo);
		// if (fr == FR_OK) {
			f_close(&fp1);

			file_open_flag = false;
			need_file_close = false;
			start_storage_flag = false;
		// }
	}
}

int _file_mount(DISK_NUMBER number)
{
	int res = 0;

    FRESULT fr;
    char cFileName[FF_MAX_LFN];

    FATFS *pfs = psram_malloc(sizeof(FATFS));
	if(NULL == pfs) {
		LOGE("f_mount malloc failed!\r\n");
		res = -1;
		goto failed_mount;
	}

    sprintf(cFileName, "%d:", number);
    fr = f_mount(pfs, cFileName, 1);
    if (fr != FR_OK) {
        LOGE("f_mount failed:%d\r\n", fr);
		res = -1;
    } else {
        LOGI("f_mount OK!\r\n");
		res = 0;
    }

failed_mount:
	if (pfs) {
		psram_free(pfs);
	}
    LOGI("----- test_mount %d over  -----\r\n\r\n", number);
	return res;
}

void _file_unmount(DISK_NUMBER number)
{
    FRESULT fr;
    char cFileName[FF_MAX_LFN];

    sprintf(cFileName, "%d:", number);
    fr = f_unmount(DISK_NUMBER_SDIO_SD, cFileName, 1);
    if (fr != FR_OK) {
        LOGE("f_unmount failed:%d\r\n", fr);
    } else {
        LOGI("f_unmount OK!\r\n");
    }
    LOGI("----- test_unmount %d over  -----\r\n\r\n", number);
}
#endif


void app_media_read_frame_callback(frame_buffer_t * frame)
{
#if defined (CONFIG_ENABLE_APP_DATA_BACK)
	return;
#endif

	video_frame_info_t info = { 0 };

#if defined(CONFIG_FATFS)
	_storage_save_frame(frame);
#endif

	if (false == g_connected_flag) {
		/* agora rtc is not running, do not send video. */
		return;
	}

	info.stream_type = VIDEO_STREAM_HIGH;
	if (frame->fmt == PIXEL_FMT_JPEG) {
		info.data_type = VIDEO_DATA_TYPE_GENERIC_JPEG;
		info.frame_type = VIDEO_FRAME_KEY;
	} else if (frame->fmt == PIXEL_FMT_H264) {
		info.data_type = VIDEO_DATA_TYPE_H264;
		info.frame_type = VIDEO_FRAME_AUTO_DETECT;
	} else if (frame->fmt == PIXEL_FMT_H265) {
		info.data_type = VIDEO_DATA_TYPE_H265;
		info.frame_type = VIDEO_FRAME_AUTO_DETECT;
	} else {
		LOGE("not support format: %d \r\n", frame->fmt);
	}

	bk_agora_rtc_video_data_send((uint8_t *)frame->frame, (size_t)frame->length, &info);
}

static int agora_rtc_user_audio_rx_data_handle(unsigned char *data, unsigned int size, const audio_frame_info_t *info_ptr)
{
	bk_err_t ret = BK_OK;

	// if ((info_ptr->data_type == AUDIO_DATA_TYPE_PCMA && aud_intf_voc_setup.data_type == AUD_INTF_VOC_DATA_TYPE_G711A)
	// 		|| (info_ptr->data_type == AUDIO_DATA_TYPE_PCMU && aud_intf_voc_setup.data_type == AUD_INTF_VOC_DATA_TYPE_G711U)
	// 		|| (info_ptr->data_type == AUDIO_DATA_TYPE_PCM && aud_intf_voc_setup.data_type == AUD_INTF_VOC_DATA_TYPE_PCM)) {
		/* write a fram speaker data to speaker_ring_buff */
		ret = bk_aud_intf_write_spk_data((uint8_t *)data, (uint32_t)size);
		if (ret != BK_OK) {
			LOGE("write spk data fail \r\n");
		}
	// } else {
	// 	LOGE("audio data type:%d is not match voice data type: %d, size: %d.\n", info_ptr->data_type, aud_intf_voc_setup.data_type, size);
	// 	ret = BK_FAIL;
	// }

	return ret;
}

void agora_main(void)
{
	bk_err_t ret = BK_OK;

	aud_intf_drv_setup_t aud_intf_drv_setup = DEFAULT_AUD_INTF_DRV_SETUP_CONFIG();
	aud_intf_voc_setup_t aud_intf_voc_setup = DEFAULT_AUD_INTF_VOC_SETUP_CONFIG();
	aud_intf_work_mode_t aud_work_mode = AUD_INTF_WORK_MODE_NULL;

	memory_free_show();

	// 2. API: init agora rtc sdk

	//service_opt.license_value[0] = '\0';
	agora_rtc_config.p_appid = (char *)psram_malloc(strlen(agora_appid) + 1);
	os_strcpy((char *)agora_rtc_config.p_appid, agora_appid);
	agora_rtc_config.log_disable = false;
	agora_rtc_config.bwe_param_max_bps = BANDWIDTH_ESTIMATE_MAX_BITRATE;
	ret = bk_agora_rtc_create(&agora_rtc_config, (agora_rtc_msg_notify_cb)agora_rtc_user_notify_msg_handle);
	if (ret != BK_OK) {
		LOGI("bk_agora_rtc_create fail \r\n");
	}
	// LOGI("-----start agora rtc process-----\r\n");

	if (audio_en) {
		audio_tras_init();

		aud_intf_drv_setup.aud_intf_tx_mic_data = send_audio_data_to_agora;
		ret = bk_aud_intf_drv_init(&aud_intf_drv_setup);
		if (ret != BK_ERR_AUD_INTF_OK) {
			LOGE("bk_aud_intf_drv_init fail, ret:%d \r\n", ret);
		} else {
			LOGI("bk_aud_intf_drv_init complete \r\n");
		}

		aud_work_mode = AUD_INTF_WORK_MODE_VOICE;
		ret = bk_aud_intf_set_mode(aud_work_mode);
		if (ret != BK_ERR_AUD_INTF_OK) {
			LOGE("bk_aud_intf_set_mode fail, ret:%d \r\n", ret);
		} else {
			LOGI("bk_aud_intf_set_mode complete \r\n");
		}

		aud_intf_voc_setup.data_type  = AUD_INTF_VOC_DATA_TYPE_G711U;
		// aud_intf_voc_setup.data_type  = AUD_INTF_VOC_DATA_TYPE_PCM;
		aud_intf_voc_setup.spk_mode   = AUD_DAC_WORK_MODE_SIGNAL_END;
		aud_intf_voc_setup.aec_enable = aec_enable;
		aud_intf_voc_setup.samp_rate  = audio_samp_rate;
		if (audio_type == 1) {
			aud_intf_voc_setup.mic_type = AUD_INTF_MIC_TYPE_UAC;
			aud_intf_voc_setup.spk_type = AUD_INTF_MIC_TYPE_UAC;
		} else {
			aud_intf_voc_setup.mic_type = AUD_INTF_MIC_TYPE_BOARD;
			aud_intf_voc_setup.spk_type = AUD_INTF_MIC_TYPE_BOARD;
		}

		ret = bk_aud_intf_voc_init(aud_intf_voc_setup);
		if (ret != BK_ERR_AUD_INTF_OK) {
			LOGE("bk_aud_intf_voc_init fail, ret:%d \r\n", ret);
		} else {
			LOGI("bk_aud_intf_voc_init complete \r\n");
		}

		ret = bk_aggora_rtc_register_audio_rx_handle((agora_rtc_audio_rx_data_handle)agora_rtc_user_audio_rx_data_handle);
		if (ret != BK_OK) {
			LOGE("bk_aggora_rtc_register_audio_rx_handle fail, ret:%d \r\n", ret);
		} else {
			LOGI("bk_aggora_rtc_register_audio_rx_handle complete \r\n");
		}
	}

	agora_rtc_option.p_channel_name = (char *)psram_malloc(os_strlen(CONFIG_CHANNEL_NAME) + 1);
	os_strcpy((char *)agora_rtc_option.p_channel_name, CONFIG_CHANNEL_NAME);
	agora_rtc_option.audio_config.audio_data_type = CONFIG_AUDIO_CODEC_TYPE;
#if defined(CONFIG_SEND_PCM_DATA)
	agora_rtc_option.audio_config.pcm_sample_rate = CONFIG_PCM_SAMPLE_RATE;
	agora_rtc_option.audio_config.pcm_channel_num = CONFIG_PCM_CHANNEL_NUM;
#endif
    agora_rtc_option.p_token = CONFIG_AGORA_TOKEN;
    agora_rtc_option.uid = CONFIG_AGORA_UID;

	ret = bk_agora_rtc_start(&agora_rtc_option);
	if (ret != BK_OK) {
		LOGE("bk_agora_rtc_start fail, ret:%d \r\n", ret);
		return;
	}


#if defined (CONFIG_FATFS)
	LOGE("fs mount for %d\r\n", DISK_NUMBER_SDIO_SD);
	fatfs_state = _file_mount(DISK_NUMBER_SDIO_SD);
#endif


	// LOGI("-----agora_rtc_join_channel start-----\r\n");
	// 5. wait until we join channel successfully
	while (!g_connected_flag) {
		// memory_free_show();
		rtos_dump_task_runtime_stats();
		rtos_delay_milliseconds(1000);
	}

	LOGI("-----agora_rtc_join_channel success-----\r\n");
	agora_runing = true;

	if (audio_en) {
		LOGI("audio start \r\n");

		ret = bk_aud_intf_voc_start();
		if (ret != BK_ERR_AUD_INTF_OK) {
			LOGE("bk_aud_intf_voc_start fail, ret:%d \r\n", ret);
		} else {
			LOGI("bk_aud_intf_voc_start complete \r\n");
		}

		memory_free_show();
	}

	if (video_en) {
		bk_wifi_set_wifi_media_mode(true);
		bk_wifi_set_video_quality(WIFI_VIDEO_QUALITY_FD);

		rwnxl_set_video_transfer_flag(true);

#if defined(CONFIG_UVC_CAMERA)
	#if defined(CONFIG_ENABLE_DUAL_STREAM)
	#else
		media_app_uvc_register_info_notify_cb(media_checkout_uvc_device_info);
	#endif
#endif

	#if defined (CONFIG_ENABLE_APP_DATA_BACK) /* receive app video and send back app video to app */
		bk_aggora_rtc_register_video_rx_handle((agora_rtc_video_rx_data_handle)bk_agora_rtc_video_data_send);
	#endif

#if 1
		ret = media_app_camera_open(&camera_device);
		if (ret != BK_OK) {
			LOGE("%s media_app_camera_open failed\n", __func__);
			return;
		}

		bool media_mode = false;
		uint8_t quality = 0;
		bk_wifi_get_wifi_media_mode_config(&media_mode);
		bk_wifi_get_video_quality_config(&quality);
		LOGE("~~~~~~~~~~wifi media mode %d, video quality %d~~~~~~\r\n", media_mode, quality);

#if defined(CONFIG_UVC_CAMERA)

	#if defined(CONFIG_ENABLE_DUAL_STREAM)
		ret = media_app_register_read_frame_callback(camera_device.d_fmt, app_media_read_frame_callback);
		if (ret != BK_OK) {
			LOGE("%s register read_frame_cb failed\n", __func__);
			return;
		}
	#else
		ret = media_app_h264_pipeline_open();
		if (ret != BK_OK) {
			LOGE("%s h264_pipeline_open failed\n", __func__);
			return;
		}

		ret = media_app_register_read_frame_callback(PIXEL_FMT_H264, app_media_read_frame_callback);
		if (ret != BK_OK) {
			LOGE("%s register read_frame_cb failed\n", __func__);
			return;
		}

	#endif
#elif defined(CONFIG_DVP_CAMERA)
		ret = media_app_register_read_frame_callback(camera_device.fmt, app_media_read_frame_callback);
		if (ret != BK_OK) {
			LOGE("%s register read_frame_cb failed\n", __func__);
			return;
		}
#endif
		memory_free_show();

	/* lcd display open */
	#if defined (CONFIG_ENABLE_LCD_DISPLAY)
		_display_turn_on(LCD_DEVICE_ST7701S, 0, 1);
	#endif

#endif
	}

	while (agora_runing) {
		rtos_delay_milliseconds(5000);
		// memory_free_show();
		// rtos_dump_task_runtime_stats();
	}

	/* free audio  */
	if (audio_en) {
		bk_aggora_rtc_register_audio_rx_handle(NULL);

		/* stop voice */
		ret = bk_aud_intf_voc_stop();
		if (ret != BK_ERR_AUD_INTF_OK) {
			LOGE("bk_aud_intf_voc_stop fail, ret:%d \r\n", ret);
		} else {
			LOGI("bk_aud_intf_voc_stop complete \r\n");
		}

		/* deinit vioce */
		ret = bk_aud_intf_voc_deinit();
		if (ret != BK_ERR_AUD_INTF_OK) {
			LOGE("bk_aud_intf_voc_deinit fail, ret:%d \r\n", ret);
		} else {
			LOGI("bk_aud_intf_voc_deinit complete \r\n");
		}

		/* deinit audio */
		aud_work_mode = AUD_INTF_WORK_MODE_NULL;
		bk_aud_intf_set_mode(aud_work_mode);

		ret = bk_aud_intf_drv_deinit();
		if (ret != BK_ERR_AUD_INTF_OK) {
			LOGE("bk_aud_intf_drv_deinit fail, ret:%d \r\n", ret);
		} else {
			LOGI("bk_aud_intf_drv_deinit complete \r\n");
		}
	}

	/* free audio  */
	if (video_en) {
		ret = media_app_unregister_read_frame_callback();
		if (ret != BK_OK) {
			LOGE("%s unregister read_frame_cb failed\n", __func__);
			return;
		}

	#if defined(CONFIG_ENABLE_DUAL_STREAM)
	#else
		ret = media_app_h264_pipeline_close();
		if (ret != BK_OK) {
			LOGE("%s h264_pipeline_close failed\n", __func__);
			return;
		}
    #endif

		ret = media_app_camera_close(camera_device.type);
		if (ret != BK_OK) {
			LOGE("%s media_app_camera_close failed\n", __func__);
			return;
		}

		rwnxl_set_video_transfer_flag(false);

		bk_wifi_set_wifi_media_mode(false);
		bk_wifi_set_video_quality(WIFI_VIDEO_QUALITY_HD);
	}

#if defined (CONFIG_FATFS)
	LOGE("fs unmount for %d\r\n", DISK_NUMBER_SDIO_SD);
	_file_unmount(DISK_NUMBER_SDIO_SD);
#endif

	/* free agora */
	/* stop agora rtc */
	bk_agora_rtc_stop();

	/* destory agora rtc */
	bk_agora_rtc_destroy();

	if (agora_rtc_config.p_appid) {
		psram_free((char *)agora_rtc_config.p_appid);
		agora_rtc_config.p_appid = NULL;
	}

	if (agora_rtc_option.p_channel_name) {
		psram_free((char *)agora_rtc_option.p_channel_name);
		agora_rtc_option.p_channel_name = NULL;
	}

	audio_en = false;
	video_en = false;

	g_connected_flag = false;

	/* delete task */
	agora_thread_hdl = NULL;
	rtos_delete_thread(NULL);
}

void agora_start(void)
{
	bk_err_t ret = BK_OK;

	ret = rtos_create_thread(&agora_thread_hdl,
						 4,
						 "agora",
						 (beken_thread_function_t)agora_main,
						 6*1024,
						 NULL);
	if (ret != kNoErr) {
		LOGE("create agora app task fail \r\n");
		agora_thread_hdl = NULL;
	}
	LOGI("create agora app task complete \r\n");
}


void cli_agora_rtc_test_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	/* audio test */
	if (os_strcmp(argv[1], "audio") == 0) {
		if (os_strcmp(argv[2], "start") == 0) {
			sprintf(agora_appid, "%s", argv[3]);
			audio_type = os_strtoul(argv[4], NULL, 16) & 0xFF;
			audio_samp_rate = os_strtoul(argv[5], NULL, 10);
			aec_enable = os_strtoul(argv[6], NULL, 10);
			LOGI("start agora audio test audio_type %d, samp_rate %d, aec %d\r\n", audio_type, audio_samp_rate, aec_enable);

			audio_en = true;
			video_en = false;
			agora_start();
		} else if (os_strcmp(argv[2], "stop") == 0) {
			//TODO stop audio
			agora_runing = false;
		} else {
			goto cmd_fail;
		}
	} else if (os_strcmp(argv[1], "video") == 0) {
		if (os_strcmp(argv[2], "start") == 0) {
			sprintf(agora_appid, "%s", argv[3]);

			if (os_strcmp(argv[4], "DVP_JPEG") == 0) {
				camera_device.type = DVP_CAMERA;
				camera_device.mode = JPEG_MODE;
				camera_device.fmt  = PIXEL_FMT_JPEG;
			} else if (os_strcmp(argv[4], "DVP_YUV") == 0) {
				camera_device.type = DVP_CAMERA;
				camera_device.mode = YUV_MODE;
				camera_device.fmt  = PIXEL_FMT_YUYV;
			} else if (os_strcmp(argv[4], "DVP_H264") == 0) {
				camera_device.type = DVP_CAMERA;
				camera_device.mode = H264_MODE;
				camera_device.fmt  = PIXEL_FMT_H264;
			} else if (os_strcmp(argv[4], "UVC_MJPEG") == 0) {
				camera_device.type = UVC_CAMERA;
				camera_device.mode = JPEG_MODE;
				camera_device.fmt  = PIXEL_FMT_JPEG;
			} else if (os_strcmp(argv[4], "UVC_H264") == 0) {
				camera_device.type = UVC_CAMERA;
				camera_device.mode = JPEG_MODE;
				camera_device.fmt  = PIXEL_FMT_JPEG;
			} else {
				LOGW("the type is not support \n");
				goto cmd_fail;
			}

			audio_en = false;
			video_en = true;
			agora_start();
		} else if (os_strcmp(argv[2], "stop") == 0) {
			//TODO stop video
			agora_runing = false;
		} else {
			goto cmd_fail;
		}
	} else if (os_strcmp(argv[1], "both") == 0) {
		if (os_strcmp(argv[2], "start") == 0) {
			sprintf(agora_appid, "%s", argv[3]);
			audio_type = os_strtoul(argv[4], NULL, 16) & 0xFF;
			audio_samp_rate = os_strtoul(argv[5], NULL, 10);

			if (os_strcmp(argv[6], "DVP_JPEG") == 0) {
				camera_device.type = DVP_CAMERA;
				camera_device.mode = JPEG_MODE;
				camera_device.fmt  = PIXEL_FMT_JPEG;
			} else if (os_strcmp(argv[6], "DVP_YUV") == 0) {
				camera_device.type = DVP_CAMERA;
				camera_device.mode = YUV_MODE;
				camera_device.fmt  = PIXEL_FMT_YUYV;
			} else if (os_strcmp(argv[6], "DVP_H264") == 0) {
				camera_device.type = DVP_CAMERA;
				camera_device.mode = H264_YUV_MODE;
				camera_device.fmt  = PIXEL_FMT_H264;
			} else if (os_strcmp(argv[6], "UVC_MJPEG") == 0) {
				camera_device.type = UVC_CAMERA;
				camera_device.mode = JPEG_MODE;
				camera_device.fmt  = PIXEL_FMT_JPEG;
			} else if (os_strcmp(argv[6], "UVC_H264") == 0) {
				camera_device.type = UVC_CAMERA;
				camera_device.mode = JPEG_MODE;
				camera_device.fmt  = PIXEL_FMT_JPEG;
			} else {
				LOGW("the type is not support. \n");
				goto cmd_fail;
			}

			aec_enable = os_strtoul(argv[7], NULL, 10);

			audio_en = true;
			video_en = true;
			agora_start();
		} else if (os_strcmp(argv[2], "stop") == 0) {
			//TODO stop video
			agora_runing = false;
		} else {
			goto cmd_fail;
		}
	} else {
		cli_agora_rtc_help();
	}

	return;

cmd_fail:
	cli_agora_rtc_help();
}


void temp_agora_run(void)
{
	sprintf(agora_appid, "%s", CONFIG_AGORA_APP_ID);

	audio_en = true;
	video_en = false;
	agora_start();
}

