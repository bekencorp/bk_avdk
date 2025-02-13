#include "bk_uvc_common.h"
#include "uvc_urb_list_common.h"
#include <modules/pm.h>

#define TAG "uvc_stream"

#define LOGI(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

//#define UVC_DEBUG_TIME

#ifdef UVC_DEBUG_TIME
#define UVC_POWER_ON_START()            GPIO_UP(32);
#define UVC_POWER_ON_END()              GPIO_DOWN(32);

#define UVC_INIT_START()                GPIO_UP(33);
#define UVC_INIT_END()                  GPIO_DOWN(33);

#define UVC_EOF_START()                 GPIO_UP(34);
#define UVC_EOF_END()                   GPIO_DOWN(34);
#else
#define UVC_POWER_ON_START()
#define UVC_POWER_ON_END()

#define UVC_INIT_START()
#define UVC_INIT_END()

#define UVC_EOF_START()
#define UVC_EOF_END()
#endif

uvc_stream_handle_t *s_uvc_stream_handle = NULL;
uvc_separate_config_t uvc_separate_packet_cb = {0};
uvc_separate_info_t uvc_separate_info;
camera_state_cb_t uvc_connect_state_cb = NULL;

static void uvc_camera_stream_receive_complete_callback(void *pCompleteParam, int nbytes);
static bk_err_t uvc_camera_stream_rx_config(uint8_t id);
static void uvc_camera_process_task_deinit(uvc_stream_handle_t *uvc_handle);
void uvc_camera_stream_task_deinit(void);

#if (MEDIA_DEBUG_TIMER_ENABLE)
static void uvc_camera_stream_timer_handle(void *arg1)
{
    uvc_stream_handle_t *uvc_stream_handle = (uvc_stream_handle_t *)arg1;

    uvc_pro_config_t *pro_config = uvc_stream_handle->pro_config;

    if (pro_config != NULL)
    {
        LOGW("uvc_id0:%d[%d %dKB], uvc_id1: %d[%d %dKB], uvc_id2:%d[%d %dKB], packets:[all:%d, err:%d]\n",
             ((pro_config->frame_id[0] - pro_config->later_id[0]) / UVC_TIME_INTERVAL),
             pro_config->frame_id[0], (pro_config->curr_length[0] / 1024),
             ((pro_config->frame_id[1] - pro_config->later_id[1]) / UVC_TIME_INTERVAL),
             pro_config->frame_id[1], (pro_config->curr_length[1] / 1024),
             ((pro_config->frame_id[2] - pro_config->later_id[2]) / UVC_TIME_INTERVAL),
             pro_config->frame_id[2], (pro_config->curr_length[2] / 1024),
             pro_config->all_packet_num, pro_config->packet_err_num);

        for (uint8_t i = 0; i < CAMERA_ID_MAX; i++)
        {
            pro_config->later_id[i] = pro_config->frame_id[i];
        }
    }
}
#endif

bk_err_t uvc_stream_task_send_msg(uvc_event_t event, uint32_t param)
{
    int ret = BK_OK;
    uvc_stream_handle_t *uvc_stream_handle = s_uvc_stream_handle;
    uvc_stream_event_t msg;

    if (uvc_stream_handle == NULL
        || uvc_stream_handle->stream_queue == NULL)
    {
        LOGD("%s, %d task have closed\r\n", __func__, event);
        return BK_FAIL;
    }

    msg.event = event;
    msg.param = param;

    ret = rtos_push_to_queue(&uvc_stream_handle->stream_queue, &msg, BEKEN_NO_WAIT);
    if (BK_OK != ret)
    {
        LOGE("push failed, event:%d\r\n", event);
        return ret;
    }

    return ret;
}

static bool uvc_camera_stream_check_all_uvc_closed(uvc_stream_handle_t *uvc_handle)
{
    bool all_closed = true;
    for (uint8_t i = 0; i < CAMERA_ID_MAX; i++)
    {
        if (uvc_handle->camera[i].info) // this vaule not null, must have called open api
        {
            all_closed = false;
            break;
        }
    }

    return all_closed;
}

static bk_err_t uvc_camera_stream_check_config(camera_param_t *param)
{
    int ret = BK_OK;
    uint8_t frame_num = 0;
    uint8_t index = 0;
    uint8_t resolution_flag = false;
    uint8_t fps_flag = false;

    bk_usb_hub_port_info *uvc_port_info = param->port_info;
    uvc_config_t *user_config = param->info;
    LOGI("%s, %d, port:%d, format:%d\r\n", __func__, __LINE__, user_config->port, user_config->img_format);

    bk_uvc_device_brief_info_t *uvc_device_param = (bk_uvc_device_brief_info_t *)uvc_port_info->usb_device_param;
    bk_uvc_config_t *uvc_device_param_config = (bk_uvc_config_t *)uvc_port_info->usb_device_param_config;

    LOGD("PORT:0x%x\r\n", user_config->port);
    LOGD("VID:0x%x\r\n", uvc_device_param->vendor_id);
    LOGD("PID:0x%x\r\n", uvc_device_param->product_id);
    LOGD("BCD:0x%x\r\n", uvc_device_param->device_bcd);
    uvc_device_param_config->vendor_id = uvc_device_param->vendor_id;
    uvc_device_param_config->product_id = uvc_device_param->product_id;

    switch (user_config->img_format)
    {
        case IMAGE_YUV:
            uvc_device_param_config->format_index = uvc_device_param->format_index.yuv_format_index;
            frame_num = uvc_device_param->all_frame.yuv_frame_num;
            for (index = 0; index < frame_num; index++)
            {
                LOGD("YUV width:%d heigth:%d index:%d\r\n",
                     uvc_device_param->all_frame.yuv_frame[index].width,
                     uvc_device_param->all_frame.yuv_frame[index].height,
                     uvc_device_param->all_frame.yuv_frame[index].index);

                if (uvc_device_param->all_frame.yuv_frame[index].width == user_config->width
                    && uvc_device_param->all_frame.yuv_frame[index].height == user_config->height)
                {
                    uvc_device_param_config->frame_index = uvc_device_param->all_frame.yuv_frame[index].index;
                    uvc_device_param_config->width = uvc_device_param->all_frame.yuv_frame[index].width;
                    uvc_device_param_config->height = uvc_device_param->all_frame.yuv_frame[index].height;
                    resolution_flag = true;
                }

                for (int i = 0; i < uvc_device_param->all_frame.yuv_frame[index].fps_num; i++)
                {
                    LOGD("YUV fps:%d\r\n", uvc_device_param->all_frame.yuv_frame[index].fps[i]);

                    if (resolution_flag
                        && uvc_device_param->all_frame.yuv_frame[index].fps[i] == user_config->fps)
                    {
                        uvc_device_param_config->fps = uvc_device_param->all_frame.yuv_frame[index].fps[i];
                        fps_flag = true;
                    }
                }

                if (resolution_flag)
                {
                    // have adapt this resolution
                    if (fps_flag == false)
                    {
                        uvc_device_param_config->fps = uvc_device_param->all_frame.yuv_frame[index].fps[0];
                        fps_flag = true;
                    }
                    break;
                }
            }
            break;

        case IMAGE_MJPEG:
            uvc_device_param_config->format_index = uvc_device_param->format_index.mjpeg_format_index;
            frame_num = uvc_device_param->all_frame.mjpeg_frame_num;
            for (index = 0; index < frame_num; index++)
            {
                LOGD("MJPEG width:%d heigth:%d index:%d\r\n",
                     uvc_device_param->all_frame.mjpeg_frame[index].width,
                     uvc_device_param->all_frame.mjpeg_frame[index].height,
                     uvc_device_param->all_frame.mjpeg_frame[index].index);

                if (uvc_device_param->all_frame.mjpeg_frame[index].width == user_config->width
                    && uvc_device_param->all_frame.mjpeg_frame[index].height == user_config->height)
                {
                    uvc_device_param_config->frame_index = uvc_device_param->all_frame.mjpeg_frame[index].index;
                    uvc_device_param_config->width = uvc_device_param->all_frame.mjpeg_frame[index].width;
                    uvc_device_param_config->height = uvc_device_param->all_frame.mjpeg_frame[index].height;
                    resolution_flag = true;
                }

                // iterate all support fps of current resolution
                for (int i = 0; i < uvc_device_param->all_frame.mjpeg_frame[index].fps_num; i++)
                {
                    LOGD("MJPEG fps:%d\r\n", uvc_device_param->all_frame.mjpeg_frame[index].fps[i]);

                    if (resolution_flag
                        && uvc_device_param->all_frame.mjpeg_frame[index].fps[i] == user_config->fps)
                    {
                        uvc_device_param_config->fps = uvc_device_param->all_frame.mjpeg_frame[index].fps[i];
                        fps_flag = true;
                    }
                }

                if (resolution_flag)
                {
                    // have adapt this resolution
                    if (fps_flag == false)
                    {
                        uvc_device_param_config->fps = uvc_device_param->all_frame.mjpeg_frame[index].fps[0];
                        fps_flag = true;
                    }
                    break;
                }
            }
            break;

        case IMAGE_H264:
            uvc_device_param_config->format_index = uvc_device_param->format_index.h264_format_index;
            frame_num = uvc_device_param->all_frame.h264_frame_num;
            for (index = 0; index < frame_num; index++)
            {
                LOGD("H264 width:%d heigth:%d index:%d\r\n",
                     uvc_device_param->all_frame.h264_frame[index].width,
                     uvc_device_param->all_frame.h264_frame[index].height,
                     uvc_device_param->all_frame.h264_frame[index].index);

                if (uvc_device_param->all_frame.h264_frame[index].width == user_config->width
                    && uvc_device_param->all_frame.h264_frame[index].height == user_config->height)
                {
                    uvc_device_param_config->frame_index = uvc_device_param->all_frame.h264_frame[index].index;
                    uvc_device_param_config->width = uvc_device_param->all_frame.h264_frame[index].width;
                    uvc_device_param_config->height = uvc_device_param->all_frame.h264_frame[index].height;
                    resolution_flag = true;
                }

                // iterate all support fps of current resolution
                for (int i = 0; i < uvc_device_param->all_frame.h264_frame[index].fps_num; i++)
                {
                    LOGD("H264 fps:%d\r\n", uvc_device_param->all_frame.h264_frame[index].fps[i]);

                    if (resolution_flag
                        && uvc_device_param->all_frame.h264_frame[index].fps[i] == user_config->fps)
                    {
                        uvc_device_param_config->fps = uvc_device_param->all_frame.h264_frame[index].fps[i];
                        fps_flag = true;
                    }
                }

                if (resolution_flag)
                {
                    // have adapt this resolution
                    if (fps_flag == false)
                    {
                        uvc_device_param_config->fps = uvc_device_param->all_frame.h264_frame[index].fps[0];
                        fps_flag = true;
                    }
                    break;
                }
            }
            break;

        case IMAGE_H265:
            uvc_device_param_config->format_index = uvc_device_param->format_index.h265_format_index;
            frame_num = uvc_device_param->all_frame.h265_frame_num;
            for (index = 0; index < frame_num; index++)
            {
                LOGD("H265 width:%d heigth:%d index:%d\r\n",
                     uvc_device_param->all_frame.h265_frame[index].width,
                     uvc_device_param->all_frame.h265_frame[index].height,
                     uvc_device_param->all_frame.h265_frame[index].index);

                if (uvc_device_param->all_frame.h265_frame[index].width == user_config->width
                    && uvc_device_param->all_frame.h265_frame[index].height == user_config->height)
                {
                    uvc_device_param_config->frame_index = uvc_device_param->all_frame.h265_frame[index].index;
                    uvc_device_param_config->width = uvc_device_param->all_frame.h265_frame[index].width;
                    uvc_device_param_config->height = uvc_device_param->all_frame.h265_frame[index].height;
                    resolution_flag = true;
                }

                // iterate all support fps of current resolution
                for (int i = 0; i < uvc_device_param->all_frame.h265_frame[index].fps_num; i++)
                {
                    LOGD("H265 fps:%d\r\n", uvc_device_param->all_frame.h265_frame[index].fps[i]);

                    if (resolution_flag
                        && uvc_device_param->all_frame.h265_frame[index].fps[i] == user_config->fps)
                    {
                        uvc_device_param_config->fps = uvc_device_param->all_frame.h265_frame[index].fps[i];
                        fps_flag = true;
                    }
                }

                if (resolution_flag)
                {
                    // have adapt this resolution
                    if (fps_flag == false)
                    {
                        uvc_device_param_config->fps = uvc_device_param->all_frame.h265_frame[index].fps[0];
                        fps_flag = true;
                    }
                    break;
                }
            }
            break;

        default:
            ret = BK_FAIL;
            LOGE("%s, please check usb output format:%d\r\n", __func__, user_config->img_format);
            break;
    }

    if (ret != BK_OK)
    {
        LOGE("%s please check usb output format:%d\r\n", __func__, user_config->img_format);
        return ret;
    }

    if (resolution_flag == false)
    {
        LOGE("%s, not support this resolution:%dX%d\r\n", __func__, user_config->width, user_config->height);
        ret = BK_FAIL;
        return ret;
    }

    uvc_device_param_config->ep_desc = uvc_device_param->ep_desc;

    LOGI("%s, %d\r\n", __func__, __LINE__);

    return BK_OK;
}

