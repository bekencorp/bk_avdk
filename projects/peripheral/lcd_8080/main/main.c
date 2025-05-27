#include "bk_private/bk_init.h"
#include <components/system.h>
#include <os/os.h>
#include <components/shell_task.h>
#include "cli.h"
#include <driver/media_types.h>
#include "media_ipc.h"
#include "media_service.h"
#include "media_app.h"


extern void user_app_main(void);
extern void rtos_set_user_app_entry(beken_thread_function_t entry);

meida_ipc_t ipc = NULL;


const lcd_open_t lcd_open =
{
	.device_ppi = PPI_320X480,
	.device_name = "st7796s",
};


#if (CONFIG_SYS_CPU1)
#include <frame_buffer.h>
#include "yuv_encode.h"


static frame_buffer_t *disp_frame = NULL;

static void lcd_fill_rand_color(uint8_t *addr)
{
	uint32_t color_rand = 0;
	uint8_t *p_addr = addr;
    int i = 0;

	color_rand = (uint32_t)rand();

	for( i = 0; i < disp_frame->width * disp_frame->height * 2; i+=2)
	{
		*(uint16_t *)(p_addr + i) = color_rand;
	}
}
#endif

static int bk_mcu_lcd_example_init(uint8_t *data, uint32_t size, void *param)
{
#if (CONFIG_SYS_CPU1)
    int ret;
    lcd_open_t *lcd_open = (lcd_open_t *)data;

    while(1) 
    {
        disp_frame = frame_buffer_display_malloc(ppi_to_pixel_x(lcd_open->device_ppi) * ppi_to_pixel_y(lcd_open->device_ppi) * 2);
        if (disp_frame == NULL) {
            os_printf("%s %d disp_frame malloc fail\r\n", __func__, __LINE__);
            return -1;
        }

        disp_frame->fmt = PIXEL_FMT_RGB565;
        disp_frame->width = ppi_to_pixel_x(lcd_open->device_ppi);
        disp_frame->height = ppi_to_pixel_y(lcd_open->device_ppi);
        disp_frame->length = disp_frame->width * disp_frame->height * 2;

        lcd_fill_rand_color(disp_frame->frame);
        ret = lcd_display_frame_request(disp_frame);
        if (ret != BK_OK)
        {
            os_printf("lcd_display_frame_request fail\r\n");
            frame_buffer_display_free(disp_frame);
            disp_frame = NULL;
            continue;
        }
        rtos_delay_milliseconds(1000);
    }
#endif

    return 0;
}

#if (CONFIG_SYS_CPU0)
void bk_mcu_lcd_main(void)
{
    bk_err_t ret;

    os_printf("!!!BK7258 LCD mcu DISPLAY EXAMPLE!!!\r\n");

    ret = media_app_lcd_pipeline_disp_open((lcd_open_t *)&lcd_open);
    if (ret != BK_OK)
    {
        os_printf("media_app_lcd_pipeline_open failed\r\n");
        return;
    }

    ret = media_ipc_send(&ipc, (void*)&lcd_open, sizeof(lcd_open), MIPC_CHAN_SEND_FLAG_SYNC);
    if (ret != BK_OK)
    {
        os_printf("media_ipc_send failed\n");
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

#if (CONFIG_SYS_CPU0 || CONFIG_SYS_CPU1)

    media_ipc_chan_cfg_t cfg = {0};
    cfg.cb = bk_mcu_lcd_example_init;
    cfg.name = "app";
    cfg.param = NULL;

    if (media_ipc_channel_open(&ipc, &cfg) != BK_OK)
    {
        os_printf("open ipc failed\n");
        return -1;
    }
#endif

#if (CONFIG_SYS_CPU0)
    bk_mcu_lcd_main();
#endif

	return 0;
}



