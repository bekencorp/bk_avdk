#include "bk_private/bk_init.h"
#include <components/system.h>
#include <os/os.h>
#include <components/shell_task.h>

#include "cli.h"

#include "lcd_act.h"
#include "media_app.h"
#if CONFIG_LVGL
#include "lv_vendor.h"
#include "lvgl.h"
#endif
#include "driver/drv_tp.h"
#include <driver/lcd.h>
#include "media_service.h"
#include "media_ipc.h"

#define TAG "MAIN"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

extern void user_app_main(void);
extern void rtos_set_user_app_entry(beken_thread_function_t entry);

#define CMDS_COUNT  (sizeof(s_widgets_commands) / sizeof(struct cli_command))

const lcd_open_t lcd_open =
{
    .device_ppi = PPI_160X160,
    .device_name = "gc9d01",
};

void cli_widgets_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    LOGI("%s\r\n", __func__);
}

static const struct cli_command s_widgets_commands[] =
{
    {"widgets", "widgets", cli_widgets_cmd},
};

int cli_widgets_init(void)
{
    return cli_register_commands(s_widgets_commands, CMDS_COUNT);
}

#if (CONFIG_SYS_CPU1)
#include "yuv_encode.h"
#include "modules/avilib.h"
#include "modules/jpeg_decode_sw.h"

static avi_t *avi = NULL;
static uint32_t *video_frame = NULL;
static uint32_t video_num = 0;
static uint32_t video_len = 0;
static uint32_t pos = 0;
static uint16_t *framebuffer = NULL;
static uint16_t *segmentbuffer = NULL;
static uint32_t frame_size = 0;

static lv_obj_t *img = NULL;
static lv_img_dsc_t img_dsc = {
    .header.cf = LV_IMG_CF_TRUE_COLOR,
    .header.always_zero = 0,
    .header.w = 0,
    .header.h = 0,
    .data_size = 0,
    .data = NULL,
};

static void avi_video_frame_parse_to_rgb565(avi_t *AVI, uint32_t frame, uint8_t *src_buf, uint8_t *dst_buf, uint32_t src_size, uint32_t outbuf_size)
{
    sw_jpeg_dec_res_t result;

    AVI_set_video_position(AVI, frame, (long *)&src_size);
    AVI_read_frame(AVI, (char *)src_buf, src_size);

    if (src_size == 0) {
        frame = frame + 1;
        AVI_set_video_position(AVI, frame, (long *)&src_size);
        AVI_read_frame(AVI, (char *)src_buf, src_size);
    }

    bk_jpeg_dec_sw_start(JPEGDEC_BY_FRAME, (uint8_t *)src_buf, (uint8_t *)dst_buf, src_size, outbuf_size, (sw_jpeg_dec_res_t *)&result);
}

static void lv_timer_cb(lv_timer_t * timer)
{
    pos++;
    if (pos >= video_num) {
        pos = 0;
    }

    avi_video_frame_parse_to_rgb565(avi, pos, (uint8_t *)video_frame, (uint8_t *)framebuffer, video_len, frame_size);

    for (int i = 0; i < avi->height; i++) {
        os_memcpy(segmentbuffer + i * (avi->width >> 1), framebuffer + i * avi->width, avi->width);
        os_memcpy(segmentbuffer + (avi->width >> 1) * avi->height + i * (avi->width >> 1), framebuffer + i * avi->width + (avi->width >> 1), avi->width);
    }

    lv_img_set_src(img, &img_dsc);
}