static uint8_t uvc_camera_dma_config(void)
{
    dma_config_t dma_config = {0};

    uint8_t channel = bk_dma_alloc(DMA_DEV_DTCM);

    if (channel == DMA_ID_MAX)
    {
        return channel;
    }

    dma_config.mode = DMA_WORK_MODE_SINGLE;
    dma_config.chan_prio = 1;

    dma_config.src.dev = DMA_DEV_DTCM;
    dma_config.src.width = DMA_DATA_WIDTH_32BITS;
    dma_config.src.addr_inc_en = DMA_ADDR_INC_ENABLE;
    dma_config.src.start_addr = 0x28000000;
    dma_config.src.end_addr = 0x28000000 + 1024;

    dma_config.dst.dev = DMA_DEV_DTCM;
    dma_config.dst.width = DMA_DATA_WIDTH_32BITS;
    dma_config.dst.addr_inc_en = DMA_ADDR_INC_ENABLE;
    dma_config.dst.start_addr = 0x28000500;
    dma_config.dst.end_addr = 0x28000500 + 1024;

    BK_LOG_ON_ERR(bk_dma_init(channel, &dma_config));
    BK_LOG_ON_ERR(bk_dma_set_transfer_len(channel, 1024));
#if (CONFIG_SPE && CONFIG_GDMA_HW_V2PX)
    BK_LOG_ON_ERR(bk_dma_set_src_burst_len(channel, BURST_LEN_INC4));
    BK_LOG_ON_ERR(bk_dma_set_dest_burst_len(channel, BURST_LEN_INC4));
    BK_LOG_ON_ERR(bk_dma_set_dest_sec_attr(channel, DMA_ATTR_SEC));
    BK_LOG_ON_ERR(bk_dma_set_src_sec_attr(channel, DMA_ATTR_SEC));
#endif

    return channel;
}

static void uvc_camera_stream_mem_cpy_data(camera_param_t *param, uint8_t *src, uint32_t length)
{
    if (length >= 256 && param->dma_channel != DMA_ID_MAX)
    {
        // threr have question, do not open marco UVC_DMA_CPY_ENABLE
        uint32_t fack_length = (length + 3) & ~0x3;
        BK_WHILE(bk_dma_get_enable_status(param->dma_channel));
        bk_dma_set_src_start_addr(param->dma_channel, (uint32_t)src);
        bk_dma_set_dest_start_addr(param->dma_channel, (uint32_t)(param->frame->frame + param->frame->length));
        bk_dma_set_transfer_len(param->dma_channel, fack_length);
        bk_dma_start(param->dma_channel);
    }
    else
    {
        os_memcpy(param->frame->frame + param->frame->length, src, length);
    }
}

static void uvc_camera_stream_connect_callback(bk_usb_hub_port_info *port_info, uint32_t n)
{
    uint8_t index = port_info->port_index - 1;

    uvc_stream_handle_t *uvc_handle = s_uvc_stream_handle;

    if (index >= CAMERA_ID_MAX || uvc_handle == NULL)
    {
        LOGW("%s, over the max camera range or %p\r\n", __func__, uvc_handle);
        return;
    }

    camera_param_t *camera_param = &uvc_handle->camera[index];

    LOGI("%s, port:%d, state:%d\r\n", __func__, port_info->port_index, camera_param->camera_state);

    if (camera_param->camera_state == UVC_CONNECT_STATE)
    {
        LOGW("%s, %d, port %d already connected...\r\n", __func__, __LINE__, port_info->port_index);
    }
    else if (camera_param->camera_state == UVC_STREAMING_STATE)
    {
        LOGW("%s, port:%d, already streaming....\r\n", __func__, port_info->port_index);
        camera_param->camera_state = UVC_DISCONNECT_STATE;
        uvc_stream_task_send_msg(UVC_DISCONNECT_IND, port_info->port_index);
    }
    else if (camera_param->camera_state == UVC_CLOSING_STATE)
    {
        // uplayer have send stop cmd, but not respones immediately
        LOGW("%s, port:%d, is closing....\r\n", __func__, port_info->port_index);
        return;
    }

    camera_param->port_info = port_info;

    struct usbh_video *video_class = (struct usbh_video *)(port_info->usb_device);
    bk_uvc_device_brief_info_t *uvc_device_param = (bk_uvc_device_brief_info_t *)port_info->usb_device_param;
    usbh_hport_activate_epx(&video_class->isoin, video_class->hport, (struct usb_endpoint_descriptor *)uvc_device_param->ep_desc);

    uvc_handle->connect_camera_count++;
    camera_param->camera_state = UVC_CONNECT_STATE;
    LOGI("%s, port:%d, state:%d\r\n", __func__, port_info->port_index, camera_param->camera_state);
    xEventGroupSetBits(uvc_handle->handle, UVC_CONNECT_BIT);

    if (camera_param->info)
    {
        uvc_stream_task_send_msg(UVC_CONNECT_IND, port_info->port_index);
    }

    if (uvc_connect_state_cb)
    {
        uvc_connect_state_cb(port_info, BK_UVC_CONNECT);
    }
}

static void uvc_camera_stream_disconnect_callback(bk_usb_hub_port_info *port_info, uint32_t n)
{
    uint8_t index = port_info->port_index - 1;
    uint8_t delay_cnt = 5;
    uvc_stream_handle_t *uvc_handle = s_uvc_stream_handle;


    if (index >= CAMERA_ID_MAX || uvc_handle == NULL)
    {
        LOGW("%s, over the max camera range or %p\r\n", __func__, uvc_handle);
        return;
    }

    LOGI("%s, %d, port:%d\r\n", __func__, __LINE__, port_info->port_index);
    camera_param_t *camera_param = &uvc_handle->camera[index];

    xEventGroupClearBits(uvc_handle->handle, UVC_CONNECT_BIT);

    if (camera_param->camera_state == UVC_DISCONNECT_STATE)
    {
        LOGW("%s, %d, this port %d uvc have disconnected\r\n", __func__, __LINE__, port_info->port_index);
        return;
    }

    while (camera_param->camera_state == UVC_CONFIGING_STATE && delay_cnt > 0)
    {
        rtos_delay_milliseconds(2);// this is task callback
        delay_cnt--;
    }

    uvc_handle->connect_camera_count--;
    camera_param->camera_state = UVC_DISCONNECT_STATE;

    camera_param->port_info = NULL;

    if (uvc_connect_state_cb)
    {
        uvc_connect_state_cb(port_info, BK_UVC_DISCONNECT);
    }

    uvc_stream_task_send_msg(UVC_DISCONNECT_IND, (uint32_t)port_info->port_index);
}

