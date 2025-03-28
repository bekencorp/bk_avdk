#include "bk_private/bk_init.h"
#include <components/system.h>
#include <os/os.h>
#include <components/shell_task.h>

#include "doorbell_comm.h"
#include "media_service.h"

#include "cli.h"

#if (CONFIG_SYS_CPU1) || (CONFIG_SYS_CPU0)
#include "img_service.h"
#include "media_app.h"
#include "media_evt.h"
#include <driver/lcd.h>
#endif

#if (CONFIG_SYS_CPU1 && CONFIG_BLEND_UI)
#include "blend.h"
#endif

extern void user_app_main(void);
extern void rtos_set_user_app_entry(beken_thread_function_t entry);

#if (CONFIG_SYS_CPU0)
#include "doorbell_devices.h"

extern db_device_info_t *db_device_info;

media_camera_device_t camera_device = {
    .type = DVP_CAMERA,
    .format = IMAGE_MJPEG,
    .fps = FPS30,
    .width = 864,
    .height = 480,
    .port = 0,
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

    if (device == NULL) {
        os_printf("%s device is NULL!\r\n", __func__);
        return;
    }

    if (db_device_info->camera_id != device_id) {
        media_app_camera_switch(device);
        db_device_info->camera_id = device_id;
    }
}

void cli_lvcamera_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    uint32_t device_id = 0;

    if (argc < 2) {
        os_printf("[%s] The cmd is lvcam {camera_open|camera_switch} {0|1|2}\r\n", __func__);
        return;
    }
    else
    {
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

#define CMDS_COUNT  (sizeof(s_lvcamera_commands) / sizeof(struct cli_command))

static const struct cli_command s_lvcamera_commands[] =
{
    {"lvcam", "lvcam", cli_lvcamera_cmd},
};

int cli_lvcamera_init(void)
{
    return cli_register_commands(s_lvcamera_commands, CMDS_COUNT);
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
    cli_lvcamera_init();
#endif

#if (CONFIG_SYS_CPU1 && CONFIG_BLEND_UI)
    get_blend_assets_array(blend_assets);
    get_blend_default_array(blend_info);
#endif

    return 0;
}
