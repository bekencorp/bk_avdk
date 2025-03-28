#include "bk_private/bk_init.h"
#include <components/system.h>
#include <os/os.h>
#include <components/shell_task.h>

#include "doorbell_comm.h"
#include "media_service.h"

#include "cli.h"
#if (CONFIG_LVGL)
#include "lvgl.h"
#include "lv_vendor.h"
#endif
#include "driver/drv_tp.h"

#if (CONFIG_SYS_CPU1) || (CONFIG_SYS_CPU0)
#include "img_service.h"
#include "media_app.h"
#include "media_evt.h"
#include <driver/lcd.h>
#endif

extern void user_app_main(void);
extern void rtos_set_user_app_entry(beken_thread_function_t entry);

#if (CONFIG_SYS_CPU0)
#include "doorbell_devices.h"

static bool lvcam_is_open = false;

extern db_device_info_t *db_device_info;

media_camera_device_t camera_device = {
    .type = DVP_CAMERA,
    .format = IMAGE_MJPEG,
    .fps = FPS30,
    .width = 864,
    .height = 480,
    .port = 0,
};

const lcd_open_t lcd_open =
{
    .device_ppi = PPI_480X854,
    .device_name = "st7701sn",
};

static void media_app_camera_switch(media_camera_device_t *device)
{
    os_printf("%s\r\n", __func__);
    bk_err_t ret;

    if (db_device_info->video_handle != NULL) {
        ret = media_app_pipeline_h264_close();
        if (ret != BK_OK)
        {
            os_printf("media_app_pipeline_h264_close failed\n");
            return;
        }

        ret = media_app_frame_jdec_close();
        if (ret != BK_OK) {
            os_printf("media_app_frame_jdec_close failed\r\n");
            return;
        }

        ret = media_app_pipeline_jdec_close();
        if (ret != BK_OK) {
            os_printf("media_app_pipeline_jdec_close failed\r\n");
            return;
        }

        ret = media_app_camera_close(&db_device_info->video_handle);
        if (ret != BK_OK) {
            os_printf("media_app_camera_close failed\r\n");
            return;
        }
    }

    media_app_set_rotate(ROTATE_90);

    ret = media_app_camera_open(&db_device_info->video_handle, device);
    if (ret != BK_OK) {
        os_printf("media_app_camera_open failed\r\n");
        return;
    }

    if (device->type == DVP_CAMERA) {
        ret = media_app_frame_jdec_open(NULL);
        if (ret != BK_OK) {
            os_printf("media_app_frame_jdec_open failed\r\n");
            return;
        }
    } else {
        ret = media_app_pipeline_jdec_open();
        if (ret != BK_OK) {
            os_printf("media_app_pipeline_jdec_open failed\r\n");
            return;
        }

        if (db_device_info->h264_transfer) {
            ret = media_app_pipeline_h264_open();
            if (ret != BK_OK)
            {
                os_printf("media_app_pipeline_h264_open failed\n");
                return;
            }
        }
    }
}

void lvcamera_open(uint8_t device_id, media_camera_device_t *device)
{
    os_printf("%s\r\n", __func__);
    bk_err_t ret;

    if (device == NULL) {
        os_printf("%s device is NULL!\r\n", __func__);
        return;
    }

    if (db_device_info->camera_id != device_id) {
        media_app_camera_switch(device);
        db_device_info->camera_id = device_id;
    }

    if (!lvcam_is_open) {
        ret = media_app_lvcam_lvgl_close();
        if (ret != BK_OK) {
            os_printf("media_app_lvcam_lvgl_close failed\r\n");
            return;
        }

        lvcam_is_open = true;
    }
}

void lvcamera_close(void)
{
    os_printf("%s\r\n", __func__);
    bk_err_t ret;

    if (lvcam_is_open) {
        ret = media_app_lvcam_lvgl_open((lcd_open_t *)&lcd_open);
        if (ret != BK_OK) {
            os_printf("media_app_lvcam_lvgl_open failed\r\n");
            return;
        }

        lvcam_is_open = false;
    }
}


#define CMDS_COUNT  (sizeof(s_lvcamera_commands) / sizeof(struct cli_command))

