#ifndef _BK_UVC_COMMON_H_
#define _BK_UVC_COMMON_H_

#include <common/bk_err.h>
#include <common/bk_include.h>
#include <bk_list.h>

#include <os/os.h>
#include <os/mem.h>
#include <stdio.h>

#include <components/usbh_hub_multiple_classes_api.h>
#include <driver/uvc_camera.h>
#include <driver/dma.h>

#include "FreeRTOS.h"
#include "event_groups.h"

#define INDEX_MASK(bit)   (1U << bit)
#define INDEX_UNMASK(bit) (~(1U << bit))

#define UVC_STREAM_TASK_ENABLE_BIT   INDEX_MASK(0)
#define UVC_STREAM_TASK_DISABLE_BIT  INDEX_MASK(1)
#define UVC_PROCESS_TASK_ENABLE_BIT  INDEX_MASK(2)
#define UVC_PROCESS_TASK_DISABLE_BIT INDEX_MASK(3)
#define UVC_STREAM_START_BIT         INDEX_MASK(4)

#define UVC_CONNECT_BIT INDEX_MASK(6)
#define UVC_CLOSE_BIT   INDEX_MASK(7)

#define UVC_STREAM_H26X_TASK_ENABLE_BIT   INDEX_MASK(8)
#define UVC_STREAM_H26X_TASK_DISABLE_BIT  INDEX_MASK(9)
#define UVC_H26X_PROCESS_TASK_ENABLE_BIT  INDEX_MASK(10)
#define UVC_H26X_PROCESS_TASK_DISABLE_BIT INDEX_MASK(11)
#define UVC_H26X_CONNECT_BIT              INDEX_MASK(12)
#define UVC_H26X_CLOSE_BIT                INDEX_MASK(13)
#define UVC_DMA_CPY_ENABLE        (0)
#define UVC_PORT_MAX                      (CONFIG_USBHOST_HUB_PORT_SUPPORT_MAX_DEVICE)
#define CAMERA_ID_MAX                     (UVC_PORT_MAX - 1)

#define UVC_TIME_INTERVAL       (4)


typedef enum
{
	UVC_CONNECT_IND = 0,
	UVC_DISCONNECT_IND,
	UVC_START_IND,
	UVC_STOP_IND,
	UVC_SUSPEND_IND,
	UVC_RESUME_IND,
	UVC_SET_PARAM_IND,
	UVC_DATA_REQUEST_IND,
	UVC_DATA_CLEAR_IND,

#if (CONFIG_STANDARD_DUALSTREAM)
	UVC_H26X_CONNECT_IND,
	UVC_H26X_DISCONNECT_IND,
	UVC_H26X_START_IND,
	UVC_H26X_STOP_IND,
	UVC_H26X_DATA_REQUEST_IND,
#endif

    UVC_EXIT_IND,
    UVC_UNKNOW_IND,
} uvc_event_t;

typedef struct
{
    uvc_event_t event;
    uint32_t  param;
} uvc_stream_event_t;

typedef struct
{
	uint8_t camera_state;
	uint8_t dma_channel;
	beken_semaphore_t sem;
	uvc_config_t *info;
	struct usbh_urb *urb;
	frame_buffer_t *frame;
	bk_usb_hub_port_info *port_info;
	void *stream0;
	void *stream1;
#if CONFIG_STANDARD_DUALSTREAM
	uvc_config_t *sec_info;
	uint8_t h26x_camera_state;
	struct usbh_urb * h26x_urb;
	frame_buffer_t * h26x_frame;
	bk_usb_hub_port_info *h26x_port_info;
#endif
} camera_param_t;

typedef struct
{
    uint8_t transfer_bulk[CAMERA_ID_MAX];// transfer ways, 1:for bulk, 0:for iso
    uint8_t packet_error[CAMERA_ID_MAX];
    uint8 head_bit0[CAMERA_ID_MAX];
    uint8_t dma[CAMERA_ID_MAX];
    uint16_t max_packet_size[CAMERA_ID_MAX]; // transfer max packet size
    uint32_t frame_id[CAMERA_ID_MAX];

#if (MEDIA_DEBUG_TIMER_ENABLE)
    beken_timer_t timer;
    uint32_t later_id[CAMERA_ID_MAX];
    uint32_t curr_length[CAMERA_ID_MAX];
    uint32_t all_packet_num;
    uint32_t packet_err_num;
#endif
} uvc_pro_config_t;

typedef struct {
    LIST_HEADER_T list;
    camera_param_t node;
} uvc_node_t;

typedef struct
{
	uint8_t pro_enable;
	EventGroupHandle_t handle;
	beken_thread_t stream_thread;
	beken_queue_t stream_queue;
	beken_thread_t pro_thread;
	camera_param_t camera[CAMERA_ID_MAX];
	uint8_t connect_camera_count;
	uvc_pro_config_t *pro_config; // uvc process config info
	void (*packet_cb)(struct usbh_urb *urb);
	bk_uvc_callback_t callback;
#if (CONFIG_STANDARD_DUALSTREAM)
	uint8_t h26x_pro_enable;
	beken_thread_t h26x_pro_thread;
	uvc_pro_config_t *h26x_pro_config; // uvc process config info
#endif
} uvc_stream_handle_t;

#if (CONFIG_STANDARD_DUALSTREAM)

extern uvc_stream_handle_t *s_uvc_stream_handle;
extern uvc_separate_config_t uvc_separate_packet_cb;
extern uvc_separate_info_t uvc_separate_info;
extern camera_state_cb_t uvc_connect_state_cb;

bk_err_t uvc_stream_task_send_msg(uvc_event_t event, uint32_t param);
int uvc_camera_stream_check_frame_buffer_sof_eof_mask(frame_buffer_t *frame);
bk_err_t uvc_camera_stream_check_frame_buffer_length(frame_buffer_t *frame, uint32_t total_length);

void uvc_camera_stream_task_main(beken_thread_arg_t data);
void uvc_camera_stream_task_deinit(void);

//bk_err_t bk_uvc_h26x_power_on(uint32_t trigger);
//bk_err_t bk_uvc_h26x_power_off(void);

//bk_err_t bk_uvc_h26x_init(uvc_config_t *config, bk_uvc_callback_t *cb);
//bk_err_t bk_uvc_h26x_deinit(uint8_t port);
//bk_usb_hub_port_info *bk_uvc_h26x_get_enum_info(uint8_t port);

void uvc_camera_stream_h26x_connect_handle(uint32_t param);
void uvc_camera_stream_h26x_disconnect_handle(uint32_t param);
bk_err_t uvc_camera_stream_h26x_start_handle(uint32_t param);
void uvc_camera_stream_h26x_stop_handle(uint32_t param);
void uvc_camera_stream_h26x_data_request_handle(uint32_t param);
#endif



#endif
