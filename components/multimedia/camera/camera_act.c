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
#include <components/log.h>

#include "media_core.h"
#include "camera_act.h"
#include "media_evt.h"
#include <driver/int.h>
#include <os/mem.h>

#include "camera.h"
#include "frame_buffer.h"
#include "yuv_encode.h"

#include <driver/dvp_camera.h>
#include <driver/uvc_camera_types.h>
#include <driver/dvp_camera_types.h>
#include <driver/video_common_driver.h>
#include <driver/uvc_camera.h>
#include <driver/h264.h>
#include <driver/jpeg_enc.h>

#define TAG "cam_act"

#define LOGI(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

typedef struct {
    uint32_t param1;
    uint32_t param2;
} media_device_t;

media_mailbox_msg_t camera_media_msg = {0};

static void camera_uvc_device_info_notify_to_cp0(bk_usb_hub_port_info *info, uint32_t state)
{
#if CONFIG_MEDIA_PIPELINE
	if (state == UVC_DISCONNECT_STATE)
	{
		jpeg_decode_restart();
	}
#endif

	camera_media_msg.event = EVENT_UVC_DEVICE_INFO_NOTIFY;
	camera_media_msg.param = (uint32_t)info;
	camera_media_msg.result = state;
	msg_send_notify_to_media_major_mailbox(&camera_media_msg, APP_MODULE);
}

static bk_err_t camera_open_handle(media_mailbox_msg_t *msg)
{
	int ret = BK_FAIL;

	media_device_t *device = (media_device_t*)msg->param;
	camera_handle_t *handle = (camera_handle_t *)device->param1;
	media_camera_device_t *config = (media_camera_device_t *)device->param2;
	LOGI("%s\n", __func__);

	if (config->type == DVP_CAMERA)
	{
#ifdef CONFIG_DVP_CAMERA
		ret = bk_dvp_camera_open(handle, config);
#endif
	}
	else if (config->type == UVC_CAMERA)
	{
#ifdef CONFIG_USB_CAMERA
		bk_uvc_register_connect_state_cb(camera_uvc_device_info_notify_to_cp0);
		ret = bk_uvc_camera_open(handle, config);
#endif
	}
	else if (config->type == NET_CAMERA)
	{
		ret = bk_net_camera_open(handle, config);
	}
	else
	{
		LOGI("%s, not support\n", __func__);
	}

	msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);

	return ret;
}

static bk_err_t camera_close_handle(media_mailbox_msg_t *msg)
{
	int ret = BK_FAIL;

	camera_handle_t *handle = (camera_handle_t *)msg->param;
	camera_config_t *config = (camera_config_t *)*handle;

	if (config->type == DVP_CAMERA)
	{
#ifdef CONFIG_DVP_CAMERA
		ret = bk_dvp_camera_close(handle);
#endif
	}
	else if (config->type == UVC_CAMERA)
	{
#ifdef CONFIG_USB_CAMERA
		ret = bk_uvc_camera_close(handle);
#endif
	}
	else if (config->type == NET_CAMERA)
	{
		ret = bk_net_camera_close(handle);
	}
	else
	{
		LOGW("%s, not support %x %x\n", __func__, config->id, config->type);
	}

	msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);

	return ret;
}

static bk_err_t camera_dvp_h264_reset_handle(media_mailbox_msg_t *msg)
{
	int ret = BK_FAIL;

#ifdef CONFIG_DVP_CAMERA
	ret = bk_dvp_h264_idr_reset();
#endif

	LOGI("%s complete\n", __func__);

	msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);

	return ret;
}

static bk_err_t camera_uvc_register_device_info_cb_handle(media_mailbox_msg_t *msg)
{
	int ret = BK_FAIL;
#ifdef CONFIG_USB_CAMERA
	ret = bk_uvc_register_connect_state_cb(camera_uvc_device_info_notify_to_cp0);
#endif
	msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);

	return BK_OK;
}

static bk_err_t camera_set_uvc_param_handle(media_mailbox_msg_t *msg)
{
   int ret = BK_FAIL;

#ifdef CONFIG_USB_CAMERA
    media_device_t *device = (media_device_t*)msg->param;
    camera_handle_t *handle = (camera_handle_t *)device->param1;
    uvc_config_t *config = (uvc_config_t *)device->param2;
   ret = bk_uvc_set_start(handle, config);
#endif

   msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);

   return ret;
}

static bk_err_t camera_compression_ratio_config_handle(media_mailbox_msg_t *msg)
{
    int ret = BK_FAIL;

    LOGI("%s\n", __func__);

#if (defined(CONFIG_H264) || defined(CONFIG_JPEGENC_HW))
    compress_ratio_t *ratio = (compress_ratio_t *)msg->param;

    if (ratio->mode == JPEG_MODE)
    {
        ret = bk_jpeg_enc_encode_config(ratio->enable, ratio->jpeg_up, ratio->jpeg_low);
    }
    else
    {
        ret = bk_h264_set_base_config(ratio);
    }
#endif
    msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);

    return ret;
}

