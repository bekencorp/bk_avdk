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
#include <os/str.h>
#include <os/mem.h>
#include <components/log.h>
#include <common/bk_include.h>
#include <driver/int.h>
#include <driver/dvp_camera_types.h>
#include <driver/pwr_clk.h>

#include "media_core.h"
#include "media_evt.h"
#include "media_app.h"
#include "transfer_act.h"
#include "camera_act.h"
#include "img_service.h"
#include "camera_handle_list.h"

#include "storage_act.h"

#include "media_mailbox_list_util.h"

#ifdef CONFIG_BT_REUSE_MEDIA_MEMORY
#include "components/bluetooth/bk_ble.h"
#include "components/bluetooth/bk_dm_ble.h"
#include "components/bluetooth/bk_dm_bluetooth.h"
#endif
#include "frame_buffer.h"

#define TAG "media_app"

#define LOGI(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

static media_modules_state_t *media_modules_state = NULL;

#if CONFIG_CHERRY_USB && CONFIG_USB_DEVICE
extern void usbd_video_h264_init();
extern void usbd_video_h264_deinit();
#endif

static beken_thread_t media_app_th_hd = NULL;
static beken_queue_t media_app_msg_queue = NULL;
static camera_state_cb_t uvc_device_info_cb = NULL;
static compress_ratio_t compress_factor = {0};

typedef struct {
	media_rotate_t rotate;
} jpeg_decode_pipeline_param_t;

typedef struct {
    uint32_t param1;
    uint32_t param2;
} media_device_t;

static jpeg_decode_pipeline_param_t jpeg_decode_pipeline_param = {0};

bk_err_t media_send_msg_sync(uint32_t event, uint32_t param)
{
	int ret = BK_FAIL;
	ret = msg_send_req_to_media_app_mailbox_sync(event, param, NULL);
	if (ret != BK_OK)
	{
		LOGE("%s failed 0x%x\n", __func__, ret);
	}
	return ret;
}

uint32_t media_send_msg_sync_return_param(uint32_t event, uint32_t in_param, uint32_t *out_param)
{
	int ret = BK_FAIL;

	ret = msg_send_req_to_media_app_mailbox_sync(event, in_param, out_param);
	if (ret != BK_OK)
	{
		LOGE("%s failed 0x%x\n", __func__, ret);
	}

	return ret;
}

bk_err_t media_app_camera_open(camera_handle_t *handle, media_camera_device_t *device)
{
	int ret = BK_FAIL;

	LOGI("%s, type:%d, id:%d, W*H:%d*%d, format:%d\n",
		__func__, device->type, device->port, device->width,
		device->height, device->format);

	if (device == NULL || device->port >= CAMERA_MAX_NUM)
	{
		LOGE("%s, device (%p) null or port over valied range\n", __func__, device);
		return ret;
	}

	camera_handle_t tmp = bk_camera_handle_node_get_by_id_and_fomat(device->port, device->format);
	if (tmp)
	{
		ret = BK_OK;
		LOGI("%s already opened, %p\n", __func__, tmp);
		*handle = tmp;
		return ret;
	}

#ifdef CONFIG_BT_REUSE_MEDIA_MEMORY
#if CONFIG_BLUETOOTH
	bk_bluetooth_deinit();
#endif
#endif

	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_VIDP_JPEG_EN, PM_POWER_MODULE_STATE_ON);

	media_device_t media_device = {0};
	media_device.param1 = (uint32_t)handle;
	media_device.param2 = (uint32_t)device;

	ret = media_send_msg_sync(EVENT_CAM_OPEN_IND, (uint32_t)&media_device);

	if (ret == BK_OK)
	{
		media_camera_node_t *node = bk_camera_handle_node_init(device->port, device->format);
		if (node == NULL)
		{
			LOGE("%s, %d\n", __func__, __LINE__);
		}
		else
		{
			node->cam_handle = *handle;
		}
	}
	else
	{
		if (list_empty(&media_modules_state->cam_list))
		{
			bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_VIDP_JPEG_EN, PM_POWER_MODULE_STATE_OFF);
		}
	}

	LOGI("%s complete\n", __func__);

	return ret;
}

