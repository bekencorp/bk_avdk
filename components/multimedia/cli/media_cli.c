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
#include "cli.h"
#include "media_cli.h"

#include "media_cli_comm.h"
#include "media_app.h"
#include "camera_handle_list.h"
#include <driver/dvp_camera.h>
#include <driver/jpeg_enc.h>
#include "img_service.h"
#include "storage_act.h"
#include <driver/audio_ring_buff.h>
#include "aud_intf.h"
#include <driver/uvc_camera.h>
#include <driver/h264_types.h>
#include "media_evt.h"
#include "bk_draw_blend.h"

#include "media_utils.h"

#define TAG "mcli"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

#define UNKNOW_ERROR (-686)
#define PARAMS_ERROR (-687)

#define CMD_CONTAIN(value) cmd_contain(argc, argv, value)
#define GET_PPI(value)     get_ppi_from_cmd(argc, argv, value)
#define GET_NAME(value)    get_name_from_cmd(argc, argv, value)
#define GET_ROTATE()       get_rotate_from_cmd(argc, argv)
#define GET_H26X_PPI()     get_h26x_ppi_from_cmd(argc, argv)

void uvc_connect_state_callback(bk_usb_hub_port_info *info, uint32_t state)
{
	LOGI("%s %d+++\n", __func__, state);
}

void media_read_frame_callback(frame_buffer_t *frame)
{
	if (frame && frame->sequence % 30 == 0)
	{
		LOGI("frame_id:%d, length:%d, h264_fmt:%x, frame_addr:%p\r\n", frame->sequence,
			frame->length, frame->h264_type,frame->frame);
	}
}

uint32_t get_ppi_from_cmd(int argc, char **argv, uint32_t pre)
{
	int i;
	uint32_t value = pre;

	for (i = 0; i < argc; i++)
	{
		value = get_string_to_ppi(argv[i]);

		if (value != PPI_DEFAULT)
		{
			break;
		}
	}

	if (value == PPI_DEFAULT)
	{
		value = pre;
	}

	LOGD("%s %d-%d+++\n", __func__, value >> 16, value & 0xFFFF);

	return value;
}

uint32_t get_h26x_ppi_from_cmd(int argc, char **argv)
{
	int i;
	uint32_t value = PPI_DEFAULT;

	for (i = 5; i < argc; i++)
	{
		value = get_string_to_ppi(argv[i]);

		if (value != PPI_DEFAULT)
		{
			break;
		}
	}
	LOGD("%s %d-%d+++\n", __func__, value >> 16, value & 0xFFFF);
	return value;
}


char * get_name_from_cmd(int argc, char **argv, char * pre)
{
	int i;
	char* value = pre;

	for (i = 3; i < argc; i++)
	{
		value = get_string_to_lcd_name(argv[i]);

		if (value != NULL)
		{
			break;
		}
	}

	return value;
}

media_rotate_t get_rotate_from_cmd(int argc, char **argv)
{
	int i;
	media_rotate_t value = ROTATE_90;

	for (i = 3; i < argc; i++)
	{
		value = get_string_to_angle(argv[i]);
	}

	return value;
}

bool cmd_contain(int argc, char **argv, char *string)
{
	int i;
	bool ret = false;

	for (i = 0; i < argc; i++)
	{
		if (os_strcmp(argv[i], string) == 0)
		{
			ret = true;
		}
	}

	return ret;
}