static bk_err_t uvc_camera_stream_packet_urb(uint32_t port)
{
    LOGD("%s, %d\r\n", __func__, __LINE__);
    struct usbh_video *uvc_device = NULL;
    struct usbh_hubport *hport = NULL;
    struct usbh_urb *urb = NULL;
    uint8_t index = port - 1;
    camera_param_t *camera_param = &s_uvc_stream_handle->camera[index];
    uvc_pro_config_t *pro_config = s_uvc_stream_handle->pro_config;
    urb = camera_param->urb;

    if (camera_param->port_info == NULL || camera_param->camera_state != UVC_STREAMING_STATE)
    {
        LOGW("%s, param or state error:%d...\r\n", __func__, camera_param->camera_state);
        return BK_UVC_DISCONNECT;
    }

    uvc_device = (struct usbh_video *)(camera_param->port_info->usb_device);
    hport = camera_param->port_info->hport;
    urb->pipe = (usbh_pipe_t)(uvc_device->isoin);
    if (urb->pipe == NULL)
    {
        LOGE("%s, %d\r\n", __func__, __LINE__);
    }
    urb->complete = (usbh_complete_callback_t)uvc_camera_stream_receive_complete_callback;
    urb->arg = (void *)port;

    if (pro_config->transfer_bulk[index]) // bulk transmission
    {
        urb->timeout = 0;
    }
    else // iso transmission
    {
        if (s_uvc_stream_handle->connect_camera_count >= 2)
        {
            if (((struct usbh_hubport *)hport)->speed == 3) //USB_SPEED_HIGH
            {
                urb->timeout = 0;//150us
            }
            if (((struct usbh_hubport *)hport)->speed == 2) //USB_SPEED_FULL
            {
                urb->timeout = 0;//1ms
            }
        }
        else
        {
            urb->timeout = 0;
        }
    }

    return BK_OK;
}

static void uvc_camera_stream_receive_complete_callback(void *pCompleteParam, int nbytes)
{
    LOGD("%s, %d, %d\r\n", __func__, __LINE__, (uint32_t)pCompleteParam);
    struct usbh_urb *urb = NULL;//, *new_urb = NULL;
    uint32_t port = (uint32_t)pCompleteParam;
    uint8_t index = port - 1;
    camera_param_t *camera_param = &s_uvc_stream_handle->camera[index];

    urb = camera_param->urb;

    if (urb == NULL)
    {
        LOGW("%s, %d, %d\r\n", __func__, __LINE__, (uint32_t)pCompleteParam);
        return;
    }

    if (nbytes < 0)
    {
        urb->errorcode = nbytes;
    }

    camera_param->urb = NULL;

    uvc_camera_urb_push(urb);

    if (camera_param->camera_state != UVC_STREAMING_STATE)
    {
        LOGD("[%d]%s, %d, %d\r\n", index, __func__, __LINE__, camera_param->camera_state);
        rtos_set_semaphore(&camera_param->sem);
        return;
    }

    if (uvc_stream_task_send_msg(UVC_DATA_REQUEST_IND, port) != BK_OK)
    {
        LOGI("%s, %d send failed...\r\n", __func__, __LINE__);
    }
}

static bk_err_t uvc_camera_stream_data_request_retry_handle(uint32_t port, int value)
{
    int ret = BK_OK;
    LOGI("%s, %d, port:%d\r\n", __func__, value, port);
    switch (-value)
    {
        case 16:
            LOGD("%s port:%d, Urb is EBUSY\r\n", __func__, port);
            ret = uvc_stream_task_send_msg(UVC_DATA_REQUEST_IND, port);
            break;
        case 19:
            LOGD("%s port:%d, ENODEV Please check device connect\r\n", __func__, port);
            ret = BK_FAIL;
            break;
        case 22:
            LOGD("%s port:%d, EINVAL Please check pipe or urb\r\n", __func__, port);
            //bk_usb_drv_send_msg(USB_DRV_VIDEO_START, id);
            ret = BK_FAIL;
            break;
        case 110:
            LOGD("%s port:%d, ESHUTDOWN Check device Disconnect\r\n", __func__, port);
            ret = BK_FAIL;
            break;
        case 116:
            LOGD("%s port:%d, ETIMEDOUT Timeout wait\r\n", __func__, port);
            ret = uvc_stream_task_send_msg(UVC_DATA_REQUEST_IND, port);
            break;
        default:
            LOGD("%s port:%d, Fail to submit urb:%d\r\n", __func__, value, port);
            ret = BK_FAIL;
            break;
    }

    return ret;
}

static void uvc_camera_stream_data_request_handle(uint32_t param)
{
    struct usbh_urb *new_urb = NULL;
    uint8_t index = param - 1;
    camera_param_t *uvc_param = &s_uvc_stream_handle->camera[index];
    int ret = BK_OK;

    do
    {

        if (uvc_param->camera_state != UVC_STREAMING_STATE)
        {
            LOGW("[%d]%s, %d stream have stoped...\r\n", index, __func__, __LINE__);
            rtos_set_semaphore(&uvc_param->sem);
            break;
        }

        if (uvc_param->urb == NULL)
        {
            new_urb = uvc_camera_urb_malloc();

            if (new_urb)
            {
                // apply data
                uvc_param->urb = new_urb;
            }
            else
            {
                // malloc fail, retry
                rtos_delay_milliseconds(5);
                LOGI("%s, %d retry.....\r\n", __func__, __LINE__);
                if (uvc_stream_task_send_msg(UVC_DATA_REQUEST_IND, param) != BK_OK)
                {
                    LOGW("%s, %d send fail.\r\n", __func__, __LINE__);
                }
                break;
            }
        }

        ret = uvc_camera_stream_packet_urb(param);
        if (ret != BK_OK)
        {
            LOGW("%s, %d, port:%d, disconnect.....\r\n", __func__, __LINE__, param);
            break;
        }

        ret = bk_usbh_hub_dev_request_data(param, USB_UVC_DEVICE, uvc_param->urb);
        {
            if (ret == BK_OK)
            {
                break;
            }
            else if (ret == BK_FAIL)
            {
                LOGW("%s, %d, port:%d, disconnect.....\r\n", __func__, __LINE__, param);
                break;
            }
            else
            {
                ret = uvc_camera_stream_data_request_retry_handle(param, ret);
                if (ret != BK_OK)
                {
                    LOGW("%s, %d, port:%d, retry error:%d.....\r\n", __func__, __LINE__, param, ret);
                    break;
                }
            }
        }

    }
    while (0);


    if (ret != BK_OK && uvc_param->urb)
    {
        uvc_camera_urb_free(uvc_param->urb);
        new_urb = uvc_param->urb = NULL;
    }
}

static void uvc_camera_stream_printf_config(bk_usb_hub_port_info *port_info)
{
    bk_uvc_config_t *uvc_device_param_config = (bk_uvc_config_t *)port_info->usb_device_param_config;

    struct s_bk_usb_endpoint_descriptor *ep_desc = uvc_device_param_config->ep_desc;

    LOGI("=========================================================================\r\n");
    LOGI("------------ Endpoint Descriptor -----------\r\n");
    LOGI("bLength					: 0x%x (%d bytes)\r\n", ep_desc->bLength, ep_desc->bLength);
    LOGI("bDescriptorType				: 0x%x (Endpoint Descriptor)\r\n", ep_desc->bDescriptorType);
    LOGI("bEndpointAddress				: 0x%x (Direction=IN  EndpointID=%d)\r\n", ep_desc->bEndpointAddress, (ep_desc->bEndpointAddress & 0x0F));
    LOGI("bmAttributes				: 0x%x\r\n", ep_desc->bmAttributes);
    LOGI("wMaxPacketSize				: 0x%x (%d bytes)\r\n", ep_desc->wMaxPacketSize, ep_desc->wMaxPacketSize);
    LOGI("bInterval 				: 0x%x (%d ms)\r\n", ep_desc->bInterval, ep_desc->bInterval);
    LOGI("%s uvc_set_param VID:0x%x\r\n", __func__, uvc_device_param_config->vendor_id);
    LOGI("%s uvc_set_param PID:0x%x\r\n", __func__, uvc_device_param_config->product_id);
    LOGI("%s uvc_set_param width:%d\r\n", __func__, uvc_device_param_config->width);
    LOGI("%s uvc_set_param height:%d\r\n", __func__, uvc_device_param_config->height);
    LOGI("%s uvc_set_param fps:%d\r\n", __func__, uvc_device_param_config->fps);
    LOGI("%s uvc_set_param frame_index:%d\r\n", __func__, uvc_device_param_config->frame_index);
    LOGI("%s uvc_set_param format_index:%d\r\n", __func__, uvc_device_param_config->format_index);
}