bk_err_t media_app_camera_close(camera_handle_t *handle)
{
	int ret = BK_FAIL;

	if (*handle == NULL)
	{
		LOGI("%s already closed\n", __func__);
		return BK_OK;
	}

	camera_config_t *config = (camera_config_t *)*handle;

	camera_handle_t tmp = bk_camera_handle_node_get_by_id_and_fomat(config->id, config->image_format);
	if (tmp == NULL)
	{
		LOGI("%s already closed\n", __func__);
		return BK_OK;
	}

	ret = media_send_msg_sync(EVENT_CAM_CLOSE_IND, (uint32_t)handle);

	if (ret == BK_OK)
	{
		bk_camera_handle_node_deinit(tmp);
	}

	if (list_empty(&media_modules_state->cam_list))
	{
		bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_VIDP_JPEG_EN, PM_POWER_MODULE_STATE_OFF);
	}

	LOGI("%s complete %d\n", __func__, ret);

	return ret;
}

bk_err_t media_app_switch_main_camera(uint16_t id, camera_type_t type, image_format_t format)
{
    int ret = BK_FAIL;

    camera_handle_t tmp = bk_camera_handle_node_get_by_id_and_fomat(id, format);
    if (tmp == NULL)
    {
        LOGI("%s camera not open\n", __func__);
        return ret;
    }

    camera_config_t config = {0};

    config.id = id;
    config.type = type;
    config.image_format = format;

    ret = media_send_msg_sync(EVENT_CAM_SWITCH_MAIN_IND, (uint32_t)&config);

    return ret;
}


bk_err_t media_app_get_main_camera_stream(frame_list_node_t *node)
{
    int ret = BK_FAIL;

    if (list_empty(&media_modules_state->cam_list))
    {
        LOGI("%s camera not open!\n", __func__);
        return ret;
    }
    ret = media_send_msg_sync(EVENT_CAM_GET_MAIN_STREAM_IND, (uint32_t)node);

    LOGI("%s complete\n", __func__);

    return ret;
}



bk_err_t media_app_get_h264_encode_config(h264_base_config_t *config)
{
    int ret = BK_FAIL;

    if (list_empty(&media_modules_state->cam_list))
    {
        LOGI("%s camera not open!\n", __func__);
        return ret;
    }

    ret = media_send_msg_sync(EVENT_CAM_GET_H264_INFO_IND, (uint32_t)config);

    LOGI("%s complete\n", __func__);

    return ret;
}

bk_err_t media_app_uvc_register_info_notify_cb(camera_state_cb_t cb)
{
	int ret = BK_FAIL;

	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_VIDP_JPEG_EN, PM_POWER_MODULE_STATE_ON);

	uvc_device_info_cb = cb;

	ret = media_send_msg_sync(EVENT_CAM_REG_UVC_INFO_CB_IND, 0);

	return ret;
}

bk_err_t media_app_set_uvc_device_param(uvc_config_t *config)
{
	int ret = BK_FAIL;

	if (config == NULL)
	{
		LOGI("%s param not init\n", __func__);
		return ret;
	}

	if (list_empty(&media_modules_state->cam_list))
	{
		LOGE("%s uvc not open\n", __func__);
		return ret;
	}

	ret = media_send_msg_sync(EVENT_CAM_SET_UVC_PARAM_IND, (uint32_t)config);

	return ret;
}

bk_err_t media_app_set_compression_ratio(compress_ratio_t *ratio)
{
	int ret = BK_FAIL;

	os_memcpy(&compress_factor, ratio, sizeof(compress_ratio_t));

	if (list_empty(&media_modules_state->cam_list))
	{
		LOGE("%s camera not open\n", __func__);
		return ret;
	}

	ret = media_send_msg_sync(EVENT_CAM_COMPRESS_IND, (uint32_t)ratio);

	LOGI("%s complete\n", __func__);

	return ret;
}

bk_err_t media_app_pipeline_h264_open(void)
{
	int ret = BK_OK;

#ifdef CONFIG_BT_REUSE_MEDIA_MEMORY
#if CONFIG_BLUETOOTH
	bk_bluetooth_deinit();
#endif
#endif

	ret = media_send_msg_sync(EVENT_PIPELINE_SET_ROTATE_IND, jpeg_decode_pipeline_param.rotate);

	LOGD("%s set rotate %x\n", __func__, ret);

	ret = media_send_msg_sync(EVENT_PIPELINE_H264_OPEN_IND, 0);

	LOGI("%s complete %x\n", __func__, ret);

	return ret;
}