void lvgl_event_handle(media_mailbox_msg_t *msg)
{
    LOGI("%s EVENT_LVGL_OPEN_IND \n", __func__);

    lv_vnd_config_t lv_vnd_config = {0};
    lcd_open_t *lcd_open = (lcd_open_t *)msg->param;
    jd_output_format *format = NULL;

    format = os_malloc(sizeof(jd_output_format));
    if (format == NULL) {
        LOGE("%s %d format malloc fail\r\n", __func__, __LINE__);
        return;
    }

    bk_jpeg_dec_sw_init(NULL, 0);
    format->format = JD_FORMAT_RGB565;
    format->scale = 0;
    format->byte_order = JD_BIG_ENDIAN;
    jd_set_output_format(format);

#ifdef CONFIG_LVGL_USE_PSRAM
#define PSRAM_DRAW_BUFFER ((0x60000000UL) + 5 * 1024 * 1024)
    lv_vnd_config.draw_pixel_size = ppi_to_pixel_x(lcd_open->device_ppi) * ppi_to_pixel_y(lcd_open->device_ppi);
    lv_vnd_config.draw_buf_2_1 = (lv_color_t *)PSRAM_DRAW_BUFFER;
    lv_vnd_config.draw_buf_2_2 = (lv_color_t *)(PSRAM_DRAW_BUFFER + lv_vnd_config.draw_pixel_size * sizeof(lv_color_t));
#else
#define PSRAM_FRAME_BUFFER ((0x60000000UL) + 5 * 1024 * 1024)
    lv_vnd_config.draw_pixel_size = ppi_to_pixel_x(lcd_open->device_ppi) * ppi_to_pixel_y(lcd_open->device_ppi) / 10;
    lv_vnd_config.draw_buf_2_1 = LV_MEM_CUSTOM_ALLOC(lv_vnd_config.draw_pixel_size * sizeof(lv_color_t));
    lv_vnd_config.draw_buf_2_2 = NULL;
    lv_vnd_config.frame_buf_1 = (lv_color_t *)PSRAM_FRAME_BUFFER;
    lv_vnd_config.frame_buf_2 = NULL;//(lv_color_t *)(PSRAM_FRAME_BUFFER + ppi_to_pixel_x(lcd_open->device_ppi) * ppi_to_pixel_y(lcd_open->device_ppi) * sizeof(lv_color_t));
#endif
#if (CONFIG_LCD_SPI_DEVICE_NUM > 1)
    lv_vnd_config.lcd_hor_res = ppi_to_pixel_x(lcd_open->device_ppi);
    lv_vnd_config.lcd_ver_res = ppi_to_pixel_y(lcd_open->device_ppi) * 2;
#else
    lv_vnd_config.lcd_hor_res = ppi_to_pixel_x(lcd_open->device_ppi);
    lv_vnd_config.lcd_ver_res = ppi_to_pixel_y(lcd_open->device_ppi);
#endif
    lv_vnd_config.rotation = ROTATE_NONE;

    lv_vendor_init(&lv_vnd_config);

    lcd_display_open(lcd_open);

#if (CONFIG_TP)
    drv_tp_open(ppi_to_pixel_x(lcd_open->device_ppi), ppi_to_pixel_y(lcd_open->device_ppi), TP_MIRROR_NONE);
#endif

    img_dsc.header.w = lv_vnd_config.lcd_hor_res;
    img_dsc.header.h = lv_vnd_config.lcd_ver_res;
    img_dsc.data_size = img_dsc.header.w * img_dsc.header.h * 2;

    video_frame = os_malloc(30 * 1024);
    if (video_frame == NULL) {
        LOGE("%s %d video_frame malloc fail\r\n", __func__, __LINE__);
        return;
    }

    avi = AVI_open_input_file("3:/1_mjpeg.avi", 1);
    if (avi != NULL) {
        video_num = AVI_video_frames(avi);
        frame_size = avi->width * avi->height * 2;
        LOGI("avi video_num: %d, width: %d, height: %d, frame_size: %d\r\n", video_num, avi->width, avi->height, frame_size);
    } else {
        LOGE("open avi fail\r\n");
        AVI_close(avi);
        os_free(video_frame);
        return;
    }

    framebuffer = psram_malloc(frame_size);
    if (framebuffer == NULL) {
        LOGE("%s %d framebuffer malloc fail\r\n", __func__, __LINE__);
        AVI_close(avi);
        os_free(video_frame);
        return;
    }

    segmentbuffer = psram_malloc(frame_size);
    if (segmentbuffer == NULL) {
        LOGE("%s %d framebuffer malloc fail\r\n", __func__, __LINE__);
        AVI_close(avi);
        os_free(video_frame);
        os_free(framebuffer);
        return;
    }

    avi_video_frame_parse_to_rgb565(avi, pos, (uint8_t *)video_frame, (uint8_t *)framebuffer, video_len, frame_size);

    for (int i = 0; i < avi->height; i++) {
        os_memcpy(segmentbuffer + i * (avi->width >> 1), framebuffer + i * avi->width, avi->width);
        os_memcpy(segmentbuffer + (avi->width >> 1) * avi->height + i * (avi->width >> 1), framebuffer + i * avi->width + (avi->width >> 1), avi->width);
    }

    img_dsc.data = (const uint8_t *)segmentbuffer;

    lv_vendor_disp_lock();
    img = lv_img_create(lv_scr_act());
    lv_img_set_src(img, &img_dsc);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);

    lv_timer_t *timer = lv_timer_create(lv_timer_cb, 60, NULL);
    lv_timer_set_repeat_count(timer, -1);
    lv_vendor_disp_unlock();

    lv_vendor_start();

    msg_send_rsp_to_media_major_mailbox(msg, BK_OK, APP_MODULE);
}
#endif


#if (CONFIG_SYS_CPU0)
void widgets_init(void)
{
    bk_err_t ret;

    cli_widgets_init();

    LOGI("!!!BK7258 LVGL WIDGETS!!!\r\n");

    ret = media_app_lvgl_open((lcd_open_t *)&lcd_open);
    if (ret != BK_OK) {
        LOGE("media_app_lvgl_open failed\r\n");
        return;
    }
}
#endif

void user_app_main(void)
{

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
    widgets_init();
#endif

    return 0;
}

