#include <common/bk_include.h>
#include <common/bk_err.h>
#include <os/mem.h>
#include <os/str.h>
#include <os/os.h>
#include <driver/int.h>
#include <common/bk_err.h>
#include <getopt.h>

#include <components/video_transfer.h>
#include <driver/dma.h>
#include <driver/audio_ring_buff.h>
#include "aud_intf.h"

#include "driver/dvp_camera.h"
#include <driver/dvp_camera_types.h>
#include <driver/lcd.h>
#include "media_app.h"
#include "camera_handle_list.h"
#include "img_service.h"

#include "rlk_server_multimedia_devices.h"
#include "wifi_transfer.h"

#define TAG "db-device"

#define LOGI(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

#define DB_SAMPLE_RARE_8K (8000)
#define DB_SAMPLE_RARE_16K (16000)

#define CAMERA_DEVICES_REPORT (BK_FALSE)//(BK_TRUE)


db_device_info_t *db_device_info = NULL;


static aud_intf_drv_setup_t aud_intf_drv_setup = DEFAULT_AUD_INTF_DRV_SETUP_CONFIG();
static aud_intf_work_mode_t aud_work_mode = AUD_INTF_WORK_MODE_NULL;
static aud_intf_voc_setup_t aud_voc_setup = DEFAULT_AUD_INTF_VOC_SETUP_CONFIG();


int mm_devices_set_camera_transfer_callback(void *cb)
{
    if (db_device_info == NULL)
    {
        LOGE("db_device_info null");
        return  BK_FAIL;
    }

    db_device_info->camera_transfer_cb = (media_transfer_cb_t *)cb;

    return BK_OK;
}

int mm_devices_set_audio_transfer_callback(const void *cb)
{
    if (db_device_info == NULL)
    {
        LOGE("db_device_info null");
        return  BK_FAIL;
    }

    db_device_info->audio_transfer_cb = (const media_transfer_cb_t *)cb;

    return BK_OK;
}

int mm_camera_turn_on(camera_parameters_t *parameters)
{
    bk_err_t ret = BK_FAIL;
    uint8_t rot_angle = 0;
    media_camera_device_t device = {0};

    LOGI("%s, id: %d, %d X %d, format: %d, Protocol: %d\n", __func__, 
        parameters->id, parameters->width, parameters->height,
        parameters->format, parameters->protocol);

    if (db_device_info->video_handle != NULL)
    {
        LOGI("%s, id: %d already open\n", __func__, parameters->id);
        return BK_FAIL;
    }

    if (parameters->id == UVC_DEVICE_ID)
    {
        device.type = NET_CAMERA;
        device.port = 1;
        db_device_info->camera_id = 1;
    }
    else
    {
        device.type = DVP_CAMERA;
        device.port = 0;
        db_device_info->camera_id = 0;
    }

    if (parameters->format == 0) // wifi transfer format 0/1:mjpeg/h264
    {
        device.format = IMAGE_MJPEG;
        if (device.type == DVP_CAMERA)
        {
            device.format = IMAGE_YUV | IMAGE_MJPEG;
        }

        db_device_info->h264_transfer = false;
    }
    else
    {
        if (device.type == DVP_CAMERA)
        {
            device.format = IMAGE_YUV | IMAGE_H264;
            db_device_info->pipeline_enable = false;
        }
        else
        {
            device.format = IMAGE_MJPEG;// uvc output mjpeg(not h264 stream)
            db_device_info->pipeline_enable = true;
        }
        db_device_info->h264_transfer = true;
    }

    LOGI("%s, device:fmt:%d, transfer:%s\n", __func__, device.format, db_device_info->h264_transfer ? "h264" : "mjpeg");
    device.width = parameters->width;
    device.height = parameters->height;
    device.fps = FPS30;
#if 0
    ret = media_app_camera_open(&db_device_info->video_handle, &device);

    if (ret != BK_OK)
    {
        LOGE("%s failed\n", __func__);
        return ret;
    }
#endif
    ret = wifi_transfer_net_camera_open(&device);

    if (ret != BK_OK)
    {
        LOGE("%s %d failed\n", __func__,__LINE__);
        return ret;
    }

    // check, output image need rotate or not (for soft jpegdec)
    switch (parameters->rotate)
    {
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
            rot_angle = ROTATE_NONE;
            break;
        default:
            rot_angle = ROTATE_90;
            break;
    }
    media_app_set_rotate(rot_angle);


    if (db_device_info->pipeline_enable)
    {
        ret = media_app_pipeline_h264_open();
        if (ret != BK_OK)
        {
            LOGE("%s h264_pipeline_open failed\n", __func__);
            return ret;
        }
    }

    //jdec video
    if ((device.type == NET_CAMERA)|| (device.type == UVC_CAMERA))
    {
        media_app_pipeline_jdec_open();
    }
    else if (device.type == DVP_CAMERA)
    {
        media_app_frame_jdec_open(NULL);
    }

    return ret;
}