static bk_err_t uvc_camera_stream_rx_config(uint8_t port)
{
    int ret = BK_OK;
    struct usbh_urb *urb = NULL;
    uint8_t index = port - 1;
    frame_buffer_t *new_frame = NULL;
    uvc_stream_handle_t *uvc_handle = s_uvc_stream_handle;
    camera_param_t *uvc_param = &uvc_handle->camera[index];

    if (uvc_param->camera_state != UVC_CONNECT_STATE || uvc_handle->pro_enable == false)
    {
        LOGE("%s, state:%d, pro_task:%d\r\n", __func__, uvc_param->camera_state, uvc_handle->pro_enable);
        ret = BK_UVC_DISCONNECT;
        return ret;
    }

    // setp 1 check presuppose frame_info
    if (uvc_param->info == NULL)
    {
        LOGE("%s, camera presuppose frame_info is empty....\r\n", __func__);
        ret = BK_UVC_NO_RESOURCE;
        return ret;
    }

    uvc_param->camera_state = UVC_CONFIGING_STATE;

    // step 2: need compare param support and self defined
    ret = uvc_camera_stream_check_config(uvc_param);
    if (ret != BK_OK)
    {
        LOGE("%s, not support this solution, please retry...\r\n", __func__);
        ret = BK_UVC_PPI_ERROR;
        return ret;
    }

    // step 3: open
    //uvc_camera_stream_printf_config(uvc_stream_drv->info[id]);

    if (uvc_separate_packet_cb.uvc_init_packet_cb != NULL && uvc_separate_packet_cb.id == port)
    {
        ret = uvc_separate_packet_cb.uvc_init_packet_cb(uvc_param->info, 1, &uvc_handle->callback);
        if (ret != BK_OK)
        {
            LOGE("%s, %d\r\n", __func__, __LINE__);
            ret = BK_UVC_NO_MEMORY;
            return ret;
        }
    }

    // there can modify to adapt one uvc output two or more stream, there default ues stream0
    uvc_param->stream0 = uvc_handle->callback.frame_init(port, UVC_CAMERA, uvc_param->info->img_format);
    if (uvc_param->stream0 == NULL)
    {
        LOGE("%s, %d not register callback, or stream create fail\r\n", __func__, __LINE__);
        ret = BK_UVC_NOT_PERMIT;
        return ret;
    }

    ret = bk_usbh_hub_port_dev_open(uvc_param->info->port, USB_UVC_DEVICE, uvc_param->port_info);
    if (ret != BK_OK)
    {
        // uvc open fail;
        LOGE("%s, %d, port:%d\n", __func__, __LINE__, uvc_param->info->port);
        ret = BK_UVC_NOT_PERMIT;
        return ret;
    }

#if UVC_DMA_CPY_ENABLE
    if (uvc_param->dma_channel == DMA_ID_MAX)
    {
        uvc_param->dma_channel = uvc_camera_dma_config();
    }
#endif

    // step 4: malloc frame buffer
    if (uvc_param->frame == NULL)
    {
        switch (uvc_param->info->img_format)
        {
            case IMAGE_MJPEG:
                new_frame = uvc_handle->callback.frame_malloc(IMAGE_MJPEG, uvc_param->stream0, CONFIG_JPEG_FRAME_SIZE);
                if (new_frame)
                {
                    new_frame->fmt = PIXEL_FMT_JPEG;
                }
                break;

            case IMAGE_H264:
                new_frame = uvc_handle->callback.frame_malloc(IMAGE_H264, uvc_param->stream0, CONFIG_H264_FRAME_SIZE);
                if (new_frame)
                {
                    new_frame->fmt = PIXEL_FMT_H264;
                }
                break;

            case IMAGE_H265:
                new_frame = uvc_handle->callback.frame_malloc(IMAGE_H265, uvc_param->stream0, CONFIG_H264_FRAME_SIZE);
                if (new_frame)
                {
                    new_frame->fmt = PIXEL_FMT_H265;
                }
                break;

            case IMAGE_YUV:
                new_frame = uvc_handle->callback.frame_malloc(IMAGE_YUV, uvc_param->stream0, uvc_param->info->width * uvc_param->info->height * 2);
                if (new_frame)
                {
                    new_frame->fmt = PIXEL_FMT_YUV422;
                }
                break;

            default:
                break;
        }

        if (new_frame == NULL)
        {
            LOGE("%s, %d, %d\r\n", __func__, __LINE__, uvc_param->info->img_format);
            if (uvc_param->urb)
            {
                uvc_camera_urb_free(uvc_param->urb);
                uvc_param->urb = NULL;
            }
            ret = BK_UVC_NO_RESOURCE;
            return ret;
        }

        new_frame->width = uvc_param->info->width;
        new_frame->height = uvc_param->info->height;
        new_frame->sequence = uvc_handle->pro_config->frame_id[index]++;
        uvc_param->frame = new_frame;
    }

    // step 5: malloc urb
    if (uvc_param->urb == NULL)
    {
        urb = uvc_camera_urb_malloc();
        if (urb == NULL)
        {
            uvc_handle->callback.frame_free(uvc_param->info->img_format, uvc_param->stream0, uvc_param->frame);
            uvc_param->frame = NULL;
            LOGW("%s, malloc fb failed---%d\r\n", __func__, __LINE__);
            ret = BK_UVC_NO_RESOURCE;
            return ret;
        }

        uvc_param->urb = urb;
    }

    // step 6: make sure transmission mode
    bk_uvc_config_t *uvc_config = (bk_uvc_config_t *)uvc_param->port_info->usb_device_param_config;
    uvc_handle->pro_config->transfer_bulk[index] = ((uvc_config->ep_desc->bmAttributes & 0x3) == USB_ENDPOINT_BULK_TRANSFER) ? true : false;
    uvc_handle->pro_config->max_packet_size[index] = uvc_config->ep_desc->wMaxPacketSize > 1024 ? 1024 : uvc_config->ep_desc->wMaxPacketSize;
    LOGI("/*****port:%d, transmission mode:%s, max_packet_zise:%d*****/\r\n", port, uvc_handle->pro_config->transfer_bulk[index] == 1 ? "BULK" : "ISO",
         uvc_handle->pro_config->max_packet_size[index]);

    // step 7: config urb
    uvc_param->camera_state = UVC_STREAMING_STATE;
    ret = uvc_camera_stream_packet_urb(port);
    LOGD("%s, %d, %p, %p, ret:%d\r\n", __func__, __LINE__, urb, uvc_param->urb, ret);

    // step 8: requeset uvc data
    ret = bk_usbh_hub_dev_request_data(port, USB_UVC_DEVICE, urb);
    if (ret != BK_OK)
    {
        // maybe send notify to user
        if (uvc_param->camera_state != UVC_DISCONNECT_STATE)
        {
            uvc_param->camera_state = UVC_CONNECT_STATE;
        }

        uvc_handle->callback.frame_free(uvc_param->info->img_format, uvc_param->stream0, uvc_param->frame);
        uvc_camera_urb_free(uvc_param->urb);
        uvc_param->frame = NULL;
        uvc_param->urb = NULL;
        LOGE("[%d]%s, %d, ret:%d\r\n", port, __func__, __LINE__, ret);
        ret = BK_UVC_NO_RESPON;
    }

    LOGI("[%d]%s, %d, state:%d\r\n", port, __func__, __LINE__, uvc_param->camera_state);

    return ret;
}

static void uvc_camera_stream_stop_handle(uint32_t param)
{
    uint8_t index = param - 1;
    LOGD("%s, %d, %d\r\n", __func__, __LINE__, param);
    uvc_stream_handle_t *uvc_handle = s_uvc_stream_handle;
    camera_param_t *uvc_param = &uvc_handle->camera[index];

    if (uvc_param->camera_state == UVC_CLOSING_STATE)
    {
        if (rtos_get_semaphore(&uvc_param->sem, 100) != BK_OK)
        {
            LOGE("%s, %d timeout\r\n", __func__, __LINE__);
        }
    }

    if (uvc_param->port_info)
    {
        bk_usbh_hub_port_dev_close(param, USB_UVC_DEVICE, uvc_param->port_info);
    }

    // step 1: free urb
    if (uvc_param->urb)
    {
        uvc_camera_urb_free(uvc_param->urb);
        uvc_param->urb = NULL;
    }

    LOGD("%s, %d\r\n", __func__, __LINE__);

    // step 2: free frame_buffer
    if (uvc_param->frame)
    {
        uvc_handle->callback.frame_free(uvc_param->info->img_format, uvc_param->stream0, uvc_param->frame);
        uvc_param->frame = NULL;
    }

    LOGD("%s, %d\r\n", __func__, __LINE__);

    // step 3: free device info
    if (uvc_param->info)
    {
        os_free(uvc_param->info);
        uvc_param->info = NULL;
    }

    LOGD("%s, %d\r\n", __func__, __LINE__);

    // step 4: update camera state to init
    uvc_param->camera_state = UVC_CONNECT_STATE;

    uvc_handle->callback.frame_clear(uvc_param->stream0);

    uvc_handle->callback.frame_deinit(uvc_param->stream0);

    uvc_param->stream0 = NULL;

    if (uvc_separate_packet_cb.uvc_init_packet_cb != NULL && uvc_separate_packet_cb.id == param)
    {
        uvc_separate_packet_cb.uvc_init_packet_cb(NULL, 0, NULL);
    }

    LOGI("%s, %d, %d\r\n", __func__, __LINE__, uvc_param->camera_state);

    xEventGroupSetBits(uvc_handle->handle, UVC_CLOSE_BIT);
}

static bk_err_t uvc_camera_stream_start_handle(uint32_t param)
{
    int ret = BK_OK;

    uint8_t index = param - 1;
    uvc_stream_handle_t *uvc_handle = s_uvc_stream_handle;
    camera_param_t *uvc_param = &uvc_handle->camera[index];

    ret = uvc_camera_stream_rx_config(param);

    if (ret != BK_OK)
    {
        LOGW("uvc config error, camera_id:%d\r\n", param);
        if (uvc_connect_state_cb)
        {
            uvc_connect_state_cb(uvc_param->port_info, ret);
        }
        uvc_camera_stream_stop_handle(param);
    }
    else
    {
        UVC_INIT_END();
    }

    xEventGroupSetBits(uvc_handle->handle, UVC_STREAM_START_BIT);

    return ret;
}

static void uvc_camera_stream_connect_handle(uint32_t param)
{
    int ret = BK_OK;
    uvc_stream_handle_t *uvc_handle = s_uvc_stream_handle;
    uint8_t index = param - 1;

    ret = uvc_camera_stream_rx_config(param);
    if (ret != BK_OK)
    {
        LOGW("uvc config error, camera_id:%d\r\n", param);
        if (uvc_connect_state_cb)
        {
            uvc_connect_state_cb(uvc_handle->camera[index].port_info, ret);
        }

        uvc_camera_stream_stop_handle(param);
    }
}

static void uvc_camera_stream_disconnect_handle(uint32_t param)
{
    uint8_t index = param - 1;

    if (index >= CAMERA_ID_MAX)
    {
        LOGE("%s, over the max camera range\r\n", __func__);
        return;
    }

    camera_param_t *uvc_param = &s_uvc_stream_handle->camera[index];

    // step 1: free urb
    if (uvc_param->urb)
    {
        uvc_camera_urb_free(uvc_param->urb);
        uvc_param->urb = NULL;
    }

    // step 2: free frame_buffer
    if (uvc_param->frame)
    {
        s_uvc_stream_handle->callback.frame_free(uvc_param->info->img_format, uvc_param->stream0, uvc_param->frame);
        uvc_param->frame = NULL;
    }
}

bk_err_t uvc_camera_stream_check_frame_buffer_length(frame_buffer_t *frame, uint32_t total_length)
{
    if (frame->size <= total_length)
    {
        return BK_OK;
    }

    return BK_FAIL;
}

int uvc_camera_stream_check_frame_buffer_sof_eof_mask(frame_buffer_t *frame)
{
    int ret = BK_FAIL;
    uint8_t *data = frame->frame;
    uint32_t length = frame->length;

    switch (frame->fmt)
    {
        case PIXEL_FMT_JPEG:
            if (data[0] == 0xFF && data[1] == 0xD8)
            {
                for (uint32_t i = length - 1; i > length - 20; i--)
                {
                    if (data[i - 1] == 0xFF && data[i] == 0xD9)
                    {
                        ret = i + 1;
                        break;
                    }
                }
            }
            break;

        case PIXEL_FMT_H264:
        case PIXEL_FMT_H265:
            if (data[0] == 0x00 && data[1] == 0x00
                && data[2] == 0x00 && data[3] == 0x01)
            {
                ret = BK_OK;
            }
            break;

        default:
            break;
    }

    return ret;
}