int open_camera_display(int camera_port, image_format_t fmt)  // uvc 1/ uvc 2/ dvp 3
{
    int ret = UNKNOW_ERROR;

    media_camera_device_t device = {0};
    media_ppi_t ppi;
    camera_handle_t handle = NULL;

	
    frame_list_node_t node;
    ret = media_app_get_main_camera_stream(&node);
    if (ret == BK_OK)
    {
        if(node.camera_id == 8)
        {
            node.camera_id = 0;
        }
        LOGI("%s opened:%x,  switch:%x\n", __func__, node.camera_id, camera_port);
        if(node.camera_id == camera_port)
        {
            LOGI("%s open repetition, opened:%x,  switch:%x\n", __func__, node.camera_id, camera_port);
            return BK_OK;
        }
    }

    handle = bk_camera_handle_node_get_by_id_and_fomat(1, IMAGE_MJPEG);

    if (handle != NULL)
    {
        LOGI("%s media_app_get_camera_handle_by_id 1\n", __func__);
        ret = media_app_camera_close(&handle);
    }

    handle = NULL;
    handle = bk_camera_handle_node_get_by_id_and_fomat(2, IMAGE_MJPEG);
    if (handle != NULL)
    {
        LOGI("%s media_app_get_camera_handle_by_id 2\n", __func__);
        ret = media_app_camera_close(&handle);
    }

    ret = media_app_pipeline_h264_close();
    ret = media_app_pipeline_jdec_close();
    ret = media_app_frame_jdec_close();

    handle = NULL;
    handle = bk_camera_handle_node_get_by_id_and_fomat(0, IMAGE_YUV | IMAGE_MJPEG);
    if (handle != NULL)
    {
        LOGI("%s media_app_get_camera_handle_by_id 0\n", __func__);
        ret = media_app_camera_close(&handle);
    }
    else
    {
        handle = bk_camera_handle_node_get_by_id_and_fomat(0, IMAGE_YUV | IMAGE_H264);
        if (handle != NULL)
        {
            ret = media_app_camera_close(&handle);
        }
    }

    os_memset(&device, 0, sizeof(media_camera_device_t));

    if ((camera_port == 1) || (camera_port == 2))
    {
        ppi = PPI_864X480;
        device.type = UVC_CAMERA;
        device.width = ppi >> 16;
        device.height = ppi & 0xFFFF;
        device.fps = FPS25;
        device.format = IMAGE_MJPEG;
        device.port = camera_port;
        handle = NULL;
        ret = media_app_camera_open(&handle, &device);

        ret = media_app_set_rotate(ROTATE_90);

        if (fmt == IMAGE_H264)
        {
            ret = media_app_pipeline_h264_open();
        }
        ret = media_app_pipeline_jdec_open();
    }
    else if (camera_port == 0)
    {
        ppi = PPI_864X480;
        device.type = DVP_CAMERA;
        device.width = ppi >> 16;
        device.height = ppi & 0xFFFF;
        device.fps = FPS25;
        if (fmt == IMAGE_H264)
        {
            device.format = IMAGE_YUV | IMAGE_H264;
        }
        else
        {
            device.format = IMAGE_YUV | IMAGE_MJPEG;
        }
        device.port = 0;
        handle = NULL;
        ret = media_app_camera_open(&handle, &device);
        ret = media_app_set_rotate(ROTATE_90);
        ret = media_app_frame_jdec_open(NULL);
    }
    else
    {
        LOGI("%s not support camera id %d\n", __func__, camera_port);
    }
    return ret;
}

extern media_share_ptr_t *media_share_ptr;

