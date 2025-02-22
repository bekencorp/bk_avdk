// Copyright 2024-2025 Beken
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
//#include "media_cli.h"

//#include "media_cli_comm.h"
#include "media_app.h"

#define TAG "test"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)


typedef struct {
	uint8_t enable;
	uint8_t mode;
	uint16_t img_format;
	uint32_t timeout;
	beken_semaphore_t sem;
	beken_thread_t thread;
} camera_switch_t;

static camera_switch_t *s_camera_switch = NULL;

static void camera_switch_read_frame_callback(frame_buffer_t *frame)
{
	if (frame && frame->sequence % 30 == 0)
	{
		LOGI("frame_id:%d, length:%d, h264_fmt:%x, frame_addr:%p\r\n", frame->sequence,
			frame->length, frame->h264_type,frame->frame);
		//rtos_delay_milliseconds(40);
	}

	rtos_delay_milliseconds(20);
}

void camera_switch_test_entry(void *param)
{
	camera_switch_t *config = (camera_switch_t *)param;
	media_camera_device_t device = DEFAULT_CAMERA_CONFIG();
	uint16_t format = IMAGE_UNKNOW;
	uint32_t test_times = 0;
	uint32_t timer = config->timeout;
	camera_handle_t handle = NULL;

	rtos_set_semaphore(&config->sem);

	config->enable = true;

	// step 1: open lcd display
	media_app_set_rotate(ROTATE_90);
	lcd_open_t lcd_open =
	{
		.device_ppi = PPI_480X854,
		.device_name = "st7701sn",
	};
	media_app_lcd_disp_open(&lcd_open);

	// step 2: enable wifi transfer
	media_app_register_read_frame_callback(config->img_format, camera_switch_read_frame_callback);


	while (config->enable)
	{
		test_times++;
		format = config->img_format;

		LOGI("-----------------%s test time:%d---------------\n", __func__, test_times);

		{
			// open uvc1
			device.type = UVC_CAMERA;
			device.format = IMAGE_MJPEG;
			device.fps = FPS30;
			device.width = 864;
			device.height = 480;
			device.port = 1;
			media_app_camera_open(&handle, &device);
		}

		{
			// open pipeline jpeg decode and rotate
			media_app_pipeline_jdec_open();
		}

		if (format == IMAGE_H264)
		{
			// open h264 encode pipeline and yuv rotate pipeline
			media_app_pipeline_h264_open();
		}

		rtos_delay_milliseconds(timer);

		if (format == IMAGE_H264)
		{
			// close h264 encode pipeline
			media_app_pipeline_h264_close();
		}

		{
			// close pipeline jpeg decode and rotate
			media_app_pipeline_jdec_close();
		}

		{
			// close uvc1
			media_app_camera_close(&handle);
		}

		if (config->enable == false)
		{
			break;
		}

		{
			// open lcd complete yuv frame rotate
			media_app_frame_jdec_open(NULL);
		}

		{
			// open dvp
			device.type = DVP_CAMERA;
			if (format == IMAGE_H264)
			{
				device.format = IMAGE_YUV | IMAGE_H264;
			}
			else
			{
				device.format = IMAGE_YUV | IMAGE_MJPEG;
			}

			device.port = 0;
			media_app_camera_open(&handle, &device);
		}

		rtos_delay_milliseconds(timer);

		{
			// close lcd complete yuv frame rotate
			media_app_frame_jdec_close();
		}

		{
			// close dvp
			media_app_camera_close(&handle);
		}

		if (config->enable == false)
		{
			break;
		}

		{
			// open uvc2
			device.type = UVC_CAMERA;
			device.format = IMAGE_MJPEG;
			device.port = 2;
			media_app_camera_open(&handle, &device);
		}

		{
			// open pipeline jpeg decode and yuv rotate pipeline
			media_app_pipeline_jdec_open();
		}

		if (format == IMAGE_H264)
		{
			// open h264 encode pipeline
			media_app_pipeline_h264_open();
		}

		rtos_delay_milliseconds(timer);

		if (format == IMAGE_H264)
		{
			// close h264 encode pipeline
			media_app_pipeline_h264_close();
		}

		{
			// close pipeline jpeg decode and yuv roate pipeline
			media_app_pipeline_jdec_close();
		}

		{
			// close uvc2
			media_app_camera_close(&handle);
		}
	}

	// check and close all function
	media_app_pipeline_jdec_close();
	media_app_frame_jdec_close();

	media_app_unregister_read_frame_callback();

	media_app_pipeline_h264_close();

	media_app_lcd_disp_close();

	config->thread = NULL;
	rtos_set_semaphore(&config->sem);
	rtos_delete_thread(NULL);
}