static void uvc_camera_stream_eof_handle(uint8_t index, uvc_pro_config_t *pro_config)
{
    UVC_EOF_START();
    uvc_stream_handle_t *uvc_handle = s_uvc_stream_handle;
    camera_param_t *camera_param = &uvc_handle->camera[index];
    frame_buffer_t *new_frame = NULL, *curr_frame_buffer = camera_param->frame;

    if (pro_config->packet_error[index]
        || curr_frame_buffer->length == 0)
    {
        LOGD("%s, %d, length:%d\r\n", __func__, __LINE__, curr_frame_buffer->length);
        pro_config->packet_error[index] = false; // clear packet_error flag
        curr_frame_buffer->length = 0;
        goto out;
    }

    int check_length = uvc_camera_stream_check_frame_buffer_sof_eof_mask(curr_frame_buffer);

    if (check_length < 0)
    {
        LOGD("%s, %d, frame_length:%d\r\n", __func__, __LINE__, curr_frame_buffer->length);
        curr_frame_buffer->length = 0;
        goto out;
    }
    else if (check_length > 0)
    {
        curr_frame_buffer->length = check_length;
    }
    else
    {
        // h264/h265
    }

#if (MEDIA_DEBUG_TIMER_ENABLE)
    pro_config->curr_length[index] = curr_frame_buffer->length;
#endif

    curr_frame_buffer->timestamp = media_get_current_timer();

    if (camera_param->info->drop_num > 0)
    {
        camera_param->info->drop_num--;
        LOGD("[%d]%s, drop_num:%d\r\n", index, __func__, camera_param->info->drop_num);
    }
    else
    {
        new_frame = uvc_handle->callback.frame_malloc(camera_param->info->img_format, camera_param->stream0, curr_frame_buffer->size);
        if (new_frame)
        {
            new_frame->fmt = curr_frame_buffer->fmt;
            new_frame->width = curr_frame_buffer->width;
            new_frame->height = curr_frame_buffer->height;
            new_frame->sequence = pro_config->frame_id[index]++;
            new_frame->length = 0;
            uvc_handle->callback.frame_complete(camera_param->info->img_format, camera_param->stream0, curr_frame_buffer);
            camera_param->frame = new_frame;
        }
    }

    LOGD("%s, %d, length:%d, fmt:%d\r\n", __func__, __LINE__, curr_frame_buffer->length, curr_frame_buffer->fmt);

    if (new_frame == NULL)
    {
        LOGW("%s, malloc frame error %d\n", __func__, __LINE__);
        curr_frame_buffer->length = 0;
        curr_frame_buffer->sequence = pro_config->frame_id[index]++;
    }

    if (uvc_separate_packet_cb.uvc_eof_packet_cb != NULL && uvc_separate_packet_cb.id == (index + 1))
    {
        uvc_separate_packet_cb.uvc_eof_packet_cb(camera_param->info);
    }

out:

    UVC_EOF_END();
}

static void uvc_camera_stream_packet_process(uint8_t index, uint8_t *payload, uint32_t payload_len)
{
    uvc_stream_handle_t *uvc_handle = s_uvc_stream_handle;
    uvc_pro_config_t *pro_config = uvc_handle->pro_config;
    frame_buffer_t *curr_frame_buffer = uvc_handle->camera[index].frame;
    uint8_t *data = NULL;
    uint8_t header_info = 0;
    uint8_t header_len = 0;
    uint8_t flag_zlp = 0;
    uint8_t flag_lstp = 0;
#if 0
    uint8_t variable_offset = 0;
#endif
    uint32_t data_len = 0;
    uint32_t bulk_req_len = 0;

    uint8_t bulk_trans = pro_config->transfer_bulk[index];

    if (curr_frame_buffer == NULL
        || curr_frame_buffer->frame == NULL)
    {
        LOGE("curr_frame_buffer NULL\n");
        return;
    }

    if (bulk_trans)
    {
        bulk_req_len = pro_config->max_packet_size[index];
        if (payload_len == 0)
        {
            flag_zlp = 1;
            LOGD("%s, payload_len == 0\r\n", __func__);
        }
        else
        {
            if (bulk_req_len != payload_len)
            {
                flag_lstp = 1;
            }
        }
    }
    else if (payload_len == 0)
    {
        // ignore empty payload transfers, for iso transfer
        return;
    }

    LOGD("length:%d, index:%d\n", payload_len, index);

    /********************* processing header *******************/
    if (!flag_zlp)
    {
        LOGD("zlp=%d, lstp=%d, payload_len=%d, first=0x%02x, second=0x%02x\r\n", flag_zlp, flag_lstp, payload_len, payload[0], payload_len > 1 ? payload[1] : 0);

        // make sure this is a header, judge from header length and bit field
        // For SCR, PTS, some vendors not set bit, but also offer 12 Bytes header. so we just check SET condition
        if (payload_len >= payload[0]
            && (payload[0] == 12 || (payload[0] == 2 && !(payload[1] & 0x0C)) || (payload[0] == 6 && !(payload[1] & 0x08)))
            && (payload[1] & 0x80) && !(payload[1] & 0x30)
#ifdef CONFIG_UVC_CHECK_BULK_JPEG_HEADER
            && (!bulk_trans || ((payload[payload[0]] == 0xff) && (payload[payload[0] + 1] == 0xd8)))
#endif
           )
        {
            header_len = payload[0];
            data_len = payload_len - header_len;
            /* checking the end-of-header */
#if 0
            variable_offset = 2;
#endif
            header_info = payload[1];

            LOGD("header=%u info=0x%02x, payload_len = %u\r\n", header_len, header_info, payload_len);

            /* ERR bit defined in Stream Header*/
            if (header_info & 0x40)
            {
                LOGW("bad packet: %02x, head_len:%d error bit set\r\n", header_info, header_len);
                pro_config->packet_error[index] = true;
#if (MEDIA_DEBUG_TIMER_ENABLE)
                pro_config->packet_err_num++;
#endif
                return;
            }
        }
        else
        {
            LOGD("reassembling %u + %u\r\n", curr_frame_buffer->length, payload_len);
            data_len = payload_len;
        }
    }

    if (header_info)
    {
        if (pro_config->head_bit0[index] != (header_info & 1))
        {
            if (curr_frame_buffer->length != 0)
            {
                if (curr_frame_buffer->length < 1024)
                {
                    LOGD("[head_bit0]id:%d, %02x-%02x-%02x-%02x-%02x-%02x\r\n", index,
                         curr_frame_buffer->frame[0],
                         curr_frame_buffer->frame[1],
                         curr_frame_buffer->frame[2],
                         curr_frame_buffer->frame[3],
                         curr_frame_buffer->frame[curr_frame_buffer->length - 2],
                         curr_frame_buffer->frame[curr_frame_buffer->length - 1]);
                }
                uvc_camera_stream_eof_handle(index, pro_config);
                curr_frame_buffer = uvc_handle->camera[index].frame;
            }

            pro_config->packet_error[index] = false;

            pro_config->head_bit0[index] = (header_info & 1);
        }

#if 0 // do not explain pts and last_scr
        if (header_info & (1 << 2))
        {
            pts = DW_TO_INT(payload + variable_offset);
            variable_offset += 4;
        }

        if (header_info & (1 << 3))
        {
            last_scr = DW_TO_INT(payload + variable_offset);
            variable_offset += 6;
        }
#endif
    }

    /********************* processing data *****************/
    if (data_len >= 1)
    {
        data = payload + header_len;

        if (uvc_separate_packet_cb.uvc_separate_packet_cb != NULL && uvc_separate_packet_cb.id == (index + 1))
        {
            uvc_separate_packet_cb.uvc_separate_packet_cb(payload + header_len, data_len, &uvc_separate_info);

            if (uvc_separate_info.data_len != 0)
            {
                data     = uvc_separate_info.data_off;
                data_len = uvc_separate_info.data_len;
            }
            else
            {
                data_len = 0;
            }
        }

        if (data_len >= 1)
        {
            if (pro_config->packet_error[index] == false && uvc_camera_stream_check_frame_buffer_length(curr_frame_buffer, (curr_frame_buffer->length + data_len)) == BK_OK)
            {
                LOGI("%s, %d\r\n", __func__, __LINE__);
                pro_config->packet_error[index] = true;
            }
            else
            {
                if (pro_config->packet_error[index] == false)
                {
                    LOGD("uvc payload = %02x %02x...%02x %02x\n", payload[header_len], payload[header_len + 1], payload[payload_len - 2], payload[payload_len - 1]);
                    uvc_camera_stream_mem_cpy_data(&uvc_handle->camera[index], data, data_len);
                    curr_frame_buffer->length += data_len;
                }
            }
        }
    }

    /* Just ignore the EOF bit if using bulk transfer */
    if (((header_info & (1 << 1)) && !bulk_trans) || flag_zlp || flag_lstp)
    {
        LOGD("eof:%d, bulk_trans:%d, flag_zlp:%d, flag_lstp:%d\r\n", header_info & 0x2, bulk_trans, flag_zlp, flag_lstp);

        /* The EOF bit is set, so publish the complete frame */
        if (curr_frame_buffer->length != 0)
        {
            if (curr_frame_buffer->fmt == PIXEL_FMT_JPEG)
            {
                // some uvc may out eof bit two times
                if (uvc_camera_stream_check_frame_buffer_sof_eof_mask(curr_frame_buffer) > 0)
                {
                    uvc_camera_stream_eof_handle(index, pro_config);
                }
                else
                {
                    LOGD("[EOF_bit]id:%d, %02x-%02x-%02x-%02x-%02x-%02x\r\n", index,
                         curr_frame_buffer->frame[0],
                         curr_frame_buffer->frame[1],
                         curr_frame_buffer->frame[2],
                         curr_frame_buffer->frame[3],
                         curr_frame_buffer->frame[curr_frame_buffer->length - 2],
                         curr_frame_buffer->frame[curr_frame_buffer->length - 1]);
                }
            }
            else
            {
                // for other fmt(h264/yuv), need debug
                uvc_camera_stream_eof_handle(index, pro_config);
            }
        }

        pro_config->packet_error[index] = false;
    }
}