void media_cli_threecam_auto_test_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    LOGI("%s +++ cmd:'three_camera test 3000 rgb565/rgb888] +++\n", __func__);
    image_format_t fmt = IMAGE_MJPEG;
    static uint32_t cnt = 0;
    if (os_strcmp(argv[1], "test") == 0)
    {
        uint32_t delay = 3000;
        uint32_t order = 0;
        uint32_t old_order = 0xFF;
        static uint32_t test_err = 0;
        if (argc >= 2)
         {
            delay = os_strtoul(argv[2], NULL, 10) & 0xFFFFFFFF;
            if (delay < 200)
                delay = 3000;
         }
        if (CMD_CONTAIN("rgb888"))
        {
            media_app_lcd_fmt(PIXEL_FMT_RGB888);
        }
        else
        {
            media_app_lcd_fmt(PIXEL_FMT_RGB565);
        }
        
        if (CMD_CONTAIN("h264"))
        {
            fmt = IMAGE_H264;
        }
        LOGI("delay %d, camera_fmt = %x (4:mjpg, 8:h264 ) \r\n", delay, fmt);

        lcd_open_t lcd_open;
        lcd_open.device_ppi = PPI_864X480;
        lcd_open.device_name = "st7701sn";
        media_app_lcd_disp_open(&lcd_open);
        media_debug_t *media_debug = media_share_ptr->media_debug;

        while (1)
        {
            media_debug->isr_decoder = 0;
            media_debug->fps_lcd = 0;
            order = (uint32_t)rand();   //((order&0xf0) >> 4)&0x3
            order = ((order & 0xf) & 0x3);
            if (old_order == order || (order == 3))
            {
                rtos_delay_milliseconds(100);
                continue;
            }
            cnt++;
            LOGI("\r\n");
            LOGI("===== %s test_order=%x, test_cnt=%ld, err=%d ===== \n", __func__, order, cnt, test_err);
            LOGI("\r\n");
            open_camera_display(order, fmt);
            old_order = order;
            rtos_delay_milliseconds(delay);

            if (media_debug->fps_lcd == 0)
            {
                test_err++;
                LOGE("============= %s test_error %x ============ \n", __func__, order);
            }
        }
    }
    else if (os_strcmp(argv[1], "switch") == 0)
    {
        if (!cnt)
        {
            cnt = 1;
            lcd_open_t lcd_open;
            lcd_open.device_ppi = PPI_864X480;
            lcd_open.device_name = "st7701sn";
            media_app_lcd_disp_open(&lcd_open);
        }
        uint8_t port = os_strtoul(argv[2], NULL, 10) & 0xFF;
        if (CMD_CONTAIN("h264"))
        {
            fmt = IMAGE_H264;
        }
        open_camera_display(port, fmt);
    }
    LOGI("%s ---complete\n", __func__);
}