bk_err_t camera_switch_open(uint16_t format, uint32_t test_times)
{
	int ret = BK_FAIL;

	LOGI("%s, %d, format:%s\n", __func__, format, format == IMAGE_MJPEG ? "JPEG_MODE" : "H264_MODE");

	camera_switch_t *config = s_camera_switch;

	if (config && config->enable)
	{
		LOGW("%s, %d already enable\n", __func__, __LINE__);
		return ret;
	}

	config = (camera_switch_t *)os_malloc(sizeof(camera_switch_t));
	if (config == NULL)
	{
		LOGE("%s, %d malloc fail\n", __func__, __LINE__);
		return ret;
	}

	os_memset(config, 0, sizeof(camera_switch_t));

	ret = rtos_init_semaphore(&config->sem, 1);
	if (ret != BK_OK)
	{
		LOGE("%s, %d init sem\n", __func__, __LINE__);
		goto out;
	}

	config->img_format = format;
	config->timeout = test_times;

	ret = rtos_create_thread(&config->thread,
							BEKEN_APPLICATION_PRIORITY,
							"camera_switch",
							(beken_thread_function_t)camera_switch_test_entry,
							1024 * 2,
							(beken_thread_arg_t)config);
	if (ret != BK_OK)
	{
		LOGE("%s, %d create task fail\n", __func__, __LINE__);
		goto out;
	}

	s_camera_switch = config;

	rtos_get_semaphore(&config->sem, BEKEN_NEVER_TIMEOUT);

	return ret;

out:

	if (config)
	{
		if (config->sem)
		{
			rtos_deinit_semaphore(&config->sem);
		}

		os_free(config);

		s_camera_switch = NULL;
	}

	return ret;
}

bk_err_t camera_switch_close(void)
{
	int ret = BK_OK;

	LOGI("%s, %d\n", __func__, __LINE__);

	camera_switch_t *config = s_camera_switch;

	if (config == NULL || config->enable == false)
	{
		LOGW("%s, %d already close\n", __func__, __LINE__);
		return ret;
	}

	config->enable = false;

	rtos_get_semaphore(&config->sem, BEKEN_NEVER_TIMEOUT);

	rtos_deinit_semaphore(&config->sem);

	os_free(config);

	s_camera_switch = NULL;

	return ret;
}


bk_err_t camera_switch_test_mode(uint16_t format)
{
	int ret = BK_FAIL;

	LOGI("%s, %d, format:%s\n", __func__, format, format == IMAGE_MJPEG ? "JPEG_MODE" : "H264_MODE");

	camera_switch_t *config = s_camera_switch;

	if (config == NULL || config->enable == false)
	{
		LOGE("%s, %d already close\n", __func__, __LINE__);
		return ret;
	}

	if (config->img_format == format)
	{
		LOGW("%s, %d already been set target mode, do not need switch\n", __func__, __LINE__);
	}
	else
	{
		media_app_unregister_read_frame_callback();
		media_app_register_read_frame_callback(format, camera_switch_read_frame_callback);
		config->img_format = format;
	}

	ret = BK_OK;

	return ret;
}