static void uvc_camera_process_task_deinit(uvc_stream_handle_t *uvc_handle)
{
    uvc_pro_config_t *pro_config = uvc_handle->pro_config;
    if (uvc_handle->pro_enable)
    {
        GLOBAL_INT_DECLARATION();
        GLOBAL_INT_DISABLE();
        uvc_handle->pro_enable = false;
        GLOBAL_INT_RESTORE();

        xEventGroupWaitBits(s_uvc_stream_handle->handle, UVC_PROCESS_TASK_DISABLE_BIT, true, true, BEKEN_WAIT_FOREVER);

#if (MEDIA_DEBUG_TIMER_ENABLE)
        if (pro_config->timer.handle)
        {
            rtos_stop_timer(&pro_config->timer);
            rtos_deinit_timer(&pro_config->timer);
        }
#endif

        os_free(pro_config);
        pro_config = NULL;
    }

    uvc_handle->pro_config = NULL;
}

static void uvc_camera_process_task_main(beken_thread_arg_t data)
{
    struct usbh_urb *urb = NULL;
    uint32_t port = 0;
    uint8_t *payload = NULL;
    uvc_stream_handle_t *uvc_handle = (uvc_stream_handle_t *)data;
    uvc_pro_config_t *pro_config = uvc_handle->pro_config;

    uvc_handle->pro_enable = true;
    xEventGroupSetBits(uvc_handle->handle, UVC_PROCESS_TASK_ENABLE_BIT);

    while (uvc_handle->pro_enable)
    {
        urb = uvc_camera_urb_pop();

        if (urb == NULL)
        {
            continue;
        }

        port = (uint32_t)urb->arg;
        uint8_t index = port - 1;

        camera_param_t *camera_param = &uvc_handle->camera[index];
        if (camera_param->camera_state != UVC_STREAMING_STATE)
        {
            LOGD("%s, %d\r\n", __func__, __LINE__);
            uvc_camera_urb_free(urb);
            continue;
        }

        // complete urb error, do not need process
        if (uvc_handle->packet_cb)
        {
            uvc_handle->packet_cb(urb);
        }
        else
        {
            if (urb->errorcode != 0)
            {
                pro_config->packet_error[index] = true;
                // clear error code
                LOGW("%s, %d, %d\n", __func__, __LINE__, urb->errorcode);
                urb->errorcode = 0;
            }
            else
            {
                for (uint8_t i = 0; i < urb->num_of_iso_packets; i++)
                {
#if (MEDIA_DEBUG_TIMER_ENABLE)
                    pro_config->all_packet_num++;
#endif
                    payload = urb->iso_packet[i].transfer_buffer;
                    if (urb->iso_packet[i].errorcode != BK_OK)
                    {
                        LOGW("[%d]%s, %d packet error:%d...\r\n", port, __func__, __LINE__, urb->iso_packet[i].errorcode);
                        pro_config->packet_error[index] = true;
                        // clear error code
                        urb->iso_packet[i].errorcode = 0;
#if (MEDIA_DEBUG_TIMER_ENABLE)
                        pro_config->packet_err_num++;
#endif
                    }
                    else
                    {
                        uvc_camera_stream_packet_process(index, payload, urb->iso_packet[i].actual_length);
                    }
                }
            }
        }

        uvc_camera_urb_free(urb);
    };

    LOGI("%s, %d\r\n", __func__, __LINE__);
    uvc_handle->pro_thread = NULL;
    xEventGroupSetBits(uvc_handle->handle, UVC_PROCESS_TASK_DISABLE_BIT);
    rtos_delete_thread(NULL);
}

void uvc_camera_stream_task_deinit(void)
{
    uvc_stream_handle_t *uvc_handle = s_uvc_stream_handle;
    uint8_t i = 0;

    if (uvc_handle)
    {
        for (i = 0; i < CAMERA_ID_MAX; i++)
        {
            if (uvc_handle->camera[i].urb)
            {
                uvc_camera_urb_free(uvc_handle->camera[i].urb);
                uvc_handle->camera[i].urb = NULL;
            }

            if (uvc_handle->camera[i].frame)
            {
                uvc_handle->callback.frame_free(uvc_handle->camera[i].info->img_format, uvc_handle->camera[i].stream0, uvc_handle->camera[i].frame);
                uvc_handle->camera[i].frame = NULL;
            }

            if (uvc_handle->camera[i].info)
            {
                os_free(uvc_handle->camera[i].info);
                uvc_handle->camera[i].info = NULL;
            }
        }

    }
}

void uvc_camera_stream_task_main(beken_thread_arg_t data)
{
    int ret = BK_OK;
    uvc_stream_handle_t *uvc_handle = (uvc_stream_handle_t *)data;
    xEventGroupSetBits(uvc_handle->handle, UVC_STREAM_TASK_ENABLE_BIT);

    while (1)
    {
        uvc_stream_event_t msg;
        ret = rtos_pop_from_queue(&uvc_handle->stream_queue, &msg, BEKEN_WAIT_FOREVER);
        if (ret == BK_OK)
        {
            LOGD("%s, %d, event:%d\r\n", __func__, __LINE__, msg.event);
            switch (msg.event)
            {
                case UVC_START_IND:
                    uvc_camera_stream_start_handle(msg.param);
                    break;

                case UVC_STOP_IND:
                    uvc_camera_stream_stop_handle(msg.param);
                    break;

                case UVC_CONNECT_IND:
                    uvc_camera_stream_connect_handle(msg.param);
                    break;

                case UVC_DISCONNECT_IND:
                    uvc_camera_stream_disconnect_handle(msg.param);
                    break;

                case UVC_DATA_REQUEST_IND:
                    uvc_camera_stream_data_request_handle(msg.param);
                    break;

#if (CONFIG_STANDARD_DUALSTREAM)
                case UVC_H26X_CONNECT_IND:
                    uvc_camera_stream_h26x_connect_handle(msg.param);
                    break;
                case UVC_H26X_DISCONNECT_IND:
                    uvc_camera_stream_h26x_disconnect_handle(msg.param);
                    break;
                case UVC_H26X_START_IND:
                    LOGI("UVC_H26X_START_IND\r\n");
                    uvc_camera_stream_h26x_start_handle(msg.param);
                    break;
                case UVC_H26X_STOP_IND:
                    uvc_camera_stream_h26x_stop_handle(msg.param);
                    break;
                case UVC_H26X_DATA_REQUEST_IND:
                    uvc_camera_stream_h26x_data_request_handle(msg.param);
                    break;
#endif

                case UVC_EXIT_IND:
                    goto out;
                    break;

                default:
                    break;
            }
        }
    }

out:
    LOGI("%s, exit\r\n", __func__);
    rtos_deinit_queue(&uvc_handle->stream_queue);
    uvc_handle->stream_queue = NULL;
    uvc_handle->stream_thread = NULL;
    xEventGroupSetBits(uvc_handle->handle, UVC_STREAM_TASK_DISABLE_BIT);
    rtos_delete_thread(NULL);
}

static bk_err_t uvc_camera_stream_packet_start_msg(uvc_stream_handle_t *uvc_handle, uvc_config_t *config)
{
    int ret = BK_OK;
    uint32_t port = config->port;
    uint8 index = port - 1;

    if (uvc_handle->camera[index].info == NULL)
    {
        uvc_handle->camera[index].info = (uvc_config_t *)os_malloc(sizeof(uvc_config_t));
        if (uvc_handle->camera[index].info == NULL)
        {
            LOGE("%s malloc frame_info failed\n", __func__);
            return BK_UVC_NO_MEMORY;
        }
    }

    os_memcpy(uvc_handle->camera[index].info, config, sizeof(uvc_config_t));

    ret = uvc_stream_task_send_msg(UVC_START_IND, port);
    if (ret != BK_OK)
    {
        LOGI("%s, %d\r\n", __func__, __LINE__);
        os_free(uvc_handle->camera[index].info);
        uvc_handle->camera[index].info = NULL;
        ret = BK_UVC_NOT_PERMIT;
    }

    LOGD("%s, %d\r\n", __func__, __LINE__);

    return ret;
}