void media_cli_test_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	int ret = UNKNOW_ERROR;
	char *msg = NULL;
	camera_handle_t handle = NULL;

	LOGI("%s +++\n", __func__);

	if (argc <= 2)
	{
		ret = BK_FAIL;
		goto output;
	}
	else
	{
		if (os_strcmp(argv[1], "dvp") == 0)
		{
			media_ppi_t ppi = GET_PPI(PPI_640X480);
			media_camera_device_t device = DEFAULT_CAMERA_CONFIG();
			device.width = ppi >> 16;
			device.height = ppi & 0xFFFF;
			device.fps = FPS25;

			if (CMD_CONTAIN("yuv"))
			{
				device.format = IMAGE_YUV;
			}

			if (CMD_CONTAIN("jpeg"))
			{
				device.format = IMAGE_MJPEG;
				if (CMD_CONTAIN("enc_yuv"))
				{
					device.format |= IMAGE_YUV;
				}
			}

			if (CMD_CONTAIN("h264"))
			{
				device.format = IMAGE_H264;
				if (CMD_CONTAIN("enc_yuv"))
				{
					device.format |= IMAGE_YUV;
				}
			}

			if (os_strcmp(argv[2], "open") == 0)
			{
				ret = media_app_camera_open(&handle, &device);
			}

			if (os_strcmp(argv[2], "close") == 0)
			{
				do {
					handle = bk_camera_handle_node_get_by_id_and_fomat(0, IMAGE_YUV);
					if (handle)
					{
						break;
					}

					handle = bk_camera_handle_node_get_by_id_and_fomat(0, IMAGE_MJPEG);
					if (handle)
					{
						break;
					}

					handle = bk_camera_handle_node_get_by_id_and_fomat(0, IMAGE_H264);
					if (handle)
					{
						break;
					}

					handle = bk_camera_handle_node_get_by_id_and_fomat(0, IMAGE_YUV | IMAGE_H264);
					if (handle)
					{
						break;
					}

					handle = bk_camera_handle_node_get_by_id_and_fomat(0, IMAGE_YUV | IMAGE_MJPEG);
				} while (0);

				if (handle != NULL)
				{
					ret = media_app_camera_close(&handle);
				}
				else
				{
					LOGE("%s, %d handle is null\n", __func__, __LINE__);
				}
			}
		}

		if (os_strcmp(argv[1], "uvc") == 0)
		{
			camera_handle_t handle = NULL;
			media_ppi_t ppi = GET_PPI(PPI_640X480);
			media_camera_device_t device = DEFAULT_CAMERA_CONFIG();
			device.type = UVC_CAMERA;
			device.width = ppi >> 16;
			device.height = ppi & 0xFFFF;
			device.fps = FPS30;

			if (os_strcmp(argv[2], "open") == 0)
			{
				if (CMD_CONTAIN("h264"))
				{
					device.format = IMAGE_H264;
				}

				if (CMD_CONTAIN("h265"))
				{
					device.format = IMAGE_H265;
				}

				if (CMD_CONTAIN("yuv"))
				{
					device.format = IMAGE_YUV;
				}

				if (CMD_CONTAIN("dual"))
				{
					device.format = IMAGE_MJPEG | IMAGE_H264;
				}

				device.port = os_strtoul(argv[3], NULL, 10);
				//media_app_register_uvc_connect_state_cb(uvc_connect_state_callback);
				ret = media_app_camera_open(&handle, &device);
			}

			if (os_strcmp(argv[2], "close") == 0)
			{
				uint8_t port = os_strtoul(argv[3], NULL, 10);

				do {
					handle = bk_camera_handle_node_get_by_id_and_fomat(port, IMAGE_MJPEG);
					if (handle)
					{
						break;
					}

					handle = bk_camera_handle_node_get_by_id_and_fomat(port, IMAGE_H264);
					if (handle)
					{
						break;
					}

					handle = bk_camera_handle_node_get_by_id_and_fomat(port, IMAGE_H265);
					if (handle != NULL)
					{
						ret = media_app_camera_close(&handle);
					}

					handle = bk_camera_handle_node_get_by_id_and_fomat(port, IMAGE_MJPEG | IMAGE_H264);
					if (handle != NULL)
					{
						ret = media_app_camera_close(&handle);
					}
				} while (0);

				if (handle != NULL)
				{
					ret = media_app_camera_close(&handle);
				}
				else
				{
					LOGE("%s, %d handle is null\n", __func__, __LINE__);
				}
			}
		}

		/*cmd:
		* media switch 1 uvc mjpg 
		* media switch 2 uvc mjpg
		* media switch 8 dvp mjpg   */
		if (os_strcmp(argv[1], "switch") == 0)
		{
			if (argc < 3)
			{
				LOGE("%s, param error\n", __func__);
				goto output;
			}

			uint16_t id = os_strtoul(argv[2], NULL, 16) & 0xFF;
			uint8_t camera_type = os_strtoul(argv[3], NULL, 10) & 0xFF;
			if (CMD_CONTAIN("dvp"))
			{
				camera_type = DVP_CAMERA;
			}
			else
			{
				camera_type = UVC_CAMERA;
			}

			image_format_t image_format = IMAGE_MJPEG;
			if (CMD_CONTAIN("h264"))
			{
				image_format = IMAGE_H264;
			}

			if (CMD_CONTAIN("h265"))
			{
				image_format = IMAGE_H265;
			}

			ret = media_app_switch_main_camera(id, camera_type, image_format);
		}

		if (os_strcmp(argv[1], "compress") == 0)
		{
			compress_ratio_t compress = {0};

			if (os_strcmp(argv[2], "h264") == 0)
			{
				if (argc >= 8)
				{
					compress.mode = H264_MODE;
					compress.enable = 1;
					compress.qp.init_qp = os_strtoul(argv[3], NULL, 10);
					compress.qp.i_max_qp = os_strtoul(argv[4], NULL, 10);
					compress.qp.p_max_qp = os_strtoul(argv[5], NULL, 10);
					compress.imb_bits = os_strtoul(argv[6], NULL, 10);
					compress.pmb_bits = os_strtoul(argv[7], NULL, 10);

					ret = media_app_set_compression_ratio(&compress);
				}
			}
			else if (os_strcmp(argv[2], "jpeg") == 0)
			{
				if (argc >= 6)
				{
					compress.mode = JPEG_MODE;
					compress.enable = os_strtoul(argv[3], NULL, 10);
					compress.jpeg_up = 1024 * os_strtoul(argv[4], NULL, 10);
					compress.jpeg_low = 1024 * os_strtoul(argv[5], NULL, 10);

					ret = media_app_set_compression_ratio(&compress);
				}
			}
		}

		if (os_strcmp(argv[1], "register") == 0)
		{
			if (os_strcmp(argv[2], "get_config") == 0)
			{
				ret = media_app_uvc_register_info_notify_cb(uvc_connect_state_callback);
			}

			if (os_strcmp(argv[2], "set_config") == 0)
			{
				if (argc >= 8)
				{
					uvc_config_t uvc_config_param = {0};
					uvc_config_param.width = os_strtoul(argv[3], NULL, 10);
					uvc_config_param.height = os_strtoul(argv[4], NULL, 10);
					uvc_config_param.fps = os_strtoul(argv[5], NULL, 10);
					uvc_config_param.port = os_strtoul(argv[6], NULL, 10);
					uvc_config_param.drop_num = os_strtoul(argv[7], NULL, 10);
					ret = media_app_set_uvc_device_param(&uvc_config_param);
				}
				else
					ret = kParamErr;
			}
		}

		/* open display cmd: media lcd open st7701sn rotate 90 rgb888(d)/rgb565*/
		if (os_strcmp(argv[1], "lcd") == 0)
		{
			media_ppi_t ppi = PPI_480X272;
			char *name = "NULL";
			media_rotate_t rotate = ROTATE_NONE;
			ppi = GET_PPI(PPI_480X272);
			name = GET_NAME(name);

			if (CMD_CONTAIN("rotate"))
			{
				rotate = GET_ROTATE();
				ret = media_app_set_rotate(rotate);
			}

			if (CMD_CONTAIN("rgb888"))
			{
				media_app_lcd_fmt(PIXEL_FMT_RGB888);
			}
			else
			{
				media_app_lcd_fmt(PIXEL_FMT_RGB565_LE);
			}

			if (os_strcmp(argv[2], "open") == 0)
			{
				lcd_open_t lcd_open;
				lcd_open.device_ppi = ppi;
				lcd_open.device_name = name;
				ret = media_app_lcd_disp_open(&lcd_open);
			}
			if (os_strcmp(argv[2], "close") == 0)
			{
				ret = media_app_lcd_disp_close();
			}
			if (os_strcmp(argv[2], "status") == 0)
			{
				uint32_t lcd_status = 0;
				lcd_status = media_app_get_lcd_status();
				LOGI("lcd status %d\r\n", lcd_status);
				ret = BK_OK;
			}
		}

		/* open decode cmd: media decode frame/line open */
		if (os_strcmp(argv[1], "decode") == 0)
		{
			ret = media_app_set_rotate(ROTATE_90);
			if (os_strcmp(argv[2], "frame") == 0)
			{
				if (os_strcmp(argv[3], "open") == 0)
				{
					ret = media_app_frame_jdec_open(NULL);
				}
				else if (os_strcmp(argv[3], "close") == 0)
				{
					ret = media_app_frame_jdec_close();
				}
			}
			else if (os_strcmp(argv[2], "line") == 0)
			{
    			ret = media_app_set_rotate(ROTATE_90);
				if (os_strcmp(argv[3], "open") == 0)
				{
					ret = media_app_pipeline_jdec_open();
				}
				else if (os_strcmp(argv[3], "close") == 0)
				{
					ret = media_app_pipeline_jdec_close();
				}
			}
		}

		if (os_strcmp(argv[1], "pipeline") == 0)
		{
			if (os_strcmp(argv[2], "h264_open") == 0)
			{
				ret = media_app_pipeline_h264_open();
			}

			if (os_strcmp(argv[2], "h264_close") == 0)
			{
				ret = media_app_pipeline_h264_close();
			}

			if (os_strcmp(argv[2], "dump") == 0)
			{
				ret = media_app_pipeline_dump();
			}
		}

		if (os_strcmp(argv[1], "h264") == 0)
		{
			if (os_strcmp(argv[2], "get_config") == 0)
			{
				h264_base_config_t config;
				ret = media_app_get_h264_encode_config(&config);
				if (ret == BK_OK)
				{
					LOGI("h264_encode_state:%d\r\n", config.h264_state);
					LOGI("p_frame_cnt      :%d\r\n", config.p_frame_cnt);
					LOGI("profile_id       :%d\r\n", config.profile_id);
					LOGI("qp               :%d\r\n", config.qp);
					LOGI("num_imb_bits     :%d\r\n", config.num_imb_bits);
					LOGI("num_pmb_bits     :%d\r\n", config.num_pmb_bits);
					LOGI("width            :%d\r\n", config.width);
					LOGI("height           :%d\r\n", config.height);
				}
			}

			if (os_strcmp(argv[2], "reset") == 0)
			{
				if (os_strcmp(argv[3], "dvp") == 0)
					ret = media_app_h264_regenerate_idr(DVP_CAMERA);
				else
					ret = media_app_h264_regenerate_idr(UVC_CAMERA);
			}
		}

		if (os_strcmp(argv[1], "net_camera") == 0)
		{
			handle = bk_camera_handle_node_get_by_id_and_fomat(0, IMAGE_MJPEG);

			if (os_strcmp(argv[2], "open") == 0)
			{
				media_camera_device_t device = DEFAULT_CAMERA_CONFIG();
				device.type = NET_CAMERA;
				ret = media_app_camera_open(&handle, &device);
			}
			else if (os_strcmp(argv[2], "close") == 0)
			{
				if (handle != NULL)
				{
					ret = media_app_camera_close(&handle);
				}
				else
				{
					LOGE("%s, %d handle is null\n", __func__, __LINE__);
				}
			}
			else if (os_strcmp(argv[2], "malloc") == 0)
			{
				frame_buffer_t *frame = media_app_frame_buffer_malloc(&handle);
				if (frame != NULL)
				{
					LOGE("%s, %d frame malloc:%p\n", __func__, __LINE__, frame);
					ret = BK_OK;
				}
				else
				{
					LOGE("%s, %d frame malloc failed\n", __func__, __LINE__);
				}
			}
			else if (os_strcmp(argv[2], "push") == 0)
			{
				if (argc > 3)
				{
					frame_buffer_t *frame = (frame_buffer_t *)os_strtoul(argv[3], NULL, 16);

					ret = media_app_frame_buffer_push(&handle, frame);

					LOGI("push frame:%p, ret:%d, %d\n", frame, ret, __LINE__);
				}
			}
			else if (os_strcmp(argv[2], "free") == 0)
			{
				if (argc > 3)
				{
					frame_buffer_t *frame = (frame_buffer_t *)os_strtoul(argv[3], NULL, 16);

					ret = media_app_frame_buffer_free(&handle, frame);

					LOGI("free frame:%p, ret:%d, %d\n", frame, ret, __LINE__);
				}
			}
			else
			{
				LOGE("%s, %d param error\n", __func__, __LINE__);
			}
		}
	}