static bk_err_t camera_get_h264_encode_param_handle(media_mailbox_msg_t *msg)
{
	int ret = BK_FAIL;

#ifdef CONFIG_H264
	h264_base_config_t base_config = {0};

	ret = bk_h264_get_h264_base_config(&base_config);

	os_memcpy((h264_base_config_t *)msg->param, &base_config, sizeof(h264_base_config_t));
#endif

	msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);

	return ret;
}

static bk_err_t camera_switch_main_stream_handle(media_mailbox_msg_t *msg)
{
	int ret = BK_FAIL;

	camera_config_t *config = (camera_config_t *)msg->param;

	ret = frame_buffer_list_set_main_stream(config->id, config->type, config->image_format);

	msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);

	return ret;
}

static bk_err_t camera_get_main_stream_handle(media_mailbox_msg_t *msg)
{
	int ret = BK_FAIL;

	frame_list_node_t *node = NULL;

	node = frame_buffer_list_get_main_stream();
	if (node != NULL)
	{
		os_memcpy((h264_base_config_t *)msg->param, node, sizeof(frame_list_node_t));
		ret = BK_OK;
	}

	msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);

	return ret;
}

static bk_err_t camera_net_frame_buffer_malloc_handle(media_mailbox_msg_t *msg)
{
	bk_err_t ret = BK_FAIL;

	media_device_t *device = (media_device_t*)msg->param;
	camera_handle_t *handle = (camera_handle_t *)device->param1;
	frame_buffer_t **frame = (frame_buffer_t **)device->param2;

	*frame = bk_net_camera_frame_buffer_malloc(handle);

	if (frame)
	{
		ret = BK_OK;
	}

	msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);

	return ret;
}

static bk_err_t camera_net_frame_buffer_push_handle(media_mailbox_msg_t *msg)
{
	bk_err_t ret = BK_FAIL;

	media_device_t *device = (media_device_t*)msg->param;
	camera_handle_t *handle = (camera_handle_t *)device->param1;
	frame_buffer_t *frame = (frame_buffer_t *)device->param2;

	ret = bk_net_camera_frame_buffer_push(handle, frame);

	msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);

	return ret;
}

static bk_err_t camera_net_frame_buffer_free_handle(media_mailbox_msg_t *msg)
{
	bk_err_t ret = BK_FAIL;

	media_device_t *device = (media_device_t*)msg->param;
	camera_handle_t *handle = (camera_handle_t *)device->param1;
	frame_buffer_t *frame = (frame_buffer_t *)device->param2;

	ret = bk_net_camera_frame_buffer_free(handle, frame);

	msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);

	return ret;
}

bk_err_t camera_event_handle(media_mailbox_msg_t *msg)
{
	bk_err_t ret = BK_FAIL;
	switch (msg->event)
	{
		case EVENT_CAM_OPEN_IND:
			ret = camera_open_handle(msg);
			break;

		case EVENT_CAM_CLOSE_IND:
			ret = camera_close_handle(msg);
			break;

		case EVENT_CAM_H264_RESET_IND:
			ret = camera_dvp_h264_reset_handle(msg);
			break;

		case EVENT_CAM_REG_UVC_INFO_CB_IND:
			ret = camera_uvc_register_device_info_cb_handle(msg);
			break;

		case EVENT_CAM_SET_UVC_PARAM_IND:
			ret = camera_set_uvc_param_handle(msg);
			break;

		case EVENT_CAM_COMPRESS_IND:
			ret = camera_compression_ratio_config_handle(msg);
			break;

		case EVENT_CAM_GET_H264_INFO_IND:
			ret = camera_get_h264_encode_param_handle(msg);
			break;

		case EVENT_CAM_SWITCH_MAIN_IND:
			ret = camera_switch_main_stream_handle(msg);
			break;

		case EVENT_CAM_GET_MAIN_STREAM_IND:
			ret = camera_get_main_stream_handle(msg);
			break;

		case EVENT_FRAME_BUFFER_MALLOC_IND:
			ret = camera_net_frame_buffer_malloc_handle(msg);
			break;

		case EVENT_FRAME_BUFFER_PUSH_IND:
			ret = camera_net_frame_buffer_push_handle(msg);
			break;

		case EVENT_FRAME_BUFFER_FREE_IND:
			ret = camera_net_frame_buffer_free_handle(msg);
			break;

		default:
			break;
	}

	return ret;
}