void cli_lvcamera_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    uint32_t device_id = 0;

    if (argc < 2) {
        os_printf("[%s] The cmd is lvcam {camera_open|camera_switch} {0|1|2}\r\n", __func__);
        os_printf("[%s] The cmd is lvcam camera_close\r\n", __func__);
        os_printf("[%s] The cmd is lvcam lvgl_reopen\r\n", __func__);
        return;
    } else if (argc == 2) {
        if (os_strcmp(argv[1], "camera_close") == 0) {
            lvcamera_close();
        }

        if (os_strcmp(argv[1], "lvgl_reopen") == 0) {
            lvcam_is_open = false;
            media_app_lvcam_lvgl_open((lcd_open_t *)&lcd_open);
        }
    } else {
        device_id = os_strtoul(argv[2], NULL, 10) & 0xFFFFFFFF;
        camera_device.port = device_id;

        if ((device_id == 1) || (device_id == 2)) {
            camera_device.type = UVC_CAMERA;
            camera_device.format = IMAGE_MJPEG;
            if (argv[3]!= NULL && os_strcmp(argv[3], "h264") == 0) {
                db_device_info->h264_transfer = true;
            }
        } else if (device_id == 0) {
            camera_device.type = DVP_CAMERA;
            if (db_device_info->h264_transfer || (argv[3]!= NULL  && os_strcmp(argv[3], "h264") == 0)) {
                camera_device.format = IMAGE_YUV | IMAGE_H264;
            } else {
                camera_device.format = IMAGE_YUV | IMAGE_MJPEG;
            }
        } else {
            os_printf("Unsupported device ID! The range of device id is from 0 to 2!\r\n");
            return;
        }

        if (os_strcmp(argv[1], "camera_open") == 0) {
            lvcamera_open(device_id, &camera_device);
        }
    }
}

static const struct cli_command s_lvcamera_commands[] =
{
    {"lvcam", "lvcam", cli_lvcamera_cmd},
};

int cli_lvcamera_init(void)
{
    return cli_register_commands(s_lvcamera_commands, CMDS_COUNT);
}
#endif


#if (CONFIG_SYS_CPU1)
#include "frame_buffer.h"
#include "lcd_display_service.h"
#include "lv_vendor.h"
#include "driver/media_types.h"
#include "media_evt.h"


extern uint8_t lvgl_disp_enable;
extern lv_vnd_config_t vendor_config;
extern frame_buffer_t *lvgl_frame_buffer;
lv_obj_t * qr;