output:

	if (ret == UNKNOW_ERROR)
	{
		LOGE("%s unknow cmd\n", __func__);
	}

	if (ret == PARAMS_ERROR)
	{
		LOGE("%s param error cmd\n", __func__);
	}

	if (ret != BK_OK)
	{
		msg = CLI_CMD_RSP_ERROR;
	}
	else
	{
		msg = CLI_CMD_RSP_SUCCEED;
	}

	LOGI("%s ---complete\n", __func__);

	os_memcpy(pcWriteBuffer, msg, os_strlen(msg));
}

void media_cli_storage_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	int ret = UNKNOW_ERROR;
	char *msg = NULL;
	image_format_t fmt = IMAGE_MJPEG;

	if (argc < 2)
	{
		goto output;
	}

	if (CMD_CONTAIN("h264"))
	{
		fmt = IMAGE_H264;
	}

	if (CMD_CONTAIN("h265"))
	{
		fmt = IMAGE_H265;
	}

	if (os_strcmp(argv[1], "open") == 0)
	{
		ret = media_app_storage_open(NULL);
	}
	else if (os_strcmp(argv[1], "close") == 0)
	{
		ret = media_app_storage_close();
	}
	else if (os_strcmp(argv[1], "open_read") == 0)
	{
		ret = media_app_storage_open(media_read_frame_callback);
		if (ret != BK_OK)
		{
			goto output;
		}

		ret = media_app_save_start(fmt, "unkonw");
	}
	else if (os_strcmp(argv[1], "close_read") == 0)
	{
		ret = media_app_save_stop();
		if (ret != BK_OK)
		{
			goto output;
		}

		ret = media_app_storage_close();
	}
	else if (os_strcmp(argv[1], "capture") == 0)
	{
		if (argc > 3)
		{
			ret = media_app_capture(fmt, argv[3]);
		}
		else
		{
			LOGE("%s, param error, cmd:storage capture mjpeg|h264|h265 capture_name\n", __func__);
			ret = PARAMS_ERROR;
		}
	}
	else if (os_strcmp(argv[1], "save") == 0)
	{
		if (argc > 3)
		{
			ret = media_app_save_start(fmt, argv[3]);
		}
		else
		{
			LOGE("%s, param error, cmd:storage save mjpeg|h264|h265 save_name\n", __func__);
			ret = PARAMS_ERROR;
		}
	}
	else if (os_strcmp(argv[1], "save_stop") == 0)
	{
		ret = media_app_save_stop();
	}
	else
	{
		ret = UNKNOW_ERROR;
	}