int mm_camera_turn_off(void)
{
#if 0
    if (db_device_info->video_handle == NULL)
    {
        LOGI("%s, %d already close\n", __func__);
        return BK_FAIL;
    }
#endif

    if (db_device_info->pipeline_enable)
    {
        media_app_pipeline_h264_close();
        LOGI("%s h264_pipeline close\n", __func__);
    }

    media_app_pipeline_jdec_close();
    media_app_frame_jdec_close();

#if 0
    do {
        db_device_info->video_handle = bk_camera_handle_node_pop();
        if (db_device_info->video_handle)
        {
            LOGI("%s, %d, %p\n", __func__, __LINE__, db_device_info->video_handle);
            media_app_camera_close(&db_device_info->video_handle);
        }
        else
        {
            break;
        }
    } while (1);
#endif

    wifi_transfer_net_camera_close();

    db_device_info->video_handle = NULL;
    db_device_info->camera_id = CAMERA_MAX_NUM;
    db_device_info->pipeline_enable = false;

    db_device_info->h264_transfer = false;

    return 0;
}


int mm_video_transfer_turn_on(void)
{
    int ret = -1;

    if (db_device_info->transfer_enable)
    {
        LOGI("%s, id: %d already open\n", __func__, db_device_info->transfer_enable);
        return BK_FAIL;
    }

    if (db_device_info->camera_transfer_cb)
    {
        if (db_device_info->h264_transfer)
        {
            ret = bk_wifi_transfer_frame_open(db_device_info->camera_transfer_cb, IMAGE_H264);
        }
        else
        {
            ret = bk_wifi_transfer_frame_open(db_device_info->camera_transfer_cb, IMAGE_MJPEG);
        }
    }
    else
    {
        LOGE("media_transfer_cb: NULL\n");
    }

    if (ret == BK_OK)
    {
        db_device_info->transfer_enable = 1;
    }

    return ret;
}

int mm_video_transfer_turn_off(void)
{
    int ret = -1;

    if (db_device_info->transfer_enable == false)
    {
        LOGI("%s, id: %d already close\n", __func__, db_device_info->transfer_enable);
        return BK_FAIL;
    }

    ret = bk_wifi_transfer_frame_close();

    db_device_info->transfer_enable = false;

    return ret;
}