bk_err_t media_app_pipeline_h264_close(void)
{
	int ret = BK_OK;
	ret = media_send_msg_sync(EVENT_PIPELINE_H264_CLOSE_IND, 0);

	LOGI("%s complete %x\n", __func__, ret);

	return ret;
}

bk_err_t media_app_h264_regenerate_idr(camera_type_t type)
{
	int ret = BK_FAIL;

	if (list_empty(&media_modules_state->cam_list))
	{
		LOGE("%s camera not open\n", __func__);
		return ret;
	}

	if (type == UVC_CAMERA)
	{
		ret = media_send_msg_sync(EVENT_PIPELINE_H264_RESET_IND, 0);
	}
	else
	{
		ret = media_send_msg_sync(EVENT_CAM_H264_RESET_IND, 0);
	}

	LOGI("%s complete %x\n", __func__, ret);

	return ret;
}

bk_err_t media_app_lcd_fmt(pixel_format_t fmt)
{
	bk_err_t ret;

	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_VIDP_LCD, PM_POWER_MODULE_STATE_ON);
	ret = media_send_msg_sync(EVENT_LCD_SET_FMT_IND, fmt);

	LOGI("%s complete\n", __func__);

	return ret;
}

bk_err_t media_app_set_rotate(media_rotate_t rotate)
{
	int ret = BK_OK;

	jpeg_decode_pipeline_param.rotate = rotate;
	LOGI("%s %d %d(0:0, 1:90, 2:180,3:270)\n", __func__, __LINE__, rotate);
	return ret;
}

bk_err_t media_app_pipeline_jdec_disp_open(void *config)
{
    int ret = BK_OK;

    ret = media_app_lcd_disp_open(config);
    if (ret != BK_OK) {
        LOGE("%s media_app_lcd_disp_open fail\n", __func__);
        return ret;
    }

    ret = media_app_pipeline_jdec_open();

    return ret;
}

bk_err_t media_app_pipeline_jdec_disp_close(void)
{
    int ret = BK_OK;

    ret = media_app_pipeline_jdec_close();
    if (ret != BK_OK) {
        LOGE("%s media_app_pipeline_jdec_close fail\n", __func__);
        return ret;
    }

    ret = media_app_lcd_disp_close();
    if (ret != BK_OK) {
        LOGE("%s media_app_lcd_disp_close fail\n", __func__);
        return ret;
    }

    return ret;
}

bk_err_t media_app_lcd_disp_open(void *config)
{
	int ret = BK_OK;

	if (config == NULL) {
		LOGE("malloc lcd_open_t failed\r\n");
		return BK_FAIL;
	}

	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_VIDP_LCD, PM_POWER_MODULE_STATE_ON);

	ret = media_send_msg_sync(EVENT_LCD_DISP_OPEN_IND, (uint32_t)config);

	LOGI("%s complete %x\n", __func__, ret);

	return ret;
}

bk_err_t media_app_lcd_disp_close(void)
{
	int ret = BK_OK;

	ret = media_send_msg_sync(EVENT_PIPELINE_LCD_DISP_CLOSE_IND, 0);

	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_VIDP_LCD, PM_POWER_MODULE_STATE_OFF);

	LOGI("%s complete %x\n", __func__, ret);

	return ret;
}

bk_err_t media_app_lcd_blend(void *param)
{
    bk_err_t ret = BK_OK;
    ret = media_send_msg_sync(EVENT_IMG_BLEND_IND, (uint32_t)param);

    if (ret != BK_OK)
    {
        LOGE("%s fail\n", __func__);
        return ret;
    }

    return ret;
}


bk_err_t media_app_pipeline_jdec_open(void)
{
    int ret = BK_OK;

#ifdef CONFIG_BT_REUSE_MEDIA_MEMORY
#if (CONFIG_BLUETOOTH)
    bk_bluetooth_deinit();
#endif
#endif

    bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_VIDP_JPEG_DE, PM_POWER_MODULE_STATE_ON);
    ret = media_send_msg_sync(EVENT_PIPELINE_SET_ROTATE_IND, jpeg_decode_pipeline_param.rotate);
    ret = media_send_msg_sync(EVENT_PIPELINE_LCD_JDEC_OPEN_IND, 0);
    LOGI("%s complete %x\n", __func__, ret);

    return ret;
}