output:

	if (ret == UNKNOW_ERROR)
	{
		LOGE("%s unknow cmd\n", __func__);
	}

	if (ret == PARAMS_ERROR)
	{
		LOGE("%s param error cmd\n", __func__);
	}

	if (ret != BK_OK)
	{
		msg = CLI_CMD_RSP_ERROR;
	}
	else
	{
		msg = CLI_CMD_RSP_SUCCEED;
	}

	LOGI("%s ---complete\n", __func__);

	os_memcpy(pcWriteBuffer, msg, os_strlen(msg));
}

void media_cli_transfer_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	int ret = UNKNOW_ERROR;
	char *msg = NULL;
	image_format_t fmt = IMAGE_MJPEG;

	if (argc < 2)
	{
		goto output;
	}

	if (CMD_CONTAIN("h264"))
	{
		fmt = IMAGE_H264;
	}

	if (CMD_CONTAIN("h265"))
	{
		fmt = IMAGE_H265;
	}

	if (os_strcmp(argv[1], "open") == 0)
	{
		ret = media_app_register_read_frame_callback(fmt, media_read_frame_callback);
	}
	else if (os_strcmp(argv[1], "close") == 0)
	{
		ret = media_app_unregister_read_frame_callback();
	}
	else
	{
		ret = UNKNOW_ERROR;
	}