bk_err_t bk_uvc_power_on(uint32_t trigger)
{
    UVC_POWER_ON_START();
    int ret = BK_FAIL;
    uint8_t port = 1;

    // step 1: init uvc stream task
    if (s_uvc_stream_handle == NULL)
    {
        ret = uvc_camera_urb_list_init();
        if (ret != BK_OK)
        {
            LOGE("%s, %d\r\n", __func__, __LINE__);
            return BK_UVC_NO_MEMORY;
        }
    
        s_uvc_stream_handle = (uvc_stream_handle_t *)os_malloc(sizeof(uvc_stream_handle_t));
        if (s_uvc_stream_handle == NULL)
        {
            LOGE("s_uvc_stream_handle malloc failed\n");
            ret = BK_UVC_NO_MEMORY;
            return ret;
        }

        os_memset(s_uvc_stream_handle, 0, sizeof(uvc_stream_handle_t));

        bk_pm_module_vote_cpu_freq(PM_DEV_ID_USB_1, PM_CPU_FRQ_480M);

        s_uvc_stream_handle->handle = xEventGroupCreate();

        for (uint8_t i = 0; i < CAMERA_ID_MAX; i++)
        {
            ret = rtos_init_semaphore(&s_uvc_stream_handle->camera[i].sem, 1);
            if (BK_OK != ret)
            {
                LOGE("%s uvc_stream->camera[i].sem init failed\n", __func__);
                goto error;
            }

            s_uvc_stream_handle->camera[i].dma_channel = DMA_ID_MAX;
        }

        ret = rtos_init_queue(&s_uvc_stream_handle->stream_queue,
                              "uvc_stream_que",
                              sizeof(uvc_stream_event_t),
                              20);
        if (BK_OK != ret)
        {
            LOGE("%s stream_queue init failed\n", __func__);
            goto error;
        }

        xEventGroupClearBits(s_uvc_stream_handle->handle, UVC_STREAM_TASK_ENABLE_BIT);

        ret = rtos_create_thread(&s_uvc_stream_handle->stream_thread,
                                       BEKEN_DEFAULT_WORKER_PRIORITY - 2,
                                       "uvc_stream_task",
                                       (beken_thread_function_t)uvc_camera_stream_task_main,
                                       1536,
                                       (beken_thread_arg_t)s_uvc_stream_handle);

        if (BK_OK != ret)
        {
            LOGE("%s uvc stream task init failed\n", __func__);
            goto error;
        }

        xEventGroupWaitBits(s_uvc_stream_handle->handle, UVC_STREAM_TASK_ENABLE_BIT, true, true, BEKEN_WAIT_FOREVER);
    }
    else
    {
        ret = BK_OK;
        LOGW("%s, already power on\n", __func__);
        UVC_POWER_ON_END();
        return ret;
    }

    xEventGroupClearBits(s_uvc_stream_handle->handle, UVC_CONNECT_BIT);

    // step 3: power on uvc
    for (port = 1; port < UVC_PORT_MAX; port++)
    {
        // step 3.1: register connect callback
        ret = bk_usbh_hub_port_register_connect_callback(port, USB_UVC_DEVICE, uvc_camera_stream_connect_callback, NULL);
        if (ret != BK_OK)
        {
            LOGE("%s, %d, port:%d\r\n", __func__, __LINE__, port);
            ret = BK_UVC_NOT_PERMIT;
            goto error;
        }

        // step 3.2: register disconect callback
        ret = bk_usbh_hub_port_register_disconnect_callback(port, USB_UVC_DEVICE, uvc_camera_stream_disconnect_callback, NULL);
        if (ret != BK_OK)
        {
            LOGE("%s, %d, port:%d\r\n", __func__, __LINE__, port);
            ret = BK_UVC_NOT_PERMIT;
            goto error;
        }

        // step 3.3: power_on
        ret = bk_usbh_hub_multiple_devices_power_on(USB_HOST_MODE, port, USB_UVC_DEVICE);
        if (ret != BK_OK)
        {
            LOGE("%s, %d, port:%d\r\n", __func__, __LINE__, port);
            ret = BK_UVC_POWER_ERROR;
            goto error;
        }
    }

    if (trigger)
    {
        // check already connected
        for (port = 1; port < UVC_PORT_MAX; port++)
        {
            ret = bk_usbh_hub_port_check_device(port, USB_UVC_DEVICE, (bk_usb_hub_port_info **)&s_uvc_stream_handle->camera[port - 1].port_info);
            if (ret == BK_OK)
            {
                // already connected
                break;
            }
        }

        if (ret != BK_OK) // need wait connect callback
        {
            ret = xEventGroupWaitBits(s_uvc_stream_handle->handle, UVC_CONNECT_BIT, true, true, 4000); // 4s timeout
            if (ret != UVC_CONNECT_BIT)
            {
                LOGE("%s, %d, %x:ret, connect timeout\r\n", __func__, __LINE__, ret);
                ret = BK_UVC_POWER_ERROR;
                goto error;
            }
        }
    }

    ret = BK_OK;

    LOGI("%s, %d, complete....\r\n", __func__, __LINE__);

    UVC_POWER_ON_END();

    return ret;

error:
    // create task error, need free some mem

    if (s_uvc_stream_handle)
    {
        if (s_uvc_stream_handle->stream_thread)
        {
            xEventGroupClearBits(s_uvc_stream_handle->handle, UVC_STREAM_TASK_DISABLE_BIT);
            if (uvc_stream_task_send_msg(UVC_EXIT_IND, 0) != BK_OK)
            {
                LOGE("%s, %d\r\n", __func__, __LINE__);
            }

            xEventGroupWaitBits(s_uvc_stream_handle->handle, UVC_STREAM_TASK_DISABLE_BIT, true, true, BEKEN_WAIT_FOREVER);
            uvc_camera_stream_task_deinit();
        }
        else
        {
            if (s_uvc_stream_handle->stream_queue)
            {
                rtos_deinit_queue(&s_uvc_stream_handle->stream_queue);
                s_uvc_stream_handle->stream_queue = NULL;
            }

            for (uint8_t k = 0; k < CAMERA_ID_MAX; k++)
            {
                if (s_uvc_stream_handle->camera[k].sem)
                {
                    rtos_deinit_semaphore(&s_uvc_stream_handle->camera[k].sem);
                    s_uvc_stream_handle->camera[k].sem = NULL;
                }
            }

            if (s_uvc_stream_handle->handle)
            {
                vEventGroupDelete(s_uvc_stream_handle->handle);
            }
        }

        uvc_camera_urb_list_deinit();

        os_free(s_uvc_stream_handle);
        s_uvc_stream_handle = NULL;
    }

    for (port = 1; port < UVC_PORT_MAX; port++)
    {
        bk_usbh_hub_port_register_connect_callback(port, USB_UVC_DEVICE, NULL, NULL);
        bk_usbh_hub_port_register_disconnect_callback(port, USB_UVC_DEVICE, NULL, NULL);
        bk_usbh_hub_multiple_devices_power_down(USB_HOST_MODE, port, USB_UVC_DEVICE);
    }

    bk_pm_module_vote_cpu_freq(PM_DEV_ID_USB_1, PM_CPU_FRQ_DEFAULT);

    LOGI("%s, %d failed\r\n", __func__, __LINE__);

    return ret;
}

bk_err_t bk_uvc_power_off(void)
{
    int ret = BK_OK;

    uvc_stream_handle_t *uvc_handle = s_uvc_stream_handle;

    if (uvc_handle == NULL)
    {
        LOGE("%s, %d task have closed\r\n", __func__, __LINE__);
        return ret;
    }

    if (uvc_camera_stream_check_all_uvc_closed(uvc_handle))
    {
        LOGI("%s, %d\r\n", __func__, __LINE__);
        uvc_camera_process_task_deinit(uvc_handle);

        for (uint8_t port = 1; port < UVC_PORT_MAX; port++)
        {
            bk_usbh_hub_port_register_connect_callback(port, USB_UVC_DEVICE, NULL, NULL);
            bk_usbh_hub_port_register_disconnect_callback(port, USB_UVC_DEVICE, NULL, NULL);
            bk_usbh_hub_multiple_devices_power_down(USB_HOST_MODE, port, USB_UVC_DEVICE);
            uvc_handle->camera[port - 1].camera_state = UVC_CLOSED_STATE;
        }
        uvc_camera_stream_task_deinit();
        uvc_camera_urb_list_deinit();

#if (CONFIG_STANDARD_DUALSTREAM)
        if (!uvc_handle->pro_enable && !uvc_handle->h26x_pro_enable)
#else
        if (!uvc_handle->pro_enable)
#endif
        {
            xEventGroupClearBits(uvc_handle->handle, UVC_STREAM_TASK_DISABLE_BIT);
            ret = uvc_stream_task_send_msg(UVC_EXIT_IND, 0);
            if (ret != BK_OK)
            {
                LOGE("%s, %d\r\n", __func__, __LINE__);
            }

            xEventGroupWaitBits(uvc_handle->handle, UVC_STREAM_TASK_DISABLE_BIT, true, true, BEKEN_WAIT_FOREVER);
            for (uint8_t i = 0; i < CAMERA_ID_MAX; i++)
            {
                if (uvc_handle->camera[i].sem)
                {
                    rtos_deinit_semaphore(&uvc_handle->camera[i].sem);
                    uvc_handle->camera[i].sem = NULL;
                }

                if (uvc_handle->camera[i].dma_channel != DMA_ID_MAX)
                {
                    bk_dma_stop(uvc_handle->camera[i].dma_channel);
                    bk_dma_deinit(uvc_handle->camera[i].dma_channel);
                    bk_dma_free(DMA_DEV_DTCM, uvc_handle->camera[i].dma_channel);
                    uvc_handle->camera[i].dma_channel = DMA_ID_MAX;
                }
            }

            if (uvc_handle->handle)
            {
                vEventGroupDelete(uvc_handle->handle);
            }

            os_free(s_uvc_stream_handle);
            s_uvc_stream_handle = NULL;

            bk_pm_module_vote_cpu_freq(PM_DEV_ID_USB_1, PM_CPU_FRQ_DEFAULT);
        }
        else
        {
            ret = BK_FAIL;
        }
    }
    else
    {
        LOGE("%s, %d all camera not been closed\r\n", __func__, __LINE__);
        ret = BK_FAIL;
        return ret;
    }

    return ret;
}