bk_err_t media_app_pipeline_jdec_close(void)
{
	int ret = BK_OK;

	ret = media_send_msg_sync(EVENT_PIPELINE_LCD_JDEC_CLOSE_IND, 0);

	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_VIDP_JPEG_DE, PM_POWER_MODULE_STATE_OFF);

	LOGI("%s complete %x\n", __func__, ret);

	return ret;
}


bk_err_t media_app_mailbox_test(void)
{
	bk_err_t ret = BK_FAIL;
	uint32_t param = 0x88888888;

	LOGI("%s +++\n", __func__);

	ret = media_send_msg_sync(EVENT_LCD_DEFAULT_CMD, param);

	LOGI("%s ---\n", __func__);

	return ret;
}

bk_err_t media_app_register_read_frame_callback(image_format_t fmt, frame_cb_t cb)
{
	int ret = BK_OK;

	LOGI("%s\n", __func__);
#ifdef CONFIG_WIFI_TRANSFER
	if (media_modules_state->trs_state == TRS_STATE_ENABLED)
	{
		LOGI("%s, transfer have been opened!\r\n", __func__);
		return ret;
	}

	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_MULTIMEDIA, PM_POWER_MODULE_STATE_ON);

	ret = transfer_app_task_init(cb);
	if (ret != BK_OK)
	{
		return ret;
	}

	ret = media_send_msg_sync(EVENT_TRANSFER_OPEN_IND, (uint32_t)fmt);

	if (ret == BK_OK)
	{
		media_modules_state->trs_state = TRS_STATE_ENABLED;
	}
#endif
	LOGI("%s complete\n", __func__);

	return ret;
}

bk_err_t media_app_transfer_pause(bool pause)
{
	int ret = BK_OK;
#ifdef CONFIG_WIFI_TRANSFER
	ret = media_send_msg_sync(EVENT_TRANSFER_PAUSE_IND, pause);
#endif
	return ret;
}

bk_err_t media_app_unregister_read_frame_callback(void)
{
	bk_err_t ret = BK_OK;
#ifdef CONFIG_WIFI_TRANSFER
	media_mailbox_msg_t mb_msg;

	if (media_modules_state->trs_state == TRS_STATE_DISABLED)
	{
		LOGI("%s, transfer have been closed!\r\n", __func__);
		return ret;
	}

	LOGI("%s\n", __func__);

	ret = media_send_msg_sync(EVENT_TRANSFER_CLOSE_IND, 0);

	mb_msg.event = EVENT_TRANSFER_CLOSE_IND;
	transfer_app_event_handle(&mb_msg);

	if (ret == BK_OK)
	{
		media_modules_state->trs_state = TRS_STATE_DISABLED;
	}

	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_MULTIMEDIA, PM_POWER_MODULE_STATE_OFF);
#endif
	LOGI("%s complete\n", __func__);

	return ret;
}

bk_err_t media_app_usb_open(video_setup_t *setup_cfg)
{
	int ret = BK_OK;
#if CONFIG_CHERRY_USB && CONFIG_USB_DEVICE

	usbd_video_h264_init();

	ret = usb_app_task_init((video_setup_t *)setup_cfg);
	if (ret != BK_OK)
	{
		LOGE("usb_app_task_init failed\r\n");
		return ret;
	}

	LOGI("%s, %d %d\n", __func__, type, setup_cfg->open_type);

	ret = media_send_msg_sync(EVENT_TRANSFER_USB_OPEN_IND, setup_cfg->open_type);

	LOGI("%s complete\n", __func__);
#endif

	return ret;
}

bk_err_t media_app_usb_close(void)
{
	bk_err_t ret = BK_OK;

#if CONFIG_CHERRY_USB && CONFIG_USB_DEVICE
	media_mailbox_msg_t mb_msg;

	LOGI("%s\n", __func__);

	usbd_video_h264_deinit();

	mb_msg.event = EVENT_TRANSFER_USB_CLOSE_IND;
	usb_app_event_handle(&mb_msg);

	ret = media_send_msg_sync(EVENT_TRANSFER_USB_CLOSE_IND, 0);
#endif

	return ret;
}