output:

	if (ret == UNKNOW_ERROR)
	{
		LOGE("%s unknow cmd\n", __func__);
	}

	if (ret == PARAMS_ERROR)
	{
		LOGE("%s param error cmd\n", __func__);
	}

	if (ret != BK_OK)
	{
		msg = CLI_CMD_RSP_ERROR;
	}
	else
	{
		msg = CLI_CMD_RSP_SUCCEED;
	}

	LOGI("%s ---complete\n", __func__);

	os_memcpy(pcWriteBuffer, msg, os_strlen(msg));
}

/*blend clock 12:30 | blend wifi wifi0*/
void media_cli_blend_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    int ret = UNKNOW_ERROR;
    char *msg = NULL;

    if (argc < 1)
    {
        goto output;
    }

    blend_info_t blend = {0};
    if (argv[1] != NULL)
        os_strcpy((char *)blend.name, argv[1]);
    if (argv[2] != NULL)
        os_strcpy((char *)blend.content, argv[2]);
    ret = media_app_lcd_blend(&blend);

output:

    if (ret == UNKNOW_ERROR)
    {
        LOGE("%s unknow cmd\n", __func__);
    }

    if (ret == PARAMS_ERROR)
    {
        LOGE("%s param error cmd\n", __func__);
    }

    if (ret != BK_OK)
    {
        msg = CLI_CMD_RSP_ERROR;
    }
    else
    {
        msg = CLI_CMD_RSP_SUCCEED;
    }

    LOGI("%s ---complete\n", __func__);

    os_memcpy(pcWriteBuffer, msg, os_strlen(msg));

}




