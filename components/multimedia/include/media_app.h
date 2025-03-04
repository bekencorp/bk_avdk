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

#pragma once

#include <common/bk_include.h>
#include <driver/uvc_camera_types.h>
#include <components/video_types.h>
#include <components/usb_types.h>
#include <driver/h264_types.h>
#include "frame_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*media_transfer_send_cb)(uint8_t *data, uint32_t length);
typedef int (*media_transfer_prepare_cb)(uint8_t *data, uint32_t length);
typedef void* (*media_transfer_get_tx_buf_cb)(void);
typedef int (*media_transfer_get_tx_size_cb)(void);
typedef void (*frame_cb_t)(frame_buffer_t *frame);
typedef bool (*media_transfer_drop_check_cb)(frame_buffer_t *frame,uint32_t count, uint16_t ext_size);

typedef struct {
	media_transfer_send_cb send;
	media_transfer_prepare_cb prepare;
	media_transfer_drop_check_cb drop_check;
	media_transfer_get_tx_buf_cb get_tx_buf;
	media_transfer_get_tx_size_cb get_tx_size;
} media_transfer_cb_t;

bk_err_t media_app_camera_open(camera_handle_t *handle, media_camera_device_t *device);
bk_err_t media_app_camera_close(camera_handle_t *handle);
bk_err_t media_app_switch_main_camera(uint16_t id, camera_type_t type, image_format_t format);
bk_err_t media_app_get_main_camera_stream(frame_list_node_t *node);
bk_err_t media_app_get_h264_encode_config(h264_base_config_t *config);
bk_err_t media_app_set_compression_ratio(compress_ratio_t * ratio);
bk_err_t media_app_uvc_register_info_notify_cb(camera_state_cb_t cb);
bk_err_t media_app_set_uvc_device_param(uvc_config_t *config);
bk_err_t media_app_register_read_frame_callback(image_format_t fmt, frame_cb_t cb);
bk_err_t media_app_unregister_read_frame_callback(void);
bk_err_t media_app_usb_open(video_setup_t *setup_cfg);
bk_err_t media_app_usb_close(void);

bk_err_t media_app_storage_open(frame_cb_t cb);
bk_err_t media_app_storage_close(void);
bk_err_t media_app_capture(image_format_t fmt, char *name);
bk_err_t media_app_save_start(image_format_t fmt, char *name);
bk_err_t media_app_save_stop(void);
bk_err_t media_app_mailbox_test(void);
bk_err_t media_app_transfer_pause(bool pause);

uint32_t media_app_get_lcd_devices_num(void);
uint32_t media_app_get_lcd_devices_list(void);
uint32_t media_app_get_lcd_device_by_id(uint32_t id);
bk_err_t media_app_get_lcd_status(void);

bk_err_t media_app_lvgl_open(void *lcd_open);
bk_err_t media_app_lvgl_close(void);
bk_err_t media_app_lvcam_lvgl_open(void *lcd_open);
bk_err_t media_app_lvcam_lvgl_close(void);


/******************************displa / decode / rotate API******************************************************/
/**
 * @brief  init lcd and display driver, open lcd display task, open decode by line task
 */
bk_err_t media_app_pipeline_jdec_disp_open(void *config);
/**
 * @brief  close display and decode task
 */
bk_err_t media_app_pipeline_jdec_disp_close(void);

/**
 * @brief  init lcd and display driver, open lcd display task, not open decode, rotate
 */
bk_err_t media_app_lcd_disp_open(void *config);
/**
 * @brief  deinit display driver and close task
 */
bk_err_t media_app_lcd_disp_close(void);

/**
 * @brief open task of decode and rotate, decode by line
 */
bk_err_t media_app_pipeline_jdec_open(void);
/**
 * @brief close task of decode and rotate, decode by line
 */

bk_err_t media_app_pipeline_jdec_close(void);

/**
 * @brief open task of decode and rotate, decode by frame
 */
bk_err_t media_app_frame_jdec_open(void *lcd_open);

/**
 * @brief close task of decode and rotate, decode by frame
 */
bk_err_t media_app_frame_jdec_close(void);

/**
 * @brief set rotate angle, need set before API "media_app_pipeline_jdec_open" and "media_app_frame_jdec_open"
 */
bk_err_t media_app_set_rotate(media_rotate_t rotate);

/**
 * @brief set lcd output RGB565 or RGB888, if RGB888 use sw rotate
 */
bk_err_t media_app_lcd_fmt(pixel_format_t fmt);
bk_err_t media_app_lcd_set_backlight(uint8_t level);

bk_err_t media_app_lcd_blend(void *param);
bk_err_t media_app_lcd_blend_close(void);
bk_err_t media_app_lcd_blend_open(void);

/******************************h264 API******************************************************/
bk_err_t media_app_pipeline_h264_open(void);
bk_err_t media_app_pipeline_h264_close(void);
bk_err_t media_app_h264_regenerate_idr(camera_type_t type);

bk_err_t media_app_pipeline_dump(void);

bk_err_t media_app_pipeline_mem_show(void);
bk_err_t media_app_pipeline_mem_leak(void);

// before use this api, must call media_app_camera_open() to create a stream, and return camera handle
frame_buffer_t *media_app_frame_buffer_malloc(camera_handle_t *handle);
bk_err_t media_app_frame_buffer_push(camera_handle_t *handle, frame_buffer_t *frame);
bk_err_t media_app_frame_buffer_free(camera_handle_t *handle, frame_buffer_t *frame);

#ifdef __cplusplus
}
#endif