// step 1
uint32_t media_app_get_lcd_devices_num(void)
{
	uint32_t num;
	bk_err_t ret;
	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_GET_MEDIA_MSG, PM_POWER_MODULE_STATE_ON);
	ret = media_send_msg_sync_return_param(EVENT_LCD_GET_DEVICES_NUM_IND, 0, &num);
	if (ret != BK_OK)
	{
		LOGE("%s error\n", __func__);
	}
	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_GET_MEDIA_MSG, PM_POWER_MODULE_STATE_OFF);
	return num;
}

// step 2
uint32_t media_app_get_lcd_devices_list(void)
{
	uint32_t device_addr = 0;
	bk_err_t ret;
	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_GET_MEDIA_MSG, PM_POWER_MODULE_STATE_ON);
	ret = media_send_msg_sync_return_param(EVENT_LCD_GET_DEVICES_LIST_IND, 0, &device_addr);
	if (ret != BK_OK)
	{
		LOGE("%s error\n", __func__);
	}
	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_GET_MEDIA_MSG, PM_POWER_MODULE_STATE_OFF);
	return device_addr;
}

uint32_t media_app_get_lcd_device_by_id(uint32_t id)
{
	uint32_t lcd_device = 0;
	bk_err_t ret;
	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_GET_MEDIA_MSG, PM_POWER_MODULE_STATE_ON);
	ret = media_send_msg_sync_return_param(EVENT_LCD_GET_DEVICES_IND, id, &lcd_device);
	if (ret != BK_OK)
	{
		LOGE("%s error\n", __func__);
	}
	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_GET_MEDIA_MSG, PM_POWER_MODULE_STATE_OFF);
	return lcd_device;
}

bk_err_t media_app_get_lcd_status(void)
{
	uint32_t lcd_status = 0;
	bk_err_t ret;
	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_GET_MEDIA_MSG, PM_POWER_MODULE_STATE_ON);
    ret = media_send_msg_sync_return_param(EVENT_LCD_GET_STATUS_IND, 0, &lcd_status);
	if (ret != BK_OK)
	{
		LOGE("%s error\n", __func__);
	}
	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_GET_MEDIA_MSG, PM_POWER_MODULE_STATE_OFF);
	return lcd_status;
}

bk_err_t media_app_frame_jdec_open(void *lcd_open)
{
	int ret = kNoErr;

	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_VIDP_JPEG_DE, PM_POWER_MODULE_STATE_ON);

	ret = media_send_msg_sync(EVENT_IMG_ROTATE_IND, jpeg_decode_pipeline_param.rotate);
	ret = media_send_msg_sync(EVENT_IMG_OPEN_IND, (uint32_t)lcd_open);


	LOGI("%s complete %x\n", __func__, ret);

	return ret;
}

bk_err_t media_app_frame_jdec_close(void)
{
	bk_err_t ret = 0;

	LOGD("%s\n", __func__);

	ret = media_send_msg_sync(EVENT_IMG_CLOSE_IND, 0);
	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_VIDP_JPEG_DE, PM_POWER_MODULE_STATE_OFF);

	LOGI("%s complete\n", __func__);

	return ret;
}


bk_err_t media_app_lcd_set_backlight(uint8_t level)
{
	bk_err_t ret = BK_OK;

	LOGI("%s\n", __func__);

	ret = media_send_msg_sync(EVENT_LCD_SET_BACKLIGHT_IND, level);

	LOGI("%s complete\n", __func__);

	return ret;
}

bk_err_t media_app_storage_open(frame_cb_t cb)
{
	int ret = BK_OK;
#ifdef CONFIG_IMAGE_STORAGE
	media_mailbox_msg_t msg = {0};

	if (media_modules_state->stor_state == STORAGE_STATE_ENABLED)
	{
		LOGD("%s, storage have been opened!\r\n", __func__);
		return ret;
	}

	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_MULTIMEDIA, PM_POWER_MODULE_STATE_ON);

	msg.event = EVENT_STORAGE_OPEN_IND;
	msg.param = (uint32_t)cb;
	ret = storage_app_event_handle(&msg);
	if (ret != BK_OK)
	{
		return ret;
	}

	ret = media_send_msg_sync(EVENT_STORAGE_OPEN_IND, 0);

	if (ret == BK_OK)
	{
		media_modules_state->stor_state = STORAGE_STATE_ENABLED;
	}