void media_cli_switch_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	int ret = UNKNOW_ERROR;
	char *msg = NULL;
	image_format_t fmt = IMAGE_MJPEG;
	uint32_t timeout = 10000;//10s

	if (argc < 2)
	{
		goto output;
	}

	if (CMD_CONTAIN("h264"))
	{
		fmt = IMAGE_H264;
	}

	if (CMD_CONTAIN("h265"))
	{
		fmt = IMAGE_H265;
	}

	if (os_strcmp(argv[1], "open") == 0)
	{
		if (argc >= 4)
		{
			timeout = os_strtoul(argv[3], NULL, 10) & 0xFFFFFFFF;
		}
		ret = camera_switch_open(fmt, timeout);
	}
	else if (os_strcmp(argv[1], "close") == 0)
	{
		ret = camera_switch_close();
	}
	else if (os_strcmp(argv[1], "switch") == 0)
	{
		ret = camera_switch_test_mode(fmt);
	}
	else
	{
		ret = UNKNOW_ERROR;
	}

output:

	if (ret == UNKNOW_ERROR)
	{
		LOGE("%s unknow cmd\n", __func__);
	}

	if (ret == PARAMS_ERROR)
	{
		LOGE("%s param error cmd\n", __func__);
	}

	if (ret != BK_OK)
	{
		msg = CLI_CMD_RSP_ERROR;
	}
	else
	{
		msg = CLI_CMD_RSP_SUCCEED;
	}

	LOGI("%s ---complete\n", __func__);

	os_memcpy(pcWriteBuffer, msg, os_strlen(msg));
}
#if (CONFIG_USB_CDC_ACM_DEMO)
extern void ex_cdc_send_msg(uint8_t type, uint32_t param);
extern void demo_bulk_out(uint32_t port);
extern void bk_usb_cdc_demo(void);

void media_cli_for_usb_cdc_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	if (argc > 2) {
		uint8_t port = os_strtoul(argv[2], NULL, 10) & 0xFF;
		if (os_strcmp(argv[1], "open") == 0) {
			bk_usb_cdc_demo();
			rtos_delay_milliseconds(500);///wait the cp1 starting up
			ex_cdc_send_msg(0, port);///CDC_STATUS_IDLE 
		}
		else if (os_strcmp(argv[1], "out") == 0) {
			demo_bulk_out(port);
		}
	}
	else if (os_strcmp(argv[1], "close") == 0) {
		ex_cdc_send_msg(6, 0);///CDC_STATUS_CLOSE 
	}
}
#endif

#define MEDIA_CMD_CNT   (sizeof(s_media_commands) / sizeof(struct cli_command))

static const struct cli_command s_media_commands[] =
{
	{"media", "media...", media_cli_test_cmd},
	{"three_camera", "auto test", media_cli_threecam_auto_test_cmd},
	{"storage", "open|close|capture|save|save_stop...", media_cli_storage_cmd},
	{"transfer", "open fmt|close...", media_cli_transfer_cmd},
	{"test", "open|close|switch fmt", media_cli_switch_cmd},
	{"blend", "wifi0|clock 12:30|bat", media_cli_blend_cmd},
#if (CONFIG_USB_CDC_ACM_DEMO)
	{"cdc_test", "open|out|close|...", media_cli_for_usb_cdc_cmd},
#endif
};

int media_cli_init(void)
{
	return cli_register_commands(s_media_commands, MEDIA_CMD_CNT);
}