bk_err_t bk_uvc_init(camera_handle_t *handle, uvc_config_t *config, bk_uvc_callback_t *cb)
{
    int ret = BK_OK;
    camera_config_t *output_handle = NULL;
    uvc_stream_handle_t *uvc_handle = s_uvc_stream_handle;

    UVC_INIT_START();

    if (uvc_handle == NULL
        || config == NULL
        || config->port == 0
        || config->port > UVC_PORT_MAX)
    {
        LOGE("%s, %d task or param error.....\r\n", __func__, config->port);
        ret = BK_FAIL;
        goto out;
    }

    if (uvc_handle->camera[config->port - 1].camera_state == UVC_STREAMING_STATE)
    {
        LOGW("%s, camera %d already start\r\n", __func__, config->port);
        ret = BK_FAIL;
        goto out;
    }
    else if (uvc_handle->camera[config->port - 1].camera_state == UVC_CLOSED_STATE)
    {
        // in this situation, this port device may enum slow, or not insert, need wait some secconds
        uint8_t wait_times = 30;

        do {
            ret = bk_usbh_hub_port_check_device(config->port, USB_UVC_DEVICE, (bk_usb_hub_port_info **)&uvc_handle->camera[config->port - 1].port_info);
            if (ret == BK_OK)
            {
                // already connected
                bk_usb_hub_port_info *port_info = uvc_handle->camera[config->port - 1].port_info;
                uvc_handle->camera[config->port - 1].camera_state = UVC_CONNECT_STATE;
                struct usbh_video *video_class = (struct usbh_video *)(port_info->usb_device);
                bk_uvc_device_brief_info_t *uvc_device_param = (bk_uvc_device_brief_info_t *)port_info->usb_device_param;
                usbh_hport_activate_epx(&video_class->isoin, video_class->hport, (struct usb_endpoint_descriptor *)uvc_device_param->ep_desc);
                break;
            }

            wait_times--;
            rtos_delay_milliseconds(200);
        } while (wait_times);

        if (uvc_handle->camera[config->port - 1].camera_state == UVC_CLOSED_STATE)
        {
            ret = BK_FAIL;
            LOGW("%s, this port:%d not connect\n", __func__, config->port);
            goto out;
        }
    }
    else if (uvc_handle->camera[config->port - 1].camera_state == UVC_CLOSING_STATE ||
        uvc_handle->camera[config->port - 1].camera_state == UVC_DISCONNECT_STATE ||
        uvc_handle->camera[config->port - 1].camera_state == UVC_CONFIGING_STATE)
    {
        ret = BK_FAIL;
        LOGW("%s, this port:%d state:%d cannot open\r\n", __func__, config->port, uvc_handle->camera[config->port - 1].camera_state);
        goto out;
    }

    // step 2: init uvc process task
    if (uvc_handle->pro_thread == NULL)
    {
        uvc_handle->pro_config = (uvc_pro_config_t *)os_malloc(sizeof(uvc_pro_config_t));
        if (uvc_handle->pro_config == NULL)
        {
            LOGE("s_uvc_pro_config malloc failed\n");
            ret = BK_UVC_NO_MEMORY;
            goto out;
        }

        os_memset(uvc_handle->pro_config, 0, sizeof(uvc_pro_config_t));
        os_memcpy(&uvc_handle->callback, cb, sizeof(bk_uvc_callback_t));

#if (MEDIA_DEBUG_TIMER_ENABLE)
        ret = rtos_init_timer(&uvc_handle->pro_config->timer, UVC_TIME_INTERVAL * 1000,
                              uvc_camera_stream_timer_handle, uvc_handle);

        if (ret != BK_OK)
        {
            os_free(uvc_handle->pro_config);
            uvc_handle->pro_config = NULL;
            LOGE("%s, init uvc_handle->pro_config->timer fail....\r\n", __func__);
            ret = BK_UVC_NO_MEMORY;
            goto out;
        }

        rtos_start_timer(&uvc_handle->pro_config->timer);
#endif

        xEventGroupClearBits(uvc_handle->handle, UVC_PROCESS_TASK_ENABLE_BIT);

        ret = rtos_create_thread(&uvc_handle->pro_thread,
                                       BEKEN_DEFAULT_WORKER_PRIORITY - 3,
                                       "uvc_pro_task",
                                       (beken_thread_function_t)uvc_camera_process_task_main,
                                       1024,
                                       (beken_thread_arg_t)uvc_handle);

        if (BK_OK != ret)
        {
            LOGE("%s uvc process task init failed\n", __func__);
            os_free(uvc_handle->pro_config);
            uvc_handle->pro_config = NULL;
            goto out;
        }

        xEventGroupWaitBits(uvc_handle->handle, UVC_PROCESS_TASK_ENABLE_BIT, true, true, BEKEN_WAIT_FOREVER);
    }

    LOGI("%s, %d, port_id:%d\r\n", __func__, __LINE__, config->port);

    // ensure uvc must be connected
    if (uvc_handle->camera[config->port - 1].camera_state == UVC_CONNECT_STATE)
    {
        output_handle = (camera_config_t *)os_malloc(sizeof(camera_config_t));
        if (output_handle == NULL)
        {
            LOGW("%s, output_handle malloc fail\n", __func__);
            goto out;
        }

        os_memset(output_handle, 0, sizeof(camera_config_t));
        xEventGroupClearBits(uvc_handle->handle, UVC_STREAM_START_BIT);
        ret = uvc_camera_stream_packet_start_msg(uvc_handle, config);
        if (ret != BK_OK)
        {
            goto out;
        }

        xEventGroupWaitBits(uvc_handle->handle, UVC_STREAM_START_BIT, true, true, BEKEN_WAIT_FOREVER);

        if (uvc_handle->camera[config->port - 1].camera_state != UVC_STREAMING_STATE)
        {
            LOGW("%s, start fail....\n", __func__);
            ret = BK_FAIL;
        }
    }
    else
    {
        LOGW("%s, this port:%d state:%d cannot open\r\n", __func__, config->port, uvc_handle->camera[config->port - 1].camera_state);
        ret = BK_FAIL;
    }

out:

    if (ret != BK_OK)
    {
        UVC_INIT_END();
        if (output_handle)
        {
            os_free(output_handle);
            output_handle = NULL;
        }
    }
    else
    {
        output_handle->type = UVC_CAMERA;
        output_handle->fps = FPS30;
        output_handle->id = config->port;
        output_handle->image_format = config->img_format;
        output_handle->width = config->width;
        output_handle->height = config->height;
        output_handle->arg = (void *)uvc_handle->camera[config->port - 1].port_info;
        *handle = (camera_handle_t)output_handle;
    }

    return ret;
}

bk_err_t bk_uvc_deinit(camera_handle_t *handle)
{
    int ret = BK_OK;

    if (*handle == NULL)
    {
        LOGW("%s, %d already closed\r\n", __func__, __LINE__);
        return ret;
    }

    camera_config_t *config = (camera_config_t *)*handle;
    uint8_t port = config->id;

    LOGI("%s, %d, %d\r\n", __func__, __LINE__, port);

    if (s_uvc_stream_handle == NULL
        || port == 0
        || port >= UVC_PORT_MAX)
    {
        LOGE("%s, %d task have closed\r\n", __func__, __LINE__);
        os_free(config);
        *handle = NULL;
        goto out;
    }

    camera_param_t *uvc_param = &s_uvc_stream_handle->camera[port - 1];

    if (uvc_param->camera_state == UVC_CLOSED_STATE)
    {
        LOGE("%s, %d camera have been closed\r\n", __func__, __LINE__);
        goto out;
    }
    else if (uvc_param->camera_state == UVC_CONNECT_STATE || uvc_param->camera_state == UVC_DISCONNECT_STATE)
    {
        LOGE("%s, %d camera not start\r\n", __func__, __LINE__);
        goto out;
    }
    else if (uvc_param->camera_state == UVC_STREAMING_STATE || uvc_param->camera_state == UVC_CONFIGING_STATE)
    {
        uvc_param->camera_state = UVC_CLOSING_STATE;
    }

    xEventGroupClearBits(s_uvc_stream_handle->handle, UVC_CLOSE_BIT);

    if (uvc_stream_task_send_msg(UVC_STOP_IND, port) != BK_OK)
    {
        ret = BK_FAIL;
    }
    else
    {
        xEventGroupWaitBits(s_uvc_stream_handle->handle, UVC_CLOSE_BIT, true, true, BEKEN_NEVER_TIMEOUT);
        os_free(config);
        *handle = NULL;
    }

out:
    LOGI("%s, %d complete\r\n", __func__, __LINE__);

    return ret;
}

bk_usb_hub_port_info *bk_uvc_get_enum_info(uint8_t port)
{
    uint8_t count = 10;
    if (s_uvc_stream_handle == NULL
        || port == 0
        || port >= UVC_PORT_MAX)
    {
        LOGE("%s, %d %p param error\r\n", __func__, port, s_uvc_stream_handle);
        return NULL;
    }

    do
    {
        if (s_uvc_stream_handle->camera[port - 1].port_info)
        {
            break;
        }
        else
        {
            int ret = bk_usbh_hub_port_check_device(port, USB_UVC_DEVICE, (bk_usb_hub_port_info **)&s_uvc_stream_handle->camera[port - 1].port_info);

            if (ret != BK_OK)
            {
                rtos_delay_milliseconds(500);
                count--;
            }
            else
            {
                break;
            }
        }

    }
    while (count);

    if (count == 0)
    {
        return NULL;
    }

    return s_uvc_stream_handle->camera[port - 1].port_info;
}

bk_err_t bk_uvc_set_param(uvc_config_t *config)
{
    LOGI("%s, %d\r\n", __func__, __LINE__);
    int ret = BK_FAIL;
    uint8_t index = config->port - 1;

    if (s_uvc_stream_handle == NULL
        || config->port == 0
        || config->port >= UVC_PORT_MAX)
    {
        LOGE("%s, %d task or param error...\r\n", __func__, __LINE__);
        return ret;
    }

    camera_param_t *uvc_param = &s_uvc_stream_handle->camera[index];

    if (uvc_param->camera_state != UVC_CONNECT_STATE)
    {
        LOGE("[%d]%s, %d  state:%d, camera not connect success!\r\n",
             config->port, __func__, __LINE__, uvc_param->camera_state);
        return ret;
    }

    // there can change by self, default only change camera resolution
    //os_memcpy(uvc_stream->camera[config->id].info, config, sizeof(sizeof(media_camera_device_t)));
    uvc_param->info->width = config->width;
    uvc_param->info->height = config->height;

    ret = uvc_stream_task_send_msg(UVC_CONNECT_IND, config->port);
    if (ret != BK_OK)
    {
        LOGE("[%d]%s, %d\r\n", config->port, __func__, __LINE__);
    }

    return ret;
}

bk_err_t bk_uvc_register_packet_cb(void *cb)
{
    uvc_stream_handle_t *uvc_handle = s_uvc_stream_handle;
    if (uvc_handle == NULL)
    {
        LOGE("%s, %d，uvc stream task not init\r\n", __func__, __LINE__);
        return BK_FAIL;
    }

    if (uvc_handle->packet_cb)
    {
        LOGE("%s, %d，have been register\r\n", __func__, __LINE__);
        return BK_FAIL;
    }

    uvc_handle->packet_cb = cb;

    return BK_OK;
}

bk_err_t bk_uvc_register_connect_state_cb(camera_state_cb_t cb)
{
    if (!uvc_connect_state_cb)
    {
        uvc_connect_state_cb = cb;
    }
    return BK_OK;
}

bk_err_t bk_uvc_register_separate_packet_callback(uvc_separate_config_t *cb)
{
    uvc_separate_packet_cb.id                     = cb->id;
    uvc_separate_packet_cb.uvc_eof_packet_cb      = cb->uvc_eof_packet_cb;
    uvc_separate_packet_cb.uvc_init_packet_cb     = cb->uvc_init_packet_cb;
    uvc_separate_packet_cb.uvc_separate_packet_cb = cb->uvc_separate_packet_cb;

    return BK_OK;
}