#endif
	LOGI("%s, complete\n", __func__);

	return ret;
}

bk_err_t media_app_storage_close(void)
{
	int ret = BK_FAIL;
#ifdef CONFIG_IMAGE_STORAGE
	media_mailbox_msg_t msg = {0};
	if (media_modules_state->stor_state == STORAGE_STATE_DISABLED)
	{
		LOGI("%s, storage have been closed!\r\n", __func__);
		return ret;
	}

	ret = media_send_msg_sync(EVENT_STORAGE_CLOSE_IND, 0);
	if (ret != BK_OK)
	{
		LOGE("storage_major_task deinit failed\r\n");
	}

	msg.event = EVENT_STORAGE_CLOSE_IND;
	ret = storage_app_event_handle(&msg);

	if (ret == BK_OK)
	{
		media_modules_state->stor_state = STORAGE_STATE_DISABLED;
		bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_MULTIMEDIA, PM_POWER_MODULE_STATE_OFF);
	}
#endif
	LOGI("%s, complete\n", __func__);

	return ret;
}

bk_err_t media_app_capture(image_format_t fmt, char *name)
{
	int ret = BK_OK;
#ifdef CONFIG_IMAGE_STORAGE
	if (name == NULL)
	{
		return ret;
	}

	media_app_storage_open(NULL);

	ret = storage_app_set_frame_name(name);
	if (ret != BK_OK)
	{
		return ret;
	}

	ret = media_send_msg_sync(EVENT_STORAGE_CAPTURE_IND, (uint32_t)fmt);
#endif
	LOGI("%s complete\n", __func__);

	return ret;
}

bk_err_t media_app_save_start(image_format_t fmt, char *name)
{
	int ret = BK_OK;
#ifdef CONFIG_IMAGE_STORAGE
	if (name == NULL)
	{
		return ret;
	}

	media_app_storage_open(NULL);

	ret = storage_app_set_frame_name(name);
	if (ret != BK_OK)
	{
		return ret;
	}

	ret = media_send_msg_sync(EVENT_STORAGE_SAVE_START_IND, (uint32_t)fmt);
#endif
	LOGI("%s complete\n", __func__);

	return ret;
}

bk_err_t media_app_save_stop(void)
{
	int ret = BK_OK;
#ifdef CONFIG_IMAGE_STORAGE
	if (media_modules_state->stor_state == STORAGE_STATE_DISABLED)
	{
		LOGI("%s storage function not init\n", __func__);
		return ret;
	}

	ret = media_send_msg_sync(EVENT_STORAGE_SAVE_STOP_IND, 0);
	if (ret != BK_OK)
	{
		LOGE("storage_major_task stop save video failed\r\n");
	}
#endif
	LOGI("%s complete\n", __func__);

	return ret;
}

bk_err_t media_app_lvgl_open(void *lcd_open)
{
	int ret = BK_OK;
	lcd_open_t *ptr = NULL;

	ptr = (lcd_open_t *)os_malloc(sizeof(lcd_open_t));
	if (ptr == NULL) {
		LOGE("malloc lcd_open_t failed\r\n");
		return BK_ERR_NO_MEM;
	}
	os_memcpy(ptr, (lcd_open_t *)lcd_open, sizeof(lcd_open_t));

	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_LVGL, PM_POWER_MODULE_STATE_ON);

	ret = media_send_msg_sync(EVENT_LVGL_OPEN_IND, (uint32_t)ptr);

	if (ptr) {
		os_free(ptr);
		ptr = NULL;
	}

	LOGI("%s complete %x\n", __func__, ret);

	return ret;
}

bk_err_t media_app_lvgl_close(void)
{
	int ret = BK_OK;

	LOGI("%s\n", __func__);

	ret = media_send_msg_sync(EVENT_LVGL_CLOSE_IND, 0);

	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_LVGL, PM_POWER_MODULE_STATE_OFF);

	LOGI("%s complete %x\n", __func__, ret);

	return ret;
}