int mm_display_turn_on(uint16_t id, uint16_t rotate, uint16_t fmt)
{
    LOGI("%s, id: %d, rotate: %d fmt: %d\n", __func__, id, rotate, fmt);

    if (db_device_info->lcd_id != 0)
    {
        LOGI("%s, id: %d already open\n", __func__, id);
        return BK_FAIL;
    }
    const lcd_device_t *device = (const lcd_device_t *)media_app_get_lcd_device_by_id(id);
    if ((uint32_t)device == BK_FAIL || device == NULL)
    {
        LOGI("%s, could not find device id: %d\n", __func__, id);
        return BK_FAIL;
    }

    lcd_open_t lcd_open = {0};
    lcd_open.device_ppi = device->ppi;
    lcd_open.device_name = device->name;

    uint8_t rot_angle = 0;
    if (fmt == 0)
    {
        media_app_lcd_fmt(PIXEL_FMT_RGB565_LE);
    }
    else if (fmt == 1)
    {
        media_app_lcd_fmt(PIXEL_FMT_RGB888);
    }

    switch (rotate)
    {
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
    media_app_set_rotate(rot_angle);
    media_app_lcd_disp_open(&lcd_open);

    db_device_info->lcd_id = id;
    return 0;
}

int mm_display_turn_off(void)
{
    LOGI("%s, id: %d", __func__, db_device_info->lcd_id);

    if (db_device_info->lcd_id == 0)
    {
        LOGI("%s, %d already close\n", __func__);
        return BK_FAIL;
    }

    media_app_lcd_disp_close();

    db_device_info->lcd_id = 0;
    return 0;
}

int mm_voice_send_callback(unsigned char *data, unsigned int len)
{
    if (db_device_info == NULL)
    {
        LOGE("%s, db_device_info NULL\n", __func__);
        return BK_FAIL;
    }

    if (db_device_info->audio_transfer_cb == NULL)
    {
        LOGE("%s, audio_transfer_cb NULL\n", __func__);
        return BK_FAIL;
    }

    if (len > db_device_info->audio_transfer_cb->get_tx_size())
    {
        LOGE("%s, buffer over flow %d %d\n", __func__, len, db_device_info->audio_transfer_cb->get_tx_size());
        return BK_FAIL;
    }

    uint8_t *buffer = db_device_info->audio_transfer_cb->get_tx_buf();

    if (db_device_info->audio_transfer_cb->prepare)
    {
        db_device_info->audio_transfer_cb->prepare(data, len);
    }

    return db_device_info->audio_transfer_cb->send(buffer, len);
}

static void mm_audio_connect_state_cb_handle(uint8_t state)
{
    os_printf("[--%s--] state: %d \n", __func__, state);
}

int mm_audio_turn_on(audio_parameters_t *parameters)
{
    int ret;

    if (db_device_info->audio_enable == BK_TRUE)
    {
        LOGI("%s already turn on\n", __func__);

        return BK_FAIL;
    }

    LOGI("%s, AEC: %d, UAC: %d, sample rate: %d, %d, fmt: %d, %d\n", __func__,
        parameters->aec, parameters->uac, parameters->rmt_recorder_sample_rate,
        parameters->rmt_player_sample_rate, parameters->rmt_recoder_fmt, parameters->rmt_player_fmt);

    if (parameters->aec == 1)
    {
        aud_voc_setup.aec_enable = true;
    }
    else
    {
        aud_voc_setup.aec_enable = false;
    }

    //aud_voc_setup.data_type = AUD_INTF_VOC_DATA_TYPE_G711A;
    //aud_voc_setup.data_type = AUD_INTF_VOC_DATA_TYPE_PCM;
    aud_voc_setup.spk_mode = AUD_DAC_WORK_MODE_SIGNAL_END;
    //aud_voc_setup.mic_en = AUD_INTF_VOC_MIC_OPEN;
    //aud_voc_setup.spk_en = AUD_INTF_VOC_SPK_OPEN;

    if (parameters->uac == 1)
    {
        aud_voc_setup.mic_type = AUD_INTF_MIC_TYPE_UAC;
        aud_voc_setup.spk_type = AUD_INTF_SPK_TYPE_UAC;
        //aud_voc_setup.samp_rate = AUD_INTF_VOC_SAMP_RATE_16K;
    }
    else
    {
        aud_voc_setup.mic_type = AUD_INTF_MIC_TYPE_BOARD;
        aud_voc_setup.spk_type = AUD_INTF_SPK_TYPE_BOARD;
    }

    if (aud_voc_setup.mic_type == AUD_INTF_MIC_TYPE_BOARD && aud_voc_setup.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
            aud_voc_setup.data_type = parameters->rmt_recoder_fmt - 1;
    }

    switch (parameters->rmt_recorder_sample_rate)
    {
        case DB_SAMPLE_RARE_8K:
            aud_voc_setup.samp_rate = 8000;
            break;

        case DB_SAMPLE_RARE_16K:
            aud_voc_setup.samp_rate = 16000;
            break;

        default:
            aud_voc_setup.samp_rate = 8000;
            break;
    }

    aud_intf_drv_setup.aud_intf_tx_mic_data = mm_voice_send_callback;
    ret = bk_aud_intf_drv_init(&aud_intf_drv_setup);
    if (ret != BK_ERR_AUD_INTF_OK)
    {
        LOGE("bk_aud_intf_drv_init fail, ret:%d\n", ret);
        goto error;
    }

    aud_work_mode = AUD_INTF_WORK_MODE_VOICE;
    ret = bk_aud_intf_set_mode(aud_work_mode);
    if (ret != BK_ERR_AUD_INTF_OK)
    {
        LOGE("bk_aud_intf_set_mode fail, ret:%d\n", ret);
        goto error;
    }


    ret = bk_aud_intf_voc_init(aud_voc_setup);
    if (ret != BK_ERR_AUD_INTF_OK)
    {
        LOGE("bk_aud_intf_voc_init fail, ret:%d\n", ret);
        goto error;
    }

    ret = bk_aud_intf_voc_start();
    if (ret != BK_ERR_AUD_INTF_OK)
    {
        LOGE("bk_aud_intf_voc_start fail, ret:%d\n", ret);
        goto error;
    }

    /* uac recover connection */
    if (aud_voc_setup.mic_type == AUD_INTF_MIC_TYPE_UAC)
    {
        ret = bk_aud_intf_register_uac_connect_state_cb(mm_audio_connect_state_cb_handle);
        if (ret != BK_ERR_AUD_INTF_OK)
        {
            LOGE("bk_aud_intf_register_uac_connect_state_cb fail, ret:%d\n", ret);
            goto error;
        }

        ret = bk_aud_intf_uac_auto_connect_ctrl(true);
        if (ret != BK_ERR_AUD_INTF_OK)
        {
            LOGE("aud_tras_uac_auto_connect_ctrl fail, ret:%d\n", ret);
            goto error;
        }
    }

    db_device_info->audio_enable = BK_TRUE;

    return BK_OK;
error:
    aud_work_mode = AUD_INTF_WORK_MODE_NULL;
    bk_aud_intf_set_mode(aud_work_mode);
    bk_aud_intf_drv_deinit();

    return BK_FAIL;
}

int mm_audio_turn_off(void)
{
    if (db_device_info->audio_enable == BK_FALSE)
    {
        LOGI("%s already turn off\n", __func__);

        return BK_FAIL;
    }

    LOGI("%s entry\n", __func__);

    db_device_info->audio_enable = BK_FALSE;

    bk_aud_intf_voc_stop();
    bk_aud_intf_voc_deinit();
    /* deinit aud_tras task */
    aud_work_mode = AUD_INTF_WORK_MODE_NULL;
    bk_aud_intf_set_mode(aud_work_mode);
    bk_aud_intf_drv_deinit();

    LOGI("%s out\n", __func__);

    return 0;
}

void mm_audio_data_callback(uint8_t *data, uint32_t length)
{
    bk_err_t ret = BK_OK;

    ret = bk_aud_intf_write_spk_data(data, length);

    if (ret != BK_OK)
    {
        //LOGE("write speaker data fail\n", length);
    }
}

int mm_devices_init(void)
{
    if (db_device_info == NULL)
    {
        db_device_info = os_malloc(sizeof(db_device_info_t));
    }

    if (db_device_info == NULL)
    {
        LOGE("malloc db_device_info failed");
        return  BK_FAIL;
    }

    os_memset(db_device_info, 0, sizeof(db_device_info_t));
    db_device_info->camera_id = CAMERA_MAX_NUM;

    return BK_OK;
}

void mm_devices_deinit(void)
{
    if (db_device_info)
    {
        os_free(db_device_info);
        db_device_info = NULL;
    }
}