static void lv_example_qrcode(void)
{
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(lv_scr_act(), 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * label = lv_label_create(lv_scr_act());
    lv_obj_set_width(label, LV_SIZE_CONTENT);
    lv_obj_set_height(label, LV_SIZE_CONTENT);
    lv_obj_set_x(label, 0);
    lv_obj_set_y(label, 200);
    lv_obj_set_align(label, LV_ALIGN_TOP_MID);
    lv_label_set_text(label, "Welcome to BEKEN");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_32, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_color_t bg_color = lv_palette_lighten(LV_PALETTE_LIGHT_BLUE, 5);
    lv_color_t fg_color = lv_palette_darken(LV_PALETTE_BLUE, 4);
    qr = lv_qrcode_create(lv_scr_act(), 260, fg_color, bg_color);

    /*Set data*/
    const char * data = "http://www.bekencorp.com";
    lv_qrcode_update(qr, data, os_strlen(data));
    lv_obj_align(qr, LV_ALIGN_CENTER, 0, 40);

    /*Add a border with bg_color*/
    lv_obj_set_style_border_color(qr, bg_color, 0);
    lv_obj_set_style_border_width(qr, 5, 0);
}

void lvgl_event_open_handle(media_mailbox_msg_t *msg)
{
    os_printf("%s EVENT_LVGL_OPEN_IND \n", __func__);

    lvgl_disp_enable = 1;

    lv_vnd_config_t lv_vnd_config = {0};
    lcd_open_t *lcd_open = (lcd_open_t *)msg->param;

    frame_buffer_t *lv_frame_buffer= frame_buffer_display_malloc(ppi_to_pixel_x(lcd_open->device_ppi) * ppi_to_pixel_y(lcd_open->device_ppi) * sizeof(lv_color_t));
    if (lv_frame_buffer == NULL) {
        os_printf("[%s] lv_frame_buffer malloc fail\r\n", __func__);
        msg_send_rsp_to_media_major_mailbox(msg, BK_FAIL, APP_MODULE);
        return;
    }

    lv_vnd_config.draw_pixel_size = (60 * 1024) / sizeof(lv_color_t);
    lv_vnd_config.draw_buf_2_1 = LV_MEM_CUSTOM_ALLOC(lv_vnd_config.draw_pixel_size * sizeof(lv_color_t));
    lv_vnd_config.draw_buf_2_2 = NULL;
    lv_vnd_config.frame_buf_1 = (lv_color_t *)lv_frame_buffer->frame;
    lv_vnd_config.frame_buf_2 = NULL;

    lv_vnd_config.lcd_hor_res = ppi_to_pixel_x(lcd_open->device_ppi);
    lv_vnd_config.lcd_ver_res = ppi_to_pixel_y(lcd_open->device_ppi);
    lv_vnd_config.rotation = ROTATE_NONE;

#if (CONFIG_TP)
    drv_tp_open(ppi_to_pixel_x(lcd_open->device_ppi), ppi_to_pixel_y(lcd_open->device_ppi), TP_MIRROR_NONE);
#endif

    lv_vendor_init(&lv_vnd_config);

    lv_vendor_disp_lock();
    lv_example_qrcode();
    lv_vendor_disp_unlock();

    lv_vendor_start();

    msg_send_rsp_to_media_major_mailbox(msg, BK_OK, APP_MODULE);
}

void lvgl_event_close_handle(media_mailbox_msg_t *msg)
{
    lv_vendor_stop();

    lvgl_disp_enable = 0;

    lv_vendor_deinit();

#if (CONFIG_TP)
    drv_tp_close();
#endif

    os_printf("%s\r\n", __func__);

    msg_send_rsp_to_media_major_mailbox(msg, BK_OK, APP_MODULE);
}

void lvgl_event_lvcam_lvgl_open_handle(media_mailbox_msg_t *msg)
{
    lvgl_disp_enable = 1;

    lv_vendor_start();

//    lv_vendor_disp_lock();
//    lv_example_qrcode();
//    lv_vendor_disp_unlock();

    //if you return to displaying a static image, no need to redraw, otherwise you need to redraw ui.
    lcd_display_frame_request(lvgl_frame_buffer);

    msg_send_rsp_to_media_major_mailbox(msg, BK_OK, APP_MODULE);
}

void lvgl_event_lvcam_lvgl_close_handle(media_mailbox_msg_t *msg)
{
    lv_vendor_stop();
//    lv_qrcode_delete(qr);
    lvgl_disp_enable = 0;
    msg_send_rsp_to_media_major_mailbox(msg, BK_OK, APP_MODULE);
}

void lvgl_event_handle(media_mailbox_msg_t *msg)
{
    switch (msg->event)
    {
        case EVENT_LVGL_OPEN_IND:
            lvgl_event_open_handle(msg);
            break;

        case EVENT_LVGL_CLOSE_IND:
            lvgl_event_close_handle(msg);
            break;

        case EVENT_LVCAM_LVGL_OPEN_IND:
            lvgl_event_lvcam_lvgl_open_handle(msg);
            break;

        case EVENT_LVCAM_LVGL_CLOSE_IND:
            lvgl_event_lvcam_lvgl_close_handle(msg);
            break;

        default:
            break;
    }
}
#endif

#if (CONFIG_SYS_CPU0)
void lvcamera_main_init(void)
{
    bk_err_t ret;

    cli_lvcamera_init();

    ret = media_app_lcd_disp_open((lcd_open_t *)&lcd_open);
    if (ret != BK_OK) {
        os_printf("media_app_pipeline_jdec_disp_open failed\r\n");
        return;
    }

    ret = media_app_lvgl_open((lcd_open_t *)&lcd_open);
    if (ret != BK_OK) {
        os_printf("media_app_lvgl_draw failed\r\n");
        return;
    }

    lvcam_is_open = false;
}
#endif

void user_app_main(void)
{
#if CONFIG_INTEGRATION_DOORBELL
    doorbell_core_init();
#endif
}

int main(void)
{
#if (CONFIG_SYS_CPU0)
    rtos_set_user_app_entry((beken_thread_function_t)user_app_main);
    // bk_set_printf_sync(true);
    // shell_set_log_level(BK_LOG_WARN);
#endif
    bk_init();
    media_service_init();

#if (CONFIG_SYS_CPU0)
    lvcamera_main_init();
#endif

    return 0;
}