bk_err_t media_app_lvcam_lvgl_open(void *lcd_open)
{
    bk_err_t ret = 0;
    lcd_open_t *ptr = NULL;

    ptr = (lcd_open_t *)os_malloc(sizeof(lcd_open_t));
    if (ptr == NULL) {
        LOGE("malloc lcd_open_t failed\r\n");
        return BK_ERR_NO_MEM;
    }
    os_memcpy(ptr, (lcd_open_t *)lcd_open, sizeof(lcd_open_t));

    ret =  media_send_msg_sync(EVENT_LVCAM_LVGL_OPEN_IND, (uint32_t)ptr);

    if (ptr) {
        os_free(ptr);
        ptr = NULL;
    }

    LOGI("%s complete\n", __func__);

    return ret;
}

bk_err_t media_app_lvcam_lvgl_close(void)
{
    bk_err_t ret = 0;

    ret =  media_send_msg_sync(EVENT_LVCAM_LVGL_CLOSE_IND, 0);

    LOGI("%s complete\n", __func__);

    return ret;
}

bk_err_t media_app_frame_buffer_init(void)
{
	int ret = BK_FAIL;

	// need modify fix

	ret = media_send_msg_sync(EVENT_FRAME_BUFFER_INIT_IND, 0);

	if (ret != BK_OK)
	{
		LOGE("%s, malloc fail\r\n", __func__);
	}

	return ret;
}

frame_buffer_t *media_app_frame_buffer_jpeg_malloc(void)
{
	int ret = BK_FAIL;
	frame_buffer_t *frame = NULL;

	ret = media_send_msg_sync(EVENT_FRAME_BUFFER_JPEG_MALLOC_IND, (uint32_t)&frame);

	if (ret != BK_OK)
	{
		LOGE("%s, malloc fail\r\n", __func__);
	}

	LOGD("%s, %p\r\n", __func__, frame);

	if (frame)
		frame->fmt = PIXEL_FMT_JPEG;

	return frame;
}

frame_buffer_t *media_app_frame_buffer_h264_malloc(void)
{
	int ret = BK_FAIL;
	frame_buffer_t *frame = NULL;

	ret = media_send_msg_sync(EVENT_FRAME_BUFFER_H264_MALLOC_IND, (uint32_t)&frame);

	if (ret != BK_OK)
	{
		LOGE("%s, malloc fail\r\n", __func__);
	}

	if (frame)
		frame->fmt = PIXEL_FMT_H264;

	return frame;
}

bk_err_t media_app_frame_buffer_push(frame_buffer_t *frame)
{
	int ret = BK_FAIL;

	ret = media_send_msg_sync(EVENT_FRAME_BUFFER_PUSH_IND, (uint32_t)frame);

	return ret;
}

bk_err_t media_app_frame_buffer_clear(frame_buffer_t *frame)
{
	int ret = BK_FAIL;

	ret = media_send_msg_sync(EVENT_FRAME_BUFFER_FREE_IND, (uint32_t)frame);

	return ret;
}

bk_err_t media_app_pipeline_mem_show(void)
{
	int ret = BK_OK;

	ret = media_send_msg_sync(EVENT_PIPELINE_MEM_SHOW_IND, 0);

	LOGI("%s complete %x\n", __func__, ret);

	return ret;
}

bk_err_t media_app_pipeline_mem_leak(void)
{
	int ret = BK_OK;

	ret = media_send_msg_sync(EVENT_PIPELINE_MEM_LEAK_IND, 0);

	LOGI("%s complete %x\n", __func__, ret);

	return ret;
}

bk_err_t media_app_pipeline_dump(void)
{
	int ret = BK_OK;

	ret = media_send_msg_sync(EVENT_PIPELINE_DUMP_IND, 0);

	LOGI("%s complete %x\n", __func__, ret);

	return ret;
}

void media_app_save_frame_data_handle(uint32_t param)
{
	int ret = BK_OK;

	media_mailbox_msg_t *msg = (media_mailbox_msg_t *)param;

#ifdef CONFIG_IMAGE_STORAGE
	frame_buffer_t *frame = (frame_buffer_t *)msg->param;
	char file_name[20] = {0};

	if (frame != NULL)
	{
		if (frame->fmt == PIXEL_FMT_JPEG)
		{
			LOGI("%s, seq:%d, length:%d\n", __func__, frame->sequence, frame->length);
			os_snprintf(file_name, 20, "%d.jpg", frame->sequence);
			bk_mem_save_to_sdcard(file_name, frame->frame, frame->length);
		}
		else
		{
			os_snprintf(file_name, 20, "%d.yuv", frame->sequence);
			bk_mem_save_to_sdcard(file_name, frame->frame, frame->size);
		}
	}

#endif

	msg_send_rsp_to_media_app_mailbox(msg, ret);
}


bk_err_t media_app_send_msg(media_msg_t *msg)
{
	bk_err_t ret;

	if (media_app_msg_queue)
	{
		ret = rtos_push_to_queue(&media_app_msg_queue, msg, BEKEN_NO_WAIT);

		if (kNoErr != ret)
		{
			LOGE("%s failed\n", __func__);
			return kOverrunErr;
		}

		return ret;
	}
	return kNoResourcesErr;
}

static void media_app_message_handle(void)
{
	bk_err_t ret = BK_OK;
	media_msg_t msg;
    media_mailbox_msg_t *mailbox_msg;

	while (1)
	{
		ret = rtos_pop_from_queue(&media_app_msg_queue, &msg, BEKEN_WAIT_FOREVER);

		if (BK_OK == ret)
		{
			switch (msg.event)
			{
				case EVENT_UVC_DEVICE_INFO_NOTIFY:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					if (uvc_device_info_cb)
					{
						uvc_device_info_cb((bk_usb_hub_port_info *)mailbox_msg->param, mailbox_msg->result);
					}
					break;

				case EVENT_SAVE_FRAME_DATA_IND:
					media_app_save_frame_data_handle(msg.param);
					break;


				case EVENT_MEDIA_APP_EXIT_IND:
					goto exit;
					break;

				default:
					break;
			}
		}
	}

exit:

	/* delate msg queue */
	ret = rtos_deinit_queue(&media_app_msg_queue);

	if (ret != BK_OK)
	{
		LOGE("delate message queue fail\n");
	}

	media_app_msg_queue = NULL;

	LOGE("delate message queue complete\n");

	/* delate task */
	media_app_th_hd = NULL;
	rtos_delete_thread(NULL);

}

bk_err_t media_app_init(void)
{
	bk_err_t ret = BK_OK;

	if (media_app_msg_queue != NULL)
	{
		LOGE("%s, media_app_msg_queue already init, exit!\n", __func__);
		goto error;
	}

	if (media_app_th_hd != NULL)
	{
		LOGE("%s, media_app_th_hd already init, exit!\n", __func__);
		goto error;
	}

	if (media_modules_state == NULL)
	{
		media_modules_state = (media_modules_state_t *)os_malloc(sizeof(media_modules_state_t));
		if (media_modules_state == NULL)
		{
			LOGE("%s, media_modules_state malloc failed!\n", __func__);
			return BK_ERR_NO_MEM;
		}
	}

	media_modules_state->aud_state = AUDIO_STATE_DISABLED;
	INIT_LIST_HEAD(&media_modules_state->cam_list);
	media_modules_state->lcd_state = LCD_STATE_DISABLED;
	media_modules_state->stor_state = STORAGE_STATE_DISABLED;
	media_modules_state->trs_state = TRS_STATE_DISABLED;
	bk_camera_handle_list_init((void *)&media_modules_state->cam_list);

	ret = rtos_init_queue(&media_app_msg_queue,
	                      "media_app_msg_queue",
	                      sizeof(media_msg_t),
	                      20);

	if (ret != kNoErr)
	{
		LOGE("%s, create media minor message queue failed\n", __func__);
		goto error;
	}

	ret = rtos_create_thread(&media_app_th_hd,
	                         BEKEN_DEFAULT_WORKER_PRIORITY,
	                         "media_app_thread",
	                         (beken_thread_function_t)media_app_message_handle,
	                         2560,
	                         NULL);

	if (ret != kNoErr)
	{
		LOGE("create media app thread fail\n");
		goto error;
	}

	LOGI("media app thread startup complete\n");

	return ret;

error:

	if (media_app_msg_queue)
	{
		rtos_deinit_queue(&media_app_msg_queue);
		media_app_msg_queue = NULL;
	}

	return ret;

}
